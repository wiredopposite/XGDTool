#include "UnityTool/UnityTool.h"
#include "Common/Utils.h"
#include "ImageWriter/GoDWriter/GoD_live_header.h"
#include "ImageWriter/GoDWriter/GoDWriter.h"

GoDWriter::GoDWriter(std::shared_ptr<ImageReader> image_reader, const ScrubType scrub_type, const bool allowed_media_patch)
    :   image_reader_(image_reader),
        scrub_type_(scrub_type),
        allowed_media_patch_(allowed_media_patch) {

    if (scrub_type_ == ScrubType::FULL) {
        avl_tree_ = std::make_unique<AvlTree>(image_reader_->name(), image_reader_->directory_entries());
    }
}

GoDWriter::GoDWriter(const std::filesystem::path& in_dir_path, const bool allowed_media_patch)
    :   allowed_media_patch_(allowed_media_patch), 
        in_dir_path_(in_dir_path),
        avl_tree_(std::make_unique<AvlTree>(in_dir_path.filename().string(), in_dir_path)) {
}

GoDWriter::~GoDWriter() {
    for (auto& file : out_files_) {
        file->close();
    }
    out_files_.clear();
}

std::vector<std::filesystem::path> GoDWriter::convert(const std::filesystem::path& out_god_directory) {
    std::unique_ptr<ExeTool> exe_tool;

    if (image_reader_) {
        exe_tool = std::make_unique<ExeTool>(*image_reader_, image_reader_->executable_entry().path);

    } else {
        std::filesystem::path exe_path;

        for (const auto& entry : std::filesystem::directory_iterator(in_dir_path_)) {
            if (entry.is_regular_file()) {
                if (StringUtils::case_insensitive_search(entry.path().filename().string(), "default.xbe")) {
                    exe_path = entry.path();
                    break;
                } else if (StringUtils::case_insensitive_search(entry.path().filename().string(), "default.xex")) {
                    exe_path = entry.path();
                    break;
                }
            }
        }

        if (exe_path.empty()) {
            throw XGDException(ErrCode::FILE_OPEN, HERE(), "Failed to find default.xbe or default.xex in directory: " + in_dir_path_.string());
        }

        exe_tool = std::make_unique<ExeTool>(exe_path);
    }

    std::string dir_name;

    switch (exe_tool->platform()) {
        case Platform::OGX:
            dir_name = StringUtils::uint32_to_hex_string(GoD::Type::ORIGINAL_XBOX);
            break;
        case Platform::X360:
            dir_name = StringUtils::uint32_to_hex_string(GoD::Type::GAMES_ON_DEMAND);
            break;
        default:
            throw XGDException(ErrCode::MISC, HERE(), "Unknown platform");
    }

    std::string unique_name = create_unique_name(exe_tool->xex_cert());

    std::filesystem::path out_data_directory = out_god_directory / dir_name / (unique_name + ".data");
    std::filesystem::path live_header_path = out_god_directory / dir_name / unique_name;

    create_directory(out_data_directory);

    std::vector<std::filesystem::path> out_part_paths;

    if (avl_tree_) {
        out_part_paths = write_data_files_from_avl(out_data_directory);
    } else {
        out_part_paths = write_data_files(out_data_directory, scrub_type_ == ScrubType::PARTIAL);
    }

    write_hashtables(out_part_paths);
    SHA1Hash final_mht_hash = finalize_hashtables(out_part_paths);

    write_live_header(live_header_path, out_part_paths, exe_tool, final_mht_hash);

    return { out_god_directory };
}

std::vector<std::filesystem::path> GoDWriter::write_data_files_from_avl(const std::filesystem::path& out_data_directory) {
    AvlTree& avl_tree = *avl_tree_;
    uint64_t out_iso_size = avl_tree.out_iso_size();

    prog_total_ = avl_tree.total_bytes();
    prog_processed_ = 0;

    uint32_t total_out_data_blocks = static_cast<uint32_t>(out_iso_size / GoD::BLOCK_SIZE);
    uint32_t total_out_parts = total_out_data_blocks / GoD::DATA_BLOCKS_PER_PART;

    std::vector<std::filesystem::path> out_part_paths;

    for (uint32_t i = 0; i < total_out_parts; ++i) {
        std::string out_name = "Data";

        if (i < 10) {
            out_name += "000" + std::to_string(i);
        } else if (i < 100) {
            out_name += "00" + std::to_string(i);
        } else if (i < 1000) {
            out_name += "0" + std::to_string(i);
        } else {
            out_name += std::to_string(i);
        }

        out_part_paths.push_back(out_data_directory / out_name);

        out_files_.push_back(std::make_unique<std::ofstream>(out_part_paths.back(), std::ios::binary));
        if (!out_files_.back()->is_open()) {
            throw std::runtime_error("Failed to open output file: " + out_data_directory.string());
        }
    }

    write_header(avl_tree);

    Remap remap = remap_offset(avl_tree.root()->start_sector * Xiso::SECTOR_SIZE);
    padded_seek(out_files_[remap.file_index], remap.offset);
    current_out_file_ = remap.file_index;

    if (image_reader_) {
        AvlTree::traverse(avl_tree.root(), AvlTree::TraversalMethod::PREFIX, [this](AvlTree::Node* node, void* context, int depth) {
            write_tree(node, static_cast<ImageReader*>(context), depth);
        }, image_reader_.get(), 0);

    } else { // Reading from directory
        AvlTree::traverse(avl_tree.root(), AvlTree::TraversalMethod::PREFIX, [this](AvlTree::Node* node, void* context, int depth) {
            write_tree(node, nullptr, depth);
        }, nullptr, 0);
    }

    pad_to_file_modulus(out_iso_size);

    for (auto& file : out_files_) {
        file->close();
    }
    out_files_.clear();

    return out_part_paths;
}

std::vector<std::filesystem::path> GoDWriter::write_data_files(const std::filesystem::path& out_data_directory, const bool scrub) {
    ImageReader& image_reader = *image_reader_;
    uint32_t sector_offset = static_cast<uint32_t>(image_reader.image_offset() / Xiso::SECTOR_SIZE);
    uint32_t end_sector = image_reader.total_sectors();
    const std::unordered_set<uint32_t>* data_sectors;

    if (scrub) {
        XGDLog() << "Getting data sectors" << XGDLog::Endl;
        data_sectors = &image_reader.data_sectors();
        end_sector = std::min(image_reader.max_data_sector() + 1, end_sector);
    }

    uint32_t total_out_sectors = end_sector - sector_offset;
    uint32_t total_out_data_blocks = total_out_sectors / (GoD::BLOCK_SIZE / Xiso::SECTOR_SIZE);
    uint32_t total_out_parts = total_out_data_blocks / GoD::DATA_BLOCKS_PER_PART;

    std::vector<std::filesystem::path> out_part_paths;

    for (uint32_t i = 0; i < total_out_parts; ++i) {
        std::string out_name = "Data";

        if (i < 10) {
            out_name += "000" + std::to_string(i);
        } else if (i < 100) {
            out_name += "00" + std::to_string(i);
        } else if (i < 1000) {
            out_name += "0" + std::to_string(i);
        } else {
            out_name += std::to_string(i);
        }

        out_part_paths.push_back(out_data_directory / out_name);

        out_files_.push_back(std::make_unique<std::ofstream>(out_part_paths.back(), std::ios::binary));
        if (!out_files_.back()->is_open()) {
            throw std::runtime_error("Failed to open output file: " + out_data_directory.string());
        }
    }

    uint32_t current_sector = sector_offset;
    std::vector<char> buffer(Xiso::SECTOR_SIZE);

    XGDLog() << "Writing data files" << XGDLog::Endl;

    while (current_sector < end_sector) {
        bool write_sector = true;

        if (scrub && image_reader.platform() == Platform::OGX) {
            write_sector = data_sectors->find(current_sector) != data_sectors->end();
        }

        if (write_sector) {
            image_reader.read_sector(current_sector, buffer.data());
        } else {
            std::fill(buffer.begin(), buffer.end(), 0);
        }

        Remap remapped = remap_sector(current_sector);
        padded_seek(out_files_[remapped.file_index], remapped.offset);

        out_files_[remapped.file_index]->write(buffer.data(), buffer.size());

        if (out_files_[remapped.file_index]->fail()) {
            throw XGDException(ErrCode::FILE_WRITE, HERE());
        }

        current_sector++;

        XGDLog().print_progress(current_sector - sector_offset, total_out_sectors);
    }

    for (auto& file : out_files_) {
        file->close();
    }
    out_files_.clear();
    return out_part_paths;
}

void GoDWriter::write_hashtables(const std::vector<std::filesystem::path>& part_paths) {
    /*  For each sub hashtable, each of it's following data blocks are hashed,
        all those hashes are then written to the sub hashtable, 
        each sub hashtable block's hash is then written to the master hashtable */

    auto parts_processed = 0;

    XGDLog() << "Writing hash tables" << XGDLog::Endl;

    for (auto& part_path : part_paths) {
        uint32_t blocks_left = static_cast<uint32_t>(std::filesystem::file_size(part_path) / GoD::BLOCK_SIZE);
        uint32_t sub_hashtables = (blocks_left - 1) / (GoD::DATA_BLOCKS_PER_SHT + 1);

        std::fstream cur_file(part_path, std::ios::binary | std::ios::in | std::ios::out);
        if (!cur_file.is_open()) {
            throw XGDException(ErrCode::FILE_OPEN, HERE(), part_path.string());
        }

        cur_file.seekp(GoD::BLOCK_SIZE, std::ios::beg);
        blocks_left--;

        std::vector<SHA1Hash> master_hashtable;

        for (uint32_t i = 0; i < sub_hashtables; ++i) {
            uint32_t blocks_in_sht = 0;
            std::vector<char> block_buffer(GoD::BLOCK_SIZE, 0);
            std::vector<SHA1Hash> sub_hashtable;

            cur_file.seekp(GoD::BLOCK_SIZE, std::ios::cur);
            blocks_left--;

            while (blocks_in_sht < GoD::DATA_BLOCKS_PER_SHT && 0 < blocks_left) {
                cur_file.read(block_buffer.data(), block_buffer.size());
                if (cur_file.fail()) {
                    throw XGDException(ErrCode::FILE_READ, HERE());
                }

                sub_hashtable.push_back(compute_sha1(block_buffer.data(), block_buffer.size()));
                blocks_in_sht++;
                blocks_left--;
            }

            uint64_t position = cur_file.tellp();

            cur_file.seekp((i * (GoD::DATA_BLOCKS_PER_SHT + 1) * GoD::BLOCK_SIZE) + GoD::BLOCK_SIZE, std::ios::beg);
            cur_file.write(reinterpret_cast<const char*>(sub_hashtable.data()), sub_hashtable.size() * sizeof(SHA1Hash));
            cur_file.seekp(position, std::ios::beg);

            // Zero padded to block size, then hashed 
            std::vector<char> sub_hashtable_buffer(GoD::BLOCK_SIZE, 0);
            std::memcpy(sub_hashtable_buffer.data(), sub_hashtable.data(), std::min(sub_hashtable.size() * sizeof(SHA1Hash), static_cast<size_t>(GoD::BLOCK_SIZE)));
            master_hashtable.push_back(compute_sha1(sub_hashtable_buffer.data(), GoD::BLOCK_SIZE));

            if (blocks_left == 0) {
                break;
            }
        }

        cur_file.seekp(0, std::ios::beg);
        cur_file.write(reinterpret_cast<const char*>(master_hashtable.data()), master_hashtable.size() * sizeof(SHA1Hash));
        cur_file.close();

        XGDLog().print_progress(parts_processed++, part_paths.size() - 1);
    }
}

GoDWriter::SHA1Hash GoDWriter::finalize_hashtables(const std::vector<std::filesystem::path>& part_paths) {
    /*  Each Data file's master hashtable block is hashed, then that's 
        written to the end of the previous Data file's master hashtable. 
        The first Data file's master hashtable hash (final_mht_hash) 
        is written to the Live header file */

    SHA1Hash final_mht_hash;

    for (size_t i = part_paths.size() - 1; i > 0; i--) {
        std::ifstream current_part = std::ifstream(part_paths[i], std::ios::binary);
        if (!current_part.is_open()) {
            throw XGDException(ErrCode::FILE_OPEN, HERE(), part_paths[i].string());
        }

        std::fstream prev_part = std::fstream(part_paths[i - 1], std::ios::binary | std::ios::in | std::ios::out);
        if (!prev_part.is_open()) {
            throw XGDException(ErrCode::FILE_OPEN, HERE(), part_paths[i - 1].string());
        }

        std::vector<char> current_hashtable(GoD::BLOCK_SIZE, 0);
        current_part.read(current_hashtable.data(), GoD::BLOCK_SIZE);

        SHA1Hash current_mht_hash = compute_sha1(reinterpret_cast<const char*>(current_hashtable.data()), GoD::BLOCK_SIZE);

        prev_part.seekp(SHA_DIGEST_LENGTH * GoD::SHT_PER_MHT, std::ios::beg);
        prev_part.write(reinterpret_cast<const char*>(&current_mht_hash.hash), SHA_DIGEST_LENGTH);

        if (i == 1) {
            std::vector<char> last_mht(GoD::BLOCK_SIZE, 0);
            prev_part.seekg(0, std::ios::beg);
            prev_part.read(last_mht.data(), GoD::BLOCK_SIZE);
            final_mht_hash = compute_sha1(last_mht.data(), GoD::BLOCK_SIZE);
        }

        current_part.close();
        prev_part.close();
    }

    return final_mht_hash;
}

void GoDWriter::write_entry(AvlTree::Node* node, void* context, int depth) {
    if (current_dir_start_ != node->directory_start) {
        current_dir_start_ = node->directory_start;
        current_dir_position_ = 0;

        Remap remap = remap_offset(node->directory_start);
        padded_seek(out_files_[remap.file_index], remap.offset);
        current_out_file_ = remap.file_index;
    }

    Xiso::DirectoryEntry::Header entry_header;
    entry_header.left_offset  = node->left_child ? static_cast<uint16_t>(node->left_child->offset / sizeof(uint32_t)) : 0;
    entry_header.right_offset = node->right_child ? static_cast<uint16_t>(node->right_child->offset / sizeof(uint32_t)) : 0;
    entry_header.start_sector = static_cast<uint32_t>(node->start_sector);
    entry_header.file_size    = static_cast<uint32_t>(node->file_size + (node->subdirectory ? ((Xiso::SECTOR_SIZE - (node->file_size % Xiso::SECTOR_SIZE)) % Xiso::SECTOR_SIZE) : 0));
    entry_header.attributes   = node->subdirectory ? Xiso::ATTRIBUTE_DIRECTORY : Xiso::ATTRIBUTE_FILE;
    entry_header.name_length  = static_cast<uint8_t>(std::min(node->filename.size(), static_cast<size_t>(UINT8_MAX)));

    EndianUtils::little_16(entry_header.left_offset);
    EndianUtils::little_16(entry_header.right_offset);
    EndianUtils::little_32(entry_header.start_sector);
    EndianUtils::little_32(entry_header.file_size);

    size_t padding_len = node->offset - current_dir_position_;
    if ((node->offset - current_dir_position_) < 0) {
        throw XGDException(ErrCode::MISC, HERE(), "Negative padding length");
    }

    size_t write_size = padding_len + sizeof(Xiso::DirectoryEntry::Header) + entry_header.name_length;
    std::vector<char> entry_buffer(write_size, Xiso::PAD_BYTE);

    std::memcpy(entry_buffer.data() + padding_len, &entry_header, sizeof(Xiso::DirectoryEntry::Header));
    std::memcpy(entry_buffer.data() + padding_len + sizeof(Xiso::DirectoryEntry::Header), node->filename.c_str(), entry_header.name_length);

    /*  All this is to make sure we don't write into the next sector 
        without knowing it's in a data block, and not some area reserved
        for a hashtable or past the end of the Data file */

    size_t position_in_sector = current_dir_position_ % Xiso::SECTOR_SIZE;
    size_t first_write_size = std::min(write_size, Xiso::SECTOR_SIZE - position_in_sector);

    Remap remapped = remap_offset(node->directory_start + current_dir_position_);
    padded_seek(out_files_[remapped.file_index], remapped.offset);

    out_files_[remapped.file_index]->write(entry_buffer.data(), first_write_size);
    current_dir_position_ += first_write_size;

    if (first_write_size < write_size) {
        remapped = remap_offset(node->directory_start + current_dir_position_);
        padded_seek(out_files_[remapped.file_index], remapped.offset);

        out_files_[remapped.file_index]->write(entry_buffer.data() + first_write_size, write_size - first_write_size);
        current_dir_position_ += write_size - first_write_size;
    }

    current_out_file_ = remapped.file_index;

    for (auto& file : out_files_) {
        if (file->fail()) {
            throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write directory entry: " + node->filename);
        }
    }
}

void GoDWriter::write_file_dir(AvlTree::Node* node, void* context, int depth) {
    if (node->subdirectory) {
        return;
    }

    Remap remapped = remap_sector(node->start_sector);
    padded_seek(out_files_[remapped.file_index], remapped.offset);

    std::ifstream in_file(node->path, std::ios::binary);
    if (!in_file.is_open()) {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), "Failed to open file: " + node->path.string());
    }

    if (allowed_media_patch_ && node->filename.size() > 4 && StringUtils::case_insensitive_search(node->filename, ".xbe")) {
        ExeTool exe_tool(node->path);
        Xbe::Cert xbe_cert = exe_tool.xbe_cert();
        exe_tool.patch_allowed_media(xbe_cert);

        auto cert_offset = (node->start_sector * Xiso::SECTOR_SIZE) + exe_tool.cert_offset();
        auto cert_pos_in_sector = cert_offset % Xiso::SECTOR_SIZE;
        auto current_sector = node->start_sector;
        auto cert_sector = current_sector + (cert_offset / Xiso::SECTOR_SIZE);
        auto bytes_remaining = static_cast<uint32_t>(node->file_size);
        std::vector<char> buffer(Xiso::SECTOR_SIZE, 0);

        while (bytes_remaining > 0) {
            if (current_sector == cert_sector) {
                auto read_size = std::min(bytes_remaining, Xiso::SECTOR_SIZE * 2);
                std::vector<char> cert_buffer(read_size);

                in_file.read(cert_buffer.data(), read_size);
                if (in_file.fail()) {
                    throw XGDException(ErrCode::FILE_READ, HERE(), "Failed to read file data: " + node->path.string());
                }

                std::memcpy(cert_buffer.data() + cert_pos_in_sector, &xbe_cert, sizeof(Xbe::Cert));

                remapped = remap_sector(current_sector);
                padded_seek(out_files_[remapped.file_index], remapped.offset);

                auto write_size = std::min(read_size, Xiso::SECTOR_SIZE);
                out_files_[remapped.file_index]->write(cert_buffer.data(), write_size);

                current_sector++;

                if (write_size < read_size) {
                    remapped = remap_sector(current_sector);
                    padded_seek(out_files_[remapped.file_index], remapped.offset);

                    out_files_[remapped.file_index]->write(cert_buffer.data() + Xiso::SECTOR_SIZE, read_size - Xiso::SECTOR_SIZE);
                    current_sector++;
                }

                bytes_remaining -= read_size;

                XGDLog().print_progress(prog_processed_ += read_size, prog_total_);
                continue;
            }

            auto read_size = std::min(bytes_remaining, Xiso::SECTOR_SIZE);

            in_file.read(buffer.data(), read_size);
            if (in_file.fail()) {
                throw XGDException(ErrCode::FILE_READ, HERE(), "Failed to read file data: " + node->path.string());
            }

            remapped = remap_sector(current_sector);
            padded_seek(out_files_[remapped.file_index], remapped.offset);

            out_files_[remapped.file_index]->write(buffer.data(), read_size * Xiso::SECTOR_SIZE);
            if (out_files_[remapped.file_index]->fail()) {
                throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write file data: " + node->filename);
            }

            current_sector++;

            XGDLog().print_progress(prog_processed_ += read_size * Xiso::SECTOR_SIZE, prog_total_);
        }
    } else {
        uint32_t bytes_remaining = static_cast<uint32_t>(node->file_size);
        uint64_t current_write_sector = node->start_sector;
        std::vector<char> buffer(Xiso::SECTOR_SIZE, 0);

        while (bytes_remaining > 0) {
            auto read_size = std::min(bytes_remaining, Xiso::SECTOR_SIZE);

            in_file.read(buffer.data(), read_size);

            remapped = remap_sector(current_write_sector);
            padded_seek(out_files_[remapped.file_index], remapped.offset);

            out_files_[remapped.file_index]->write(buffer.data(), read_size);
            if (out_files_[remapped.file_index]->fail()) {
                throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write file data: " + node->filename);
            }

            current_write_sector++;
            bytes_remaining -= read_size;

            XGDLog().print_progress(prog_processed_ += read_size, prog_total_);
        }
    }

    in_file.close();
    current_out_file_ = remapped.file_index;

    if ((node->file_size + (node->start_sector * Xiso::SECTOR_SIZE)) != to_iso_offset(out_files_[current_out_file_]->tellp(), current_out_file_)) {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "File write size mismatch: " + node->filename);
    }

    if (out_files_[current_out_file_]->tellp() % Xiso::SECTOR_SIZE) {
        auto padding = Xiso::SECTOR_SIZE - (out_files_[current_out_file_]->tellp() % Xiso::SECTOR_SIZE);
        std::vector<char> padding_buffer(padding, 0);
        out_files_[current_out_file_]->write(padding_buffer.data(), padding);
    }
}

void GoDWriter::write_file(AvlTree::Node* node, ImageReader* reader, int depth) {
    if (node->subdirectory) {
        return;
    }

    Remap remapped = remap_sector(node->start_sector);
    padded_seek(out_files_[remapped.file_index], remapped.offset);

    if (allowed_media_patch_ && node->filename.size() > 4 && StringUtils::case_insensitive_search(node->filename, ".xbe")) {
        ExeTool exe_tool(*reader, node->path);
        Xbe::Cert xbe_cert = exe_tool.xbe_cert();
        exe_tool.patch_allowed_media(xbe_cert);

        auto cert_offset = exe_tool.exe_offset() + exe_tool.cert_offset();
        auto cert_pos_in_sector = cert_offset % Xiso::SECTOR_SIZE;
        auto current_read_sector = static_cast<uint32_t>((reader->image_offset() / Xiso::SECTOR_SIZE) + node->start_sector);
        auto cert_sector = current_read_sector + (cert_offset / Xiso::SECTOR_SIZE);
        auto bytes_remaining = static_cast<uint32_t>(node->file_size);
        std::vector<char> buffer(Xiso::SECTOR_SIZE, 0);

        while (bytes_remaining > 0) {
            if (current_read_sector == cert_sector) {
                auto read_size = std::min(bytes_remaining, Xiso::SECTOR_SIZE * 2);
                std::vector<char> cert_buffer(Xiso::SECTOR_SIZE * 2);

                reader->read_sector(current_read_sector, cert_buffer.data());
                reader->read_sector(current_read_sector + 1, cert_buffer.data() + Xiso::SECTOR_SIZE);

                std::memcpy(cert_buffer.data() + cert_pos_in_sector, &xbe_cert, sizeof(Xbe::Cert));

                remapped = remap_sector(current_read_sector);
                padded_seek(out_files_[remapped.file_index], remapped.offset);

                auto write_size = std::min(read_size, Xiso::SECTOR_SIZE);
                out_files_[remapped.file_index]->write(cert_buffer.data(), write_size);

                current_read_sector++;

                if (write_size < read_size) {
                    remapped = remap_sector(current_read_sector + 1);
                    padded_seek(out_files_[remapped.file_index], remapped.offset);

                    out_files_[remapped.file_index]->write(cert_buffer.data() + write_size, read_size - write_size);
                    current_read_sector++;
                }

                if (out_files_[remapped.file_index]->fail()) {
                    throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write file data: " + node->filename);
                }

                bytes_remaining -= read_size;

                XGDLog().print_progress(prog_processed_ += read_size, prog_total_);
                continue;
            }

            auto read_size = std::min(bytes_remaining, Xiso::SECTOR_SIZE);

            reader->read_sector(current_read_sector, buffer.data());

            remapped = remap_sector(current_read_sector);
            padded_seek(out_files_[remapped.file_index], remapped.offset);

            out_files_[remapped.file_index]->write(buffer.data(), read_size * Xiso::SECTOR_SIZE);
            if (out_files_[remapped.file_index]->fail()) {
                throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write file data: " + node->filename);
            }

            current_read_sector++;

            XGDLog().print_progress(prog_processed_ += read_size * Xiso::SECTOR_SIZE, prog_total_);
        }
    } else {
        uint32_t bytes_remaining = static_cast<uint32_t>(node->file_size);
        uint64_t read_position = reader->image_offset() + (node->old_start_sector * Xiso::SECTOR_SIZE);
        uint64_t current_write_sector = node->start_sector;
        std::vector<char> buffer(Xiso::SECTOR_SIZE, 0);

        while (bytes_remaining > 0) {
            auto read_size = std::min(bytes_remaining, Xiso::SECTOR_SIZE);

            reader->read_bytes(read_position, read_size, buffer.data());

            remapped = remap_sector(current_write_sector);
            padded_seek(out_files_[remapped.file_index], remapped.offset);

            out_files_[remapped.file_index]->write(buffer.data(), read_size);
            if (out_files_[remapped.file_index]->fail()) {
                throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write file data: " + node->filename);
            }

            current_write_sector++;
            bytes_remaining -= read_size;
            read_position += read_size;

            XGDLog().print_progress(prog_processed_ += read_size, prog_total_);
        }
    }

    current_out_file_ = remapped.file_index;

    if ((node->file_size + (node->start_sector * Xiso::SECTOR_SIZE)) != to_iso_offset(out_files_[current_out_file_]->tellp(), current_out_file_)) {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "File write size mismatch: " + node->filename);
    }

    if (out_files_[current_out_file_]->tellp() % Xiso::SECTOR_SIZE) {
        auto padding = Xiso::SECTOR_SIZE - (out_files_[current_out_file_]->tellp() % Xiso::SECTOR_SIZE);
        std::vector<char> padding_buffer(padding, 0);
        out_files_[current_out_file_]->write(padding_buffer.data(), padding);
    }
}

void GoDWriter::write_tree(AvlTree::Node* node, ImageReader* image_reader, int depth) {
    if (!node->subdirectory) {
        return;
    }

    if (node->subdirectory != EMPTY_SUBDIRECTORY) {
        if (image_reader) {
            AvlTree::traverse(node->subdirectory, AvlTree::TraversalMethod::PREFIX, [this](AvlTree::Node* node, void* context, int depth) {
                write_file(node, static_cast<ImageReader*>(context), depth);
            }, image_reader, 0);

        } else { // Reading from directory
            AvlTree::traverse(node->subdirectory, AvlTree::TraversalMethod::PREFIX, [this](AvlTree::Node* node, void* context, int depth) {
                write_file_dir(node, static_cast<ImageReader*>(context), depth);
            }, nullptr, 0);
        }
        
        AvlTree::traverse(node->subdirectory, AvlTree::TraversalMethod::PREFIX, [this](AvlTree::Node* node, void* context, int depth) {
            write_tree(node, static_cast<ImageReader*>(context), depth);
        }, image_reader, 0);

        Remap remap = remap_sector(node->start_sector);
        padded_seek(out_files_[remap.file_index], remap.offset);
        current_out_file_ = remap.file_index;

        AvlTree::traverse(node->subdirectory, AvlTree::TraversalMethod::PREFIX, [this](AvlTree::Node* node, void* context, int depth) {
            write_entry(node, context, depth);
        }, nullptr, 0);

        if (out_files_[current_out_file_]->tellp() % Xiso::SECTOR_SIZE != 0) {
            size_t padding_len = Xiso::SECTOR_SIZE - (out_files_[current_out_file_]->tellp() % Xiso::SECTOR_SIZE);
            std::vector<char> pad_sector(padding_len, Xiso::PAD_BYTE);
            out_files_[current_out_file_]->write(pad_sector.data(), padding_len);
        }

    } else {
        std::vector<char> pad_sector(Xiso::SECTOR_SIZE, Xiso::PAD_BYTE);
        Remap remap = remap_sector(node->start_sector);
        padded_seek(out_files_[remap.file_index], remap.offset);

        out_files_[remap.file_index]->write(pad_sector.data(), Xiso::SECTOR_SIZE);
        current_out_file_ = remap.file_index;
        
        if (out_files_[remap.file_index]->fail()) {
            throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write padding sector");
        }
    }
}

void GoDWriter::pad_to_file_modulus(const uint64_t& out_iso_size) {
    out_files_.back()->seekp(0, std::ios::end);

    // Padding after this will be divisible by sector size
    if ((out_files_.back()->tellp() % Xiso::SECTOR_SIZE) != 0) {
        auto padding = Xiso::SECTOR_SIZE - (out_files_.back()->tellp() % Xiso::SECTOR_SIZE);
        std::vector<char> padding_buffer(padding, 0);
        out_files_.back()->write(padding_buffer.data(), padding);
    }

    uint32_t current_index = static_cast<uint32_t>(out_files_.size() - 1);
    uint64_t modulus_padding = out_iso_size - to_iso_offset(out_files_[current_index]->tellp(), current_index);
    
    for (auto i = 0; i < (modulus_padding / Xiso::SECTOR_SIZE); ++i) {
        std::vector<char> padding_buffer(Xiso::SECTOR_SIZE, 0);

        uint64_t xiso_offset = to_iso_offset(out_files_[current_index]->tellp(), current_index);
        Remap remap = remap_offset(xiso_offset);

        padded_seek(out_files_[remap.file_index], remap.offset);
        out_files_[remap.file_index]->write(padding_buffer.data(), Xiso::SECTOR_SIZE);
        current_index = remap.file_index;
    }

    for (auto& file : out_files_) {
        if (file->fail()) {
            throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write final padding");
        }
    }
}

void GoDWriter::write_header(AvlTree& avl_tree) {
    Xiso::Header xiso_header(   static_cast<uint32_t>(avl_tree.root()->start_sector),
                                static_cast<uint32_t>(avl_tree.root()->file_size),
                                static_cast<uint32_t>(avl_tree.out_iso_size() / Xiso::SECTOR_SIZE));
    
    uint32_t header_sectors = static_cast<uint32_t>(sizeof(Xiso::Header) / Xiso::SECTOR_SIZE);
    Remap remap;

    for (uint32_t i = 0; i < header_sectors; ++i) {
        remap = remap_sector(i);
        padded_seek(out_files_[remap.file_index], remap.offset);
        out_files_[remap.file_index]->write(reinterpret_cast<const char*>(&xiso_header) + (i * Xiso::SECTOR_SIZE), Xiso::SECTOR_SIZE);
    }

    // uint32_t padding_sectors = avl_tree.root()->start_sector - header_sectors;
    // std::vector<char> padding_buffer(Xiso::SECTOR_SIZE, 0);

    // for (uint32_t i = 0; i < padding_sectors; ++i) {
    //     remap = remap_sector(i + header_sectors);
    //     padded_seek(out_files_[remap.file_index], remap.offset);
    //     out_files_[remap.file_index]->write(padding_buffer.data(), Xiso::SECTOR_SIZE);
    // }

    for (auto& file : out_files_) {
        if (file->fail()) {
            throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write XISO header");
        }
    }

    current_out_file_ = remap.file_index;
}

void GoDWriter::write_live_header(const std::filesystem::path& out_header_path, const std::vector<std::filesystem::path>& out_part_paths, std::unique_ptr<ExeTool>& exe_tool, const SHA1Hash& final_mht_hash) {
    std::fstream out_file(out_header_path, std::ios::in | std::ios::out | std::ios::app);
    if (!out_file.is_open()) {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), out_header_path.string());
    }

    out_file.write(reinterpret_cast<const char*>(EMPTY_LIVE_HEADER), EMPTY_LIVE_HEADER_SIZE);
    if (out_file.fail()) {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write empty live header");
    }

    out_file.seekp(0x354, std::ios::beg);
    out_file.write(reinterpret_cast<const char*>(&exe_tool->xex_cert().media_id), sizeof(uint32_t));
    out_file.seekp(0x360, std::ios::beg);
    out_file.write(reinterpret_cast<const char*>(&exe_tool->xex_cert().title_id), sizeof(uint32_t));
    out_file.write(reinterpret_cast<const char*>(&exe_tool->xex_cert().platform), sizeof(uint8_t));
    out_file.write(reinterpret_cast<const char*>(&exe_tool->xex_cert().executable_type), sizeof(uint8_t));
    out_file.write(reinterpret_cast<const char*>(&exe_tool->xex_cert().disc_number), sizeof(uint8_t));
    out_file.write(reinterpret_cast<const char*>(&exe_tool->xex_cert().disc_count), sizeof(uint8_t));

    uint64_t parts_total_size = 0;
    for (auto& part_path : out_part_paths) {
        parts_total_size += static_cast<uint32_t>(std::filesystem::file_size(part_path));
    }

    uint32_t parts_written_size = static_cast<uint32_t>(parts_total_size / 0x100);
    uint32_t part_count = static_cast<uint32_t>(out_part_paths.size());
    uint32_t content_type = (exe_tool->platform() == Platform::X360) ? GoD::Type::GAMES_ON_DEMAND : GoD::Type::ORIGINAL_XBOX;

    EndianUtils::little_32(part_count);
    EndianUtils::big_32(parts_written_size);
    EndianUtils::big_32(content_type);

    out_file.seekp(0x344, std::ios::beg);
    out_file.write(reinterpret_cast<const char*>(&content_type), sizeof(uint32_t));

    out_file.seekp(0x37D, std::ios::beg);
    out_file.write(reinterpret_cast<const char*>(&final_mht_hash.hash), SHA_DIGEST_LENGTH);
    
    out_file.seekp(0x3A0, std::ios::beg);
    out_file.write(reinterpret_cast<const char*>(&part_count), sizeof(uint32_t));
    out_file.write(reinterpret_cast<const char*>(&parts_written_size), sizeof(uint32_t));

    UnityTool unity_tool; 
    std::string title_name;
    std::vector<char> title_icon;

    if (!offline_mode_ && 
        unity_tool.internet_connected() &&
        (title_name = unity_tool.get_title_name(exe_tool->title_id())) != unity_tool.error()) {
        unity_tool.get_title_icon(exe_tool->title_id(), title_icon);
    } else {
        title_name = out_header_path.parent_path().parent_path().filename().string();
        title_name.substr(0, title_name.find_last_of(' ['));
    }

    std::vector<char16_t> title_name_utf16 = StringUtils::utf8_to_utf16(title_name);
    size_t title_name_size = std::min(title_name_utf16.size() * sizeof(char16_t), static_cast<size_t>(80));
    
    out_file.seekp(0x411 + 1, std::ios::beg);
    out_file.write(reinterpret_cast<const char*>(title_name_utf16.data()), title_name_size);
    out_file.seekp(0x1691 + 1, std::ios::beg);
    out_file.write(reinterpret_cast<const char*>(title_name_utf16.data()), title_name_size);

    uint32_t title_icon_size = (title_icon.size() > 0) ? static_cast<uint32_t>(title_icon.size()) : 20;
    EndianUtils::big_32(title_icon_size);

    out_file.seekp(0x1712, std::ios::beg);
    out_file.write(reinterpret_cast<const char*>(&title_icon_size), sizeof(uint32_t));
    out_file.seekp(0x1716, std::ios::beg);
    out_file.write(reinterpret_cast<const char*>(&title_icon_size), sizeof(uint32_t));

    if (title_icon.size() > 0) {
        out_file.seekp(0x171A, std::ios::beg);
        out_file.write(title_icon.data(), title_icon.size());
        out_file.seekp(0x571a, std::ios::beg);
        out_file.write(title_icon.data(), title_icon.size());
    }

    std::vector<char> header_buffer(EMPTY_LIVE_HEADER_SIZE - 0x344);

    out_file.seekp(0x344, std::ios::beg);
    out_file.read(header_buffer.data(), header_buffer.size());

    SHA1Hash header_hash = compute_sha1(header_buffer.data(), header_buffer.size());

    out_file.seekp(0x32C, std::ios::beg);
    out_file.write(reinterpret_cast<const char*>(&header_hash.hash), SHA_DIGEST_LENGTH);

    if (out_file.fail()) {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write live header");
    }  

    out_file.close();
}

GoDWriter::Remap GoDWriter::remap_sector(uint64_t iso_sector) {
    uint64_t block_num = (iso_sector * Xiso::SECTOR_SIZE) / GoD::BLOCK_SIZE;
    uint32_t file_index = static_cast<uint32_t>(block_num / GoD::DATA_BLOCKS_PER_PART);
    uint64_t data_block_within_file = block_num % GoD::DATA_BLOCKS_PER_PART;
    uint32_t hash_index = static_cast<uint32_t>(data_block_within_file / GoD::DATA_BLOCKS_PER_SHT);

    uint64_t remapped_offset = GoD::BLOCK_SIZE; // master hashtable
    remapped_offset += ((hash_index + 1) * GoD::BLOCK_SIZE); // Add subhash table blocks
    remapped_offset += (data_block_within_file * GoD::BLOCK_SIZE); // Add data blocks
    remapped_offset += (iso_sector * Xiso::SECTOR_SIZE) % GoD::BLOCK_SIZE; // Add offset within data block
    return { remapped_offset, file_index };
}

GoDWriter::Remap GoDWriter::remap_offset(uint64_t iso_offset) {
    Remap remapped = remap_sector(iso_offset / Xiso::SECTOR_SIZE);
    remapped.offset += (iso_offset % Xiso::SECTOR_SIZE);
    return { remapped.offset, remapped.file_index };
}

uint64_t GoDWriter::to_iso_offset(uint64_t god_offset, uint32_t god_file_index) {
    auto block_num = god_offset / GoD::BLOCK_SIZE;
    auto previous_data_blocks = god_file_index * GoD::DATA_BLOCKS_PER_PART;
    auto sub_hash_index = block_num / (GoD::DATA_BLOCKS_PER_SHT + 1);
    auto data_block_num = block_num - (sub_hash_index + 2) + previous_data_blocks;
    return (data_block_num * GoD::BLOCK_SIZE) + (god_offset % GoD::BLOCK_SIZE);
}

void GoDWriter::padded_seek(std::unique_ptr<std::ofstream>& out_file, uint64_t offset) {
    if (!out_file->is_open()) {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), "Output file is not open");
    }

    out_file->seekp(0, std::ios::end);

    if (static_cast<uint64_t>(out_file->tellp()) < offset) {
        std::vector<char> padding(offset - out_file->tellp(), 0);

        out_file->write(padding.data(), padding.size());
        if (out_file->fail() || out_file->tellp() != offset) {
            throw XGDException(ErrCode::FILE_WRITE, HERE());
        }
        return;
    }
    
    out_file->seekp(offset, std::ios::beg);
    if (out_file->fail() || out_file->tellp() != offset) {
        throw XGDException(ErrCode::FILE_SEEK, HERE());
    }
}

GoDWriter::SHA1Hash GoDWriter::compute_sha1(const char* data, size_t size) {
    SHA1Hash result;
    SHA1(reinterpret_cast<const unsigned char*>(data), size, result.hash);
    return result;
}

std::string GoDWriter::create_unique_name(const Xex::ExecutionInfo& xex_cert) {
    std::ostringstream ss;
    write_little_endian(ss, xex_cert.title_id);
    write_little_endian(ss, xex_cert.media_id);
    write_little_endian(ss, xex_cert.disc_number);
    write_little_endian(ss, xex_cert.disc_count);

    std::string data = ss.str();

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(data.c_str()), data.size(), hash);

    std::ostringstream hex_stream;
    for (size_t i = 0; i < SHA_DIGEST_LENGTH / 2; ++i) {
        hex_stream << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(hash[i]);
    }

    return hex_stream.str();
}

template <typename T>
void GoDWriter::write_little_endian(std::ostream& os, T value) {
    for (size_t i = 0; i < sizeof(T); ++i) {
        os.put(static_cast<char>(value & 0xFF));
        value >>= 8;
    }
}
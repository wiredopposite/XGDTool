#include <cctype>

#include "Utils/EndianUtils.h"
#include "Utils/StringUtils.h"
#include "AvlTree/AvlIterator.h"
#include "ImageWriter/GoDWriter/GoDLiveHeader.h"
#include "ImageWriter/GoDWriter/GoDWriter.h"

GoDWriter::GoDWriter(std::shared_ptr<ImageReader> image_reader, TitleHelper& title_helper, const ScrubType scrub_type)
    : image_reader_(image_reader), title_helper_(title_helper), scrub_type_(scrub_type) {}

GoDWriter::GoDWriter(const std::filesystem::path& in_dir_path, TitleHelper& title_helper)
    : in_dir_path_(in_dir_path), title_helper_(title_helper) {}

std::vector<std::filesystem::path> GoDWriter::convert(const std::filesystem::path& out_god_directory) 
{
    std::string platform_str;

    switch (title_helper_.platform()) 
    {
        case Platform::OGX:
            platform_str = StringUtils::uint32_to_hex_string(GoD::Type::ORIGINAL_XBOX);
            break;
        case Platform::X360:
            platform_str = StringUtils::uint32_to_hex_string(GoD::Type::GAMES_ON_DEMAND);
            break;
        default:
            throw XGDException(ErrCode::MISC, HERE(), "Unknown platform");
    }

    std::filesystem::path out_data_directory = out_god_directory / platform_str / (title_helper_.unique_name() + ".data");
    std::filesystem::path live_header_path = out_god_directory / platform_str / title_helper_.unique_name();

    create_directory(out_data_directory);

    std::vector<std::filesystem::path> out_part_paths;

    if (image_reader_ && scrub_type_ == ScrubType::FULL) //Full scrub image
    {
        AvlTree avl_tree(image_reader_->name(), image_reader_->directory_entries());
        out_part_paths = write_data_files_from_avl(avl_tree, out_data_directory);
    }
    else if (!in_dir_path_.empty()) //Write from directory
    {
        AvlTree avl_tree(in_dir_path_.filename().string(), in_dir_path_); 
        out_part_paths = write_data_files_from_avl(avl_tree, out_data_directory);  
    }
    else if (!image_reader_)
    {
        throw XGDException(ErrCode::MISC, HERE(), "No input data");
    }
    else //Write from image w no/partial scrub
    {
        out_part_paths = write_data_files(out_data_directory, scrub_type_ == ScrubType::PARTIAL);
    }

    write_hashtables(out_part_paths);
    SHA1Hash final_mht_hash = finalize_hashtables(out_part_paths);

    write_live_header(live_header_path, out_part_paths, final_mht_hash);

    return { out_god_directory };
}

void GoDWriter::write_iso_header(std::vector<std::unique_ptr<std::ofstream>>& out_files, AvlTree& avl_tree)
{
    Xiso::Header iso_header(static_cast<uint32_t>(avl_tree.root()->start_sector),
                            static_cast<uint32_t>(avl_tree.root()->file_size),
                            static_cast<uint32_t>(avl_tree.out_iso_size() / Xiso::SECTOR_SIZE),
                            image_reader_ ? image_reader_->file_time() : Xiso::FileTime());

    for (size_t i = 0; i < sizeof(Xiso::Header) / Xiso::SECTOR_SIZE; i++) 
    {
        Remap remapped = remap_sector(i);
        out_files[remapped.file_index]->seekp(remapped.offset, std::ios::beg);
        out_files[remapped.file_index]->write(reinterpret_cast<const char*>(&iso_header) + (i * Xiso::SECTOR_SIZE), Xiso::SECTOR_SIZE);
        if (out_files[remapped.file_index]->fail()) 
        {
            throw XGDException(ErrCode::FILE_WRITE, HERE());
        }
    }
}

void GoDWriter::write_padding_sectors(std::vector<std::unique_ptr<std::ofstream>>& out_files, const uint32_t start_sector, const uint32_t num_sectors, const char pad_byte)
{
    std::vector<char> pad_sector(Xiso::SECTOR_SIZE, pad_byte);

    for (uint32_t i = 0; i < num_sectors; ++i) 
    {
        Remap remapped = remap_sector(start_sector + i);
        out_files[remapped.file_index]->seekp(remapped.offset, std::ios::beg);
        out_files[remapped.file_index]->write(pad_sector.data(), Xiso::SECTOR_SIZE);
        if (out_files[remapped.file_index]->fail()) 
        {
            throw XGDException(ErrCode::FILE_WRITE, HERE());
        }
    }
}

std::vector<std::filesystem::path> GoDWriter::write_data_files_from_avl(AvlTree& avl_tree, const std::filesystem::path& out_data_directory) 
{
    prog_total_ = avl_tree.total_bytes();
    prog_processed_ = 0;

    uint64_t out_iso_size = avl_tree.out_iso_size();
    uint32_t total_out_sectors = num_sectors(out_iso_size);
    uint32_t total_out_data_blocks = num_blocks(out_iso_size);
    uint32_t total_out_parts = num_parts(total_out_data_blocks);

    std::vector<std::filesystem::path> out_part_paths = get_part_paths(out_data_directory, total_out_parts);
    std::vector<std::unique_ptr<std::ofstream>> out_files;

    for (auto& part_path : out_part_paths) 
    {
        out_files.push_back(std::make_unique<std::ofstream>(part_path, std::ios::binary));
        if (!out_files.back()->is_open()) 
        {
            throw XGDException(ErrCode::FILE_OPEN, HERE(), part_path.string());
        }
    }

    XGDLog() << "Writing GoD data files" << XGDLog::Endl;

    write_iso_header(out_files, avl_tree);

    AvlIterator avl_iterator(avl_tree);
    const std::vector<AvlIterator::Entry>& avl_entries = avl_iterator.entries();

    uint32_t current_out_sector = static_cast<uint32_t>(avl_entries.front().offset / Xiso::SECTOR_SIZE);

    Remap remapped = remap_sector(current_out_sector);  
    out_files[remapped.file_index]->seekp(remapped.offset, std::ios::beg);

    for (size_t i = 0; i < avl_entries.size(); ++i)
    {
        if (avl_entries[i].offset != static_cast<uint64_t>(current_out_sector) * Xiso::SECTOR_SIZE) 
        {
            throw XGDException(ErrCode::MISC, HERE(), "GoD file has become misaligned");
        }

        if (avl_entries[i].directory_entry)
        {
            std::vector<char> entry_buffer;
            size_t entries_processed = write_directory_to_buffer(avl_entries, i, entry_buffer);
            i += entries_processed - 1;

            for (size_t j = 0; j < entry_buffer.size(); j += Xiso::SECTOR_SIZE)
            {
                Remap remapped = remap_sector(current_out_sector);
                out_files[remapped.file_index]->seekp(remapped.offset, std::ios::beg); 
                out_files[remapped.file_index]->write(entry_buffer.data() + j, Xiso::SECTOR_SIZE);
                if (out_files[remapped.file_index]->fail()) 
                {
                    throw XGDException(ErrCode::FILE_WRITE, HERE());
                }

                current_out_sector++;
            }
        }
        else
        {
            if (image_reader_)
            {
                write_file_from_reader(out_files, *avl_entries[i].node);
            }
            else
            {
                write_file_from_directory(out_files, *avl_entries[i].node);
            }

            current_out_sector += num_sectors(avl_entries[i].node->file_size);
        }

        if (i != avl_entries.size() - 1 && avl_entries[i + 1].offset > current_out_sector * Xiso::SECTOR_SIZE) 
        {
            uint32_t pad_sectors = static_cast<uint32_t>(avl_entries[i + 1].offset / Xiso::SECTOR_SIZE) - current_out_sector;
            write_padding_sectors(out_files, current_out_sector, pad_sectors, Xiso::PAD_BYTE);
            current_out_sector += pad_sectors;
        }
    }

    if (current_out_sector < total_out_sectors) 
    {
        uint32_t pad_sectors = total_out_sectors - current_out_sector;
        write_padding_sectors(out_files, current_out_sector, pad_sectors, 0x00);
    }

    for (auto& file : out_files) 
    {
        file->close();
    }
    return out_part_paths;
}

void GoDWriter::write_file_from_reader(std::vector<std::unique_ptr<std::ofstream>>& out_files, const AvlTree::Node& node)
{
    uint64_t current_write_sector = node.start_sector;
    uint64_t read_position = image_reader_->image_offset() + (node.old_start_sector * Xiso::SECTOR_SIZE);
    uint64_t bytes_remaining = node.file_size;

    std::vector<char> read_buffer(Xiso::SECTOR_SIZE);

    while (bytes_remaining > 0)
    {
        uint64_t read_size = std::min(bytes_remaining, Xiso::SECTOR_SIZE);   

        image_reader_->read_bytes(read_position, read_size, read_buffer.data());

        if (read_size < Xiso::SECTOR_SIZE) 
        {
            std::memset(read_buffer.data() + read_size, Xiso::PAD_BYTE, Xiso::SECTOR_SIZE - read_size);
        }

        Remap remapped = remap_sector(current_write_sector);
        out_files[remapped.file_index]->seekp(remapped.offset, std::ios::beg);
        out_files[remapped.file_index]->write(read_buffer.data(), Xiso::SECTOR_SIZE);
        if (out_files[remapped.file_index]->fail()) 
        {
            throw XGDException(ErrCode::FILE_WRITE, HERE());
        }

        XGDLog().print_progress(prog_processed_ += read_size, prog_total_);

        read_position += read_size;
        bytes_remaining -= read_size;
        current_write_sector++;
    }
}

void GoDWriter::write_file_from_directory(std::vector<std::unique_ptr<std::ofstream>>& out_files, const AvlTree::Node& node)
{
    std::ifstream in_file(node.path, std::ios::binary);
    if (!in_file.is_open()) 
    {
        throw XGDException(ErrCode::FILE_OPEN, HERE());
    }

    uint64_t current_write_sector = node.start_sector;
    uint64_t bytes_remaining = node.file_size;
    std::vector<char> read_buffer(Xiso::SECTOR_SIZE);

    while (bytes_remaining > 0)
    {
        uint64_t read_size = std::min(bytes_remaining, Xiso::SECTOR_SIZE);   

        in_file.read(read_buffer.data(), read_size);
        if (in_file.fail()) 
        {
            throw XGDException(ErrCode::FILE_READ, HERE());
        }

        if (read_size < Xiso::SECTOR_SIZE) 
        {
            std::memset(read_buffer.data() + read_size, Xiso::PAD_BYTE, Xiso::SECTOR_SIZE - read_size);
        }

        Remap remapped = remap_sector(current_write_sector);
        out_files[remapped.file_index]->seekp(remapped.offset, std::ios::beg);
        out_files[remapped.file_index]->write(read_buffer.data(), Xiso::SECTOR_SIZE);
        if (out_files[remapped.file_index]->fail()) 
        {
            throw XGDException(ErrCode::FILE_WRITE, HERE());
        }

        XGDLog().print_progress(prog_processed_ += read_size, prog_total_);

        bytes_remaining -= read_size;
        current_write_sector++;
    }

    in_file.close();
}

std::vector<std::filesystem::path> GoDWriter::write_data_files(const std::filesystem::path& out_data_directory, const bool scrub) 
{
    ImageReader& image_reader = *image_reader_;
    uint32_t sector_offset = static_cast<uint32_t>(image_reader.image_offset() / Xiso::SECTOR_SIZE);
    uint32_t last_sector = image_reader.total_sectors();
    const std::unordered_set<uint32_t>* data_sectors;

    if (scrub) 
    {
        data_sectors = &image_reader.data_sectors();
        last_sector = std::min(image_reader.max_data_sector(), last_sector);
    }

    uint32_t total_out_sectors = last_sector - sector_offset;
    uint32_t total_out_data_blocks = num_blocks(static_cast<size_t>(total_out_sectors) * Xiso::SECTOR_SIZE);
    uint32_t total_out_parts = num_parts(total_out_data_blocks);

    prog_total_ = last_sector - sector_offset;
    prog_processed_ = 0;

    XGDLog(Debug) << "Total data blocks: " << total_out_data_blocks << " total parts: " << total_out_parts << XGDLog::Endl;  

    std::vector<std::filesystem::path> out_part_paths = get_part_paths(out_data_directory, total_out_parts);
    std::vector<std::unique_ptr<std::ofstream>> out_files;

    for (auto& part_path : out_part_paths) 
    {
        out_files.push_back(std::make_unique<std::ofstream>(part_path, std::ios::binary));
        if (!out_files.back()->is_open()) 
        {
            throw XGDException(ErrCode::FILE_OPEN, HERE(), part_path.string());
        }
    }

    uint32_t current_sector = sector_offset;
    std::vector<char> buffer(Xiso::SECTOR_SIZE);

    XGDLog() << "Writing data files" << XGDLog::Endl;

    while (current_sector <= last_sector) 
    {
        bool write_sector = true;

        if (scrub && image_reader.platform() == Platform::OGX) //No need to zero out padding for Xbox 360
        {
            write_sector = data_sectors->find(current_sector) != data_sectors->end();
        }

        if (write_sector) 
        {
            image_reader.read_sector(current_sector, buffer.data());
        } 
        else 
        {
            std::memset(buffer.data(), 0x00, Xiso::SECTOR_SIZE);
        }

        Remap remapped = remap_sector(current_sector - sector_offset);
        out_files[remapped.file_index]->seekp(remapped.offset, std::ios::beg);
        out_files[remapped.file_index]->write(buffer.data(), buffer.size());
        if (out_files[remapped.file_index]->fail()) 
        {
            throw XGDException(ErrCode::FILE_WRITE, HERE());
        }

        current_sector++;

        XGDLog().print_progress(prog_processed_++, prog_total_);
    }

    if (out_files.back()->tellp() % GoD::BLOCK_SIZE) 
    {
        size_t padding = GoD::BLOCK_SIZE - (out_files.back()->tellp() % GoD::BLOCK_SIZE);
        std::vector<char> pad_buffer(padding, 0);
        out_files.back()->write(pad_buffer.data(), pad_buffer.size());
    }

    for (auto& file : out_files) 
    {
        file->close();
    }

    return out_part_paths;
}

std::vector<std::filesystem::path> GoDWriter::get_part_paths(const std::filesystem::path& out_directory, const uint32_t num_files)
{
    std::vector<std::filesystem::path> out_part_paths;

    for (uint32_t i = 0; i < num_files; ++i) 
    {
        std::ostringstream out_name;
        out_name << "Data" << std::setw(4) << std::setfill('0') << i;

        out_part_paths.push_back(out_directory / out_name.str());
    }

    return out_part_paths;
}

void GoDWriter::write_hashtables(const std::vector<std::filesystem::path>& part_paths) 
{
    /*  For each sub hashtable, each of it's following data blocks are hashed (204),
        all those hashes are then written to the sub hashtable, 
        each sub hashtable block's hash is then written to the master hashtable */

    prog_total_ = part_paths.size() - 1;
    prog_processed_ = 0;

    XGDLog() << "Writing hash tables" << XGDLog::Endl;

    for (auto& part_path : part_paths) 
    {
        uint32_t blocks_left = num_blocks(std::filesystem::file_size(part_path));
        uint32_t sub_hashtables = (blocks_left - 1) / (GoD::DATA_BLOCKS_PER_SHT + 1) + ((blocks_left - 1) % (GoD::DATA_BLOCKS_PER_SHT + 1) ? 1 : 0);

        std::fstream current_file(part_path, std::ios::binary | std::ios::in | std::ios::out);
        if (!current_file.is_open()) 
        {
            throw XGDException(ErrCode::FILE_OPEN, HERE(), part_path.string());
        }

        current_file.seekp(GoD::BLOCK_SIZE, std::ios::beg);
        blocks_left--;

        std::vector<SHA1Hash> master_hashtable;

        for (uint32_t i = 0; i < sub_hashtables; ++i) 
        {
            uint32_t blocks_in_sht = 0;
            std::vector<char> block_buffer(GoD::BLOCK_SIZE, 0);
            std::vector<SHA1Hash> sub_hashtable;

            current_file.seekp(GoD::BLOCK_SIZE, std::ios::cur);
            blocks_left--;

            while (blocks_in_sht < GoD::DATA_BLOCKS_PER_SHT && 0 < blocks_left) 
            {
                current_file.read(block_buffer.data(), block_buffer.size());
                if (current_file.fail()) 
                {
                    throw XGDException(ErrCode::FILE_READ, HERE());
                }

                sub_hashtable.push_back(compute_sha1(block_buffer.data(), block_buffer.size()));

                blocks_in_sht++;
                blocks_left--;
            }

            uint64_t position = current_file.tellp();

            current_file.seekp((i * (GoD::DATA_BLOCKS_PER_SHT + 1) * GoD::BLOCK_SIZE) + GoD::BLOCK_SIZE, std::ios::beg);
            current_file.write(reinterpret_cast<const char*>(sub_hashtable.data()), sub_hashtable.size() * sizeof(SHA1Hash));
            current_file.seekp(position, std::ios::beg);

            // Zero padded to block size, then hashed 
            std::vector<char> sub_hashtable_buffer(GoD::BLOCK_SIZE, 0);
            std::memcpy(sub_hashtable_buffer.data(), sub_hashtable.data(), std::min(sub_hashtable.size() * sizeof(SHA1Hash), static_cast<size_t>(GoD::BLOCK_SIZE)));
            master_hashtable.push_back(compute_sha1(sub_hashtable_buffer.data(), GoD::BLOCK_SIZE));

            if (blocks_left == 0) 
            {
                break;
            }
        }

        current_file.seekp(0, std::ios::beg);
        current_file.write(reinterpret_cast<const char*>(master_hashtable.data()), master_hashtable.size() * sizeof(SHA1Hash));
        current_file.close();

        XGDLog().print_progress(prog_processed_++, prog_total_);
    }
}

GoDWriter::SHA1Hash GoDWriter::finalize_hashtables(const std::vector<std::filesystem::path>& part_paths) 
{
    /*  Each Data file's master hashtable block is hashed, then that's 
        written to the end of the previous Data file's master hashtable. 
        The first Data file's master hashtable hash (final_mht_hash) 
        is written to the Live header file */

    SHA1Hash final_mht_hash;

    for (size_t i = part_paths.size() - 1; i > 0; i--) 
    {
        std::ifstream current_part = std::ifstream(part_paths[i], std::ios::binary);
        if (!current_part.is_open()) 
        {
            throw XGDException(ErrCode::FILE_OPEN, HERE(), part_paths[i].string());
        }

        std::fstream prev_part = std::fstream(part_paths[i - 1], std::ios::binary | std::ios::in | std::ios::out);
        if (!prev_part.is_open()) 
        {
            throw XGDException(ErrCode::FILE_OPEN, HERE(), part_paths[i - 1].string());
        }

        std::vector<char> current_hash_buffer(GoD::BLOCK_SIZE, 0);
        current_part.read(current_hash_buffer.data(), GoD::BLOCK_SIZE);

        SHA1Hash current_mht_hash = compute_sha1(reinterpret_cast<const char*>(current_hash_buffer.data()), GoD::BLOCK_SIZE);

        prev_part.seekp(SHA_DIGEST_LENGTH * GoD::SHT_PER_MHT, std::ios::beg);
        prev_part.write(reinterpret_cast<const char*>(&current_mht_hash.hash), SHA_DIGEST_LENGTH);

        if (i == 1) 
        {
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

void GoDWriter::write_live_header(const std::filesystem::path& out_header_path, const std::vector<std::filesystem::path>& out_part_paths, const SHA1Hash& final_mht_hash) 
{
    XGDLog() << "Writing Live header" << XGDLog::Endl;

    std::stringstream out_str;
    out_str.write(reinterpret_cast<const char*>(EMPTY_LIVE_HEADER), EMPTY_LIVE_HEADER_SIZE);

    out_str.seekp(0x354, std::ios::beg);
    out_str.write(reinterpret_cast<const char*>(&title_helper_.xex_cert().media_id), sizeof(uint32_t));
    out_str.seekp(0x360, std::ios::beg);
    out_str.write(reinterpret_cast<const char*>(&title_helper_.xex_cert().title_id), sizeof(uint32_t));
    out_str.write(reinterpret_cast<const char*>(&title_helper_.xex_cert().platform), sizeof(uint8_t));
    out_str.write(reinterpret_cast<const char*>(&title_helper_.xex_cert().executable_type), sizeof(uint8_t));
    out_str.write(reinterpret_cast<const char*>(&title_helper_.xex_cert().disc_number), sizeof(uint8_t));
    out_str.write(reinterpret_cast<const char*>(&title_helper_.xex_cert().disc_count), sizeof(uint8_t));

    uint64_t parts_total_size = 0;
    for (auto& part_path : out_part_paths) 
    {
        parts_total_size += static_cast<uint32_t>(std::filesystem::file_size(part_path));
    }

    uint32_t parts_written_size = static_cast<uint32_t>(parts_total_size / 0x100);
    uint32_t part_count = static_cast<uint32_t>(out_part_paths.size());
    uint32_t content_type = (title_helper_.platform() == Platform::X360) ? GoD::Type::GAMES_ON_DEMAND : GoD::Type::ORIGINAL_XBOX;

    EndianUtils::little_32(part_count);
    EndianUtils::big_32(parts_written_size);
    EndianUtils::big_32(content_type);

    out_str.seekp(0x344, std::ios::beg);
    out_str.write(reinterpret_cast<const char*>(&content_type), sizeof(uint32_t));

    out_str.seekp(0x37D, std::ios::beg);
    out_str.write(reinterpret_cast<const char*>(&final_mht_hash.hash), SHA_DIGEST_LENGTH);
    
    out_str.seekp(0x3A0, std::ios::beg);
    out_str.write(reinterpret_cast<const char*>(&part_count), sizeof(uint32_t));
    out_str.write(reinterpret_cast<const char*>(&parts_written_size), sizeof(uint32_t));

    size_t title_name_size = std::min(title_helper_.title_name().size() * sizeof(char16_t), static_cast<size_t>(80));
    
    out_str.seekp(0x411 + 1, std::ios::beg);
    out_str.write(reinterpret_cast<const char*>(title_helper_.title_name().data()), title_name_size);
    out_str.seekp(0x1691 + 1, std::ios::beg);
    out_str.write(reinterpret_cast<const char*>(title_helper_.title_name().data()), title_name_size);

    uint32_t title_icon_size = (title_helper_.title_icon().size() > 0) ? static_cast<uint32_t>(title_helper_.title_icon().size()) : 20;
    EndianUtils::big_32(title_icon_size);

    out_str.seekp(0x1712, std::ios::beg);
    out_str.write(reinterpret_cast<const char*>(&title_icon_size), sizeof(uint32_t));
    out_str.seekp(0x1716, std::ios::beg);
    out_str.write(reinterpret_cast<const char*>(&title_icon_size), sizeof(uint32_t));

    if (title_helper_.title_icon().size() > 0) 
    {
        out_str.seekp(0x171A, std::ios::beg);
        out_str.write(title_helper_.title_icon().data(), title_helper_.title_icon().size());

        out_str.seekp(0x571a, std::ios::beg);
        out_str.write(title_helper_.title_icon().data(), title_helper_.title_icon().size());
    }

    SHA1Hash header_hash = compute_sha1(out_str.str().c_str() + 0x344, out_str.str().size() - 0x344);

    out_str.seekp(0x32C, std::ios::beg);
    out_str.write(reinterpret_cast<const char*>(&header_hash.hash), SHA_DIGEST_LENGTH);

    std::ofstream out_file(out_header_path, std::ios::binary);
    if (!out_file.is_open()) 
    {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), out_header_path.string());
    }

    out_file.write(out_str.str().c_str(), out_str.str().size());
    if (out_file.fail()) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), out_header_path.string());
    }

    out_file.close();
}

GoDWriter::Remap GoDWriter::remap_sector(const uint64_t iso_sector) 
{
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

GoDWriter::Remap GoDWriter::remap_offset(const uint64_t iso_offset) 
{
    Remap remapped = remap_sector(iso_offset / Xiso::SECTOR_SIZE);
    remapped.offset += (iso_offset % Xiso::SECTOR_SIZE);
    return { remapped.offset, remapped.file_index };
}

uint64_t GoDWriter::to_iso_offset(const uint64_t god_offset, const uint32_t god_file_index) 
{
    auto block_num = god_offset / GoD::BLOCK_SIZE;
    auto previous_data_blocks = god_file_index * GoD::DATA_BLOCKS_PER_PART;
    auto sub_hash_index = block_num / (GoD::DATA_BLOCKS_PER_SHT + 1);
    auto data_block_num = block_num - (sub_hash_index + 2) + previous_data_blocks;
    return (data_block_num * GoD::BLOCK_SIZE) + (god_offset % GoD::BLOCK_SIZE);
}

GoDWriter::SHA1Hash GoDWriter::compute_sha1(const char* data, const uint64_t size) 
{
    SHA1Hash result;
    SHA1(reinterpret_cast<const unsigned char*>(data), size, result.hash);
    return result;
}

uint32_t GoDWriter::num_blocks(const uint64_t num_bytes) 
{
    return static_cast<uint32_t>(num_bytes / GoD::BLOCK_SIZE) + ((num_bytes % GoD::BLOCK_SIZE) ? 1 : 0);
}

uint32_t GoDWriter::num_parts(const uint32_t num_data_blocks) 
{
    return (num_data_blocks / GoD::DATA_BLOCKS_PER_PART) + ((num_data_blocks % GoD::DATA_BLOCKS_PER_PART) ? 1 : 0);
}
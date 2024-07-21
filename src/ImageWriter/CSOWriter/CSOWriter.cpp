#include "Common/Utils.h"
#include "AvlTree/AvlIterator.h"
#include "ImageWriter/CSOWriter/CSOWriter.h"

CSOWriter::CSOWriter(std::shared_ptr<ImageReader> image_reader, const ScrubType scrub_type) 
    :   image_reader_(image_reader),
        scrub_type_(scrub_type)
{
    if (scrub_type_ == ScrubType::FULL) 
    {
        avl_tree_ = std::make_unique<AvlTree>(image_reader_->name(), image_reader_->directory_entries());
    }

    init_lz4f_context();
}

CSOWriter::CSOWriter(const std::filesystem::path& in_dir_path)
    :   avl_tree_(std::make_unique<AvlTree>(in_dir_path.filename().string(), in_dir_path)) 
{
    init_lz4f_context();
}

CSOWriter::~CSOWriter() 
{
    LZ4F_freeCompressionContext(lz4f_ctx_);
}

void CSOWriter::init_lz4f_context() 
{
    LZ4F_errorCode_t lz4f_error = LZ4F_createCompressionContext(&lz4f_ctx_, LZ4F_VERSION);
    if (LZ4F_isError(lz4f_error)) {
        throw XGDException(ErrCode::MISC, HERE(), LZ4F_getErrorName(lz4f_error));
    }

    lz4f_max_size_ = LZ4F_compressBound(Xiso::SECTOR_SIZE, &lz4f_prefs_);
}

std::vector<std::filesystem::path> CSOWriter::convert(const std::filesystem::path& out_cso_path) 
{
    out_filepath_base_ = out_cso_path;

    create_directory(out_filepath_base_.parent_path());
    
    out_filepath_1_ = out_filepath_base_;
    out_filepath_2_ = out_filepath_base_;
    out_filepath_1_.replace_extension(".1.cso");
    out_filepath_2_.replace_extension(".2.cso");

    if (avl_tree_) 
    {
        convert_to_cso_from_avl();
    } 
    else 
    {
        convert_to_cso(scrub_type_ == ScrubType::PARTIAL);
    }

    return out_paths();
}

void CSOWriter::convert_to_cso(const bool scrub) 
{
    ImageReader& image_reader = *image_reader_;
    uint32_t sector_offset = static_cast<uint32_t>(image_reader.image_offset() / Xiso::SECTOR_SIZE);
    uint32_t end_sector = image_reader.total_sectors();
    const std::unordered_set<uint32_t>* data_sectors;

    if (scrub) 
    {
        data_sectors = &image_reader.data_sectors();
        end_sector = std::min(image_reader.max_data_sector() + 1, end_sector);
    }

    uint32_t sectors_to_write = end_sector - sector_offset;

    prog_total_ = sectors_to_write;
    prog_processed_ = 0;

    std::ofstream out_file(out_filepath_1_, std::ios::binary);
    if (!out_file.is_open()) 
    {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), out_filepath_1_.string());
    }

    CSO::Header cso_header(static_cast<uint64_t>(sectors_to_write) * Xiso::SECTOR_SIZE);

    out_file.write(reinterpret_cast<const char*>(&cso_header), sizeof(CSO::Header));
    
    for (uint32_t i = 0; i < sectors_to_write + 1; ++i) 
    {
        uint32_t zero = 0;
        out_file.write(reinterpret_cast<const char*>(&zero), sizeof(uint32_t)); // Dummy block index
    }

    if (out_file.fail()) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), out_filepath_1_.string());
    }

    std::vector<char> read_buffer(Xiso::SECTOR_SIZE);
    std::vector<uint32_t> block_index;

    XGDLog() << "Writing CSO file" << XGDLog::Endl;

    for (uint32_t sector = sector_offset; sector < end_sector; ++sector) 
    {
        if (static_cast<uint64_t>(out_file.tellp()) > CSO::SPLIT_OFFSET) 
        {
            out_file.close();
            out_file = std::ofstream(out_filepath_2_, std::ios::binary);
            if (!out_file.is_open()) 
            {
                throw XGDException(ErrCode::FILE_OPEN, HERE(), out_filepath_2_.string());
            }
        }

        bool write_sector = true;

        if (scrub && image_reader.platform() == Platform::OGX) 
        {
            write_sector = data_sectors->find(sector) != data_sectors->end();
        }

        if (write_sector) 
        {
            image_reader.read_sector(sector, read_buffer.data());
        } 
        else 
        {
            std::fill(read_buffer.begin(), read_buffer.end(), 0);
        }

        compress_and_write_sector(out_file, block_index, read_buffer.data());
        
        XGDLog().print_progress(prog_processed_++, prog_total_);
    }

    finalize_out_files(out_file, block_index);
    out_file.close();
}

void CSOWriter::convert_to_cso_from_avl() 
{
    AvlTree& avl_tree = *avl_tree_;
    uint64_t out_iso_size = avl_tree.out_iso_size();
    uint32_t out_iso_sectors = static_cast<uint32_t>(out_iso_size / Xiso::SECTOR_SIZE);

    prog_total_ = avl_tree.total_bytes();
    prog_processed_ = 0;

    AvlIterator avl_iter(avl_tree);
    const std::vector<AvlIterator::Entry>& avl_entries = avl_iter.entries();

    std::ofstream out_file(out_filepath_1_, std::ios::binary);
    if (!out_file.is_open()) 
    {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), out_filepath_1_.string());
    }

    CSO::Header cso_header(out_iso_size);

    out_file.write(reinterpret_cast<const char*>(&cso_header), sizeof(CSO::Header));

    for (uint32_t i = 0; i < out_iso_sectors; ++i) 
    {
        uint32_t zero = 0;
        out_file.write(reinterpret_cast<const char*>(&zero), sizeof(uint32_t)); // Dummy block index
    }

    Xiso::Header* xiso_header = new Xiso::Header(   static_cast<uint32_t>(avl_tree.root()->start_sector), 
                                                    static_cast<uint32_t>(avl_tree.root()->file_size), 
                                                    out_iso_sectors);

    uint32_t xiso_header_sectors = static_cast<uint32_t>(sizeof(Xiso::Header) / Xiso::SECTOR_SIZE);
    std::vector<uint32_t> block_index;

    for (uint32_t i = 0; i < xiso_header_sectors; ++i) 
    {
        compress_and_write_sector_managed(out_file, block_index, reinterpret_cast<const char*>(xiso_header) + (i * Xiso::SECTOR_SIZE));
    }

    delete xiso_header;

    for (uint32_t i = 0; i < static_cast<uint32_t>(avl_entries.front().offset / Xiso::SECTOR_SIZE) - xiso_header_sectors; ++i) 
    {
        std::vector<char> pad_sector(Xiso::SECTOR_SIZE, 0);
        compress_and_write_sector_managed(out_file, block_index, pad_sector.data());
    }

    for (size_t i = 0; i < avl_entries.size(); i++) 
    {
        // Pad sectors if necessary, usually these will be directories
        if (avl_entries[i].offset > ((block_index.size() - 1) * Xiso::SECTOR_SIZE)) 
        {
            std::vector<char> empty_buffer(Xiso::SECTOR_SIZE, Xiso::PAD_BYTE);

            for (uint32_t j = 0; j < (avl_entries[i].offset / Xiso::SECTOR_SIZE) - block_index.size(); j++) 
            {
                compress_and_write_sector_managed(out_file, block_index, empty_buffer.data());
            }

        } 
        else if ((avl_entries[i].offset / Xiso::SECTOR_SIZE) < (block_index.size() - 1) || 
                    (avl_entries[i].offset % Xiso::SECTOR_SIZE)) 
        {
            throw XGDException(ErrCode::MISC, HERE(), "CCI file has become misaligned");
        }

        if (avl_entries[i].directory_entry) 
        {
            std::vector<char> entry_buffer;

            // Write all entries in the current directory to a buffer, then write sector by sector
            for (size_t j = i; j < avl_entries.size() - 1; ++j) 
            {
                AvlIterator::Entry avl_entry = avl_entries[j];
                Xiso::DirectoryEntry::Header dir_header;

                dir_header.left_offset  = avl_entry.node->left_child ? static_cast<uint16_t>(avl_entry.node->left_child->offset / sizeof(uint32_t)) : 0;
                dir_header.right_offset = avl_entry.node->right_child ? static_cast<uint16_t>(avl_entry.node->right_child->offset / sizeof(uint32_t)) : 0;
                dir_header.start_sector = static_cast<uint32_t>(avl_entry.node->start_sector);
                dir_header.file_size    = static_cast<uint32_t>(avl_entry.node->file_size + (avl_entry.node->subdirectory ? ((Xiso::SECTOR_SIZE - (avl_entry.node->file_size % Xiso::SECTOR_SIZE)) % Xiso::SECTOR_SIZE) : 0));
                dir_header.attributes   = avl_entry.node->subdirectory ? Xiso::ATTRIBUTE_DIRECTORY : Xiso::ATTRIBUTE_FILE;
                dir_header.name_length  = static_cast<uint8_t>(std::min(avl_entry.node->filename.size(), static_cast<size_t>(UINT8_MAX)));

                EndianUtils::little_16(dir_header.left_offset);
                EndianUtils::little_16(dir_header.right_offset);
                EndianUtils::little_32(dir_header.start_sector);
                EndianUtils::little_32(dir_header.file_size);

                size_t entry_len = sizeof(Xiso::DirectoryEntry::Header) + dir_header.name_length;
                size_t buffer_pos = entry_buffer.size();

                entry_buffer.resize(buffer_pos + entry_len, Xiso::PAD_BYTE);

                std::memcpy(entry_buffer.data() + buffer_pos, &dir_header, sizeof(Xiso::DirectoryEntry::Header));
                std::memcpy(entry_buffer.data() + buffer_pos + sizeof(Xiso::DirectoryEntry::Header), avl_entry.node->filename.c_str(), dir_header.name_length);

                if (i == avl_entries.size() - 1) 
                {
                    break;
                }

                AvlIterator::Entry next_avl_entry = avl_entries[j + 1];

                if (!next_avl_entry.directory_entry ||
                    next_avl_entry.node->directory_start != avl_entry.node->directory_start) 
                {
                    break;
                }

                size_t padding_len = next_avl_entry.node->offset - entry_buffer.size();

                if (padding_len > 0) 
                {
                    entry_buffer.resize(entry_buffer.size() + padding_len, Xiso::PAD_BYTE);
                }

                i++;
            }

            if (entry_buffer.size() % Xiso::SECTOR_SIZE) 
            {
                entry_buffer.resize(entry_buffer.size() + (Xiso::SECTOR_SIZE - (entry_buffer.size() % Xiso::SECTOR_SIZE)), Xiso::PAD_BYTE);
            }

            for (size_t j = 0; j < entry_buffer.size() / Xiso::SECTOR_SIZE; j++) 
            {
                compress_and_write_sector_managed(out_file, block_index, entry_buffer.data() + (j * Xiso::SECTOR_SIZE));
            }

        } 
        else 
        {
            AvlIterator::Entry avl_entry = avl_entries[i];

            if (image_reader_) 
            {
                write_file_from_reader(out_file, block_index, *avl_entry.node);
            } 
            else 
            {
                write_file_from_dir(out_file, block_index, *avl_entry.node);
            }
        }
    }

    finalize_out_files(out_file, block_index);
    out_file.close();
}

void CSOWriter::write_file_from_reader(std::ofstream& out_file, std::vector<uint32_t>& block_index, AvlTree::Node& node) 
{
    ImageReader& image_reader = *image_reader_;
    size_t bytes_remaining = node.file_size;
    size_t read_position = image_reader.image_offset() + (node.old_start_sector * Xiso::SECTOR_SIZE);
    std::vector<char> read_buffer(Xiso::SECTOR_SIZE);

    while (bytes_remaining > 0) 
    {
        std::fill(read_buffer.begin(), read_buffer.end(), Xiso::PAD_BYTE);

        size_t read_size = std::min(bytes_remaining, static_cast<size_t>(Xiso::SECTOR_SIZE));

        image_reader.read_bytes(read_position, read_size, read_buffer.data());
        compress_and_write_sector_managed(out_file, block_index, read_buffer.data());

        bytes_remaining -= read_size;
        read_position += read_size;

        XGDLog().print_progress(prog_processed_ += read_size, prog_total_);
    }
}

void CSOWriter::write_file_from_dir(std::ofstream& out_file, std::vector<uint32_t>& block_index, AvlTree::Node& node) 
{
    std::ifstream in_file(node.path, std::ios::binary);
    if (!in_file.is_open()) 
    {
        throw std::runtime_error("Failed to open input file: " + node.path.string());
    }

    size_t bytes_remaining = node.file_size;
    std::vector<char> read_buffer(Xiso::SECTOR_SIZE);

    while (bytes_remaining > 0) 
    {
        std::fill(read_buffer.begin(), read_buffer.end(), Xiso::PAD_BYTE);

        size_t read_size = std::min(bytes_remaining, static_cast<size_t>(Xiso::SECTOR_SIZE));

        in_file.read(read_buffer.data(), read_size);
        if (in_file.fail()) 
        {
            throw std::runtime_error("Failed to read from input file: " + node.path.string());
        }

        compress_and_write_sector_managed(out_file, block_index, read_buffer.data());

        bytes_remaining -= read_size;

        XGDLog().print_progress(prog_processed_ += read_size, prog_total_);
    }

    in_file.close();
}

void CSOWriter::compress_and_write_sector(std::ofstream& out_file, std::vector<uint32_t>& block_index, const char* in_buffer) 
{
    std::vector<char> compress_buffer(lz4f_max_size_ * 2);
    auto align = out_file.tellp() & ALIGN_M;

    if (align) 
    {
        align = ALIGN_B - align;
        std::vector<char> alignment_buffer(align, 0);
        out_file.write(alignment_buffer.data(), align);
    }

    uint32_t block_info = static_cast<uint32_t>(out_file.tellp() >> CSO::INDEX_ALIGNMENT);

    size_t header_len = LZ4F_compressBegin(lz4f_ctx_, compress_buffer.data(), Xiso::SECTOR_SIZE, &lz4f_prefs_);
    if (LZ4F_isError(header_len)) 
    {
        throw XGDException(ErrCode::MISC, HERE(), LZ4F_getErrorName(header_len));
    }

    size_t compressed_size = LZ4F_compressUpdate(lz4f_ctx_, compress_buffer.data(), compress_buffer.size(), in_buffer, Xiso::SECTOR_SIZE, nullptr);
    if (LZ4F_isError(compressed_size)) 
    {
        throw XGDException(ErrCode::MISC, HERE(), LZ4F_getErrorName(compressed_size));
    }

    if ((compressed_size == 0) || ((compressed_size + 12) >= Xiso::SECTOR_SIZE)) 
    {
        out_file.write(in_buffer, Xiso::SECTOR_SIZE);
    } 
    else 
    {
        block_info |= 0x80000000;
        out_file.write(compress_buffer.data(), compressed_size);
    }

    if (out_file.fail()) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), out_filepath_1_.string());
    }

    block_index.push_back(block_info);
}

void CSOWriter::compress_and_write_sector_managed(std::ofstream& out_file, std::vector<uint32_t>& block_index, const char* in_buffer) 
{
    if (static_cast<uint64_t>(out_file.tellp()) > CSO::SPLIT_OFFSET) 
    {
        out_file.close();
        out_file = std::ofstream(out_filepath_2_, std::ios::binary);
        if (!out_file.is_open()) 
        {
            throw XGDException(ErrCode::FILE_OPEN, HERE(), out_filepath_2_.string());
        }
    }

    compress_and_write_sector(out_file, block_index, in_buffer);
}

void CSOWriter::finalize_out_files(std::ofstream& out_file, std::vector<uint32_t>& block_index) 
{
    out_file.seekp(0, std::ios::end);

    block_index.push_back(static_cast<uint32_t>(out_file.tellp() >> CSO::INDEX_ALIGNMENT));

    IOUtils::pad_to_modulus(out_file, 0x400, 0x00);

    if (std::filesystem::exists(out_filepath_2_)) 
    {
        out_file.close();
        out_file = std::ofstream(out_filepath_1_, std::ios::binary | std::ios::in | std::ios::out);
        if (!out_file.is_open()) 
        {
            throw XGDException(ErrCode::FILE_OPEN, HERE(), out_filepath_1_.string());
        }
    }

    out_file.seekp(sizeof(CSO::Header), std::ios::beg);
    out_file.write(reinterpret_cast<const char*>(block_index.data()), block_index.size() * sizeof(uint32_t));
    if (out_file.fail()) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), out_filepath_1_.string());
    }

    out_file.seekp(0, std::ios::end);
    IOUtils::pad_to_modulus(out_file, 0x400, 0x00);

    block_index.clear();
}

std::vector<std::filesystem::path> CSOWriter::out_paths() 
{
    if (std::filesystem::exists(out_filepath_2_)) 
    {
        return { out_filepath_1_, out_filepath_2_ };
    }

    try 
    {
        std::filesystem::rename(out_filepath_1_, out_filepath_base_);
        return { out_filepath_base_ };
    } 
    catch (const std::filesystem::filesystem_error& e) 
    {
        throw XGDException(ErrCode::FS_RENAME, HERE(), e.what());
    }

    return { out_filepath_1_ };
}
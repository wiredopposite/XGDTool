#include "AvlTree/AvlIterator.h"
#include "ImageWriter/CSOWriter/CSOWriter.h"

CSOWriter::CSOWriter(std::shared_ptr<ImageReader> image_reader, const ScrubType scrub_type) 
    :   image_reader_(image_reader),
        scrub_type_(scrub_type)
{
    init_lz4f_context();
}

CSOWriter::CSOWriter(const std::filesystem::path& in_dir_path)
    :   in_dir_path_(in_dir_path) 
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

    if (image_reader_ && scrub_type_ == ScrubType::FULL) 
    {
        AvlTree avl_tree(image_reader_->name(), image_reader_->directory_entries());
        convert_to_cso_from_avl(avl_tree);
    }
    else if (!in_dir_path_.empty())
    {
        AvlTree avl_tree(in_dir_path_.filename().string(), in_dir_path_);
        convert_to_cso_from_avl(avl_tree);
    }
    else if (!image_reader_)
    {
        throw XGDException(ErrCode::MISC, HERE(), "No input data to convert to CSO");
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

void CSOWriter::write_header(std::ofstream& out_file, std::vector<uint32_t>& block_index, AvlTree& avl_tree)
{
    Xiso::Header iso_header(static_cast<uint32_t>(avl_tree.root()->start_sector), 
                            static_cast<uint32_t>(avl_tree.root()->file_size), 
                            static_cast<uint32_t>(avl_tree.out_iso_size() / Xiso::SECTOR_SIZE), 
                            image_reader_ ? image_reader_->file_time() : Xiso::FileTime()); 

    for (size_t i = 0; i < sizeof(Xiso::Header) / Xiso::SECTOR_SIZE; ++i) 
    {
        compress_and_write_sector_managed(out_file, block_index, reinterpret_cast<const char*>(&iso_header) + (i * Xiso::SECTOR_SIZE));
    }
}

void CSOWriter::write_padding_sectors(std::ofstream& out_file, std::vector<uint32_t>& block_index, const uint32_t num_sectors, const char pad_byte)
{
    std::vector<char> pad_sector(Xiso::SECTOR_SIZE, pad_byte);

    for (uint32_t i = 0; i < num_sectors; ++i) 
    {
        compress_and_write_sector_managed(out_file, block_index, pad_sector.data());
    }
}

void CSOWriter::convert_to_cso_from_avl(AvlTree& avl_tree) 
{
    uint32_t out_iso_sectors = static_cast<uint32_t>(avl_tree.out_iso_size() / Xiso::SECTOR_SIZE);

    prog_total_ = avl_tree.total_bytes();
    prog_processed_ = 0;

    std::ofstream out_file(out_filepath_1_, std::ios::binary);
    if (!out_file.is_open()) 
    {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), out_filepath_1_.string());
    }

    XGDLog() << "Writing CSO file" << XGDLog::Endl;

    CSO::Header cso_header(avl_tree.out_iso_size());

    out_file.write(reinterpret_cast<const char*>(&cso_header), sizeof(CSO::Header));

    for (uint32_t i = 0; i < out_iso_sectors + 1; ++i) 
    {
        uint32_t zero = 0;
        out_file.write(reinterpret_cast<const char*>(&zero), sizeof(uint32_t)); // Dummy block index
    }

    std::vector<uint32_t> block_index;
    block_index.reserve(out_iso_sectors + 1);
    
    write_header(out_file, block_index, avl_tree);

    AvlIterator avl_iterator(avl_tree);
    const std::vector<AvlIterator::Entry>& avl_entries = avl_iterator.entries();

    uint32_t pad_sectors = static_cast<uint32_t>((avl_entries.front().offset - sizeof(Xiso::Header)) / Xiso::SECTOR_SIZE);
    write_padding_sectors(out_file, block_index, pad_sectors, 0x00);

    for (size_t i = 0; i < avl_entries.size(); i++) 
    {
        if (avl_entries[i].offset > block_index.size() * Xiso::SECTOR_SIZE) 
        {
            uint32_t pad_sectors = static_cast<uint32_t>((avl_entries[i].offset / Xiso::SECTOR_SIZE) - block_index.size());
            write_padding_sectors(out_file, block_index, pad_sectors, Xiso::PAD_BYTE);
        } 
        
        if ((avl_entries[i].offset / Xiso::SECTOR_SIZE) != block_index.size() || (avl_entries[i].offset % Xiso::SECTOR_SIZE)) 
        {
            throw XGDException(ErrCode::MISC, HERE(), "CSO file has become misaligned");
        }

        if (avl_entries[i].directory_entry) 
        {
            std::vector<char> entry_buffer;
            size_t processed_entries = write_directory_to_buffer(avl_entries, i, entry_buffer);
            i += processed_entries - 1;

            for (size_t j = 0; j < entry_buffer.size(); j += Xiso::SECTOR_SIZE) 
            {
                compress_and_write_sector_managed(out_file, block_index, entry_buffer.data() + j);
            }
        } 
        else 
        {
            if (image_reader_) 
            {
                write_file_from_reader(out_file, block_index, *avl_entries[i].node);
            } 
            else 
            {
                write_file_from_directory(out_file, block_index, *avl_entries[i].node);
            }
        }
    }

    if (block_index.size() < out_iso_sectors) 
    {
        uint32_t pad_sectors = out_iso_sectors - static_cast<uint32_t>(block_index.size());
        write_padding_sectors(out_file, block_index, pad_sectors, 0x00);
    }

    finalize_out_files(out_file, block_index);

    out_file.close();
}

void CSOWriter::write_file_from_reader(std::ofstream& out_file, std::vector<uint32_t>& block_index, AvlTree::Node& node) 
{
    ImageReader& image_reader = *image_reader_;
    uint64_t bytes_remaining = node.file_size;
    uint64_t read_position = image_reader.image_offset() + (node.old_start_sector * Xiso::SECTOR_SIZE);
    std::vector<char> read_buffer(Xiso::SECTOR_SIZE);

    while (bytes_remaining > 0) 
    {
        uint64_t read_size = std::min(bytes_remaining, Xiso::SECTOR_SIZE);

        image_reader.read_bytes(read_position, read_size, read_buffer.data());

        if (read_size < Xiso::SECTOR_SIZE) 
        {
            std::memset(read_buffer.data() + read_size, Xiso::PAD_BYTE, Xiso::SECTOR_SIZE - read_size);
        }

        compress_and_write_sector_managed(out_file, block_index, read_buffer.data());

        bytes_remaining -= read_size;
        read_position += read_size;

        XGDLog().print_progress(prog_processed_ += read_size, prog_total_);
    }
}

void CSOWriter::write_file_from_directory(std::ofstream& out_file, std::vector<uint32_t>& block_index, AvlTree::Node& node) 
{
    std::ifstream in_file(node.path, std::ios::binary);
    if (!in_file.is_open()) 
    {
        throw std::runtime_error("Failed to open input file: " + node.path.string());
    }

    uint64_t bytes_remaining = node.file_size;
    std::vector<char> read_buffer(Xiso::SECTOR_SIZE);

    while (bytes_remaining > 0) 
    {
        uint64_t read_size = std::min(bytes_remaining, Xiso::SECTOR_SIZE);

        in_file.read(read_buffer.data(), read_size);
        if (in_file.fail()) 
        {
            throw std::runtime_error("Failed to read from input file: " + node.path.string());
        }

        if (read_size < Xiso::SECTOR_SIZE) 
        {
            std::memset(read_buffer.data() + read_size, Xiso::PAD_BYTE, Xiso::SECTOR_SIZE - read_size);
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

    pad_to_modulus(out_file, CSO::FILE_MODULUS, 0x00);

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
    pad_to_modulus(out_file, CSO::FILE_MODULUS, 0x00);

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

void CSOWriter::pad_to_modulus(std::ofstream& out_file, const uint64_t modulus, const char pad_byte) 
{
    if ((out_file.tellp() % modulus) == 0) 
    {
        return;
    }

    uint64_t padding_len = modulus - (out_file.tellp() % modulus);
    std::vector<char> buffer(padding_len, pad_byte);

    out_file.write(buffer.data(), padding_len); 
    if (out_file.fail()) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write padding bytes");
    }
}
#include <lz4hc.h>

#include "ImageWriter/CCIWriter/CCIWriter.h"
#include "AvlTree/AvlIterator.h"

CCIWriter::CCIWriter(std::shared_ptr<ImageReader> image_reader, const ScrubType scrub_type)
    : image_reader_(image_reader), scrub_type_(scrub_type) {}

CCIWriter::CCIWriter(const std::filesystem::path& in_dir_path)
    :   in_dir_path_(in_dir_path) {}

std::vector<std::filesystem::path> CCIWriter::convert(const std::filesystem::path& out_cci_path) 
{
    out_filepath_base_ = out_cci_path;

    create_directory(out_cci_path.parent_path());

    out_filepath_1_ = out_cci_path;
    out_filepath_2_ = out_cci_path;
    out_filepath_1_.replace_extension(".1.cci");
    out_filepath_2_.replace_extension(".2.cci");

    if (image_reader_ && scrub_type_ == ScrubType::FULL) 
    {
        AvlTree avl_tree(image_reader_->name(), image_reader_->directory_entries());
        convert_to_cci_from_avl(avl_tree);
    }
    else if (!in_dir_path_.empty())
    {
        AvlTree avl_tree(in_dir_path_.filename().string(), in_dir_path_);
        convert_to_cci_from_avl(avl_tree);
    }
    else if (!image_reader_) 
    {
        throw XGDException(ErrCode::ISO_INVALID, HERE(), "No input data");
    }
    else
    {
        convert_to_cci(scrub_type_ == ScrubType::PARTIAL);
    }

    return out_paths();
}

void CCIWriter::write_iso_header(std::ofstream& out_file, std::vector<CCI::IndexInfo>& index_infos, AvlTree& avl_tree)
{
    Xiso::Header xiso_header(   static_cast<uint32_t>(avl_tree.root()->start_sector), 
                                static_cast<uint32_t>(avl_tree.root()->file_size), 
                                static_cast<uint32_t>(avl_tree.out_iso_size() / Xiso::SECTOR_SIZE),
                                image_reader_ ? image_reader_->file_time() : Xiso::FileTime());

    for (uint32_t i = 0; i < static_cast<uint32_t>(sizeof(Xiso::Header) / Xiso::SECTOR_SIZE); i++) 
    {
        compress_and_write_sector_managed(out_file, index_infos, reinterpret_cast<char*>(&xiso_header) + (i * Xiso::SECTOR_SIZE));
    }
}

void CCIWriter::write_padding_sectors(std::ofstream& out_file, std::vector<CCI::IndexInfo>& index_infos, const uint32_t num_sectors, const char pad_byte)
{
    std::vector<char> pad_sector(Xiso::SECTOR_SIZE, pad_byte);

    for (uint32_t i = 0; i < num_sectors; i++) 
    {
        compress_and_write_sector_managed(out_file, index_infos, pad_sector.data());
    }
}

void CCIWriter::convert_to_cci_from_avl(AvlTree& avl_tree) 
{
    uint64_t out_iso_size = avl_tree.out_iso_size();

    prog_total_ = avl_tree.total_bytes();
    prog_processed_ = 0;

    AvlIterator avl_iterator(avl_tree);
    std::vector<AvlIterator::Entry> avl_entries = avl_iterator.entries();

    std::ofstream out_file(out_filepath_1_, std::ios::binary);
    if (!out_file.is_open()) 
    {
        throw std::runtime_error("Failed to open output file: " + out_filepath_1_.string());
    }

    XGDLog() << "Writing CCI file..." << XGDLog::Endl;

    std::vector<CCI::IndexInfo> index_infos;

    write_iso_header(out_file, index_infos, avl_tree);

    uint32_t pad_sectors = static_cast<uint32_t>((avl_entries.front().offset - sizeof(Xiso::Header)) / Xiso::SECTOR_SIZE);
    write_padding_sectors(out_file, index_infos, pad_sectors, 0x00);

    for (size_t i = 0; i < avl_entries.size(); i++) 
    {
        if (avl_entries[i].offset > ((index_infos.size() - 1) * Xiso::SECTOR_SIZE)) 
        {
            uint32_t pad_sectors = static_cast<uint32_t>((avl_entries[i].offset / Xiso::SECTOR_SIZE) - index_infos.size());
            write_padding_sectors(out_file, index_infos, pad_sectors, Xiso::PAD_BYTE);
        } 

        if ((avl_entries[i].offset / Xiso::SECTOR_SIZE) < (index_infos.size() - 1) || (avl_entries[i].offset % Xiso::SECTOR_SIZE)) 
        {
            throw XGDException(ErrCode::MISC, HERE(), "CCI file has become misaligned");
        }

        if (avl_entries[i].directory_entry) 
        {
            std::vector<char> dir_buffer;
            size_t entries_processed = write_directory_to_buffer(avl_entries, i, dir_buffer);
            i += entries_processed - 1;

            for (size_t j = 0; j < dir_buffer.size() / Xiso::SECTOR_SIZE; j++) 
            {
                compress_and_write_sector_managed(out_file, index_infos, dir_buffer.data() + (j * Xiso::SECTOR_SIZE));
            }
        } 
        else 
        {
            if (image_reader_) 
            {
                write_file_from_reader(out_file, index_infos, *avl_entries[i].node);
            } 
            else 
            {
                write_file_from_dir(out_file, index_infos, *avl_entries[i].node);
            }
        }
    }

    pad_sectors = static_cast<uint32_t>((out_iso_size / Xiso::SECTOR_SIZE) - index_infos.size());
    write_padding_sectors(out_file, index_infos, pad_sectors, 0x00);

    finalize_out_file(out_file, index_infos);
    out_file.close();
}

void CCIWriter::write_file_from_reader(std::ofstream& out_file, std::vector<CCI::IndexInfo>& index_infos, AvlTree::Node& node) 
{
    ImageReader& image_reader = *image_reader_;
    size_t bytes_remaining = node.file_size;
    size_t read_position = image_reader.image_offset() + (node.old_start_sector * Xiso::SECTOR_SIZE);

    while (bytes_remaining > 0) 
    {
        size_t read_size = std::min(bytes_remaining, static_cast<size_t>(Xiso::SECTOR_SIZE));
        std::vector<char> read_buffer(Xiso::SECTOR_SIZE, Xiso::PAD_BYTE);

        image_reader.read_bytes(read_position, read_size, read_buffer.data());
        compress_and_write_sector_managed(out_file, index_infos, read_buffer.data());

        bytes_remaining -= read_size;
        read_position += read_size;

        XGDLog().print_progress(prog_processed_ += read_size, prog_total_);
    }
}

void CCIWriter::write_file_from_dir(std::ofstream& out_file, std::vector<CCI::IndexInfo>& index_infos, AvlTree::Node& node) 
{
    std::ifstream in_file(node.path, std::ios::binary);
    if (!in_file.is_open()) 
    {
        throw std::runtime_error("Failed to open input file: " + node.path.string());
    }

    size_t bytes_remaining = node.file_size;

    while (bytes_remaining > 0) 
    {
        size_t read_size = std::min(bytes_remaining, static_cast<size_t>(Xiso::SECTOR_SIZE));
        std::vector<char> read_buffer(Xiso::SECTOR_SIZE, Xiso::PAD_BYTE);

        in_file.read(read_buffer.data(), read_size);
        if (in_file.fail()) 
        {
            throw std::runtime_error("Failed to read from input file: " + node.path.string());
        }

        compress_and_write_sector_managed(out_file, index_infos, read_buffer.data());

        bytes_remaining -= read_size;

        XGDLog().print_progress(prog_processed_ += read_size, prog_total_);
    }

    in_file.close();
}

void CCIWriter::convert_to_cci(const bool scrub) 
{
    ImageReader& image_reader = *image_reader_;
    uint32_t end_sector = image_reader.total_sectors();
    uint32_t sector_offset = static_cast<uint32_t>(image_reader.image_offset() / Xiso::SECTOR_SIZE);
    const std::unordered_set<uint32_t>* data_sectors;

    prog_total_ = end_sector - sector_offset;
    prog_processed_ = 0;

    if (scrub) 
    {
        data_sectors = &image_reader.data_sectors();
        end_sector = std::min(end_sector, image_reader.max_data_sector() + 1);
        prog_total_ = end_sector - sector_offset - 1;
    }

    std::ofstream out_file(out_filepath_1_, std::ios::binary);
    if (!out_file.is_open()) 
    {
        throw std::runtime_error("Failed to open output file: " + out_filepath_1_.string());
    }

    std::vector<char> read_buffer(Xiso::SECTOR_SIZE);
    const int multiple = (1 << CCI::INDEX_ALIGNMENT);
    uint32_t current_sector = sector_offset;
    std::vector<CCI::IndexInfo> index_infos;

    while (current_sector < end_sector) 
    {
        std::vector<char> empty_buffer(sizeof(CCI::Header), 0);
        out_file.write(reinterpret_cast<char*>(empty_buffer.data()), sizeof(CCI::Header));

        bool split = false;

        while (current_sector < end_sector) 
        {
            bool write_sector = true;

            if (scrub && image_reader.platform() == Platform::OGX) 
            {
                write_sector = data_sectors->find(current_sector) != data_sectors->end();
            }

            if (write_sector) 
            {
                image_reader.read_sector(current_sector, read_buffer.data());
            } 
            else 
            {
                std::fill(read_buffer.begin(), read_buffer.end(), 0);
            }

            compress_and_write_sector(out_file, index_infos, read_buffer.data());

            current_sector++;

            XGDLog().print_progress(prog_processed_++, prog_total_);

            if (static_cast<uint64_t>(out_file.tellp()) > CCI::SPLIT_OFFSET) 
            {
                split = true;
                break;
            }
        }

        finalize_out_file(out_file, index_infos);
        out_file.close();

        if (split) 
        {
            out_file = std::ofstream(out_filepath_2_, std::ios::binary);
            if (!out_file.is_open()) 
            {
                throw std::runtime_error("Failed to open output file: " + out_filepath_2_.string());
            }
        }
    }
}

void CCIWriter::compress_and_write_sector(std::ofstream& out_file, std::vector<CCI::IndexInfo>& index_infos, const char* in_buffer) 
{
    const int multiple = (1 << CCI::INDEX_ALIGNMENT);
    std::vector<char> compress_buffer(Xiso::SECTOR_SIZE);   

    int compressed_size = LZ4_compress_HC(in_buffer, compress_buffer.data(), Xiso::SECTOR_SIZE, Xiso::SECTOR_SIZE, 12);

    if (compressed_size > 0 && compressed_size < static_cast<int>(Xiso::SECTOR_SIZE - (4 + multiple))) 
    {
        uint8_t padding = static_cast<uint8_t>(((compressed_size + 1 + multiple - 1) / multiple * multiple) - (compressed_size + 1));
        out_file.write(reinterpret_cast<char*>(&padding), sizeof(uint8_t));
        out_file.write(compress_buffer.data(), compressed_size);

        if (padding != 0) 
        {
            std::vector<char> empty_buffer(padding, 0);
            out_file.write(empty_buffer.data(), padding);
        }

        index_infos.push_back({ static_cast<uint32_t>(compressed_size + 1 + padding), true });
    } 
    else 
    {
        out_file.write(in_buffer, Xiso::SECTOR_SIZE);
        index_infos.push_back({ Xiso::SECTOR_SIZE, false });
    }

    if (out_file.fail()) 
    {
        throw std::runtime_error("Failed to write to output file");
    }
}

void CCIWriter::compress_and_write_sector_managed(std::ofstream& out_file, std::vector<CCI::IndexInfo>& index_infos, const char* in_buffer) 
{
    if (index_infos.size() == 0 && out_file.tellp() == 0) 
    {
        std::vector<char> empty_buffer(sizeof(CCI::Header), 0);
        out_file.write(empty_buffer.data(), sizeof(CCI::Header));
    }

    compress_and_write_sector(out_file, index_infos, in_buffer);

    if (static_cast<uint64_t>(out_file.tellp()) > CCI::SPLIT_OFFSET) 
    {
        finalize_out_file(out_file, index_infos);
        out_file.close();

        out_file = std::ofstream(out_filepath_2_, std::ios::binary);
        if (!out_file.is_open()) 
        {
            throw XGDException(ErrCode::FILE_OPEN, HERE(), "Failed to open output file: " + out_filepath_2_.string());
        }
    }
}

void CCIWriter::finalize_out_file(std::ofstream& out_file, std::vector<CCI::IndexInfo>& index_infos) 
{
    out_file.seekp(0, std::ios::end);

    uint64_t index_offset = out_file.tellp();
    uint64_t uncompressed_size = index_infos.size() * Xiso::SECTOR_SIZE;
    uint32_t position = CCI::HEADER_SIZE;

    for (const auto& index_info : index_infos) 
    {
        uint32_t index = static_cast<uint32_t>(position >> CCI::INDEX_ALIGNMENT) | (index_info.compressed ? 0x80000000 : 0);
        out_file.write(reinterpret_cast<const char*>(&index), sizeof(uint32_t));
        position += index_info.value;
    }

    CCI::Header cci_header(uncompressed_size, index_offset);
    uint32_t index_end = static_cast<uint32_t>(position >> CCI::INDEX_ALIGNMENT);

    out_file.write(reinterpret_cast<const char*>(&index_end), sizeof(uint32_t));
    out_file.seekp(0, std::ios::beg);
    out_file.write(reinterpret_cast<char*>(&cci_header), sizeof(CCI::Header));

    if (out_file.fail()) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write to output file");  
    }

    index_infos.clear();
}

std::vector<std::filesystem::path> CCIWriter::out_paths() 
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
    catch (const std::exception& e) 
    {
        XGDLog(Error) << "Warning: Failed to rename output file: " << e.what() << XGDLog::Endl;
    }

    return { out_filepath_1_ };
}
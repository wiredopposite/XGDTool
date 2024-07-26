#include <algorithm>
#include <memory>

#include "Utils/EndianUtils.h"
#include "ImageWriter/CCIWriter/CCIWriter.h"
#include "ImageWriter/CSOWriter/CSOWriter.h"
#include "ImageWriter/GoDWriter/GoDWriter.h"
#include "ImageWriter/XisoWriter/XisoWriter.h"
#include "ImageWriter/ZARWriter/ZARWriter.h"
#include "ImageWriter/ImageWriter.h"

std::unique_ptr<ImageWriter> ImageWriter::create_instance(std::shared_ptr<ImageReader> image_reader, TitleHelper& title_helper, const OutputSettings& out_settings) 
{
    switch (out_settings.file_type) 
    {
        case FileType::ISO:
            return std::make_unique<XisoWriter>(image_reader, out_settings.scrub_type, out_settings.split);
        case FileType::ZAR:
            return std::make_unique<ZARWriter>(image_reader);
        case FileType::GoD:
            return std::make_unique<GoDWriter>(image_reader, title_helper, out_settings.scrub_type);
        case FileType::CSO:
            return std::make_unique<CSOWriter>(image_reader, out_settings.scrub_type);
        case FileType::CCI:
            return std::make_unique<CCIWriter>(image_reader, out_settings.scrub_type);
        default:
            throw XGDException(ErrCode::ISO_INVALID, HERE(), "Unknown file type");
    }
}

std::unique_ptr<ImageWriter> ImageWriter::create_instance(const std::filesystem::path& in_dir_path, TitleHelper& title_helper, const OutputSettings& out_settings) 
{
    switch (out_settings.file_type) 
    {
        case FileType::ISO:
            return std::make_unique<XisoWriter>(in_dir_path, out_settings.split);
        case FileType::ZAR:
            return std::make_unique<ZARWriter>(in_dir_path);
        case FileType::GoD:
            return std::make_unique<GoDWriter>(in_dir_path, title_helper);
        case FileType::CSO:
            return std::make_unique<CSOWriter>(in_dir_path);
        case FileType::CCI:
            return std::make_unique<CCIWriter>(in_dir_path);
        default:
            throw XGDException(ErrCode::ISO_INVALID, HERE(), "Unknown file type");
    }
}

void ImageWriter::create_directory(const std::filesystem::path& dir_path) 
{
    if (!std::filesystem::exists(dir_path)) 
    {
        try 
        { 
            std::filesystem::create_directories(dir_path); 
        } 
        catch (const std::filesystem::filesystem_error& e) 
        {
            throw XGDException(ErrCode::FS_MKDIR, HERE(), e.what());
        }
    }
}

Xiso::DirectoryEntry::Header ImageWriter::get_directory_entry_header(const AvlTree::Node& node)
{
    Xiso::DirectoryEntry::Header dir_header;

    dir_header.left_offset  = node.left_child ? static_cast<uint16_t>(node.left_child->offset / sizeof(uint32_t)) : 0;
    dir_header.right_offset = node.right_child ? static_cast<uint16_t>(node.right_child->offset / sizeof(uint32_t)) : 0;
    dir_header.start_sector = static_cast<uint32_t>(node.start_sector);
    dir_header.file_size    = static_cast<uint32_t>(node.file_size + (node.subdirectory ? ((Xiso::SECTOR_SIZE - (node.file_size % Xiso::SECTOR_SIZE)) % Xiso::SECTOR_SIZE) : 0));
    dir_header.attributes   = node.subdirectory ? Xiso::ATTRIBUTE_DIRECTORY : Xiso::ATTRIBUTE_FILE;
    dir_header.name_length  = static_cast<uint8_t>(std::min(node.filename.size(), static_cast<size_t>(UINT8_MAX)));

    EndianUtils::little_16(dir_header.left_offset);
    EndianUtils::little_16(dir_header.right_offset);
    EndianUtils::little_32(dir_header.start_sector);
    EndianUtils::little_32(dir_header.file_size);

    return dir_header;
}

//Will write range of directory table entries, starting from start_index, to buffer, then pad to sector boundary. Returns number of processed entries.
size_t ImageWriter::write_directory_to_buffer(const std::vector<AvlIterator::Entry>& avl_entries, const size_t start_index, std::vector<char>& entry_buffer)
{
    size_t entries_processed = 0;

    for (uint64_t i = start_index; i < avl_entries.size(); ++i)
    {
        Xiso::DirectoryEntry::Header dir_header = get_directory_entry_header(*avl_entries[i].node);

        uint64_t entry_len = sizeof(Xiso::DirectoryEntry::Header) + dir_header.name_length;
        uint64_t buffer_pos = entry_buffer.size();

        entry_buffer.resize(buffer_pos + entry_len, Xiso::PAD_BYTE);

        std::memcpy(entry_buffer.data() + buffer_pos, &dir_header, sizeof(Xiso::DirectoryEntry::Header));
        std::memcpy(entry_buffer.data() + buffer_pos + sizeof(Xiso::DirectoryEntry::Header), avl_entries[i].node->filename.c_str(), dir_header.name_length);

        entries_processed++;

        if (i == avl_entries.size() - 1 || 
            !avl_entries[i + 1].directory_entry ||
            avl_entries[i + 1].node->directory_start != avl_entries[i].node->directory_start) 
        {
            break;
        }

        uint64_t padding_len = avl_entries[i + 1].node->offset - entry_buffer.size();

        if (padding_len > 0) 
        {
            entry_buffer.resize(entry_buffer.size() + padding_len, Xiso::PAD_BYTE);
        }
    }

    if (entry_buffer.size() % Xiso::SECTOR_SIZE) //Pad up to sector boundary
    {
        entry_buffer.resize(entry_buffer.size() + (Xiso::SECTOR_SIZE - (entry_buffer.size() % Xiso::SECTOR_SIZE)), Xiso::PAD_BYTE);
    }

    return entries_processed;
}

uint32_t ImageWriter::num_sectors(const uint64_t num_bytes)
{
    return static_cast<uint32_t>(num_bytes / Xiso::SECTOR_SIZE) + ((num_bytes % Xiso::SECTOR_SIZE) ? 1 : 0);
}
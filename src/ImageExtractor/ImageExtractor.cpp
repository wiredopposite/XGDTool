#include "Executable/ExeTool.h"
#include "Utils/StringUtils.h"
#include "ImageExtractor/ImageExtractor.h"

ImageExtractor::ImageExtractor(ImageReader& image_reader, TitleHelper& title_helper, const bool allowed_media_patch, const bool rename_xbe)
    :   image_reader_(image_reader), 
        title_helper_(title_helper),
        allowed_media_patch_(allowed_media_patch),
        rename_xbe_(rename_xbe) {}

void ImageExtractor::extract(const std::filesystem::path& out_dir_path) 
{
    const std::vector<Xiso::DirectoryEntry>& dir_entries = image_reader_.directory_entries();
    
    prog_total_ = image_reader_.total_file_bytes();
    prog_processed_ = 0;

    if (!std::filesystem::exists(out_dir_path)) 
    {
        std::filesystem::create_directories(out_dir_path);
    }

    auto cwd = std::filesystem::current_path();
    std::filesystem::current_path(out_dir_path);

    XGDLog() << "Extracting image" << XGDLog::Endl;

    for (auto& dir_entry : dir_entries) 
    {
        if (!StringUtils::safe_string(dir_entry.filename)) 
        {
            XGDLog(Error) << "Filename contains potentially dangerous characters.\nSkipping: " << dir_entry.filename << XGDLog::Endl;
            continue;
        }

        if (dir_entry.header.attributes & Xiso::ATTRIBUTE_DIRECTORY) 
        {
            if (!dir_entry.path.empty() && !std::filesystem::exists(dir_entry.path)) 
            {
                std::filesystem::create_directories(dir_entry.path);
            }
        } 
        else 
        {
            if (!dir_entry.path.parent_path().empty() && !std::filesystem::exists(dir_entry.path.parent_path())) 
            {
                std::filesystem::create_directories(dir_entry.path.parent_path());
            }

            if ((allowed_media_patch_ || rename_xbe_) &&
                dir_entry.filename.size() > 4 &&
                StringUtils::case_insensitive_search(dir_entry.filename, ".xbe"))
            {
                extract_file_xbe_patch(dir_entry);
            }
            else
            {
                extract_file(dir_entry);
            }
        }
    }

    std::filesystem::current_path(cwd);
}

void ImageExtractor::extract_file(const Xiso::DirectoryEntry& dir_entry) 
{
    std::ofstream out_file(dir_entry.path, std::ios::binary);
    if (!out_file.is_open()) 
    {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), "Failed to open file for writing: " + dir_entry.path.string());
    }
    
    size_t bytes_remaining = dir_entry.header.file_size;
    uint64_t read_position = image_reader_.image_offset() + static_cast<uint64_t>(dir_entry.header.start_sector) * Xiso::SECTOR_SIZE;

    std::vector<char> buffer(XGD::BUFFER_SIZE);

    while (bytes_remaining > 0) 
    {
        size_t read_size = std::min(bytes_remaining, buffer.size());

        image_reader_.read_bytes(read_position, read_size, buffer.data());

        out_file.write(buffer.data(), read_size);
        if (!out_file) 
        {
            throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write to file: " + dir_entry.path.string());
        }

        bytes_remaining -= read_size;
        read_position += read_size;

        XGDLog().print_progress(prog_processed_ += read_size, prog_total_);
    }

    out_file.close();
}

void ImageExtractor::extract_file_xbe_patch(const Xiso::DirectoryEntry& dir_entry) 
{
    ExeTool exe_tool(image_reader_, dir_entry.path);
    Xbe::Cert xbe_cert = exe_tool.xbe_cert();

    if (allowed_media_patch_)
    {
        uint32_t patch = Xbe::AllowedMedia::HARD_DISK | Xbe::AllowedMedia::NONSECURE_HARD_DISK | Xbe::AllowedMedia::MEDIA_BOARD;
        EndianUtils::little_32(patch);
        xbe_cert.allowed_media_types |= patch;
    }
    if (rename_xbe_)
    {
        std::memcpy(&xbe_cert.title_name, title_helper_.title_name().data(), std::min(title_helper_.title_name().size() * sizeof(char16_t), static_cast<size_t>(80)));
    }
    
    size_t bytes_remaining = dir_entry.header.file_size;
    uint32_t current_sector = static_cast<uint32_t>(image_reader_.image_offset() / Xiso::SECTOR_SIZE) + dir_entry.header.start_sector;

    uint64_t cert_offset = static_cast<uint64_t>(current_sector) * Xiso::SECTOR_SIZE + exe_tool.cert_offset();
    uint32_t cert_sector = static_cast<uint32_t>(cert_offset / Xiso::SECTOR_SIZE);
    uint32_t cert_offset_in_sector = static_cast<uint32_t>(cert_offset % Xiso::SECTOR_SIZE);

    std::vector<char> buffer(Xiso::SECTOR_SIZE);

    std::ofstream out_file(dir_entry.path, std::ios::binary);
    if (!out_file.is_open()) 
    {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), "Failed to open file for writing: " + dir_entry.path.string());
    }

    while (bytes_remaining > 0) 
    {
        if (current_sector == cert_sector) 
        {
            std::vector<char> cert_buffer(Xiso::SECTOR_SIZE * 2);
            size_t write_size = std::min(bytes_remaining, cert_buffer.size());
            
            image_reader_.read_sector(current_sector, cert_buffer.data());
            current_sector++;

            if (current_sector <= image_reader_.total_sectors())
            {
                image_reader_.read_sector(current_sector, cert_buffer.data() + Xiso::SECTOR_SIZE);
                current_sector++;
            }

            std::memcpy(cert_buffer.data() + cert_offset_in_sector, &xbe_cert, sizeof(Xbe::Cert));

            // XGDLog(Debug)   << "Patching XBE cert at offset: " << cert_sector * Xiso::SECTOR_SIZE + cert_offset_in_sector 
            //                 << "\nCalculated Offset: " 
            //                 << image_reader_.image_offset() + (dir_entry.header.start_sector * Xiso::SECTOR_SIZE) + exe_tool.cert_offset() 
            //                 << XGDLog::Endl;

            out_file.write(cert_buffer.data(), write_size);
            if (out_file.fail())
            {
                throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write to file: " + dir_entry.path.string());
            }

            bytes_remaining -= write_size;

            XGDLog().print_progress(prog_processed_ += write_size, prog_total_);

            continue;
        } 

        size_t write_size = std::min(bytes_remaining, static_cast<size_t>(Xiso::SECTOR_SIZE));

        image_reader_.read_sector(current_sector, buffer.data());

        out_file.write(buffer.data(), write_size);
        if (!out_file) 
        {
            throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write to file: " + dir_entry.path.string());
        }

        bytes_remaining -= write_size;
        current_sector++;

        XGDLog().print_progress(prog_processed_ += write_size, prog_total_);
    }

    out_file.close();
}
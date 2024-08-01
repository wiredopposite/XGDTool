#include <thread>

#include "XGD.h"
#include "ZARExtractor/ZARExtractor.h"

ZARExtractor::ZARExtractor(const std::filesystem::path& in_zar_path)
    : in_zar_path_(in_zar_path) {};

void ZARExtractor::list_files()
{
    std::unique_ptr<ZArchiveReader> z_reader(ZArchiveReader::OpenFromFile(in_zar_path_));

    if (!z_reader)
    {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), "Failed to open ZArchive file");
    }

    calculate_total_recursive(z_reader.get(), "", true);
}

void ZARExtractor::calculate_total_recursive(ZArchiveReader* reader, std::string src_path, bool list_files)
{
    ZArchiveNodeHandle dir_handle = reader->LookUp(src_path, false, true);

    if (dir_handle == ZARCHIVE_INVALID_NODE)
    {
        return;
    }

    uint32_t dir_entry_count = reader->GetDirEntryCount(dir_handle);

    for (uint32_t i = 0; i < dir_entry_count; ++i)
    {
        ZArchiveReader::DirEntry dir_entry;
        
        if (!reader->GetDirEntry(dir_handle, i, dir_entry))
        {
            return;
        }

        if (dir_entry.isDirectory)
        {
            calculate_total_recursive(reader, src_path + "/" + std::string(dir_entry.name), list_files);
        }
        else if (dir_entry.isFile)
        {
            prog_total_ += dir_entry.size;

            if (list_files)
            {
                XGDLog() << src_path + "/" + std::string(dir_entry.name) << " (" << dir_entry.size << " Bytes)\n";
            }
        }
    }
}

void ZARExtractor::extract(const std::filesystem::path& out_dir_path)
{
    std::unique_ptr<ZArchiveReader> z_reader(ZArchiveReader::OpenFromFile(in_zar_path_));

    if (!z_reader)
    {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), "Failed to open ZArchive file");
    }

    calculate_total_recursive(z_reader.get(), "", false);

    XGDLog() << "Extracting files from ZAR archive" << XGDLog::Endl;

    if (!extract_recursive(z_reader.get(), "", out_dir_path))
    {
        throw XGDException(ErrCode::MISC, HERE(), "Failed to extract ZArchive file");
    }
}

bool ZARExtractor::extract_recursive(ZArchiveReader* reader, std::string src_path, std::filesystem::path out_dir_path)
{
    ZArchiveNodeHandle dir_handle = reader->LookUp(src_path, false, true);

    if (dir_handle == ZARCHIVE_INVALID_NODE)
    {
        return false;
    }

    uint32_t dir_entry_count = reader->GetDirEntryCount(dir_handle);

    for (uint32_t i = 0; i < dir_entry_count; ++i)
    {
        ZArchiveReader::DirEntry dir_entry;
        
        if (!reader->GetDirEntry(dir_handle, i, dir_entry))
        {
            return false;
        }

        std::filesystem::path entry_path = out_dir_path / std::string(dir_entry.name);

        if (dir_entry.isDirectory)
        {
            if (!std::filesystem::create_directories(entry_path))
            {
                return false;
            }
            if (!extract_recursive(reader, src_path + "/" + std::string(dir_entry.name), entry_path))
            {
                return false;
            }
        }
        else if (dir_entry.isFile)
        {
            if (!extract_file(reader, src_path + "/" + std::string(dir_entry.name), entry_path))
            {
                return false;
            }
        }
    }

    return true;
}

bool ZARExtractor::extract_file(ZArchiveReader* reader, std::string_view src_path, const std::filesystem::path& path)
{
    ZArchiveNodeHandle file_handle = reader->LookUp(src_path, true, false);

    if (file_handle == ZARCHIVE_INVALID_NODE)
    {
        return false;
    }

    std::vector<uint8_t> buffer;
    buffer.resize(XGD::BUFFER_SIZE);

    std::ofstream file_out(path, std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);
    if (!file_out.is_open())
    {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), path.string());
    }

    uint64_t read_offset = 0;

    while (true)
    {
        uint64_t bytes_read = reader->ReadFromFile(file_handle, read_offset, buffer.size(), buffer.data());

        if (bytes_read == 0)
        {
            break;
        }

        file_out.write((const char*)buffer.data(), bytes_read);
        read_offset += bytes_read;

        XGDLog().print_progress(prog_processed_ += bytes_read, prog_total_);

        check_status_flags();
    }

    if (read_offset != reader->GetFileSize(file_handle))
    {
        return false;
    }

    return true;
}

void ZARExtractor::check_status_flags()
{
    if (write_cancel_flag_)
    {
        throw XGDException(ErrCode::CANCELLED, HERE(), "Extraction cancelled");
    }

    while (write_pause_flag_)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
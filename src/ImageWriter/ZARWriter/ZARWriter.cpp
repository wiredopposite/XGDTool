#include "ImageWriter/ZARWriter/ZARWriter.h"

struct PackContext {
	std::filesystem::path out_filepath;
	std::ofstream current_out_file;
	bool has_error{false};
};

void _pack_NewOutputFile(const int32_t partIndex, void* ctx)
{
	PackContext* pack_context = static_cast<PackContext*>(ctx);
    
	pack_context->current_out_file = std::ofstream(pack_context->out_filepath, std::ios::binary);
	if (!pack_context->current_out_file.is_open()) 
    {
		pack_context->has_error = true;
	}
}

void _pack_WriteOutputData(const void* data, size_t length, void* ctx)
{
	PackContext* pack_context = static_cast<PackContext*>(ctx);
	pack_context->current_out_file.write(reinterpret_cast<const char*>(data), length);
}

ZARWriter::ZARWriter(std::shared_ptr<ImageReader> image_reader)
    : image_reader_(image_reader) {}

ZARWriter::ZARWriter(const std::filesystem::path& in_dir_path)
    : in_dir_path_(in_dir_path) {}

std::vector<std::filesystem::path> ZARWriter::convert(const std::filesystem::path& out_zar_path) 
{
    create_directory(out_zar_path.parent_path());
    
    if (image_reader_) 
    {
        convert_from_iso(out_zar_path);
    } 
    else if (!in_dir_path_.empty())
    {
        convert_from_dir(out_zar_path);
    }
    else 
    {
        throw XGDException(ErrCode::ISO_INVALID, HERE(), "No input source provided");
    }

    return { out_zar_path };
}

void ZARWriter::convert_from_dir(const std::filesystem::path& out_zar_path) 
{
    uint64_t prog_total = 0;
    uint64_t prog_processed = 0;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(in_dir_path_)) 
    {
        if (entry.is_regular_file()) 
        {
            prog_total += std::filesystem::file_size(entry);
        }
    }
    
	PackContext pack_context;
	pack_context.out_filepath = out_zar_path;

    ZArchiveWriter z_writer(_pack_NewOutputFile, _pack_WriteOutputData, &pack_context);

    std::error_code ec;
    std::vector<char> buffer(64 * 1024);

    XGDLog() << "Writing files to ZAR archive" << XGDLog::Endl;

    for (auto const& dir_entry : std::filesystem::recursive_directory_iterator(in_dir_path_)) 
    {
        std::filesystem::path entry_path = std::filesystem::relative(dir_entry.path(), in_dir_path_, ec);

        if (dir_entry.is_directory()) 
        {
            if (!z_writer.MakeDir(entry_path.generic_string().c_str(), false)) 
            {
                throw XGDException(ErrCode::FILE_WRITE, HERE(), entry_path.string());
            }

        }
        else if (dir_entry.is_regular_file()) 
        {
            if (!z_writer.StartNewFile(entry_path.generic_string().c_str())) 
            {
                throw XGDException(ErrCode::FILE_WRITE, HERE(), entry_path.string());
            }

            auto bytes_remaining = std::filesystem::file_size(dir_entry);

            std::ifstream in_file(dir_entry.path(), std::ios::binary);
            if (!in_file.is_open()) 
            {
                throw XGDException(ErrCode::FILE_READ, HERE(), entry_path.string());
            }

            while (bytes_remaining > 0) 
            {
                size_t read_size = std::min(buffer.size(), static_cast<size_t>(bytes_remaining));

                in_file.read(buffer.data(), read_size);
                if (in_file.fail() || in_file.gcount() != read_size) 
                {
                    throw XGDException(ErrCode::FILE_READ, HERE(), entry_path.string());
                }

                z_writer.AppendData(buffer.data(), read_size);

                bytes_remaining -= read_size;

                XGDLog().print_progress(prog_processed += read_size, prog_total);
            }

            in_file.close();
        }
    }
    if (pack_context.has_error) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), out_zar_path.string());
    }

    z_writer.Finalize();
}

void ZARWriter::convert_from_iso(const std::filesystem::path& out_zar_path) 
{
    ImageReader& image_reader = *image_reader_;
	PackContext pack_context;
	pack_context.out_filepath = out_zar_path;
    ZArchiveWriter z_writer(_pack_NewOutputFile, _pack_WriteOutputData, &pack_context);

    uint64_t prog_total = image_reader.total_file_bytes();
    uint64_t prog_processed = 0;
    
    std::error_code ec;
    std::vector<char> buffer(64 * 1024);

    XGDLog() << "Writing files to ZAR archive" << XGDLog::Endl;

    for (auto const& dir_entry : image_reader.directory_entries()) 
    {
        std::filesystem::path abs_path = image_reader.name() / dir_entry.path;
        std::filesystem::path entry_path = std::filesystem::relative(abs_path, image_reader.name(), ec);

        if (dir_entry.header.attributes & Xiso::ATTRIBUTE_DIRECTORY) 
        {
            if (!z_writer.MakeDir(entry_path.generic_string().c_str(), false)) 
            {
                throw XGDException(ErrCode::FILE_WRITE, HERE(), entry_path.string());
            }

        } 
        else 
        {
            if (!z_writer.StartNewFile(entry_path.generic_string().c_str())) 
            {
                throw XGDException(ErrCode::FILE_WRITE, HERE(), entry_path.string());
            }

            auto bytes_remaining = dir_entry.header.file_size;
            auto file_offset = image_reader.image_offset() + (static_cast<uint64_t>(dir_entry.header.start_sector) * Xiso::SECTOR_SIZE);

            while (bytes_remaining > 0) 
            {
                uint32_t read_size = std::min(static_cast<uint32_t>(buffer.size()), bytes_remaining);

                image_reader.read_bytes(file_offset + (dir_entry.header.file_size - bytes_remaining), read_size, buffer.data());

                z_writer.AppendData(buffer.data(), read_size);

                bytes_remaining -= read_size;

                XGDLog().print_progress(prog_processed += read_size, prog_total);
            }
        }
    }

    if (pack_context.has_error) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), out_zar_path.string());
    }

    z_writer.Finalize();
}
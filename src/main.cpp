#include <filesystem>
#include <memory>

#include <CLI/CLI.hpp>

#include "XGD.h"
#include "Common/StringUtils.h"
#include "ImageExtractor/ImageExtractor.h"
#include "ImageReader/ImageReader.h"
#include "ImageWriter/ImageWriter.h"
#include "TitleHelper/TitleHelper.h"

struct InputInfo 
{
    FileType in_file_type;
    std::vector<std::filesystem::path> in_paths;
};

bool has_extension(const std::filesystem::path& path, const std::string& extension) 
{
    if (path.filename().string().size() > extension.size()) 
    {
        return StringUtils::case_insensitive_search(path.filename().string(), extension);
    }
    return false;
}

bool is_extracted_dir(const std::filesystem::path& path) 
{
    for (const auto& entry : std::filesystem::directory_iterator(path)) 
    {
        if (entry.is_regular_file()) 
        {
            if (has_extension(entry.path(), ".xbe")) 
            {
                return true;
            }
            else if (has_extension(entry.path(), ".xex")) 
            {
                return true;
            }
        }
    }
    return false;
}

bool is_god_dir_helper(const std::filesystem::path& path, int current_depth, int max_depth) {
    if (current_depth > max_depth) 
    {
        return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(path)) 
    {
        if (entry.is_regular_file() && entry.path().filename().string().rfind("Data", 0) == 0) 
        {
            if (entry.path().parent_path().extension() == ".data") 
            {
                return true;
            }
        } 
        else if (entry.is_directory()) 
        {
            if (is_god_dir_helper(entry.path(), current_depth + 1, max_depth)) 
            {
                return true;
            }
        }
    }
    return false;
}

bool is_god_dir(const std::filesystem::path& path) 
{
    return is_god_dir_helper(path, 0, 3);
}

FileType get_filetype(const std::filesystem::path& path) 
{
    if (std::filesystem::is_directory(path)) 
    {
        if (is_extracted_dir(path)) 
        {
            return FileType::DIR;
        } 
        else if (is_god_dir(path)) 
        {
            return FileType::GoD;
        }
    }
    else if (std::filesystem::is_regular_file(path))
    {
        if (has_extension(path, ".iso")) 
        {
            return FileType::ISO;
        } 
        else if (has_extension(path, ".zar")) 
        {
            return FileType::ZAR;
        } 
        else if (has_extension(path, ".cso")) 
        {
            return FileType::CSO;
        } 
        else if (has_extension(path, ".cci")) 
        {
            return FileType::CCI;
        } 
        else if (has_extension(path, ".xbe")) 
        {
            return FileType::XBE;
        }
    }
    return FileType::UNKNOWN;
}

bool is_batch_dir(const std::filesystem::path& path) 
{
    for (const auto& entry : std::filesystem::directory_iterator(path)) 
    {
        if (get_filetype(entry.path()) != FileType::UNKNOWN) 
        {
            return true;
        }
    }
    return false;
}

std::filesystem::path get_output_path(const InputInfo& input_info, const std::filesystem::path& base_output_dir, std::unique_ptr<TitleHelper>& title_helper, FileType out_file_type, bool xemu)
{
    std::filesystem::path out_path;
    
    switch (out_file_type) 
    {
        case FileType::DIR:
            out_path = base_output_dir / title_helper->folder_name();
            break;
        case FileType::ISO:
            if (xemu)
            {
                out_path = base_output_dir / (title_helper->iso_name() + ".iso");
                break;
            }
            out_path = base_output_dir / title_helper->folder_name() / (title_helper->iso_name() + ".iso");
            break;
        case FileType::GoD:
            out_path = base_output_dir / title_helper->god_folder_name();
            break;
        case FileType::CCI:
            out_path = base_output_dir / title_helper->folder_name() / (title_helper->iso_name() + ".cci");
            break;
        case FileType::CSO:
            out_path = base_output_dir / title_helper->folder_name() / (title_helper->iso_name() + ".cso");
            break;
        case FileType::ZAR:
            out_path = base_output_dir / (title_helper->iso_name() + ".zar");
            break;
        case FileType::XBE:
            if (input_info.in_file_type == FileType::DIR || 
                input_info.in_file_type == FileType::XBE || 
                title_helper->platform() != Platform::OGX) 
            {
                throw XGDException(ErrCode::MISC, HERE(), "Cannot generate attach XBE from input file");
            }
            out_path = input_info.in_paths.front().parent_path() / "default.xbe";
            break;
        default:
            throw XGDException(ErrCode::MISC, HERE(), "Unknown output format");
    }

    return std::filesystem::absolute(out_path);
}

std::vector<std::filesystem::path> find_split_filepaths(const std::filesystem::path& in_filepath) 
{
    std::string extension = in_filepath.extension().string();
    std::filesystem::path stem = in_filepath.stem();
    std::string stem_str = stem.string();

    if (stem_str.size() > 2) 
    {
        std::string subextension = stem_str.substr(stem_str.size() - 2);
        if (subextension == ".1" || subextension == ".2") 
        {
            std::string base_filename = stem_str.substr(0, stem_str.size() - 2);
            std::filesystem::path base_path = in_filepath.parent_path() / base_filename;

            if (subextension == ".1") 
            {
                std::filesystem::path in_filepath1 = in_filepath;
                std::filesystem::path in_filepath2 = base_path.string() + ".2" + extension;
                if (std::filesystem::exists(in_filepath2)) 
                {
                    return { in_filepath1, in_filepath2 };
                }
            } 
            else if (subextension == ".2") 
            {
                std::filesystem::path in_filepath1 = base_path.string() + ".1" + extension;
                std::filesystem::path in_filepath2 = in_filepath;
                if (std::filesystem::exists(in_filepath1)) 
                {
                    return { in_filepath1, in_filepath2 };
                } 
                else 
                {
                    throw XGDException(ErrCode::FS_EXISTS, HERE(), "Missing first part of split file: " + in_filepath.string());
                }
            }
        }
    }
    return { in_filepath };
}

int main(int argc, char** argv)
{
    CLI::App app{"XGDTool"};
    argv = app.ensure_utf8(argv);

    std::filesystem::path cli_in_path;
    std::filesystem::path cli_out_directory;

    FileType out_file_type = FileType::UNKNOWN;
    OutputSettings output_settings;
    std::string cli_auto_format;

    auto* output_format_group = app.add_option_group("Output Format Options", "Specify the output format")->require_option(1);

    output_format_group->add_flag_function("--extract",  [&](int64_t) { out_file_type = FileType::DIR; }, "Extracts all files to a directory");
    output_format_group->add_flag_function("--xiso",     [&](int64_t) { out_file_type = FileType::ISO; }, "Creates an Xiso image");
    output_format_group->add_flag_function("--god",      [&](int64_t) { out_file_type = FileType::GoD; }, "Creates a Games on Demand image/directory structure");
    output_format_group->add_flag_function("--cci",      [&](int64_t) { out_file_type = FileType::CCI; }, "Creates a CCI archive");
    output_format_group->add_flag_function("--cso",      [&](int64_t) { out_file_type = FileType::CSO; }, "Creates a CSO archive");
    output_format_group->add_flag_function("--zar",      [&](int64_t) { out_file_type = FileType::ZAR; }, "Creates a ZAR archive");
    output_format_group->add_flag_function("--xbe",      [&](int64_t) { out_file_type = FileType::XBE; }, "Generates an attach XBE file");

    output_format_group->add_flag_function("--ogxbox",   [&](int64_t) { cli_auto_format = "--ogxbox" ; }, "Choose format and settings for use with OG Xbox");
    output_format_group->add_flag_function("--xbox360",  [&](int64_t) { cli_auto_format = "--xbox360"; }, "Choose format and settings for use with Xbox 360");
    output_format_group->add_flag_function("--xemu",     [&](int64_t) { cli_auto_format = "--xemu"   ; }, "Choose format and settings for use with Xemu");
    output_format_group->add_flag_function("--xenia",    [&](int64_t) { cli_auto_format = "--xenia"  ; }, "Choose format and settings for use with Xenia");

    app.add_flag_function("--partial-scrub", [&](int64_t) { output_settings.scrub_type = ScrubType::PARTIAL; }, "Scrubs and trims the output image, random padding data is removed");
    app.add_flag_function("--full-scrub",    [&](int64_t) { output_settings.scrub_type = ScrubType::FULL;    }, "Completely reauthor the resulting image, this will produce the smallest file possible");
    app.add_flag_function("--split",         [&](int64_t) { output_settings.split = true;                    }, "Splits the resulting XISO file if it's too large for OG Xbox");
    app.add_flag_function("--rename",        [&](int64_t) { output_settings.rename_xbe = true;               }, "Patches the title field of resulting XBE files to one found in the database");
    app.add_flag_function("--attach-xbe",    [&](int64_t) { output_settings.attach_xbe = true;               }, "Generates an attach XBE file along with the output file");
    app.add_flag_function("--am-patch",      [&](int64_t) { output_settings.allowed_media_patch = true;      }, "Patches the \"Allowed Media\" field in resulting XBE files");
    app.add_flag_function("--offline",       [&](int64_t) { output_settings.offline_mode = true;             }, "Disables online functionality");
    app.add_flag_function("--debug",         [&](int64_t) { XGDLog().set_log_level(LogLevel::Debug);         }, "Enable debug logging");
    app.add_flag_function("--quiet",         [&](int64_t) { XGDLog().set_log_level(LogLevel::Error);         }, "Disable all logging except for warnings and errors");

    app.add_option("input_path", cli_in_path, "Input path")->required()->check(CLI::ExistingFile);
    app.add_option("output_directory", cli_out_directory, "Output directory");

    app.set_help_flag("--help", "Print this help message and exit");
    app.set_version_flag("--version", XGD::VERSION);

    CLI11_PARSE(app, argc, argv);

    if (!std::filesystem::exists(cli_in_path)) 
    {
        throw std::runtime_error("Input path does not exist");
    }

    if (cli_auto_format == "--ogxbox") 
    {
        out_file_type = FileType::DIR;
        output_settings.rename_xbe = true;
    }
    else if (cli_auto_format == "--xbox360") 
    {
        out_file_type = FileType::GoD;
        output_settings.scrub_type = ScrubType::FULL;
    }
    else if (cli_auto_format == "--xemu") 
    {
        out_file_type = FileType::ISO;
        output_settings.scrub_type = ScrubType::FULL;
        output_settings.attach_xbe = false;
        output_settings.allowed_media_patch = false;
        output_settings.split = false;
    }
    else if (cli_auto_format == "--xenia") 
    {
        out_file_type = FileType::ZAR;
    }

    std::vector<InputInfo> input_infos;
    FileType in_file_type = get_filetype(cli_in_path);

    if (in_file_type == FileType::UNKNOWN) 
    {
        if (!is_batch_dir(cli_in_path)) 
        {
            throw XGDException(ErrCode::ISO_INVALID, HERE(), "Unknown input file type");
        }

        for (const auto& entry : std::filesystem::directory_iterator(cli_in_path)) 
        {
            in_file_type = get_filetype(entry.path());
            if (in_file_type != FileType::UNKNOWN) 
            {
                if (entry.is_regular_file()) 
                {
                    input_infos.push_back({ in_file_type, find_split_filepaths(entry.path()) });
                } 
                else
                {
                    input_infos.push_back({ in_file_type, { entry.path() } });
                }
            }
        }
    }
    else
    {
        if (std::filesystem::is_regular_file(cli_in_path)) 
        {
            input_infos.push_back({ in_file_type, find_split_filepaths(cli_in_path) });
        } 
        else 
        {
            input_infos.push_back({ in_file_type, { cli_in_path } });
        }
    }

    std::filesystem::path base_out_directory = cli_out_directory.empty() ? (cli_in_path.parent_path() / "XGDTool_output") : cli_out_directory;
    std::vector<InputInfo> failed_inputs;

    XGDLog().set_log_level(LogLevel::Debug);
    
    for (const auto& input_info : input_infos) 
    {
        try 
        {
            std::shared_ptr<ImageReader> image_reader{nullptr};
            std::unique_ptr<TitleHelper> title_helper{nullptr};

            switch (input_info.in_file_type)
            {
                case FileType::DIR:
                    title_helper = std::make_unique<TitleHelper>(input_info.in_paths.front(), output_settings.offline_mode);
                    break;
                default:
                    image_reader = ReaderFactory::create(input_info.in_file_type, input_info.in_paths);
                    title_helper = std::make_unique<TitleHelper>(image_reader, output_settings.offline_mode);
                    break;
            }

            std::filesystem::path out_path = get_output_path(input_info, base_out_directory, title_helper, out_file_type, cli_auto_format == "--xemu");
            std::vector<std::filesystem::path> final_out_paths; 

            std::unique_ptr<ImageWriter> image_writer{nullptr};
            std::unique_ptr<ImageExtractor> image_extractor{nullptr};

            switch (out_file_type)
            {
                case FileType::DIR:
                    if (!image_reader)
                    {
                        throw XGDException(ErrCode::MISC, HERE(), "Input format not supported for extraction.");
                    }
                    image_extractor = std::make_unique<ImageExtractor>(image_reader, output_settings.allowed_media_patch);
                    image_extractor->extract(out_path);
                    final_out_paths = { out_path };
                    break;
                case FileType::XBE:
                    final_out_paths = { out_path };
                    break;
                default:
                    if (image_reader) 
                    {
                        image_writer = WriterFactory::create(out_file_type, image_reader, title_helper, output_settings);
                    }
                    else
                    {
                        image_writer = WriterFactory::create(out_file_type, input_info.in_paths.front(), title_helper, output_settings);
                    }
                    final_out_paths = image_writer->convert(out_path);
                    break;

            }

            XGDLog() << "Output written to: " << final_out_paths.front().string() << ((final_out_paths.size() > 1) ? ("and " + final_out_paths.back().string()) : "") << XGDLog::Endl;
        }
        catch (const XGDException& e) 
        {
            XGDLog(Error) << e.what() << XGDLog::Endl;
            failed_inputs.push_back(input_info);
        }
        catch (const std::exception& e) 
        {
            XGDLog(Error) << e.what() << XGDLog::Endl;
            failed_inputs.push_back(input_info);
        }
    }

    for (const auto& failed_input : failed_inputs) 
    {
        XGDLog(Error) << "Failed to process: " << failed_input.in_paths.front().string() << XGDLog::Endl;
    }
}
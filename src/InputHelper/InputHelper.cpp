#include "ImageReader/ImageReader.h"
#include "ImageWriter/ImageWriter.h"
#include "ImageExtractor/ImageExtractor.h"    
#include "InputHelper/InputHelper.h"
#include "Executable/AttachXbeTool.h"

InputHelper::InputHelper(std::filesystem::path in_path, std::filesystem::path out_directory, OutputSettings output_settings)
    : output_directory_(out_directory), output_settings_(output_settings)
{
    FileType in_file_type = get_filetype(in_path);

    if (in_file_type == FileType::UNKNOWN) 
    {
        if (!is_batch_dir(in_path)) 
        {
            throw XGDException(ErrCode::ISO_INVALID, HERE(), "Unknown input file type");
        }

        for (const auto& entry : std::filesystem::directory_iterator(in_path)) 
        {
            if ((in_file_type = get_filetype(entry.path())) != FileType::UNKNOWN) 
            {
                if (entry.is_regular_file() && !is_part_2_file(entry.path()))
                {
                    input_infos_.push_back({ in_file_type, find_split_filepaths(entry.path()) });
                }
                else if (entry.is_directory())
                {
                    input_infos_.push_back({ in_file_type, { entry.path() } });
                }
            }
        }
    }
    else 
    {
        if (std::filesystem::is_regular_file(in_path)) 
        {
            input_infos_.push_back({ in_file_type, find_split_filepaths(in_path) });
        } 
        else if (std::filesystem::is_directory(in_path))
        {
            input_infos_.push_back({ in_file_type, { in_path } });
        }
    }

    if (output_directory_.empty()) 
    {
        output_directory_ = in_path.parent_path() / "XGDTool_Output";
    }

    output_directory_ = std::filesystem::absolute(output_directory_);
}

void InputHelper::process() 
{
    for (const auto& input_info : input_infos_) 
    {
        try 
        {
            XGDLog() << "Processing: " << input_info.paths.front().string() + ((input_info.paths.size() > 1) ? (" and " + input_info.paths.back().string()) : "") << "\n";
            
            std::vector<std::filesystem::path> out_paths;

            switch (output_settings_.file_type) 
            {
                case FileType::DIR:
                    out_paths = create_dir(input_info);
                    break;
                case FileType::XBE:
                    out_paths = create_attach_xbe(input_info);
                    break;
                case FileType::LIST:
                    list_files(input_info);
                    break;
                default:
                    if (input_info.file_type == FileType::ZAR)
                    {
                        throw XGDException(ErrCode::ISO_INVALID, HERE(), "Cannot create image from ZAR file");
                    }
                    out_paths = create_image(input_info);
                    break;
            } 

            if (!out_paths.empty())
            {
                XGDLog() << "Successfully created: " << out_paths.front().string() + ((out_paths.size() > 1) ? (" and " + out_paths.back().string()) : "") << "\n";
            }
        } 
        catch (const XGDException& e) 
        {
            failed_inputs_.insert(failed_inputs_.end(), input_info.paths.begin(), input_info.paths.end());
            XGDLog(Error) << e.what() << "\n";
        }
        catch (const std::exception& e) 
        {
            failed_inputs_.insert(failed_inputs_.end(), input_info.paths.begin(), input_info.paths.end());
            XGDLog(Error) << e.what() << "\n";
        }
    }
}

std::vector<std::filesystem::path> InputHelper::failed_inputs() 
{
    return failed_inputs_;
}

std::vector<std::filesystem::path> InputHelper::create_image(const InputInfo& input_info)
{
    std::unique_ptr<TitleHelper> title_helper;
    std::shared_ptr<ImageReader> image_reader;

    switch (input_info.file_type) 
    {
        case FileType::DIR:
            title_helper = std::make_unique<TitleHelper>(input_info.paths.front(), output_settings_.offline_mode);
            break;
        default:
            image_reader = ImageReader::create_instance(input_info.file_type, input_info.paths);
            title_helper = std::make_unique<TitleHelper>(image_reader, output_settings_.offline_mode);
            break;
    }

    std::filesystem::path out_path = get_output_path(output_directory_, *title_helper);

    std::unique_ptr<ImageWriter> image_writer;

    switch (input_info.file_type) 
    {
        case FileType::DIR:
            image_writer = ImageWriter::create_instance(input_info.paths.front(), *title_helper, output_settings_);
            break;
        default:
            image_writer = ImageWriter::create_instance(image_reader, *title_helper, output_settings_);
            break;
    }

    std::vector<std::filesystem::path> final_out_paths = image_writer->convert(out_path);

    if (output_settings_.attach_xbe && title_helper->platform() == Platform::OGX)
    {
        AttachXbeTool attach_xbe_tool(*title_helper);
        attach_xbe_tool.generate_attach_xbe(final_out_paths.front().parent_path() / "default.xbe");
    }

    return final_out_paths;
}

std::vector<std::filesystem::path> InputHelper::create_dir(const InputInfo& input_info)
{
    if (input_info.file_type == FileType::DIR)
    {
        throw XGDException(ErrCode::ISO_INVALID, HERE(), "Cannot create directory from directory");
    }

    std::shared_ptr<ImageReader> image_reader = ImageReader::create_instance(input_info.file_type, input_info.paths);

    TitleHelper title_helper(image_reader, output_settings_.offline_mode);

    std::filesystem::path out_path = get_output_path(output_directory_, title_helper);

    ImageExtractor image_extractor(*image_reader, title_helper, output_settings_.allowed_media_patch, output_settings_.rename_xbe);
    image_extractor.extract(out_path);

    return { out_path };
}

std::vector<std::filesystem::path> InputHelper::create_attach_xbe(const InputInfo& input_info)
{
    if (input_info.file_type == FileType::DIR)
    {
        throw XGDException(ErrCode::ISO_INVALID, HERE(), "Cannot create attach XBE from directory");
    }

    std::shared_ptr<ImageReader> image_reader = ImageReader::create_instance(input_info.file_type, input_info.paths);

    if (image_reader->platform() != Platform::OGX)
    {
        throw XGDException(ErrCode::ISO_INVALID, HERE(), "Attach XBE can only be created for OGX images");
    }

    TitleHelper title_helper(image_reader, output_settings_.offline_mode);

    std::filesystem::path out_path = get_output_path(input_info.paths.front().parent_path(), title_helper);

    AttachXbeTool attach_xbe_tool(title_helper); 
    attach_xbe_tool.generate_attach_xbe(out_path);

    return { out_path };
}

void InputHelper::list_files(const InputInfo& input_info) 
{
    if (input_info.file_type == FileType::DIR || input_info.file_type == FileType::ZAR) 
    {
        throw XGDException(ErrCode::ISO_INVALID, HERE(), "Cannot list files from directory/ZAR file");
    }

    std::shared_ptr<ImageReader> image_reader = ImageReader::create_instance(input_info.file_type, input_info.paths);

    XGDLog() << "Files in image:\n";

    for (const auto& entry : image_reader->directory_entries()) 
    {
        if ((entry.header.attributes & Xiso::ATTRIBUTE_DIRECTORY) || entry.path.empty())
        {
            continue;
        }

        XGDLog() << entry.path.string() << " (" << entry.header.file_size << " bytes)\n";
    }
}
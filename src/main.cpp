#include <cstdint>
#include <filesystem>

#include <CLI/CLI.hpp>

#include "XGD.h"
#include "InputHelper/Types.h"
#include "InputHelper/InputHelper.h" 

int main(int argc, char** argv)
{
    CLI::App app{"XGDTool"};
    argv = app.ensure_utf8(argv);

    std::filesystem::path in_path;
    std::filesystem::path out_directory;
    OutputSettings output_settings;
    AutoFormat auto_format = AutoFormat::NONE;

    auto* output_format_group = app.add_option_group("Output Format Options", "Specify the output format")->require_option(1);

    output_format_group->add_flag_function("--extract",  [&](int64_t) { output_settings.out_file_type = FileType::DIR; }, "Extracts all files to a directory");
    output_format_group->add_flag_function("--xiso",     [&](int64_t) { output_settings.out_file_type = FileType::ISO; }, "Creates an Xiso image");
    output_format_group->add_flag_function("--god",      [&](int64_t) { output_settings.out_file_type = FileType::GoD; }, "Creates a Games on Demand image/directory structure");
    output_format_group->add_flag_function("--cci",      [&](int64_t) { output_settings.out_file_type = FileType::CCI; }, "Creates a CCI archive");
    output_format_group->add_flag_function("--cso",      [&](int64_t) { output_settings.out_file_type = FileType::CSO; }, "Creates a CSO archive");
    output_format_group->add_flag_function("--zar",      [&](int64_t) { output_settings.out_file_type = FileType::ZAR; }, "Creates a ZAR archive");
    output_format_group->add_flag_function("--xbe",      [&](int64_t) { output_settings.out_file_type = FileType::XBE; }, "Generates an attach XBE file");

    output_format_group->add_flag_function("--ogxbox",   [&](int64_t) { auto_format = AutoFormat::OGXBOX;  }, "Choose format and settings for use with OG Xbox");
    output_format_group->add_flag_function("--xbox360",  [&](int64_t) { auto_format = AutoFormat::XBOX360; }, "Choose format and settings for use with Xbox 360");
    output_format_group->add_flag_function("--xemu",     [&](int64_t) { auto_format = AutoFormat::XEMU;    }, "Choose format and settings for use with Xemu");
    output_format_group->add_flag_function("--xenia",    [&](int64_t) { auto_format = AutoFormat::XENIA;   }, "Choose format and settings for use with Xenia");

    app.add_flag_function("--partial-scrub", [&](int64_t) { output_settings.scrub_type = ScrubType::PARTIAL; }, "Scrubs and trims the output image, random padding data is removed");
    app.add_flag_function("--full-scrub",    [&](int64_t) { output_settings.scrub_type = ScrubType::FULL;    }, "Completely reauthor the resulting image, this will produce the smallest file possible");
    app.add_flag_function("--split",         [&](int64_t) { output_settings.split = true;                    }, "Splits the resulting XISO file if it's too large for OG Xbox");
    app.add_flag_function("--rename",        [&](int64_t) { output_settings.rename_xbe = true;               }, "Patches the title field of resulting XBE files to one found in the database");
    app.add_flag_function("--attach-xbe",    [&](int64_t) { output_settings.attach_xbe = true;               }, "Generates an attach XBE file along with the output file");
    app.add_flag_function("--am-patch",      [&](int64_t) { output_settings.allowed_media_patch = true;      }, "Patches the \"Allowed Media\" field in resulting XBE files");
    app.add_flag_function("--offline",       [&](int64_t) { output_settings.offline_mode = true;             }, "Disables online functionality");
    app.add_flag_function("--debug",         [&](int64_t) { XGDLog().set_log_level(LogLevel::Debug);         }, "Enable debug logging");
    app.add_flag_function("--quiet",         [&](int64_t) { XGDLog().set_log_level(LogLevel::Error);         }, "Disable all logging except for warnings and errors");

    app.add_option("in_path", in_path, "Input path")->required()->check(CLI::ExistingFile);
    app.add_option("out_directory", out_directory, "Output directory");

    app.set_help_flag("--help", "Print this help message and exit");
    app.set_version_flag("--version", XGD::VERSION);

    CLI11_PARSE(app, argc, argv);

    if (!std::filesystem::exists(in_path)) 
    {
        XGDLog(Error) << "Input path does not exist: " << in_path.string() << XGDLog::Endl;
        return 1;
    }

    switch (auto_format) 
    {
        case AutoFormat::OGXBOX:
            output_settings.out_file_type = FileType::DIR;
            output_settings.allowed_media_patch = true;
            output_settings.rename_xbe = true;
            break;
        case AutoFormat::XBOX360:
            output_settings.out_file_type = FileType::GoD;
            output_settings.scrub_type = ScrubType::FULL;
            break;
        case AutoFormat::XEMU:
            output_settings.out_file_type = FileType::ISO;
            output_settings.scrub_type = ScrubType::FULL;
            output_settings.attach_xbe = false;
            output_settings.allowed_media_patch = false;
            output_settings.split = false;
            output_settings.xemu_paths = true;
            break;
        case AutoFormat::XENIA:
            output_settings.out_file_type = FileType::ZAR;
            break;
        default:
            break;
    }

    InputHelper input_helper(std::filesystem::absolute(in_path), out_directory, output_settings);
    input_helper.process();

    for (const auto& failed_input : input_helper.failed_inputs()) 
    {
        XGDLog(Error) << "Failed to process input: " << failed_input.string() << "\n";
    }

    XGDLog() << "Finished processing input files" << XGDLog::Endl;

    return 0;
}
#include "GUI/MainFrame.h"

const char* MainFrame::auto_format_to_string(AutoFormat format) 
{
    switch (format) 
    {
        case AutoFormat::NONE: return "NONE";
        case AutoFormat::OGXBOX: return "OGXBOX";
        case AutoFormat::XBOX360: return "XBOX360";
        case AutoFormat::XEMU: return "XEMU";
        case AutoFormat::XENIA: return "XENIA";
        default: return "UNKNOWN";
    }
}

const char* MainFrame::file_type_to_string(FileType type) 
{
    switch (type) 
    {
        case FileType::UNKNOWN: return "UNKNOWN";
        case FileType::CCI: return "CCI";
        case FileType::CSO: return "CSO";
        case FileType::ISO: return "ISO";
        case FileType::ZAR: return "ZAR";
        case FileType::DIR: return "DIR";
        case FileType::GoD: return "GoD";
        case FileType::XBE: return "XBE";
        case FileType::LIST: return "LIST";
        default: return "UNKNOWN";
    }
}

const char* MainFrame::scrub_type_to_string(ScrubType type) 
{
    switch (type) 
    {
        case ScrubType::NONE: return "NONE";
        case ScrubType::PARTIAL: return "PARTIAL";
        case ScrubType::FULL: return "FULL";
        default: return "UNKNOWN";
    }
}

void MainFrame::log_output_settings(const OutputSettings& settings) 
{
    wxLogMessage("OutputSettings:");
    wxLogMessage("  AutoFormat: %s", auto_format_to_string(settings.auto_format));
    wxLogMessage("  FileType: %s", file_type_to_string(settings.file_type));
    wxLogMessage("  ScrubType: %s", scrub_type_to_string(settings.scrub_type));
    wxLogMessage("  Split: %s", settings.split ? "true" : "false");
    wxLogMessage("  Attach XBE: %s", settings.attach_xbe ? "true" : "false");
    wxLogMessage("  Allowed Media Patch: %s", settings.allowed_media_patch ? "true" : "false");
    wxLogMessage("  Offline Mode: %s", settings.offline_mode ? "true" : "false");
    wxLogMessage("  Rename XBE: %s", settings.rename_xbe ? "true" : "false");
    wxLogMessage("  XEMU Paths: %s", settings.xemu_paths ? "true" : "false");
}
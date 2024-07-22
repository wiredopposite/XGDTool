#include <fstream>

#include "XGD.h"
#include "Formats/Xbe.h"
#include "Executable/ExeTool.h"
#include "Executable/AttachXbe.h"
#include "Executable/AttachXbeTool.h"

AttachXbeTool::AttachXbeTool(TitleHelper& title_helper) 
    : title_helper_(title_helper) {}

void AttachXbeTool::generate_attach_xbe(const std::filesystem::path& out_xbe_path)
{
    std::fstream out_file(out_xbe_path, std::ios::binary | std::ios::out);
    if (!out_file.is_open()) 
    {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), "Failed to open file for writing: " + out_xbe_path.string());
    }

    out_file.write(reinterpret_cast<const char*>(ATTACH_XBE), ATTACH_XBE_SIZE);
    if (out_file.fail()) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write to file: " + out_xbe_path.string());
    }

    out_file.close();

    ExeTool exe_tool(out_xbe_path);

    out_file.open(out_xbe_path, std::ios::binary | std::ios::in | std::ios::out);
    if (!out_file.is_open()) 
    {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), "Failed to open file for writing: " + out_xbe_path.string());
    }   

    Xbe::Cert xbe_cert = title_helper_.xbe_cert();
    xbe_cert.allowed_media_types = exe_tool.xbe_cert().allowed_media_types; 

    std::memcpy(&xbe_cert.title_name, title_helper_.title_name().data(), std::min(sizeof(xbe_cert.title_name), title_helper_.title_name().size() * sizeof(char16_t)));

    out_file.seekp(exe_tool.cert_offset(), std::ios::beg);
    out_file.write(reinterpret_cast<const char*>(&xbe_cert), sizeof(Xbe::Cert));
    if (out_file.fail()) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write to file: " + out_xbe_path.string());
    }

    out_file.close();
}
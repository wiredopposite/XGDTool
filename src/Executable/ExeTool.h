#ifndef _EXE_TOOL_H_
#define _EXE_TOOL_H_

#include <cstdint>
#include <filesystem>

#include "ImageReader/ImageReader.h"
#include "InputHelper/Types.h"
#include "Formats/Xex.h"
#include "Formats/Xbe.h"

class ExeTool 
{
public:
    ExeTool(const std::filesystem::path& in_exe_path);
    ExeTool(ImageReader& image_reader, const std::filesystem::path& entry_path);

    ~ExeTool() = default;

    Platform platform() { return platform_; };

    // Will construct Xex cert from Xbe if Xbe input
    const Xex::ExecutionInfo& xex_cert() { return xex_cert_; }; 
    // Only valid for Xbe
    const Xbe::Cert& xbe_cert() { return xbe_cert_; }; 

    // Will byte swap if host endian != cert endian
    uint32_t title_id(); 

    // Absolute offset in image
    uint64_t exe_offset() { return exe_offset_; }; 
    // Relative to exe_offset
    uint64_t cert_offset() { return cert_offset_; }; 

private:
    Platform platform_{Platform::UNKNOWN};

    uint64_t exe_offset_{0};
    uint64_t cert_offset_{0};
    Xex::ExecutionInfo xex_cert_;
    Xbe::Cert xbe_cert_;
    uint32_t title_id_{0};

    void get_xbe_cert_from_xbe(const std::filesystem::path& in_xbe_path);
    void get_xex_cert_from_xex(const std::filesystem::path& in_xex_path);
    void get_xex_cert_from_reader(ImageReader& image_reader, const std::filesystem::path& node_path);
    void get_xbe_cert_from_reader(ImageReader& image_reader, const std::filesystem::path& node_path);
    void create_xex_cert_from_xbe();
};

#endif // _EXE_TOOL_H_
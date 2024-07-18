#include <fstream>

#include "XGD.h"
#include "Common/Utils.h"
#include "ExeTool/ExeTool.h"

ExeTool::ExeTool(const std::filesystem::path& in_exe_path) {
    if (StringUtils::case_insensitive_search(in_exe_path.string(), ".xex")) {
        platform_ = Platform::X360;
        get_xex_cert_from_xex(in_exe_path);
    } else if (StringUtils::case_insensitive_search(in_exe_path.string(), ".xbe")) {
        platform_ = Platform::OGX;
        get_xbe_cert_from_xbe(in_exe_path);
        create_xex_cert_from_xbe();
    } else {
        throw XGDException(ErrCode::MISC, HERE(), "Invalid executable file extension.");
    }
}

ExeTool::ExeTool(ImageReader& image_reader, const std::filesystem::path& node_path) {
    platform_ = image_reader.platform();

    if (platform_ == Platform::X360) {
        get_xex_cert_from_reader(image_reader, node_path);
    } else if (platform_ == Platform::OGX) {
        get_xbe_cert_from_reader(image_reader, node_path);
        create_xex_cert_from_xbe();
    }
}

uint32_t ExeTool::title_id() {
    if (title_id_ == 0) {
        if (platform_ == Platform::X360) {
            title_id_ = xex_cert_.title_id;

            if (!EndianUtils::is_big_endian()) {
                EndianUtils::swap_endian(title_id_);
            }
        } else if (platform_ == Platform::OGX) {
            title_id_ = xbe_cert_.title_id;

            if (EndianUtils::is_big_endian()) {
                EndianUtils::swap_endian(title_id_);
            }
        }
    }
    return title_id_;
}

void ExeTool::get_xbe_cert_from_xbe(const std::filesystem::path& in_xbe_path) {
    std::ifstream in_file(in_xbe_path, std::ios::binary);
    if (!in_file.is_open()) {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), in_xbe_path.string());
    }

    Xbe::Header xbe_header;
    in_file.read(reinterpret_cast<char*>(&xbe_header), sizeof(Xbe::Header));

    if (std::memcmp(&xbe_header.signature, "XBEH", 4) != 0) {
        throw XGDException(ErrCode::MISC, HERE(), "Invalid XBE header magic.");
    }

    cert_offset_ = xbe_header.cert_address - xbe_header.base_address;

    in_file.seekg(cert_offset_, std::ios::beg);
    in_file.read(reinterpret_cast<char*>(&xbe_cert_), sizeof(Xbe::Cert));
    if (in_file.fail()) {
        throw XGDException(ErrCode::FILE_READ, HERE(), in_xbe_path.string());
    }

    in_file.close();
}

void ExeTool::get_xbe_cert_from_reader(ImageReader& reader, const std::filesystem::path& node_path) {
    Xiso::DirectoryEntry exe_entry;
    bool found = false;
    
    for (auto& entry : reader.directory_entries()) {
        if (entry.path == node_path) {
            exe_entry = entry;
            found = true;
            break;
        }
    }

    if (!found) {
        throw XGDException(ErrCode::MISC, HERE(), "Failed to find executable entry in directory entries.");
    }

    exe_offset_ = static_cast<uint64_t>(exe_entry.header.start_sector) * Xiso::SECTOR_SIZE;
    exe_offset_ += reader.image_offset();

    Xbe::Header xbe_header;
    reader.read_bytes(exe_offset_, sizeof(Xbe::Header), reinterpret_cast<char*>(&xbe_header));

    if (std::memcmp(&xbe_header.signature, "XBEH", 4) != 0) {
        throw XGDException(ErrCode::MISC, HERE(), "Invalid XBE header magic.");
    }

    cert_offset_ = (xbe_header.cert_address - xbe_header.base_address);

    reader.read_bytes(exe_offset_ + cert_offset_, sizeof(Xbe::Cert), reinterpret_cast<char*>(&xbe_cert_));
}

void ExeTool::get_xex_cert_from_xex(const std::filesystem::path& in_xex_path) {
    std::ifstream in_file(in_xex_path, std::ios::binary);
    if (!in_file.is_open()) {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), in_xex_path.string());
    }

    Xex::Header xex_header;
    in_file.read(reinterpret_cast<char*>(&xex_header), sizeof(Xex::Header));

    if (std::memcmp(&xex_header.magic, "XEX2", 4) != 0) {
        throw XGDException(ErrCode::MISC, HERE(), "Invalid XEX header magic.");
    }

    uint32_t header_count = xex_header.header_count;
    if (!EndianUtils::is_big_endian()) {
        EndianUtils::swap_endian(header_count);
    }

    for (uint32_t i = 0; i < header_count; ++i) {
        uint32_t key, value;

        in_file.read(reinterpret_cast<char*>(&key), sizeof(key));
        in_file.read(reinterpret_cast<char*>(&value), sizeof(value));
        if (in_file.fail()) {
            throw XGDException(ErrCode::FILE_READ, HERE());
        }

        if (!EndianUtils::is_big_endian()) {
            EndianUtils::swap_endian(key);
            EndianUtils::swap_endian(value);
        }

        if (key == Xex::KeyValue::EXECUTION_INFO) {
            in_file.seekg(value, std::ios::beg);
            in_file.read(reinterpret_cast<char*>(&xex_cert_), sizeof(Xex::ExecutionInfo));
            if (in_file.fail()) {
                throw XGDException(ErrCode::FILE_READ, HERE(), "Failed to read XEX execution info");
            }
            break;
        }
    }
    in_file.close();
}

void ExeTool::get_xex_cert_from_reader(ImageReader& reader, const std::filesystem::path& node_path) {
    Xiso::DirectoryEntry exe_entry;
    bool found = false;
    
    for (auto& entry : reader.directory_entries()) {
        if (entry.path == node_path) {
            exe_entry = entry;
            found = true;
            break;
        }
    }

    if (!found) {
        throw XGDException(ErrCode::MISC, HERE(), "Failed to find executable entry in directory entries.");
    }

    exe_offset_ = static_cast<uint64_t>(exe_entry.header.start_sector) * Xiso::SECTOR_SIZE;
    exe_offset_ += reader.image_offset();

    Xex::Header xex_header;
    reader.read_bytes(exe_offset_, sizeof(Xex::Header), reinterpret_cast<char*>(&xex_header));

    if (std::memcmp(&xex_header.magic, "XEX2", 4) != 0) {
        throw XGDException(ErrCode::MISC, HERE(), "Invalid XEX header magic.");
    }

    uint32_t header_count = xex_header.header_count;
    if (!EndianUtils::is_big_endian()) {
        EndianUtils::swap_endian(header_count);
    }

    for (uint32_t i = 0; i < header_count; ++i) {
        uint32_t key, value;

        reader.read_bytes(exe_offset_ + sizeof(Xex::Header) + (i * 8), sizeof(key), reinterpret_cast<char*>(&key));
        reader.read_bytes(exe_offset_ + sizeof(Xex::Header) + (i * 8) + 4, sizeof(value), reinterpret_cast<char*>(&value));
        if (!EndianUtils::is_big_endian()) {
            EndianUtils::swap_endian(key);
            EndianUtils::swap_endian(value);
        }

        if (key == Xex::KeyValue::EXECUTION_INFO) {
            reader.read_bytes(exe_offset_ + value, sizeof(Xex::ExecutionInfo), reinterpret_cast<char*>(&xex_cert_));
            break;
        }
    }
}

// For use in GoD live header
void ExeTool::create_xex_cert_from_xbe() {
    xex_cert_.media_id = 0;
    xex_cert_.platform = 0;
    xex_cert_.executable_type = 0;
    xex_cert_.title_id = title_id_;
    xex_cert_.disc_count = 1;
    xex_cert_.disc_number = 1;
    EndianUtils::big_32(xex_cert_.title_id);
}

void ExeTool::patch_allowed_media(Xbe::Cert& xbe_cert) {
    uint32_t allowed_media = Xbe::AllowedMedia::ALL;
    EndianUtils::little_32(allowed_media);
    xbe_cert.allowed_media_types = allowed_media;
}
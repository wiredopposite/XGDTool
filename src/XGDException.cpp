#include "XGDException.h"

const XGDException::ErrorMap XGDException::error_map[] = {
    { Code::NONE,         "No error" },
    { Code::MEM_ALLOC,    "Memory allocation error" },
    { Code::FILE_OPEN,    "File open error" },
    { Code::FILE_READ,    "File read error" },
    { Code::FILE_WRITE,   "File write error" },
    { Code::FILE_SEEK,    "File seek error" },
    { Code::FS_CHDIR,     "Directory change error" },
    { Code::FS_MKDIR,     "Directory creation error" },
    { Code::FS_REMOVE,    "File removal error" },
    { Code::FS_RENAME,    "File rename error" },
    { Code::FS_EXISTS,    "Not a file error" },
    { Code::AVL_INSERT,   "AVL tree insertion error" },
    { Code::AVL_SIZE,     "AVL tree size error" },
    { Code::ISO_INVALID,  "Invalid ISO error" },
    { Code::XBE_INVALID,  "Invalid XBE error" },
    { Code::XEX_INVALID,  "Invalid XEX error" },
    { Code::STR_ENCODING, "String encoding error" },
    { Code::MISC,         "Miscellaneous error" },
    { Code::UNK,          "Unknown error" }
};

void XGDException::log_error(Code code, const std::string& info, const std::string& message) {
    std::string error_message = "Unknown error";

    for (const auto& error : error_map) {
        if (error.code == code) {
            error_message = error.message;
            break;
        }
    }
    error_message += " in " + info + (message.empty() ? "" : (": \n" + message));
    std::cerr << "\n" << error_message << std::endl;
}
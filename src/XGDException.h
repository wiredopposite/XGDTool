#ifndef _XGDEXCEPTION_H_
#define _XGDEXCEPTION_H_

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define HERE() (std::string(__FILE__) + " at line " + TOSTRING(__LINE__))

class XGDException : public std::exception {
public:
    enum class Code
    {
        NONE = 0,
        MEM_ALLOC,
        FILE_OPEN,
        FILE_READ,
        FILE_WRITE,
        FILE_SEEK,
        FS_CHDIR,
        FS_MKDIR,
        FS_REMOVE,
        FS_RENAME,
        FS_EXISTS,
        AVL_INSERT,
        AVL_SIZE,
        ISO_INVALID,
        XBE_INVALID,
        XEX_INVALID,
        STR_ENCODING,
        MISC,
        UNK
    };

    struct ErrorMap {
        Code code;
        std::string message;
    };

    static const ErrorMap error_map[];

    XGDException(Code code, const std::string& info, const std::string& message = "")
        : code_(code), file_line_(info), message_(message) 
        {
            log_error(code, info, message);
        };

    const char* what() const noexcept override 
    {
        return full_message_.c_str();
    }

    Code code() const noexcept 
    {
        return code_;
    }

    std::string file_line() const noexcept 
    {
        return file_line_;
    }

    std::string message() const noexcept 
    {
        return message_;
    }

private:
    Code code_;
    std::string file_line_;
    std::string message_;
    std::string full_message_;

    void log_error(Code code, const std::string& info, const std::string& message = "");
};

using ErrCode = XGDException::Code;

#endif // _XGDEXCEPTION_H_
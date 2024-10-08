#ifndef _IHTYPES_H_
#define _IHTYPES_H_

#include <cstdint>

#include "XGDLog.h"

enum class Platform { UNKNOWN, OGX, X360 };
enum class FileType { UNKNOWN, CCI, CSO, ISO, ZAR, DIR, GoD, XBE, LIST };
enum class ScrubType { NONE, PARTIAL, FULL };
enum class AutoFormat { NONE, OGXBOX, XBOX360, XEMU, XENIA };

struct OutputSettings 
{
    AutoFormat auto_format{AutoFormat::NONE};
    FileType file_type{FileType::UNKNOWN};
    ScrubType scrub_type{ScrubType::NONE};
    bool split{false};
    bool attach_xbe{false};
    bool allowed_media_patch{false};
    bool offline_mode{false};
    bool rename_xbe{false};
    bool xemu_paths{false};
};

#endif // _IHTYPES_H_
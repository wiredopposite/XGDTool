#ifndef _ATTACH_XBE_TOOL_H_
#define _ATTACH_XBE_TOOL_H_

#include <cstdint>
#include <string>

#include "TitleHelper/TitleHelper.h"

class AttachXbeTool 
{
public:
    AttachXbeTool(TitleHelper& title_helper);
    ~AttachXbeTool() = default;

    void generate_attach_xbe(const std::filesystem::path& out_xbe_path);

private:
    TitleHelper& title_helper_;
};

#endif // _ATTACH_XBE_TOOL_H_
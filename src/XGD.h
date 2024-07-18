#ifndef _XGD_H_
#define _XGD_H_

#include <cstdint>

#include "XGDLog.h"
#include "XGDException.h"

#define XGDTOOL_NAME      "XGDTool"
#define XGDTOOL_VERSION   "1.0.0 (07.11.24)"

namespace XGD {

    static constexpr char     NAME[]         = XGDTOOL_NAME;
    static constexpr char     VERSION[]      = XGDTOOL_VERSION;
    static constexpr uint32_t VERSION_LEN    = sizeof(VERSION) - 1;
    static constexpr uint32_t BUFFER_SIZE    = 0x00200000; // 2MB

    static constexpr char     OPTIMIZED_TAG[]       = "in!xgdt!" XGDTOOL_VERSION;
    static constexpr uint32_t OPTIMIZED_TAG_OFFSET  = 31337;
    static constexpr uint32_t OPTIMIZED_TAG_LEN     = 8 + VERSION_LEN;
    static constexpr uint32_t OPTIMIZED_TAG_LEN_MIN = 7;

};

#endif // _XGD_H_
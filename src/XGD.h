#ifndef _XGD_H_
#define _XGD_H_

#include <cstdint>

#include "XGDLog.h"
#include "XGDException.h"

#define XGDTOOL_NAME      "XGDTool"
#define XGDTOOL_VERSION   "1.0.0 (07.11.24)"

namespace XGD {

    constexpr char     NAME[]         = XGDTOOL_NAME;
    constexpr char     VERSION[]      = XGDTOOL_VERSION;
    constexpr uint64_t VERSION_LEN    = sizeof(VERSION) - 1;
    constexpr uint64_t BUFFER_SIZE    = 0x10000; // 64KB

    constexpr char     OPTIMIZED_TAG[]       = "in!xgdt!" XGDTOOL_VERSION;
    constexpr uint64_t OPTIMIZED_TAG_OFFSET  = 31337;
    constexpr uint64_t OPTIMIZED_TAG_LEN     = 8 + VERSION_LEN;
    constexpr uint64_t OPTIMIZED_TAG_LEN_MIN = 7;

};

#endif // _XGD_H_
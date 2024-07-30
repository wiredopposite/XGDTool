#ifndef _XGD_H_
#define _XGD_H_

#include <cstdint>

#include "XGDLog.h"
#include "XGDException.h"

#define XGDTOOL_VERSION   "1.0.0"
#define XGDTOOL_DATE      "07.27.24"

namespace XGD {

    constexpr char     NAME[]         = "XGDTool";
    constexpr char     VERSION[]      = XGDTOOL_VERSION;

    constexpr char     OPTIMIZED_TAG[]       = "in!xgdt!" XGDTOOL_VERSION " (" XGDTOOL_DATE ")";
    constexpr uint64_t OPTIMIZED_TAG_OFFSET  = 31337;
    constexpr uint64_t OPTIMIZED_TAG_LEN     = sizeof(OPTIMIZED_TAG) - 1;

    constexpr uint64_t BUFFER_SIZE    = 0x10000; // 64KB

};

#endif // _XGD_H_
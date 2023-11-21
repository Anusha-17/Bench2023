/* ===========================================================================
 * Copyright (c) 2016-2019, 2021-2022 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
=========================================================================== */

#ifndef _TEST_UTIL_DEBUG_H
#define _TEST_UTIL_DEBUG_H

#include <stdio.h>

#include "qcxlog.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__ANDROID__)
#define DEFAULT_DUMP_LOCATION "/data/vendor/camera/"
#elif defined(__INTEGRITY)
#define DEFAULT_DUMP_LOCATION "/test/Camera/frameDump/"
#else
#define DEFAULT_DUMP_LOCATION "/tmp/"
#endif

#define QCARCAM_PERFMSG(cond, _fmt_, ...) \
do { \
    if(cond){ \
        QCX_LOG(TEST_APPLICATION, HIGH,  _fmt_, ##__VA_ARGS__); \
    } \
} while (0)

#define QCARCAM_DBGMSG(_fmt_, ...) \
        QCX_LOG(TEST_APPLICATION, DEBUG,  _fmt_, ##__VA_ARGS__);

#define QCARCAM_INFOMSG(_fmt_, ...) \
        QCX_LOG(TEST_APPLICATION, HIGH,  _fmt_, ##__VA_ARGS__);

#define QCARCAM_ERRORMSG(_fmt_, ...) \
        QCX_LOG(TEST_APPLICATION, ERROR,  _fmt_, ##__VA_ARGS__);

#define QCARCAM_ALWZMSG(_fmt_, ...) \
        QCX_LOG(TEST_APPLICATION, ALWAYS,  _fmt_, ##__VA_ARGS__);


#ifdef __cplusplus
}
#endif

#endif /* _TEST_UTIL_DEBUG_H */

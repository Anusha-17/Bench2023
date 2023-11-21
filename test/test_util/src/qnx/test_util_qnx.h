/* ===========================================================================
 * Copyright (c) 2017-2021 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
=========================================================================== */
#ifndef _TEST_UTIL_QNX_H_
#define _TEST_UTIL_QNX_H_

#include "qcarcam.h"
#include <screen/screen.h>
#ifndef C2D_DISABLED
#include "c2d2.h"
#endif

/* workaround for using older screen headers without 10-bit support */
#ifndef SCREEN_FORMAT_P010
#define SCREEN_FORMAT_P010 28
#endif


typedef struct
{
    int display_id;
    int size[2];
} test_util_display_prop_t;

typedef struct
{
    void* mem_handle;

    void*  ptr[3];  //plane va
    uint32_t size[3]; //plane size

    long long phys_addr;
    uint32_t c2d_surface_id;
} test_util_buffer_t;

typedef enum
{
    TESTUTIL_ALLOCATOR_PMEM_V2 = 0,  /*PMEM V2 API to pass pmem_hndl as p_buf*/
    TESTUTIL_ALLOCATOR_PMEM_V1       /*PMEM V1 API to pass virtual address*/
} test_util_allocator_t;

struct test_util_ctxt_t
{
    test_util_ctxt_params_t params;

    screen_context_t screen_ctxt;
    screen_display_t *screen_display;
    test_util_display_prop_t *display_property;
    int screen_ndisplays;

    test_util_allocator_t allocator;
};

typedef struct
{
    int size[2];
    int pos[2];
    int ssize[2];
    int spos[2];
    int visibility;
} test_util_window_param_cpy_t;

struct test_util_window_t
{
    screen_window_t screen_win;
    char winid[64];
    test_util_window_param_cpy_t params;

    int is_offscreen;

    /*buffers*/
    screen_buffer_t* screen_bufs;
    test_util_buffer_t* buffers;

    int buffer_size[2]; // width,height
    int n_buffers;
    int n_pointers;
    int stride[3];
    int offset[3];   //offset to each plane from start of buffer
    test_util_color_fmt_t format;
    int num_planes;
    int screen_format;

    int prev_post_idx; //previously posted buffer idx
};

/** @brief UBWC tile definition. */
typedef struct
{
    uint32_t widthPixels;       ///< Tile width in pixels
    uint32_t widthBytes;        ///< Tile width in pixels
    uint32_t height;            ///< Tile height
    uint32_t widthMacroTile;    ///< Macro tile width
    uint32_t heightMacroTile;   ///< Macro tile height
    uint32_t BPPNumerator;      ///< Bytes per pixel (numerator)
    uint32_t BPPDenominator;    ///< Bytes per pixel (denominator)
}QCarCamUBWCTileInfo;

#endif

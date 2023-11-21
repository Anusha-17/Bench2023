/* ===========================================================================
 * Copyright (c) 2016-2021, 2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
=========================================================================== */

#ifndef _TEST_UTIL_COMMON_H
#define _TEST_UTIL_COMMON_H

#include "qcarcam.h"
#include "qcxutils.h"
#include "qcxosal.h"
#include "qcxmetadata.h"

#if defined QCX_TESTAPP_ENABLE_BMETRICS && defined(__QNXNTO__)
#include "libbmetrics.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ais_log_kpi(x)
#define QCARCAM_THRD_PRIO QCXThreadNormalPriority
#define QCARCAM_THRD_PRIO_HIGHREALTIME  QCXThreadRealtimePriority
#define QCARCAM_MAX_BUFF_INSTANCES 24
#define QCARCAM_MAX_STREAM_INSTANCES QCARCAM_MAX_BUFF_INSTANCES
#define QCARCAM_MAX_BUFF_PER_STREAM QCARCAM_MAX_NUM_BUFFERS

#if defined(__INTEGRITY)
// Object number 10 is defined in qcarcam_test_app INT file
#define QCARCAM_OBJ_NUM   10
#endif

#define MAX_METADATA_TAG_NUM   100
#define MAX_METADATA_TAG_DATA  65536

#define DEINTERLACE_FIELD_HEIGHT 240
#define DEINTERLACE_ODD_HEADER_HEIGHT 13
#define DEINTERLACE_ODD_HEIGHT (DEINTERLACE_FIELD_HEIGHT + DEINTERLACE_ODD_HEADER_HEIGHT)
#define DEINTERLACE_EVEN_HEADER_HEIGHT 14
#define DEINTERLACE_EVEN_HEIGHT (DEINTERLACE_FIELD_HEIGHT + DEINTERLACE_EVEN_HEADER_HEIGHT)

#if defined QCX_TESTAPP_ENABLE_BMETRICS && defined(__QNXNTO__)

/**********************************************************************************************//**
 @brief
 Issue bmetrics_log_component_start() with msg

 @param msg     The msg to issue bmetrics_log_component_start() with

 @return        None
 **************************************************************************************************/
#define QCX_TEST_BMETRICS_LOG_START()                                                   \
    do {                                                                                \
        if (0 != bmetrics_log_component_start())                                        \
        {                                                                               \
            QCARCAM_ERRORMSG("bmetrics_log_component_start failed");                    \
        }                                                                               \
    } while (0)


/**********************************************************************************************//**
 @brief
 Issue bmetrics_log_component_ready() with msg

 @param msg     The msg to issue bmetrics_log_component_ready() with

 @return        None
 **************************************************************************************************/
#define QCX_TEST_BMETRICS_LOG_READY()                                                   \
    do {                                                                                \
        if (0 != bmetrics_log_component_ready())                                        \
        {                                                                               \
            QCARCAM_ERRORMSG("bmetrics_log_component_ready failed");                    \
        }                                                                               \
    } while (0)


/**********************************************************************************************//**
 @brief
 Issue bmetrics_log_subcomponent_done() with msg

 @param msg     The msg to issue bmetrics_log_subcomponent_done() with

 @return        None
 **************************************************************************************************/
#define QCX_TEST_BMETRICS_LOG0(msg)                                                   \
    do {                                                                              \
        if (0U != bmetrics_log_subcomponent_done(msg))                                \
        {                                                                             \
            QCARCAM_ERRORMSG("bmetrics_log_subcomponent_done('%s') failed", msg);     \
        }                                                                             \
    } while (0)


/**********************************************************************************************//**
 @brief
 Issue bmetrics_log_subcomponent_done() using printf-style argument list interface

 Internally, it uses an on-stack character buffer of 64 bytes to compose the string
 msg (from its parameters) to call bmetrics_log_subcomponent_done() with.  As such,
 this macro is not stack inefficient and should be used only if QCX_TEST_BMETRICS_LOG0()
 is inadequate.  Furthermore, if the resulting message is longer than the buffer
 size, or the bmetrics call itself failed, it will log an error to slogger2.

 @param fmt,... The printf-styled argument list

 @return        None

 @sa QCX_TEST_BMETRICS_LOG0()
 **************************************************************************************************/
#define QCX_TEST_BMETRICS_LOG(fmt, ...)                                                  \
    do {                                                                                 \
        char buf[64] = { 0 };                                                            \
        const int rc = snprintf(buf, sizeof(buf), fmt, __VA_ARGS__);                     \
        if (-1 >= rc)                                                                    \
        {                                                                                \
            const int err = errno;                                                       \
            QCARCAM_ERRORMSG("bmetrics snprintf failed - %s %d", strerror(err), err);    \
        }                                                                                \
        else if(sizeof(buf) <= (size_t)rc)                                               \
        {                                                                                \
            QCARCAM_ERRORMSG("bmetrics snprintf failed due to truncation - size %d", rc);\
        }                                                                                \
        else                                                                             \
        {                                                                                \
            if (0 != bmetrics_log_subcomponent_done(buf))                              \
            {                                                                           \
                QCARCAM_ERRORMSG("bmetrics_log_subcomponent_done('%s') failed", buf);   \
            }                                                                           \
        }                                                                               \
    } while (0)

#else

#define QCX_TEST_BMETRICS_LOG_START()      ; // no-ops
#define QCX_TEST_BMETRICS_LOG_READY()      ; // no-ops
#define QCX_TEST_BMETRICS_LOG0(msg)        ; // no-ops
#define QCX_TEST_BMETRICS_LOG(fmt, ...)    ; // no-ops

#endif  // QCX_TESTAPP_ENABLE_BMETRICS

typedef enum
{
    TESTUTIL_DEINTERLACE_NONE = 0,
    TESTUTIL_DEINTERLACE_SW_WEAVE, //weaving
    TESTUTIL_DEINTERLACE_SW_BOB,   //bobbing
}testutil_deinterlace_t;

enum
{
    TEST_CUR_BUFFER = 0 ,
    TEST_PREV_BUFFER = 1,
    TEST_DISPLAY_BUFFER_NUM = 2
};

/// @brief Trigger types
typedef enum
{
    TESTUTIL_GPIO_INTERRUPT_TRIGGER_DISABLE = 0,
    TESTUTIL_GPIO_INTERRUPT_TRIGGER_HIGH    = 1,
    TESTUTIL_GPIO_INTERRUPT_TRIGGER_LOW     = 2,
    TESTUTIL_GPIO_INTERRUPT_TRIGGER_RISING  = 3,
    TESTUTIL_GPIO_INTERRUPT_TRIGGER_FALLING = 4,
    TESTUTIL_GPIO_INTERRUPT_TRIGGER_EDGE    = 5
} test_util_trigger_type_t;

/// @brief  GPIO interrupt mode types
typedef enum
{
    TESTUTIL_GPIO_MODE_VISIBILITY,
    TESTUTIL_GPIO_MODE_PLAYPAUSE,
    TESTUTIL_GPIO_MODE_STARTSTOP
} test_util_gpio_interrupt_mode_t;

/// @brief Visiblity types
typedef enum
{
    TESTUTIL_HIDE = 0,
    TESTUTIL_VISIBLE = 1
} test_util_visibility_t;

/// @brief Color formats
typedef enum
{
    TESTUTIL_FMT_MIPIRAW_8,
    TESTUTIL_FMT_MIPIRAW_10,
    TESTUTIL_FMT_MIPIRAW_12,
    TESTUTIL_FMT_MIPIRAW_14,
    TESTUTIL_FMT_RGB_565,
    TESTUTIL_FMT_RGB_888,
    TESTUTIL_FMT_RGBX_8888,
    TESTUTIL_FMT_R8_G8_B8,
    TESTUTIL_FMT_UYVY_8,
    TESTUTIL_FMT_NV12,
    TESTUTIL_FMT_NV21,
    TESTUTIL_FMT_UYVY_10,
    TESTUTIL_FMT_YU12,
    TESTUTIL_FMT_BGRX1010102,
    TESTUTIL_FMT_RGBX1010102,
    TESTUTIL_FMT_BGRX8888,
    TESTUTIL_FMT_P010,
    TESTUTIL_FMT_P01208,
    TESTUTIL_FMT_P01210,
    TESTUTIL_FMT_P010_LSB,
    TESTUTIL_FMT_P01208_LSB,
    TESTUTIL_FMT_P01210_LSB,
    TESTUTIL_FMT_PLAIN_8,
    TESTUTIL_FMT_PLAIN_10,
    TESTUTIL_FMT_PLAIN_12,
    TESTUTIL_FMT_PLAIN_14,
    TESTUTIL_FMT_PLAIN_16,
    TESTUTIL_FMT_UYVY422_10BITS,
    TESTUTIL_FMT_UBWC_NV12,
    TESTUTIL_FMT_UBWC_TP10,
    TESTUTIL_FMT_MAX = 0x7FFFFFFF
} test_util_color_fmt_t;

typedef enum
{
    TEST_UTIL_DISPLAY_ID,
    TEST_UTIL_PIPELINE_ID,
    TEST_UTIL_WINDOW_SIZE,
    TEST_UTIL_WINDOW_POS,
    TEST_UTIL_WINDOW_SOURCE_SIZE,
    TEST_UTIL_WINDOW_SOURCE_POS,
    TEST_UTIL_ZORDER,
    TEST_UTIL_VISIBILITY,
    TEST_UTIL_BUFFER_SIZE,
    TEST_UTIL_IS_DISPLAYABLE,
    TEST_UTIL_FORMAT,
    TEST_UTIL_N_BUFFER_DISPLAY,
    TEST_UTIL_COLOR_SPACE
} test_util_params_t;

typedef enum
{
    TEST_UTIL_PM_SUSPEND,
    TEST_UTIL_PM_RESUME
} test_util_pm_event_t;

typedef struct
{
    unsigned int buf_idx;
    unsigned char *p_va[3];
    unsigned int stride[3];
} test_util_buf_ptr_t;

typedef struct
{
    int disable_display;
    int enable_c2d;
    int enable_di;
    int enable_gpio_config;
    int enable_csc;
    int offscreen_allocator; //offscreen buffer allocator (OS specific)
    int enable_post_processing;
} test_util_ctxt_params_t;

typedef struct
{
    uint32_t irq;
    void (*cb_func)(void);
} test_util_intr_thrd_args_t;

typedef struct
{
    uint8_t  isVendorTag;
    uint32_t qccId;
    uint32_t tagId;
    uint8_t  data[10];
    uint32_t count;
}test_util_metadata_tag_t;

/// @brief Window properties
/// Parameters used to identify and define properties given to windows to be displayed.
typedef struct
{
    char debug_name[64];            ///< Debug name for window
    int display_id;                 ///< Specific display output ID
    int pipeline_id;                ///< Specific pipeline ID
    float window_size[2];           ///< Output window size [width, height]
    float window_pos[2];            ///< Output window position [x, y]
    float window_source_size[2];    ///< Source window size [width, height]
    float window_source_pos[2];     ///< Source window position [x, y]
    int zorder;                     ///< Window position in Z plane
    int visibility;                 ///< Window visibility (0 - not visible, else visible)

    int buffer_size[2];

    int is_offscreen;               ///< Is window to be displayed or offscreen
    test_util_color_fmt_t format;   ///< Displayable format if need to convert
    int n_buffers_display;          ///< Number of buffers if we need to convert to displayable format

    unsigned int flags;             ///< input flags
    int is_imported_buffer;        ///< Is imported buffer or allocated buffer
} test_util_window_param_t;

typedef struct
{
    unsigned long long frame_generate_time[TEST_DISPLAY_BUFFER_NUM];
    unsigned long long buf_commit_time[TEST_DISPLAY_BUFFER_NUM];
    unsigned long long buf_rel_time[TEST_DISPLAY_BUFFER_NUM];
    int bprint;
} test_util_diag_t;

typedef struct
{
    char filename[256];

    int buffer_size[2];
    int stride;             ///< Stride of input buffer in bytes
    int n_buffers;          ///< Number of buffers for injection

    QCarCamColorPattern_e pattern;
    QCarCamColorBitDepth_e bitdepth;
    QCarCamColorPack_e pack;

    int n_frames;           ///< Number of frames in injection sequence
    int repeat;             ///< Number of repetions for injection sequence
    int framerate;          ///< Frame rate for injection
    int singlebuf;          ///< Specifies if injection is done using single buffer
} test_util_injection_param_t;

typedef struct
{
    int width;          ///< Override output buffer width
    int height;         ///< Override output buffer height
    int stride;         ///< Override stride
    int n_buffers;      ///< Number of buffers for output of ISP
    int num_batch_frames;
    int detect_first_phase_timer;
    int frame_increment;
    QCarCamFrameDropConfig_t frame_rate_config;
    QCarCamColorFmt_e format;
    unsigned int buffer_flags; ///< QCarCam buffer flags
    QCarCamRegion_t   cropRegion;
} test_util_output_param_t;

typedef struct
{
    uint32_t num_inputs;                    ///< Num of cameras supported
    QCarCamInputStream_t input_param[QCARCAM_MAX_INPUT_STREAMS]; ///< Structure to hold inputid, srcid and inputmode of each camera
    int use_event_callback;                 ///< Specifies if frames from input are get in a callback
    int frame_timeout;                      ///< Specifies timeout for QCarCamGetFrame
    QCarCamOpmode_e op_mode;            ///< Specifies operation mode
    uint64_t subscribe_parameter_change;    ///< Specifies input events/settings to listen if any change
    uint32_t delay_time;                    ///< Specifies if buffer delay, if yes, specifies delay time(ms)
    bool recovery;                          ///< Specifies if recovery logic should be active
    char node_name[256];                    ///< Node name for V4L2
    bool request_mode;
} test_util_properties_t;

typedef struct
{
    float exposure_time;      ///< Exposure time.
    float gain;               ///< Gain
    int manual_exp;           ///< Enable manual exposure
} test_util_exp_properties_t;

typedef struct
{
    unsigned int num_isp_instances;
    QCarCamIspUsecaseConfig_t isp_config[QCARCAM_MAX_ISP_INSTANCES]; /// < Specifies ISP instance config
    test_util_metadata_tag_t  metadata_tags[MAX_METADATA_TAG_NUM];
    unsigned int num_tags;
} test_util_isp_param_t;

typedef struct
{
    unsigned int num_buffer_instances;

    /*Buffer Id specified*/
    unsigned int buffer_id[QCARCAM_MAX_BUFF_INSTANCES];

    /*Output buffer settings*/
    test_util_output_param_t output_params[QCARCAM_MAX_BUFF_INSTANCES];

    /*Display settings*/
    test_util_window_param_t window_params[QCARCAM_MAX_BUFF_INSTANCES];     ///< Properties for window to be displayed
} test_util_stream_input_t;

/// @brief XML inputs
/// Defines general parameters for each input that are parsed from XML config files
typedef struct
{
    /*QCarCam properties*/
    test_util_properties_t properties;

    /*injection params*/
    test_util_injection_param_t inject_params;

    /*stream params*/
    test_util_stream_input_t stream_params;

    /* Exposure params */
    test_util_exp_properties_t exp_params;

    /* ISP params */
    test_util_isp_param_t isp_params;
} test_util_xml_input_t;

typedef QCarCamRet_e (*power_event_callable)(test_util_pm_event_t event, void* usr_data);

#ifdef __cplusplus
}
#endif

#endif /* _TEST_UTIL_COMMON_H */

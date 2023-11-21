#ifndef QCARCAM_TEST_H
#define QCARCAM_TEST_H

/* ===========================================================================
 * Copyright (c) 2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
=========================================================================== */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <termios.h>
#include <queue>

#ifndef C2D_DISABLED
#include "c2d2.h"
#endif

#ifdef USE_VENDOR_EXT_PARAMS
#include "vendor_ext_properties.h"
#endif

#include "qcarcam.h"

#include "test_util.h"
#include "qcarcam_diag_types.h"
#ifdef POST_PROCESS
#include "post_process.h"
#endif

#include "camera_metadata.h"
#include "camera_metadata_hidden.h"


#define NUM_MAX_CAMERAS 28
#define NUM_MAX_DISP_BUFS 3

/*1sec delay before restart */
#define PAUSE_RESUME_USLEEP 1000000
#define START_STOP_USLEEP 1000000

/*print input state as frozen if start and no frames after 1 sec*/
#define QCARCAMTEST_SOF_FREEZE_TIMEOUT 1.0f

#define QCARCAM_TEST_DEFAULT_GET_FRAME_TIMEOUT 500000000
#define DEFAULT_PRINT_DELAY_SEC 10
#define SIGNAL_CHECK_DELAY 33333;
#define CSI_ERR_CHECK_DELAY 100000;
#define NS_TO_MS 0.000001F

#define QCARCAM_TEST_INPUT_INJECTION 11
#define BUFSIZE 10

#define SIGWAIT_TIMEOUT_MS 100
#define TIMER_THREAD_USLEEP 1000000

#define USR_PROCESS_THREAD_USLEEP 1000
#define USR_PROCESS_WAIT_USLEEP 1000

#if defined(__INTEGRITY)
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC CLOCK_REALTIME
#endif
extern "C" const Value __PosixServerPriority = CAM_POSIXSERVER_PRIORITY;
#endif

#define SET_BIT(num, nbit)   ((num) |=  (0x1<<(nbit)))
#define CHECK_BIT(num, nbit) ((num) & (0x1<<(nbit)))

#define QCARCAMTEST_BUFFER_STALE_BIT  4

typedef enum
{
    QCARCAMTEST_BUFFER_STATE_INIT,
    QCARCAMTEST_BUFFER_STATE_QCARCAM,
    QCARCAMTEST_BUFFER_STATE_GET_FRAME,
    QCARCAMTEST_BUFFER_STATE_USR_PROCESS,
    QCARCAMTEST_BUFFER_STATE_USR_PROCESS_DONE,
    QCARCAMTEST_BUFFER_STATE_POST_DISPLAY,
    QCARCAMTEST_BUFFER_STATE_MAX = 0x7FFFFFF
}qcarcam_test_buffer_state_t;

typedef enum
{
    QCARCAM_TEST_BUFFERS_OUTPUT = 0,
    QCARCAM_TEST_BUFFERS_DISPLAY,
    QCARCAM_TEST_BUFFERS_INPUT,
    QCARCAM_TEST_BUFFERS_MAX
} qcarcam_test_buffers_t;

typedef struct
{
    QCarCamBufferList_t p_buffers;

    QCarCamColorFmt_e format;
    unsigned int flags; //QCarCam buffer flags
    unsigned int n_buffers;
    unsigned int width;
    unsigned int height;
    int frameCnt;
    int releaseframeCnt;
    int prev_frameCnt;
    int prev_releaseframeCnt;
    unsigned long long t_firstFrame; //first frame time
    unsigned long long hwt_firstFrame;
    unsigned long long hwt_latestFrame;
    unsigned long long hwt_prevFrame;
    unsigned int bufferList_id;
    QCarCamRegion_t cropRegion;
    bool dumpNextFrame;

    /* buffer management tracking */
    int get_frame_buf_idx;
    int buf_idx_qcarcam;
    std::list<uint32_t> release_buf_idx;

    int buf_idx_disp;
    qcarcam_test_buffer_state_t buf_state[QCARCAM_MAX_NUM_BUFFERS];
} qcarcam_buffers_param_t;

typedef enum
{
    QCARCAMTEST_STATE_INVALID = 0,
    QCARCAMTEST_STATE_INIT,
    QCARCAMTEST_STATE_OPEN,
    QCARCAMTEST_STATE_START,
    QCARCAMTEST_STATE_STOP,
    QCARCAMTEST_STATE_PAUSE,
    QCARCAMTEST_STATE_PAUSE_STOP_PENDING,
    QCARCAMTEST_STATE_ERROR,
    QCARCAMTEST_STATE_CLOSED,
    QCARCAMTEST_STATE_SUSPEND,
    QCARCAMTEST_STATE_RECOVERY,
}qcarcamtest_state_t;

typedef struct
{
    uint32_t buf_idx;
    void *p_data;
}timer_usr_data;

typedef struct
{
    timer_usr_data usr_data;
    QCXTimer_t ptimer;
}qcarcam_test_buf_timer;


typedef struct
{
    uint32_t event_id;
    QCarCamEventPayload_t payload;
} qcarcam_event_msg_t;

typedef struct
{
    int buf_idx;            ///< Buffer index for injection
    int n_total_frames;     ///< Total number of frames for injection
    int n_dump_frames;      ///< Number of processed frames to dump
    uint32_t request_id;  ///< Request ID for next submit request
} qcarcam_test_injection_settings_t;

/* A frame request */
typedef struct
{
    uint32_t streams_pending;
    /*Each request can support QCARCAM_MAX_STREAM_INSTANCES streams*/
    QCarCamStreamRequest_t request[QCARCAM_MAX_STREAM_INSTANCES];
} qcarcam_test_frame_request_t;

typedef struct
{
    uint32_t request_id;  ///< Request ID for next submit request
    /* Pool of requests
     * The pool can store at most QCARCAM_MAX_BUFF_PER_STREAM number of requests
     * Each request will support at most QCARCAM_MAX_STREAM_INSTANCES streams*/
    qcarcam_test_frame_request_t request_pool[QCARCAM_MAX_BUFF_PER_STREAM] = {0};
} qcarcam_test_submitrequest_settings_t;


typedef struct
{
    QCarCamInput_t*     pInputDesc;
    QCarCamInputModes_t modesDesc;
}QCarCamTestInputEnumerator_t;

/************* CAMERA SIGNAL *****************/

#define CAM_SIGNAL_WAIT_NO_TIMEOUT 0xFFFFFFFF

#define OS_GET_ABSTIME(timeout, a_milliSeconds)           \
    clock_gettime(CLOCK_MONOTONIC, &timeout);             \
    timeout.tv_sec += (a_milliSeconds / 1000);            \
    timeout.tv_nsec += (a_milliSeconds % 1000) * 1000000; \
    if (timeout.tv_nsec >= 1000000000) {                  \
        timeout.tv_nsec -= 1000000000;                    \
        timeout.tv_sec++;                                 \
    }


typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t cond_var;
    int signal_count;
    int manualReset;
} CameraSignal;


/*******************************************/

typedef struct
{
    QCXThread_t thread_handle;
    QCXThread_t process_cb_event_handle;
    CameraSignal* m_eventHandlerSignal;
    QCXThread_t injection_handle;
    CameraSignal* m_injectionHandlerSignal;

    unsigned int idx;
    unsigned int query_inputs_idx;

    pthread_mutex_t mutex;
    qcarcamtest_state_t state;
    bool is_fatal_error;
    bool is_reserved;

    /*qcarcam context*/
    unsigned int sessionId;
    QCarCamHndl_t qcarcam_hndl;
    uint32_t num_inputs;
    QCarCamInputStream_t  input_param[QCARCAM_MAX_INPUT_STREAMS];

    uint32_t num_stream_instance;
    qcarcam_buffers_param_t buffers_param[QCARCAM_MAX_BUFF_INSTANCES][QCARCAM_TEST_BUFFERS_MAX];

    QCarCamBufferList_t p_buffers_output[QCARCAM_MAX_BUFF_INSTANCES];
    QCarCamBufferList_t p_buffers_disp[QCARCAM_MAX_BUFF_INSTANCES];
    QCarCamBufferList_t p_buffers_input;

    QCarCamBufferList_t meta_bufferList;
    unsigned int        input_meta_idx;
    bool                is_submit_metadata;

    QCarCamBufferList_t meta_output_bufferList;

    unsigned long long int frame_timeout;
    int use_event_callback;
    QCarCamOpmode_e op_mode;

    unsigned int num_isp_instances;
    QCarCamIspUsecaseConfig_t isp_config[QCARCAM_MAX_ISP_INSTANCES];

    bool recovery;
    bool request_mode;

    /* test util objects */
    test_util_ctxt_t *test_util_ctxt;
    test_util_window_t *qcarcam_window[QCARCAM_MAX_BUFF_INSTANCES];
    test_util_window_t *display_window[QCARCAM_MAX_BUFF_INSTANCES];
    test_util_window_t *injection_window;
    test_util_window_t *metabuffer_window;
    test_util_window_t *metabuffer_window_output;
    test_util_window_param_t window_params[QCARCAM_MAX_BUFF_INSTANCES];

    /* diag */
    bool is_first_start;
    bool is_injection;
    test_util_injection_param_t injectionParams;
    qcarcam_test_injection_settings_t injectionSettings;
    qcarcam_test_submitrequest_settings_t submitrequestSettings;
    unsigned long long t_start; //start command
    unsigned long long t_start_success;
    unsigned long long t_before;
    unsigned long long t_after;
    test_util_diag_t diag;

    /* Exposure values */
    float exp_time;
    float gain;
    int manual_exposure;

    /* frame rate parameters */
    QCarCamFrameDropConfig_t frameDropConfig;
    unsigned int num_batch_frames;
    unsigned int detect_first_phase_timer;
    unsigned int frame_increment;

    bool skip_post_display;
    QCarCamInterlaceField_e field_type_previous;

    bool signal_lost;
    int fatal_err_cnt;

#ifdef ENABLE_CL_CONVERTER
    void* g_converter = NULL;
    ClConverter_surface_t source_surface;
    ClConverter_surface_t target_surface;
#endif

    /* subscription for changed setting events notification */
    uint64_t subscribe_parameter_change;
    bool is_master;

    /* user process */
    uint32_t delay_time;
    qcarcam_test_buf_timer buf_timer[QCARCAM_MAX_NUM_BUFFERS];

    std::list<uint32_t> usr_process_buf_idx;
    int usr_process_frameCnt;
    QCXThread_t usr_process_thread_handle;
    std::queue<qcarcam_event_msg_t> eventqueue;
    pthread_mutex_t queue_mutex;
#ifdef POST_PROCESS
    void *pPostProcessing;
    void *pSource_surface;
    void *pTarget_surface;
#endif
} qcarcam_test_input_t;

typedef struct
{
    int numInputs;
    qcarcam_test_input_t inputs[NUM_MAX_CAMERAS];
    int opened_stream_cnt;

    QCarCamTestInputEnumerator_t queryInputs[NUM_MAX_CAMERAS];

    /* 0 : post buffers directly to screen
     * 1 : blit buffers to display buffers through c2d
     */
    int enable_c2d;
#ifndef C2D_DISABLED
    pthread_mutex_t mutex_c2d;
#endif


    int dumpFrame;
    int enablePauseResume;
    int enableStartStop;
    int multithreaded;
    int enableStats;
    int enableMenuMode;
    int enableBridgeErrorDetect;
    int enableFatalErrorRecover;
    int enableIFEOverflowhandle;
    int enableRetry;
    int gpioNumber;
    int gpioMode;
    int disable_display;
    int checkDualCsi;
    int enable_csc;
    int enable_cache;

    int exitSeconds;

    int fps_print_delay;
    int vis_value;
    int check_buffer_state;
    bool bEnableHWTimeStampBasedFps;

    /*abort condition*/
    pthread_mutex_t mutex_abort;
    pthread_cond_t cond_abort;
    pthread_mutex_t mutex_csi_err;
    pthread_mutex_t mutex_open_cnt;

    unsigned long long t_start; //program start
    testutil_deinterlace_t enable_deinterlace;
#ifdef POST_PROCESS
    int enable_post_processing; // enable post processing path
    const IPostProcessing *post_processing_interface;
    GetPostProcessingInterfaceType pfnGetPostProcessingInterface;
    void *pPost_processing;

    test_util_pp_ctxt_t post_processing_ctx[NUM_MAX_CAMERAS] = {};
#endif
} qcarcam_test_ctxt_t;



QCarCamRet_e qcarcam_input_open(qcarcam_test_input_t *input_ctxt);
QCarCamRet_e qcarcam_input_close(qcarcam_test_input_t *input_ctxt);
QCarCamRet_e qcarcam_input_start(qcarcam_test_input_t *input_ctxt);
QCarCamRet_e qcarcam_input_stop(qcarcam_test_input_t *input_ctxt);
QCarCamRet_e qcarcam_input_pause(qcarcam_test_input_t *input_ctxt);
QCarCamRet_e qcarcam_input_resume(qcarcam_test_input_t *input_ctxt);
void qcarcam_test_menu(void);
void abort_test(void);

QCarCamRet_e qcarcam_test_event_cb(const QCarCamHndl_t hndl,
        const uint32_t eventId,
        const QCarCamEventPayload_t *pPayload,
        void  *pPrivateData);


#endif /* QCARCAM_TEST_H */

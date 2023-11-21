/* ===========================================================================
 * Copyright (c) 2019-2021 Qualcomm Technologies, Inc.
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
#include <sys/mman.h>
#include <sys/time.h>
#include "qcarcam.h"

#include "test_util.h"

//do not wait for resources by default
#define WAIT_FOR_RESOURCES 0

#if WAIT_FOR_RESOURCES && defined(__QNXNTO__)
#include <libgen.h>
#define AIS_SERVER_SOCKET_PATH "/tmp/ais_socket_0"
#endif //WAIT_FOR_RESOURCES




#define QCARCAM_TEST_DEFAULT_GET_FRAME_TIMEOUT 500000000
#define NUM_MAX_CAMERAS 1
#define BUFSIZE 32
#define SIGWAIT_TIMEOUT_MS 100

/*Configuration Parameters*/
#define INPUT_WIDTH 1920
#define INPUT_HEIGHT 1020
#define INPUT_FORMAT QCARCAM_FMT_UYVY_8
#define INPUT_NUM_BUFFERS 4

#define DISPLAY_FORMAT TESTUTIL_FMT_UYVY_8

/*Perf logging macros*/
#define QCARCAM_TEST_ENABLE_LOG_PERF
#ifdef QCARCAM_TEST_ENABLE_LOG_PERF
#define QCARCAM_TEST_LOG_PERF(...) \
    do { \
        uint64_t tmpTime = 0; \
        qcarcam_test_get_time(&tmpTime); \
        QCARCAM_ALWZMSG("%lu %s", tmpTime-gCtxt.t_start, ##__VA_ARGS__); \
    }while(0)
#else
#define QCARCAM_TEST_LOG_PERF(...)
#endif

static int exitSeconds = 0;

typedef struct
{
    QCarCamBufferList_t p_buffers;

    QCarCamColorFmt_e format;
    unsigned int n_buffers;
    unsigned int width;
    unsigned int height;
} qcarcam_test_buffers_param_t;

typedef struct
{
    QCXThread_t qcarcam_thread_handle;
    QCXThread_t window_thread_handle;

    /*qcarcam context*/
    QCarCamHndl_t qcarcam_context;
    uint32_t qcarcam_input_id;

    qcarcam_test_buffers_param_t buffers_param;
    QCarCamBuffer_t pBuffers[INPUT_NUM_BUFFERS];

    QCarCamBufferList_t p_buffers_output;

    unsigned long long int frame_timeout;
    int use_event_callback;

    /* test util objects */
    test_util_ctxt_t *test_util_ctxt;
    test_util_window_t *qcarcam_window;
    test_util_window_t *display_window;
    test_util_window_param_t window_params;

    /* buffer management tracking */
    int buf_idx_qcarcam;
    std::list<uint32_t> release_buf_idx;

    int frameCnt;
    int prev_frame;
    int is_running;
    bool is_first_count;
    bool signal_lost;

    bool is_buf_dequeued[QCARCAM_MAX_NUM_BUFFERS];
} qcarcam_test_input_t;

typedef struct
{
    /*abort condition*/
    pthread_mutex_t mutex_abort;
    pthread_cond_t cond_abort;

    uint64_t t_start;
    int disable_display;
    int dumpFrame;
} qcarcam_test_ctxt_t;

///////////////////////////////
/// STATICS
///////////////////////////////
static qcarcam_test_input_t gRvcInput = {};

static qcarcam_test_ctxt_t gCtxt = {};

static volatile int g_aborted = 0;

static const int exceptsigs[] = {
    SIGCHLD, SIGIO, SIGURG, SIGWINCH,
    SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU, SIGCONT,
    -1,
};

int qcarcam_test_get_time(uint64_t *pTime)
{
    struct timespec time;
    uint64_t msec;

    if (clock_gettime(CLOCK_MONOTONIC, &time) == -1)
    {
        QCARCAM_ERRORMSG("Clock gettime failed");
        return 1;
    }
    msec = ((uint64_t)time.tv_sec * 1000) + (((uint64_t)time.tv_nsec / 1000) / 1000);
    *pTime = msec;

    return 0;
}

static void initialize_qcarcam_test_ctxt(void)
{
    pthread_mutex_init(&gCtxt.mutex_abort, NULL);
    pthread_cond_init(&gCtxt.cond_abort, NULL);
    qcarcam_test_get_time(&gCtxt.t_start);
}

static void abort_test(void)
{
    QCARCAM_ERRORMSG("Aborting test");
    pthread_mutex_lock(&gCtxt.mutex_abort);
    g_aborted = 1;
    pthread_cond_broadcast(&gCtxt.cond_abort);
    pthread_mutex_unlock(&gCtxt.mutex_abort);
}

static int timer_thread(void *arg)
{
    pthread_detach(pthread_self());
    uint64_t timer_start = 0;
    uint64_t timer_test = 0;
    qcarcam_test_get_time(&timer_start);
    while(!g_aborted)
    {
        qcarcam_test_get_time(&timer_test);
        if((timer_test-timer_start) >= ((unsigned long)exitSeconds*1000))
        {
            QCARCAM_ALWZMSG("TEST Aborted after running for %d secs successfully!", exitSeconds);
            abort_test();
            break;
        }
        usleep(1000000);
    }
    return 0;
}

static int signal_thread(void *arg)
{
    sigset_t sigset;
    struct timespec timeout;
    int i;

    pthread_detach(pthread_self());

    sigfillset(&sigset);
    pthread_sigmask(SIG_BLOCK, &sigset, NULL);

    for (i = 0; exceptsigs[i] != -1; i++)
    {
        sigdelset(&sigset, exceptsigs[i]);
    }

    timeout.tv_sec = 0;
    timeout.tv_nsec = SIGWAIT_TIMEOUT_MS * 1000000;

    while (!g_aborted)
    {
        if ((i = sigtimedwait(&sigset, NULL, &timeout)) > 0)
        {
            abort_test();
            break;
        }
    }
    return 0;
}

/**
 * Function to retrieve frame from qcarcam and increase frame_counter
 * @param input_ctxt
 * @return 0 on success, -1 on failure
 */
static int qcarcam_test_get_frame(qcarcam_test_input_t *pInputCtxt)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;
    QCarCamFrameInfo_t frame_info = {};
    static uint64_t prev_msec = 0, cur_msec = 0, diff_time = 0;
    struct timeval current_time = {};
    float time_sec = 0.0;
    ret = QCarCamGetFrame(pInputCtxt->qcarcam_context, &frame_info, pInputCtxt->frame_timeout, 0);
    if (ret == QCARCAM_RET_TIMEOUT)
    {
        QCARCAM_ERRORMSG("QCarCamGetFrame timeout context 0x%lx ret %d", pInputCtxt->qcarcam_context, ret);
        return -1;
    }

    if (QCARCAM_RET_OK != ret)
    {
        QCARCAM_ERRORMSG("Get frame context 0x%lx ret %d", pInputCtxt->qcarcam_context, ret);
        return -1;
    }

    if (frame_info.bufferIndex >= pInputCtxt->buffers_param.n_buffers)
    {
        QCARCAM_ERRORMSG("Get frame context 0x%lx ret invalid idx %d", pInputCtxt->qcarcam_context, frame_info.bufferIndex);
        return -1;
    }

    if (pInputCtxt->frameCnt == 0)
    {
        uint64_t t_to = 0;
        qcarcam_test_get_time(&t_to);
        ais_log_kpi(AIS_EVENT_KPI_CLIENT_FIRST_FRAME);
        printf("Success\n");
        fflush(stdout);
        QCARCAM_ALWZMSG("Get First Frame input_id %d buf_idx %i after : %lu ms, field type: %d",
            (int)pInputCtxt->qcarcam_input_id, pInputCtxt->buf_idx_qcarcam, (t_to - gCtxt.t_start), frame_info.fieldType);
        gettimeofday(&current_time, NULL);
        prev_msec = (current_time.tv_sec*1000) + (current_time.tv_usec/1000);
    }

    pInputCtxt->buf_idx_qcarcam = frame_info.bufferIndex;

    QCARCAM_DBGMSG("frameCnt:%d idx:%d", pInputCtxt->frameCnt, frame_info.bufferIndex);

    pInputCtxt->is_buf_dequeued[pInputCtxt->buf_idx_qcarcam] = 1;
    pInputCtxt->frameCnt++;

    // If we get 30 FPS, we will render 450 frames for every 15 seconds
    if (0==(pInputCtxt->frameCnt % 450))
    {
        gettimeofday(&current_time, NULL);
        cur_msec = (current_time.tv_sec*1000) + (current_time.tv_usec/1000);
        diff_time = cur_msec - prev_msec;
        {
            time_sec = (float)diff_time/1000.0;
            QCARCAM_ALWZMSG("RVC FPS = %.2f", (float)450/time_sec);
        }
        // Save current time
        prev_msec = cur_msec;
    }

    return 0;
}

static int qcarcam_test_post_to_display(qcarcam_test_input_t *pInputCtxt)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;
    static int first_frame = 1;

    QCARCAM_DBGMSG("Post Frame before buf_idx %i", pInputCtxt->buf_idx_qcarcam);

    if (first_frame)
    {
       ais_log_kpi(AIS_EVENT_KPI_CLIENT_FIRST_FRAME_POST_START);
       first_frame = 0;
    }

    ret = test_util_post_window_buffer(pInputCtxt->test_util_ctxt, pInputCtxt->qcarcam_window, pInputCtxt->buf_idx_qcarcam,
        &pInputCtxt->release_buf_idx, QCARCAM_INTERLACE_FIELD_UNKNOWN);
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("test_util_post_window_buffer failed");
    }

    if (0 != gCtxt.dumpFrame)
    {
        if (0 == pInputCtxt->frameCnt % gCtxt.dumpFrame)
        {
            char filename[128];
            snprintf(filename, sizeof(filename), DEFAULT_DUMP_LOCATION "frame_%d_%i.raw", pInputCtxt->buf_idx_qcarcam, pInputCtxt->frameCnt);
            test_util_dump_window_buffer(pInputCtxt->test_util_ctxt, pInputCtxt->qcarcam_window, pInputCtxt->buf_idx_qcarcam, filename);
        }
    }

    return 0;
}


/**
 * Release frame back to qcarcam
 * @param input_ctxt
 * @return 0 on success, -1 on failure
 */
static int qcarcam_test_release_frame(qcarcam_test_input_t *pInputCtxt)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;

    while(!pInputCtxt->release_buf_idx.empty())
    {
        uint32_t rel_idx = pInputCtxt->release_buf_idx.front();
        pInputCtxt->release_buf_idx.pop_front();

        if (rel_idx >= 0 && rel_idx < pInputCtxt->buffers_param.n_buffers)
        {
            if (pInputCtxt->is_buf_dequeued[rel_idx])
            {
                ret = QCarCamReleaseFrame(pInputCtxt->qcarcam_context, 0, rel_idx);
                if (QCARCAM_RET_OK != ret)
                {
                    QCARCAM_ERRORMSG("QCarCamReleaseFrame(%d) failed", rel_idx);
                    return -1;
                }
                pInputCtxt->is_buf_dequeued[rel_idx] = 0;
            }
            else
            {
                QCARCAM_ERRORMSG("QCarCamReleaseFrame(%d) skipped since buffer not dequeued", rel_idx);
            }
        }
        else
        {
            QCARCAM_ERRORMSG("QCarCamReleaseFrame(%d) skipped", rel_idx);
        }
    }

    return 0;
}

/**
 * Function to handle routine of fetching, displaying, and releasing frames when one is available
 * @param input_ctxt
 * @return 0 on success, -1 on failure
 */
static int qcarcam_test_handle_new_frame(qcarcam_test_input_t *pInputCtxt)
{
    if (qcarcam_test_get_frame(pInputCtxt))
    {
        /*if we fail to get frame, we silently continue...*/
        return 0;
    }

    qcarcam_test_post_to_display(pInputCtxt);

    if (qcarcam_test_release_frame(pInputCtxt))
        return -1;

    return 0;
}

static int qcarcam_test_handle_input_signal(qcarcam_test_input_t *pInputCtxt, QCarCamInputSignal_e signal_type)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;

    switch (signal_type) {
    case QCARCAM_INPUT_SIGNAL_LOST:
        QCARCAM_ERRORMSG("SIGNAL LOST: input: %d", pInputCtxt->qcarcam_input_id);

        if (pInputCtxt->is_running == 0) {
            QCARCAM_ERRORMSG("Input %d already stop, break", pInputCtxt->qcarcam_input_id);
            break;
        }

        pInputCtxt->is_running = 0;
        ret = QCarCamStop(pInputCtxt->qcarcam_context);
        if (ret == QCARCAM_RET_OK) {
            QCARCAM_ERRORMSG("Input %d QCarCamStop successfully", pInputCtxt->qcarcam_input_id);
        }
        else
        {
            QCARCAM_ERRORMSG("Input %d QCarCamStop failed: %d !", pInputCtxt->qcarcam_input_id, ret);
        }

        memset(&pInputCtxt->is_buf_dequeued, 0x0, sizeof(pInputCtxt->is_buf_dequeued));
        pInputCtxt->signal_lost = 1;

        break;
    case QCARCAM_INPUT_SIGNAL_VALID:
        QCARCAM_ERRORMSG("SIGNAL VALID: input: %d", pInputCtxt->qcarcam_input_id);

        if (pInputCtxt->is_running == 1) {
            QCARCAM_ERRORMSG("Input %d already running, break", pInputCtxt->qcarcam_input_id);
            break;
        }

        ret = QCarCamSetBuffers(pInputCtxt->qcarcam_context, &pInputCtxt->p_buffers_output);
        if (ret == QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("Input %d QCarCamSetBuffers successfully", pInputCtxt->qcarcam_input_id);
        }
        else
        {
            QCARCAM_ERRORMSG("Input %d QCarCamSetBuffers failed: %d", pInputCtxt->qcarcam_input_id, ret);
        }

        pInputCtxt->is_running = 1;
        ret = QCarCamStart(pInputCtxt->qcarcam_context);
        if (ret == QCARCAM_RET_OK) {
            QCARCAM_ERRORMSG("Input %d QCarCamStart successfully", pInputCtxt->qcarcam_input_id);
            pInputCtxt->signal_lost = 0;
        }
        else
        {
            pInputCtxt->is_running = 0;
            QCARCAM_ERRORMSG("Input %d QCarCamStart failed: %d", pInputCtxt->qcarcam_input_id, ret);
        }

        break;
    default:
        QCARCAM_ERRORMSG("Unknown Event type: %d", signal_type);
        break;
    }

    return ret;
}

static QCarCamRet_e QCarCamRVCSystemEventHndlr(const QCarCamHndl_t hndl,
        const uint32_t eventId,
        const QCarCamEventPayload_t *pPayload,
        void  *pPrivateData)
{
    if (g_aborted)
    {
        QCARCAM_ERRORMSG("Test aborted");
        return QCARCAM_RET_OK;
    }

    switch (eventId)
    {
    case QCARCAM_EVENT_ERROR:
        if (pPayload->errInfo.errorId == QCARCAM_ERROR_FATAL)
        {
            QCARCAM_ERRORMSG("Connection to server lost");
            abort_test();
        }
        break;
    default:
        QCARCAM_ERRORMSG("Received unsupported event %d", eventId);
        break;
    }

    return QCARCAM_RET_OK;
}

/**
 * Qcarcam event callback function
 * @param hndl
 * @param event_id
 * @param p_payload
 */
static QCarCamRet_e qcarcam_test_event_cb(const QCarCamHndl_t hndl,
        const uint32_t eventId,
        const QCarCamEventPayload_t *pPayload,
        void  *pPrivateData)
{
    qcarcam_test_input_t *pInputCtxt = &gRvcInput;
    if (pPrivateData != pInputCtxt || hndl != pInputCtxt->qcarcam_context)
    {
        QCARCAM_ERRORMSG("event_cb called with invalid qcarcam handle 0x%lx", hndl);
        return QCARCAM_RET_FAILED;
    }

    if (g_aborted)
    {
        QCARCAM_ERRORMSG("Test aborted");
        return QCARCAM_RET_OK;
    }

    switch (eventId)
    {
    case QCARCAM_EVENT_FRAME_READY:
        if (pInputCtxt->is_running)
        {
            QCARCAM_DBGMSG("received QCARCAM_EVENT_FRAME_READY");
            qcarcam_test_handle_new_frame(pInputCtxt);
        }
        break;
    case QCARCAM_EVENT_INPUT_SIGNAL:
        qcarcam_test_handle_input_signal(pInputCtxt, (QCarCamInputSignal_e)pPayload->u32Data);
        break;
    case QCARCAM_EVENT_ERROR:
        if (pPayload->errInfo.errorId == QCARCAM_ERROR_FATAL)
        {
            QCARCAM_ERRORMSG("Fatal Error, abort test");
            abort_test();
        }
        else
        {
            QCARCAM_ERRORMSG("Unhandled Error %d %d",
                    pPayload->errInfo.errorId,
                    pPayload->errInfo.errorCode);
        }
        break;
    default:
        QCARCAM_ERRORMSG("Received unsupported event %d", eventId);
        break;
    }

    return QCARCAM_RET_OK;
}

/**
 * Thread to setup and run qcarcam based on test input context
 *
 * @note For single threaded operation, this function only sets up qcarcam context.
 *      QCarCamStart and handling of frames is not executed.
 *
 * @param arg qcarcam_test_input_t* input_ctxt
 */
static int setup_windowing_thread(void *arg)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;
    qcarcam_test_input_t* pInputCtxt = &gRvcInput;

    pInputCtxt->buffers_param.width = INPUT_WIDTH;
    pInputCtxt->buffers_param.height = INPUT_HEIGHT;
    pInputCtxt->buffers_param.format = INPUT_FORMAT;
    pInputCtxt->buffers_param.n_buffers = INPUT_NUM_BUFFERS;

    pInputCtxt->window_params.display_id = 0;
    pInputCtxt->window_params.format = DISPLAY_FORMAT;
    pInputCtxt->window_params.window_size[0] = 1.0f;
    pInputCtxt->window_params.window_size[1] = 1.0f;
    pInputCtxt->window_params.window_source_size[0] = 1.0f;
    pInputCtxt->window_params.window_source_size[1] = 1.0f;
    pInputCtxt->window_params.buffer_size[0] = pInputCtxt->buffers_param.width;
    pInputCtxt->window_params.buffer_size[1] = pInputCtxt->buffers_param.height;

    pInputCtxt->window_params.visibility = 1;
    pInputCtxt->window_params.window_pos[0]  = 0.0;
    pInputCtxt->window_params.window_pos[1]  = 0.0;
    pInputCtxt->window_params.zorder         = 0;
    pInputCtxt->window_params.pipeline_id    = -1;
    pInputCtxt->window_params.n_buffers_display = 4;
    pInputCtxt->window_params.is_imported_buffer = 0;

    QCARCAM_DBGMSG("Window Setup Begin");

    pInputCtxt->p_buffers_output.nBuffers = pInputCtxt->buffers_param.n_buffers;
    pInputCtxt->p_buffers_output.colorFmt = pInputCtxt->buffers_param.format;
    pInputCtxt->p_buffers_output.pBuffers = pInputCtxt->pBuffers;

    for (uint32_t i = 0; i < pInputCtxt->p_buffers_output.nBuffers; ++i)
    {
        pInputCtxt->p_buffers_output.pBuffers[i].numPlanes = 1;
        pInputCtxt->p_buffers_output.pBuffers[i].planes[0].width = pInputCtxt->buffers_param.width;
        pInputCtxt->p_buffers_output.pBuffers[i].planes[0].height = pInputCtxt->buffers_param.height;
    }

    ret = test_util_init_window(pInputCtxt->test_util_ctxt, &pInputCtxt->qcarcam_window);
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("test_util_init_window failed for qcarcam_window");
        goto fail;
    }


    ret = test_util_set_window_param(pInputCtxt->test_util_ctxt, pInputCtxt->qcarcam_window, &pInputCtxt->window_params);
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("test_util_set_window_param failed");
        goto fail;
    }

    ret = test_util_init_window_buffers(pInputCtxt->test_util_ctxt, pInputCtxt->qcarcam_window, &pInputCtxt->p_buffers_output);
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("test_util_init_window_buffers failed");
        goto fail;
    }

    QCARCAM_TEST_LOG_PERF("window initialization done");

    return 0;

fail:
    return -1;
}


/**
 * Thread to setup and run qcarcam APIs
 *
 * @param arg qcarcam_test_input_t* input_ctxt
 */
static int setup_qcarcam_thread(void *arg)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;
    qcarcam_test_input_t* pInputCtxt = &gRvcInput;
    void* pWindowThreadExit = NULL;

    QCARCAM_INFOMSG("setup_qcarcam_thread input_desc=%d", pInputCtxt->qcarcam_input_id);

    QCARCAM_TEST_LOG_PERF("setup thread start");

    QCarCamInit_t qcarcam_init = { 0 };
    qcarcam_init.apiVersion = QCARCAM_VERSION;

    ret = QCarCamInitialize(&qcarcam_init);
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("QCarCamInitialize failed %d", ret);
        exit(-1);
    }
    QCARCAM_TEST_LOG_PERF("setup QCarCamInitialize done");

    ret = QCarCamRegisterEventCallback(QCARCAM_HNDL_INVALID, &QCarCamRVCSystemEventHndlr, NULL);
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("QCarCamInitialize failed %d", ret);
        return -1;
    }

    /*Poll until RVC camera is available*/
    uint32_t count = 0;
#if WAIT_FOR_RESOURCES && defined(__QNXNTO__)
    int poll_ms = 10;
    int delay_ms = 2000;
    QCARCAM_DBGMSG("Waiting for ais_socket_0");
    if (!waitfor(AIS_SERVER_SOCKET_PATH, delay_ms, poll_ms )) {
        QCARCAM_INFOMSG("We have ais_socket_0");
    } else {
        QCARCAM_ERRORMSG("Wait for ais_socket_0 failed");
        goto qcarcam_thread_fail;
    }
#endif //WAIT_FOR_RESOURCES
    do
    {
        QCarCamOpen_t openParams = {};
        openParams.opMode = QCARCAM_OPMODE_RAW_DUMP;
        openParams.numInputs = 1;
        openParams.inputs[0].inputId = pInputCtxt->qcarcam_input_id;

        ret = QCarCamOpen(&openParams, &pInputCtxt->qcarcam_context);
        if (!pInputCtxt->qcarcam_context)
        {
            if (count++ == 100)
            {
                QCARCAM_ERRORMSG("QCarCamOpen() failed");
                goto qcarcam_thread_fail;
            }
            QCARCAM_ERRORMSG("QCarCamOpen() try again...");
            OSAL_SleepMilliSecond(10);
        }
    } while(!pInputCtxt->qcarcam_context);

    QCARCAM_TEST_LOG_PERF("QCarCamOpen done");

    QCARCAM_INFOMSG("input_desc=%d context=0x%lx", pInputCtxt->qcarcam_input_id, pInputCtxt->qcarcam_context);

    if (pInputCtxt->use_event_callback)
    {
        ret = QCarCamRegisterEventCallback(pInputCtxt->qcarcam_context, &qcarcam_test_event_cb, pInputCtxt);

        uint32_t param = QCARCAM_EVENT_FRAME_READY | QCARCAM_EVENT_INPUT_SIGNAL | QCARCAM_EVENT_ERROR;
        ret = QCarCamSetParam(pInputCtxt->qcarcam_context, QCARCAM_STREAM_CONFIG_PARAM_EVENT_MASK, &param, sizeof(param));
    }

    QCARCAM_TEST_LOG_PERF("QCarCamSetParam done");

    OSAL_ThreadJoin(pInputCtxt->window_thread_handle, &pWindowThreadExit);
    if (pWindowThreadExit)
    {
        QCARCAM_ERRORMSG("Window thread failed, clean up qcarcam thread");
        goto qcarcam_thread_fail;
    }

    ret = QCarCamSetBuffers(pInputCtxt->qcarcam_context, &pInputCtxt->p_buffers_output);
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("QCarCamSetBuffers() failed");
        goto qcarcam_thread_fail;
    }
    QCARCAM_TEST_LOG_PERF("QCarCamSetBuffers done");

    pInputCtxt->is_running = 1;
    ret = QCarCamStart(pInputCtxt->qcarcam_context);
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("QCarCamStart() failed");
        goto qcarcam_thread_fail;
    }

    QCARCAM_TEST_LOG_PERF("QCarCamStart done");

    if (!pInputCtxt->use_event_callback)
    {
        while (!g_aborted)
        {
            if (qcarcam_test_handle_new_frame(pInputCtxt))
                break;
        }
    }
    else
    {
        pthread_mutex_lock(&gCtxt.mutex_abort);
        if (!g_aborted)
        {
            pthread_cond_wait(&gCtxt.cond_abort, &gCtxt.mutex_abort);
        }
        pthread_mutex_unlock(&gCtxt.mutex_abort);
    }

    QCARCAM_INFOMSG("exit setup_input_ctxt_thread");
    return 0;

qcarcam_thread_fail:
    if (pInputCtxt->qcarcam_context)
    {
        ret = QCarCamClose(pInputCtxt->qcarcam_context);
        pInputCtxt->qcarcam_context = QCARCAM_HNDL_INVALID;
        QCARCAM_INFOMSG("QCarCamClose ret=%d", ret);
    }

    return -1;
}

void display_menu()
{
    printf("\n ========================================================== \n");
    printf(" 'z'.....Exit \n");
    printf("\n =========================================================== \n");
    printf(" Enter your choice\n");
}

void process_cmds(char choice)
{
    switch (choice)
    {
    /* Exit app */
    case 'z':
        abort_test();
        break;
    default:
        QCARCAM_ERRORMSG("Invalid Input");
        break;
    }
}

int main(int argc, char **argv)
{
    QcxLogInit();

    initialize_qcarcam_test_ctxt();

    int rval = EXIT_FAILURE;
    int rc = 0;

    QCXThread_t signal_thread_handle = QCXThreadInvalid;
    QCXThread_t timer_thread_handle = QCXThreadInvalid;

    char thread_name[64];
    const char *tok;
    bool enableMenu = false;

    test_util_ctxt_t* test_util_ctxt = NULL;
    test_util_ctxt_params_t test_util_ctxt_params;
    memset(&test_util_ctxt_params, 0x0, sizeof(test_util_ctxt_params));

    QCARCAM_DBGMSG("Arg parse Begin");

    for (int idx = 1; idx < argc; idx++)
    {
        if (!strncmp(argv[idx], "-seconds=", strlen("-seconds=")))
        {
            tok = argv[idx] + strlen("-seconds=");
            exitSeconds = atoi(tok);
        }
        else if (!strncmp(argv[idx], "-menu", strlen("-menu")))
        {
            enableMenu = true;
        }
        else if (!strncmp(argv[idx], "-noDisplay", strlen("-noDisplay")))
        {
            gCtxt.disable_display = true;
        }
        else if (!strncmp(argv[idx], "-dumpFrame=", strlen("-dumpFrame=")))
        {
            tok = argv[idx] + strlen("-dumpFrame=");
            gCtxt.dumpFrame = atoi(tok);
        }
        else
        {
            QCARCAM_ERRORMSG("Invalid argument");
            exit(-1);
        }
    }

    QCARCAM_DBGMSG("Arg parse End");

    sigset_t sigset;
    sigfillset(&sigset);
    for (unsigned int i = 0; exceptsigs[i] != -1; i++)
    {
        sigdelset(&sigset, exceptsigs[i]);
    }
    pthread_sigmask(SIG_BLOCK, &sigset, NULL);

    /**/
    qcarcam_test_input_t* pInputCtxt = &gRvcInput;
    pInputCtxt->qcarcam_input_id = 0;
    pInputCtxt->use_event_callback = 1;
    /*no timeout when using event callback as frame is ready as soon as we get callback*/
    pInputCtxt->frame_timeout = pInputCtxt->use_event_callback ? 0 : QCARCAM_TEST_DEFAULT_GET_FRAME_TIMEOUT;

    if (gCtxt.disable_display == true)
    {
        test_util_ctxt_params.disable_display = 1;
    }
#if WAIT_FOR_RESOURCES && defined(__QNXNTO__)
    int poll_ms = 10;
    int delay_ms = 2000;
    QCARCAM_DBGMSG("Waiting for /dev/screen");
    if (!waitfor("/dev/screen", delay_ms, poll_ms )) {
        QCARCAM_INFOMSG("We have /dev/screen");
    } else {
        QCARCAM_ERRORMSG("Wait for /dev/screen failed");
        goto fail;
    }
#endif //WAIT_FOR_RESOURCES

    /*Initialize test utils for corresponding platform*/
    QCARCAM_DBGMSG("test_util_init");
    rc = test_util_init(&test_util_ctxt, &test_util_ctxt_params);
    if (rc != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("test_util_init failed");
        exit(-1);
    }

    QCARCAM_TEST_LOG_PERF("test_util_init done");

    pInputCtxt->test_util_ctxt = test_util_ctxt;

    /*signal handler thread*/
    snprintf(thread_name, sizeof(thread_name), "signal_thrd");
    rc = OSAL_ThreadCreate(QCARCAM_THRD_PRIO, &signal_thread, 0, 0, thread_name, &signal_thread_handle);
    if (rc)
    {
        QCARCAM_ERRORMSG("OSAL_ThreadCreate failed : %s", thread_name);
        goto fail;
    }

    QCARCAM_DBGMSG("Screen Setup End");
    QCARCAM_DBGMSG("Create qcarcam_context Begin");

    snprintf(thread_name, sizeof(thread_name), "setup_windowing_thread");
    rc = OSAL_ThreadCreate(QCARCAM_THRD_PRIO, &setup_windowing_thread, &gRvcInput, 0, thread_name, &gRvcInput.window_thread_handle);
    if (rc)
    {
        QCARCAM_ERRORMSG("OSAL_ThreadCreate failed : %s", thread_name);
        goto fail;
    }

    snprintf(thread_name, sizeof(thread_name), "setup_rvc_thrd");
    rc = OSAL_ThreadCreate(QCARCAM_THRD_PRIO, &setup_qcarcam_thread, &gRvcInput, 0, thread_name, &gRvcInput.qcarcam_thread_handle);
    if (rc)
    {
        QCARCAM_ERRORMSG("OSAL_ThreadCreate failed : %s", thread_name);
        goto fail;
    }

    if (exitSeconds)
    {
        snprintf(thread_name, sizeof(thread_name), "timer_thread");
        rc = OSAL_ThreadCreate(QCARCAM_THRD_PRIO, &timer_thread, 0, 0, thread_name, &timer_thread_handle);
        if (rc)
        {
            QCARCAM_ERRORMSG("pthread_create failed");
            goto fail;
        }
    }
    else if (enableMenu)
    {
        while (!g_aborted)
        {

            fflush(stdin);
            display_menu();

            while (!g_aborted)
            {
                int ch = 0;
                char buf[BUFSIZE];
                fd_set readfds;
                struct timeval tv;
                FD_ZERO(&readfds);
                FD_SET(fileno(stdin), &readfds);

                tv.tv_sec = 0;
                tv.tv_usec = 100000;

                int num = select(1, &readfds, NULL, NULL, &tv);
                if (num > 0)
                {
                    if (fgets(buf, sizeof(buf), stdin) != NULL)
                    {
                        ch = buf[0];
                    }

                    if (('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z'))
                    {
                        process_cmds(ch);
                        break;
                    }
                }
            }
        }
    }
    OSAL_ThreadJoin(gRvcInput.qcarcam_thread_handle, NULL);
    OSAL_ThreadDestroy(gRvcInput.qcarcam_thread_handle, 0);
    OSAL_ThreadDestroy(gRvcInput.window_thread_handle, 0);

    // success case
    rval = EXIT_SUCCESS;

fail:
    // cleanup
    {
        if (gRvcInput.qcarcam_context != QCARCAM_HNDL_INVALID)
        {
            (void)QCarCamStop(gRvcInput.qcarcam_context);
            (void)QCarCamClose(gRvcInput.qcarcam_context);
            gRvcInput.qcarcam_context = QCARCAM_HNDL_INVALID;
        }

        test_util_deinit_window_buffer(gRvcInput.test_util_ctxt, gRvcInput.qcarcam_window);
        test_util_deinit_window(gRvcInput.test_util_ctxt, gRvcInput.qcarcam_window);
    }

    if (!g_aborted)
    {
        abort_test();
    }

    test_util_deinit(gRvcInput.test_util_ctxt);

    QCarCamUninitialize();

    return rval;
}

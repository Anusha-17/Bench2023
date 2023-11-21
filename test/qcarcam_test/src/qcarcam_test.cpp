/* ===========================================================================
 * Copyright (c) 2017-2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
=========================================================================== */

#include "qcarcam_test.h"

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

#include "test_util.h"
#include "qcarcam_diag_types.h"
#ifdef POST_PROCESS
#include "post_process.h"
#endif

#include "camera_metadata.h"
#include "camera_metadata_hidden.h"

///////////////////////////////
/// GLOBAL
///////////////////////////////

 //@todo cleanup to not be global
qcarcam_test_ctxt_t gCtxt = {};
volatile int g_aborted = 0;

///////////////////////////////
/// STATICS
///////////////////////////////
static test_util_global_config_t g_xml_cfg = {};

static char g_filename[128] = "qcarcam_config.xml";


static sigset_t g_sigset;

static const int exceptsigs[] = {
    SIGCHLD, SIGIO, SIGURG, SIGWINCH,
    SIGTTIN, SIGTTOU, SIGCONT, SIGSEGV,
    -1,
};

static void initialize_qcarcam_test_ctxt(void)
{
    gCtxt.numInputs = 0;
    gCtxt.opened_stream_cnt = 0;

    gCtxt.enable_c2d = 0;
#ifdef POST_PROCESS
    gCtxt.enable_post_processing = 0;
#endif
#ifndef C2D_DISABLED
    pthread_mutex_init(&gCtxt.mutex_c2d, NULL);
#endif

    gCtxt.dumpFrame = 0;
    gCtxt.enablePauseResume = 0;
    gCtxt.enableStartStop = 0;
    gCtxt.multithreaded = 1;
    gCtxt.enableStats = 1;
    gCtxt.enableMenuMode = 1;
    gCtxt.enableBridgeErrorDetect = 1;
    gCtxt.enableFatalErrorRecover = 0;
    gCtxt.enableIFEOverflowhandle = 0;
    gCtxt.enableRetry = 0;
    gCtxt.disable_display = 0;
    gCtxt.checkDualCsi = 0;
    gCtxt.enable_csc = 0;
    gCtxt.enable_cache = 0;

    gCtxt.exitSeconds = 0;
    gCtxt.gpioNumber = 0;
    gCtxt.vis_value = 1;

    gCtxt.fps_print_delay = DEFAULT_PRINT_DELAY_SEC;
    gCtxt.check_buffer_state = 0;
    gCtxt.bEnableHWTimeStampBasedFps = FALSE;

    pthread_mutex_init(&gCtxt.mutex_abort, NULL);
    pthread_cond_init(&gCtxt.cond_abort, NULL);
    pthread_mutex_init(&gCtxt.mutex_csi_err, NULL);
    pthread_mutex_init(&gCtxt.mutex_open_cnt, NULL);

    gCtxt.t_start = 0;
    gCtxt.enable_deinterlace = TESTUTIL_DEINTERLACE_NONE;
}

/**** CAMERA SIGNAL ****/
CamStatus_e CameraCreateSignal(CameraSignal** ppSignal)
{
    pthread_condattr_t attr;

    if (ppSignal == NULL)
    {
        return CAMERA_EFAILED;
    }

    pthread_condattr_init(&attr);
#ifndef __INTEGRITY
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
#endif

    CameraSignal *pSignal = (CameraSignal*)calloc(1, sizeof(CameraSignal));
    if(NULL == pSignal)
    {
        pthread_condattr_destroy(&attr);
        return CAMERA_ENOMEMORY;
    }
    if (pthread_mutex_init(&pSignal->mutex, 0))
    {
        free(pSignal);
        pthread_condattr_destroy(&attr);
        return CAMERA_EFAILED;
    }
    if (pthread_cond_init(&pSignal->cond_var, &attr))
    {
        pthread_mutex_destroy(&pSignal->mutex);
        free(pSignal);
        pthread_condattr_destroy(&attr);
        return CAMERA_EFAILED;
    }
    pSignal->signal_count = 0;
    pSignal->manualReset = FALSE;

    *ppSignal = pSignal;

    return CAMERA_SUCCESS;
}

CamStatus_e CameraResetSignal(CameraSignal* pSignal)
{
    if (!pSignal)
    {
        return CAMERA_EBADHANDLE;
    }

    if (pthread_mutex_lock(&pSignal->mutex))
    {
        return CAMERA_EFAILED;
    }

    pSignal->signal_count = 0;

    if (pthread_mutex_unlock(&pSignal->mutex))
    {
        return CAMERA_EFAILED;
    }

    return CAMERA_SUCCESS;
}

CamStatus_e CameraWaitOnSignal(CameraSignal* pSignal, uint32_t nTimeoutMilliseconds)
{
    CamStatus_e ret = CAMERA_SUCCESS;
    int result = 0;

    if (!pSignal)
    {
        return CAMERA_EBADHANDLE;
    }

    pthread_mutex_lock(&pSignal->mutex);

    if(nTimeoutMilliseconds != CAM_SIGNAL_WAIT_NO_TIMEOUT)
    {
        struct timespec timeout;

        memset(&timeout, 0, sizeof(struct timespec));
        OS_GET_ABSTIME(timeout, nTimeoutMilliseconds);

        /* we break on being signaled or timeout and errors */
        while (pSignal->signal_count == 0 && !result)
        {
            result = pthread_cond_timedwait(&pSignal->cond_var, &pSignal->mutex, &timeout);
        }

        /*check for errors*/
        if(result && result != ETIMEDOUT)
        {
            pthread_mutex_unlock(&pSignal->mutex);
            return CAMERA_EFAILED;
        }
        ret = (!result) ? CAMERA_SUCCESS : CAMERA_ETIMEOUT;
    }
    else
    {
        while(pSignal->signal_count == 0)
        {
            result = pthread_cond_wait(&pSignal->cond_var, &pSignal->mutex);
            if (result)
            {
                ret = CAMERA_EFAILED;
            }
        }
    }

    if(!pSignal->manualReset && ret != CAMERA_ETIMEOUT)
    {
        pSignal->signal_count--;
    }
    pthread_mutex_unlock(&pSignal->mutex);

    return ret;
}

CamStatus_e CameraSetSignal(CameraSignal* pSignal)
{
    if (!pSignal)
    {
        return CAMERA_EBADHANDLE;
    }

    if (pthread_mutex_lock(&pSignal->mutex))
    {
        return CAMERA_EFAILED;
    }

    pSignal->signal_count++;
    pthread_cond_signal(&pSignal->cond_var);

    if (pthread_mutex_unlock(&pSignal->mutex))
    {
        return CAMERA_EFAILED;
    }

    return CAMERA_SUCCESS;
}

void CameraDestroySignal(CameraSignal* pSignal)
{
    if (pSignal)
    {
        pthread_cond_destroy(&pSignal->cond_var);
        pthread_mutex_destroy(&pSignal->mutex);
        free(pSignal);
    }
}
/**********************************/

static int qcarcam_test_get_time(unsigned long long *pTime)
{
    struct timespec time;
    unsigned long long msec;

    if (clock_gettime(CLOCK_MONOTONIC, &time) == -1)
    {
        QCARCAM_ERRORMSG("Clock gettime failed");
        return 1;
    }
    msec = ((unsigned long long)time.tv_sec * 1000) + (((unsigned long long)time.tv_nsec / 1000) / 1000);
    *pTime = msec;

    return 0;
}

static void qcarcam_test_clear_usr_process_list(qcarcam_test_input_t *input_ctxt)
{
    pthread_mutex_lock(&input_ctxt->mutex);
    input_ctxt->state = QCARCAMTEST_STATE_PAUSE_STOP_PENDING;

    /* Wait user process frame in progress to finish */
    while (input_ctxt->usr_process_frameCnt)
    {
        pthread_mutex_unlock(&input_ctxt->mutex);
        usleep(USR_PROCESS_WAIT_USLEEP);
        pthread_mutex_lock(&input_ctxt->mutex);
    }

    /* reclaim user process frames in list */
    while (!input_ctxt->usr_process_buf_idx.empty())
    {
        uint32_t idx = input_ctxt->usr_process_buf_idx.front();
        input_ctxt->usr_process_buf_idx.pop_front();
        for (uint32_t stream_idx = 0; stream_idx < input_ctxt->num_stream_instance; stream_idx++)
        {
            input_ctxt->buffers_param[stream_idx][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[idx] = QCARCAMTEST_BUFFER_STATE_GET_FRAME;
            input_ctxt->buffers_param[stream_idx][QCARCAM_TEST_BUFFERS_OUTPUT].release_buf_idx.push_back(idx);
        }
    }

    pthread_mutex_unlock(&input_ctxt->mutex);
}

QCarCamRet_e qcarcam_input_open(qcarcam_test_input_t *input_ctxt)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;
    QCarCamOpen_t openParams = {};

    openParams.opMode = input_ctxt->op_mode;
    openParams.numInputs = input_ctxt->num_inputs;

    for (unsigned int idx = 0; idx < input_ctxt->num_inputs; idx++)
    {
        openParams.inputs[idx].inputId = input_ctxt->input_param[idx].inputId;
        openParams.inputs[idx].srcId = input_ctxt->input_param[idx].srcId;
        openParams.inputs[idx].inputMode = input_ctxt->input_param[idx].inputMode;
    }

    if (input_ctxt->recovery)
    {
        openParams.flags = QCARCAM_OPEN_FLAGS_RECOVERY;
    }

    ret = QCarCamOpen(&openParams, &input_ctxt->qcarcam_hndl);
    if(ret != QCARCAM_RET_OK || input_ctxt->qcarcam_hndl == QCARCAM_HNDL_INVALID)
    {
        input_ctxt->state = QCARCAMTEST_STATE_ERROR;
        QCARCAM_ERRORMSG("qcarcam_open() failed %d 0x%lx", ret, input_ctxt->qcarcam_hndl);
        goto qcarcam_input_open_error;
    }

    pthread_mutex_lock(&gCtxt.mutex_open_cnt);
    gCtxt.opened_stream_cnt++;
    pthread_mutex_unlock(&gCtxt.mutex_open_cnt);
    input_ctxt->state = QCARCAMTEST_STATE_OPEN;

    //@TODO: change to set sensor mode id
#if 0
    // For HDMI/CVBS Input
    // NOTE: set HDMI IN resolution in qcarcam_config_single_hdmi.xml before
    // running the test

    if (input_ctxt->input_param[0].inputId == QCARCAM_INPUT_TYPE_DIGITAL_MEDIA)
    {
        qcarcam_param_value_t param = {};
        param.res_value.width = input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].width;
        param.res_value.height = input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].height;

        ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_PARAM_RESOLUTION, &param);
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("QCarCamSetParam resolution() failed");
            goto qcarcam_input_open_fail;;
        }
    }
#endif

    for (unsigned int idx = 0; idx < input_ctxt->num_stream_instance; idx++)
    {
        input_ctxt->p_buffers_output[idx].id = input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].bufferList_id;
        ret = QCarCamSetBuffers(input_ctxt->qcarcam_hndl, &input_ctxt->p_buffers_output[idx]);
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("qcarcam_s_buffers() failed");
            goto qcarcam_input_open_fail;
        }
    }

    if (input_ctxt->is_injection)
    {
        QCARCAM_ERRORMSG("read from input_file - set input buffers");

        ret = QCarCamSetBuffers(input_ctxt->qcarcam_hndl, &input_ctxt->p_buffers_input);
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("QCarCamSetBuffers() failed");
            goto qcarcam_input_open_fail;
        }
    }

    if (input_ctxt->use_event_callback)
    {
        uint32_t param = 0;

        ret = QCarCamRegisterEventCallback(input_ctxt->qcarcam_hndl, &qcarcam_test_event_cb, input_ctxt);
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("QCarCamSetParam(QCARCAM_PARAM_EVENT_CB) failed");
            goto qcarcam_input_open_fail;
        }


        if (input_ctxt->recovery)
        {
            param = QCARCAM_EVENT_FRAME_READY |
                QCARCAM_EVENT_INPUT_SIGNAL |
                QCARCAM_EVENT_ERROR | QCARCAM_EVENT_VENDOR |
                QCARCAM_EVENT_RECOVERY;
        }
        else
        {
            param = QCARCAM_EVENT_FRAME_READY |
                QCARCAM_EVENT_INPUT_SIGNAL |
                QCARCAM_EVENT_ERROR | QCARCAM_EVENT_VENDOR;
        }

        ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_STREAM_CONFIG_PARAM_EVENT_MASK, &param, sizeof(param));
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("QCarCamSetParam(QCARCAM_STREAM_CONFIG_PARAM_EVENT_MASK) failed");
            goto qcarcam_input_open_fail;
        }
    }
    if (input_ctxt->frameDropConfig.frameDropPeriod != 0)
    {
        ret = QCarCamSetParam(input_ctxt->qcarcam_hndl,
                QCARCAM_STREAM_CONFIG_PARAM_FRAME_DROP_CONTROL,
                &input_ctxt->frameDropConfig,
                sizeof(input_ctxt->frameDropConfig));
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("QCarCamSetParam(QCARCAM_STREAM_CONFIG_PARAM_FRAME_DROP_CONTROL) failed");
            goto qcarcam_input_open_fail;
        }
    }
    if (input_ctxt->num_batch_frames > 1)
    {
        QCarCamBatchConfig_t param = {};

        param.numBatchFrames = input_ctxt->num_batch_frames;
        param.frameIncrement = input_ctxt->frame_increment;

        ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_STREAM_CONFIG_PARAM_BATCH_MODE, &param, sizeof(param));
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("QCarCamSetParam(QCARCAM_STREAM_CONFIG_PARAM_BATCH_MODE) failed");
            goto qcarcam_input_open_fail;
        }
    }
    for (uint32_t idx = 0; idx < input_ctxt->num_isp_instances; idx++)
    {
        ret = QCarCamSetParam(input_ctxt->qcarcam_hndl,
                QCARCAM_STREAM_CONFIG_PARAM_ISP_USECASE,
                &input_ctxt->isp_config[idx],
                sizeof(input_ctxt->isp_config[idx]));
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("QCarCamSetParam(QCARCAM_STREAM_CONFIG_PARAM_ISP_USECASE) failed");
            goto qcarcam_input_open_fail;
        }
    }

qcarcam_input_open_fail:

    if(ret != QCARCAM_RET_OK)
    {
        QCarCamClose(input_ctxt->qcarcam_hndl);
        input_ctxt->state = QCARCAMTEST_STATE_CLOSED;
        input_ctxt->qcarcam_hndl = QCARCAM_HNDL_INVALID;
        pthread_mutex_lock(&gCtxt.mutex_open_cnt);
        gCtxt.opened_stream_cnt--;
        pthread_mutex_unlock(&gCtxt.mutex_open_cnt);
    }
qcarcam_input_open_error:
    return ret;
}

QCarCamRet_e qcarcam_input_close(qcarcam_test_input_t *input_ctxt)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;

    ret = QCarCamClose(input_ctxt->qcarcam_hndl);
    if (ret == QCARCAM_RET_OK)
    {
        input_ctxt->state = QCARCAMTEST_STATE_CLOSED;
        input_ctxt->qcarcam_hndl = QCARCAM_HNDL_INVALID;
        pthread_mutex_lock(&gCtxt.mutex_open_cnt);
        gCtxt.opened_stream_cnt--;
        pthread_mutex_unlock(&gCtxt.mutex_open_cnt);

        QCARCAM_INFOMSG("Client %d Input handle 0x%lx QCarCamClose successfully", input_ctxt->idx, input_ctxt->qcarcam_hndl);
    }
    else
    {
        input_ctxt->state = QCARCAMTEST_STATE_ERROR;
        QCARCAM_ERRORMSG("Client %d Input handle 0x%lx QCarCamClose failed: %d", input_ctxt->idx, input_ctxt->qcarcam_hndl, ret);
    }

    return ret;
}


QCarCamRet_e qcarcam_input_start(qcarcam_test_input_t *input_ctxt)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;

    qcarcam_test_get_time(&input_ctxt->t_start);

    if (FALSE == input_ctxt->is_reserved)
    {
        ret = QCarCamReserve(input_ctxt->qcarcam_hndl);
        if (QCARCAM_RET_OK != ret)
        {
            input_ctxt->state = QCARCAMTEST_STATE_ERROR;
            QCARCAM_ERRORMSG("Client %d hndl %lX QCarCamReserve failed: %d",
                    input_ctxt->idx, input_ctxt->qcarcam_hndl, ret);
            return ret;
        }
        else
        {
            input_ctxt->is_reserved = TRUE;
        }
    }

    ret = QCarCamStart(input_ctxt->qcarcam_hndl);
    if (ret == QCARCAM_RET_OK)
    {
        input_ctxt->state = QCARCAMTEST_STATE_START;
        for (uint32_t idx = 0; idx < input_ctxt->num_stream_instance; idx++)
        {
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt = 0;
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].releaseframeCnt = 0;
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].prev_frameCnt = 0;
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].prev_releaseframeCnt = 0;
        }
        input_ctxt->signal_lost = 0;

        qcarcam_test_get_time(&input_ctxt->t_start_success);

        QCARCAM_INFOMSG("Client %d Input handle 0x%lx QCarCamStart successfully",
                input_ctxt->idx, input_ctxt->qcarcam_hndl);

        //test for request_mode
        if (input_ctxt->request_mode)
        {
            QCarCamRequest_t request = {};
            uint32_t idx;

            /* Now all the streams have same number of buffers.
             * So using the buffer count of first stream to count total 
             * number of requests, since there will be at least one stream.
             *
             * TODO: Need to modify the logic appropriately if the number of
             *  buffers differs for different streams
             *  */
            for(idx = 0; idx < input_ctxt->p_buffers_output[0].nBuffers; idx++)
            {
                request.requestId = idx;
                request.inputMetaIdx = input_ctxt->input_meta_idx;
                request.numStreamRequests = input_ctxt->num_stream_instance;
                /* Populate the details of all the sreams required in one request */
                for(uint32_t stream = 0; stream < request.numStreamRequests; stream++)
                {
                    request.streamRequests[stream].bufferlistId = 
                        input_ctxt->buffers_param[stream][QCARCAM_TEST_BUFFERS_OUTPUT].bufferList_id;
                    request.streamRequests[stream].bufferId = idx;
                } 
                ret = QCarCamSubmitRequest(input_ctxt->qcarcam_hndl, &request);
            }
            input_ctxt->submitrequestSettings.request_id = idx;
        }
    }
    else
    {
        input_ctxt->state = QCARCAMTEST_STATE_ERROR;
        QCARCAM_ERRORMSG("Client %d Input handle 0x%lx QCarCamStart failed: %d",
                input_ctxt->idx, input_ctxt->qcarcam_hndl, ret);
    }

    return ret;
}

QCarCamRet_e qcarcam_input_stop(qcarcam_test_input_t *input_ctxt)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;

    ret = QCarCamStop(input_ctxt->qcarcam_hndl);
    if (ret == QCARCAM_RET_OK)
    {
        input_ctxt->state = QCARCAMTEST_STATE_STOP;
        for (uint32_t idx = 0; idx < input_ctxt->num_stream_instance; idx++)
        {
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt = 0;
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].releaseframeCnt = 0;
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].prev_frameCnt = 0;
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].prev_releaseframeCnt = 0;
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].release_buf_idx.clear();
        }
        input_ctxt->usr_process_buf_idx.clear();
        input_ctxt->submitrequestSettings.request_id = 0;

        QCARCAM_INFOMSG("Client %d Input handle 0x%lx QCarCamStop successfully", input_ctxt->idx, input_ctxt->qcarcam_hndl);
    }
    else
    {
        input_ctxt->state = QCARCAMTEST_STATE_ERROR;
        QCARCAM_ERRORMSG("Client %d Input handle 0x%lx QCarCamStop failed: %d", input_ctxt->idx, input_ctxt->qcarcam_hndl, ret);
    }

    for (uint32_t idx = 0; idx < input_ctxt->num_stream_instance; idx++)
    {
        memset(&input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state, 0x0, sizeof(input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state));
    }

    return ret;
}

QCarCamRet_e qcarcam_input_pause(qcarcam_test_input_t *input_ctxt)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;

    ret = QCarCamPause(input_ctxt->qcarcam_hndl);
    if (ret == QCARCAM_RET_OK)
    {
        input_ctxt->state = QCARCAMTEST_STATE_PAUSE;
        for (uint32_t idx = 0; idx < input_ctxt->num_stream_instance; idx++)
        {
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt = 0;
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].releaseframeCnt = 0;
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].prev_frameCnt = 0;
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].prev_releaseframeCnt = 0;
        }
        QCARCAM_INFOMSG("Client %d Input handle 0x%lx QCarCamPause successfully", input_ctxt->idx, input_ctxt->qcarcam_hndl);
    }
    else
    {
        input_ctxt->state = QCARCAMTEST_STATE_ERROR;
        QCARCAM_ERRORMSG("Client %d Input handle 0x%lx QCarCamPause failed: %d", input_ctxt->idx, input_ctxt->qcarcam_hndl, ret);
    }

    return ret;
}

QCarCamRet_e qcarcam_input_resume(qcarcam_test_input_t *input_ctxt)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;

    qcarcam_test_get_time(&input_ctxt->t_start);

    ret = QCarCamResume(input_ctxt->qcarcam_hndl);
    if (ret == QCARCAM_RET_OK)
    {
        input_ctxt->state = QCARCAMTEST_STATE_START;
        for (uint32_t idx = 0; idx < input_ctxt->num_stream_instance; idx++)
        {
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt = 0;
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].releaseframeCnt = 0;
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].prev_frameCnt = 0;
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].prev_releaseframeCnt = 0;
        }
        input_ctxt->signal_lost = 0;
        qcarcam_test_get_time(&input_ctxt->t_start_success);

        QCARCAM_INFOMSG("Client %d Input handle 0x%lx QCarCamResume success", input_ctxt->idx, input_ctxt->qcarcam_hndl);

        /* Queue the buffers again if in request mode */
        if (input_ctxt->request_mode)
        {
            QCarCamRequest_t request = {};
            uint32_t idx;

            /* Now all the streams have same number of buffers.
             * So using the buffer count of first stream to count total 
             * number of requests, since there will be at least one stream.
             *
             * TODO: Need to modify the logic appropriately if the number of
             *  buffers differs for different streams
             *  */
            for(idx = 0; idx < input_ctxt->p_buffers_output[0].nBuffers; idx++)
            {
                request.requestId = idx;
                request.inputMetaIdx = input_ctxt->input_meta_idx;
                request.numStreamRequests = input_ctxt->num_stream_instance;
                /* Populate the details of all the sreams required in one request */
                for(uint32_t stream = 0; stream < request.numStreamRequests; stream++)
                {
                    request.streamRequests[stream].bufferlistId = 
                        input_ctxt->buffers_param[stream][QCARCAM_TEST_BUFFERS_OUTPUT].bufferList_id;
                    request.streamRequests[stream].bufferId = idx;
                }
                ret = QCarCamSubmitRequest(input_ctxt->qcarcam_hndl, &request);
            }
            input_ctxt->submitrequestSettings.request_id = idx;
        }
    }
    else
    {
        input_ctxt->state = QCARCAMTEST_STATE_ERROR;
        QCARCAM_ERRORMSG("Client %d Input handle 0x%lx QCarCamResume failed %d", input_ctxt->idx, input_ctxt->qcarcam_hndl, ret);
    }

    return ret;
}


static void qcarcam_test_check_buffers(qcarcam_test_input_t *input_ctxt, uint32_t bufferListId)
{
    unsigned int i;
    unsigned int num_buffers = input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].n_buffers;

    for (i = 0; i < num_buffers; i++)
    {
        printf("|    |     | %d  0x%02x  ", i, input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[i]);
        if (CHECK_BIT(input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[i], QCARCAMTEST_BUFFER_STALE_BIT))
        {
            printf(" [stale]");
        }
        else
        {
            printf("        ");
        }
        printf("          |\n");

        input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[i] = (qcarcam_test_buffer_state_t)(input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[i] | (1 << QCARCAMTEST_BUFFER_STALE_BIT));
    }

    fflush(stdout);
}

static void qcarcam_test_get_frame_rate(unsigned long long timer_prev)
{
    float average_fps, average_rel_fps, average_hwfps = 0;
    int input_idx = 0;
    unsigned long long timer_now = 0;

    qcarcam_test_get_time(&timer_now);

    printf("-----------FPS Report - %.1f sec--------------\n", ((float)(timer_now - gCtxt.t_start) / 1000));
    if (TRUE == gCtxt.bEnableHWTimeStampBasedFps)
    {
        printf("| sid | cid | bid |  state (time)| fps(sw)  | fps(hw)  | rel  |\n");
    }
    else
    {
        printf("| sid | cid | bid |  state (time)| fps  | rel  |\n");
    }

    for (input_idx = 0; input_idx < gCtxt.numInputs; ++input_idx)
    {
        qcarcam_test_input_t* input_ctxt = &gCtxt.inputs[input_idx];

        pthread_mutex_lock(&input_ctxt->mutex);

        for (uint32_t idx = 0; idx < input_ctxt->num_stream_instance; idx++)
        {
            qcarcam_test_get_time(&timer_now);

            uint32_t inputIdIdx = idx % input_ctxt->num_inputs; 
            printf("| %2u  | %2u  | %2u  | ",input_ctxt->sessionId, input_ctxt->input_param[inputIdIdx].inputId, (unsigned int)input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].bufferList_id);

            if (input_ctxt->state == QCARCAMTEST_STATE_START)
            {
                if (!input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt)
                {
                    average_fps = 0.0f;
                    average_rel_fps = 0.0f;

                    //use first frame time for first report if got a frame
                    if (input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt > 1)
                    {
                        printf("ok      (%3.1f)", ((float)(timer_now - input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].t_firstFrame) / 1000));
                        if (TRUE == gCtxt.bEnableHWTimeStampBasedFps)
                        {
                            average_hwfps = (input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt-1) / 
                                             ((float)((input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].hwt_latestFrame 
                                              - input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].hwt_firstFrame) * NS_TO_MS) / 1000);
                        }
                        average_fps = (input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt-1) / ((float)(timer_now - input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].t_firstFrame) / 1000);
                        if (input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].releaseframeCnt > 1)
                        {
                            average_rel_fps = (input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].releaseframeCnt-1) / ((float)(timer_now - input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].t_firstFrame) / 1000);
                        }
                    }
                    else
                    {
                        float time_since_start = ((float)(timer_now - input_ctxt->t_start) / 1000);
                        float time_since_start_success = ((float)(timer_now - input_ctxt->t_start_success) / 1000);
                        if (time_since_start_success > QCARCAMTEST_SOF_FREEZE_TIMEOUT)
                        {
                            //TODO: maybe can add freeze detection here to try and restart?
                            printf("freeze  (%3.1f)", time_since_start);
                        }
                        else
                        {
                            printf("started (%3.1f)", time_since_start);
                        }
                    }
                }
                else
                {
                    int frames_counted;
                    int release_frames_counted;

                    frames_counted = input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt - input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].prev_frameCnt;
                    release_frames_counted = input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].releaseframeCnt - input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].prev_releaseframeCnt;
                    if (TRUE == gCtxt.bEnableHWTimeStampBasedFps)
                    {
                        average_hwfps = frames_counted / 
                                        ((float)((input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].hwt_latestFrame 
                                         - input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].hwt_prevFrame) * (NS_TO_MS)) / 1000);      
                    }
                    average_fps = frames_counted / ((float)(timer_now - timer_prev) / 1000);
                    average_rel_fps = release_frames_counted / ((float)(timer_now - timer_prev) / 1000);

                    if (frames_counted)
                    {
                        printf("ok           ");
                    }
                    else
                    {
                        //TODO: maybe can add freeze detection here to try and restart?
                        printf("freeze       ");
                    }
                }

                if (TRUE == gCtxt.bEnableHWTimeStampBasedFps)
                {
                    printf("| %4.1f     | %4.1f     | %4.1f |\n", average_fps, average_hwfps, average_rel_fps);
                }
                else
                {
                    printf("| %4.1f | %4.1f |\n", average_fps, average_rel_fps);
                }
                input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].hwt_prevFrame = input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].hwt_latestFrame;
                input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].prev_frameCnt = input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt;
                input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].prev_releaseframeCnt = input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].releaseframeCnt;

                if (gCtxt.check_buffer_state)
                {
                    qcarcam_test_check_buffers(input_ctxt, idx);
                }
            }
            else if (input_ctxt->state == QCARCAMTEST_STATE_STOP)
            {
                printf("stop         |      |      |\n");
            }
            else if (input_ctxt->state == QCARCAMTEST_STATE_PAUSE)
            {
                printf("pause        |      |      |\n");
            }
            else if (input_ctxt->state == QCARCAMTEST_STATE_ERROR)
            {
                printf("ERROR        |      |      |\n");
            }
            else if (input_ctxt->state == QCARCAMTEST_STATE_INIT)
            {
                printf("init         |      |      |\n");
            }
            else if (input_ctxt->state == QCARCAMTEST_STATE_PAUSE_STOP_PENDING)
            {
                printf("pending      |      |      |\n");
            }
            else if (input_ctxt->state == QCARCAMTEST_STATE_SUSPEND)
            {
                printf("suspend      |      |      |\n");
            }
            else if (input_ctxt->state == QCARCAMTEST_STATE_RECOVERY)
            {
                printf("recovery     |      |      |\n");
            }
            else
            {
                printf("UNKNOWN(%d)  |      |      |\n", input_ctxt->state);
            }

            pthread_mutex_unlock(&input_ctxt->mutex);
        }
    }
    printf("-----------------------------------------------\n");
    fflush(stdout);
}

static int test_signal_loss(qcarcam_test_input_t *input_ctxt, bool *signal_lost_check)
{
    if (input_ctxt->signal_lost != *signal_lost_check)
    {
        // Check if signal status has changed
        *signal_lost_check = input_ctxt->signal_lost;
    }
    else if (input_ctxt->signal_lost == 1)
    {
        for (uint32_t idx = 0 ; idx < input_ctxt->num_stream_instance; idx++)
        {
            // wait 1 cycle then post empty frame to display
            test_util_post_window_buffer(input_ctxt->test_util_ctxt,
            input_ctxt->qcarcam_window[idx],
            input_ctxt->p_buffers_output[idx].nBuffers,
            &input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].release_buf_idx,
            input_ctxt->field_type_previous);
        }
    }

    return 0;
}

static int check_signal_loss_thread(void *arg)
{
    pthread_detach(pthread_self());

    bool signal_lost_check[NUM_MAX_CAMERAS] = {};
    unsigned int signal_check_delay_us = SIGNAL_CHECK_DELAY; // 33 milliseconds

    while (!g_aborted)
    {
        // Check if signal status has changed
        for (int i = 0; i < gCtxt.numInputs; i++)
        {
            test_signal_loss(&gCtxt.inputs[i], &signal_lost_check[i]);
        }
        usleep(signal_check_delay_us);
    }
    return 0;
}

static void test_fatal_error_check(qcarcam_test_input_t *input_ctxt, volatile int *csi_err_cnt_prev)
{
    if (input_ctxt->fatal_err_cnt != *csi_err_cnt_prev)
    {
        // error happened. Set it and wait 1 more iteration before take recovery action
        *csi_err_cnt_prev = input_ctxt->fatal_err_cnt;
        input_ctxt->is_fatal_error = 1;
    }
    else if (input_ctxt->is_fatal_error)
    {
        pthread_mutex_lock(&input_ctxt->mutex);
        if (input_ctxt->state == QCARCAMTEST_STATE_START)
        {
            QCARCAM_ERRORMSG("Input handle 0x%lx already running again, return", input_ctxt->qcarcam_hndl);
        }
        else
        {
            (void)qcarcam_input_start(input_ctxt);
            input_ctxt->is_fatal_error = 0;
        }

        pthread_mutex_unlock(&input_ctxt->mutex);
    }
}

static int check_error_thread(void *arg)
{
    pthread_detach(pthread_self());

    volatile int fatal_error_prev[NUM_MAX_CAMERAS] = { 0 };
    unsigned int error_check_delay_us = CSI_ERR_CHECK_DELAY; // 100 milliseconds

    while (!g_aborted)
    {
        // Check if csi error continues or not
        for (int i = 0; i < gCtxt.numInputs; i++)
        {
            test_fatal_error_check(&gCtxt.inputs[i], &fatal_error_prev[i]);
        }
        usleep(error_check_delay_us);
    }
    return 0;
}

static int framerate_thread(void *arg)
{
    pthread_detach(pthread_self());

    unsigned int fps_print_delay_us = gCtxt.fps_print_delay * 1000000;
    unsigned long long timer1;

    while (!g_aborted)
    {
        qcarcam_test_get_time(&timer1);
        usleep(fps_print_delay_us);
        qcarcam_test_get_frame_rate(timer1);
    }
    return 0;
}

void abort_test(void)
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

    unsigned long long timer_start = 0;
    unsigned long long timer_test = 0;
    qcarcam_test_get_time(&timer_start);
    while (!g_aborted)
    {
        qcarcam_test_get_time(&timer_test);
        if ((timer_test - timer_start) >= ((unsigned long long)gCtxt.exitSeconds * 1000))
        {
            QCARCAM_ALWZMSG("TEST Aborted after running for %d secs successfully!", gCtxt.exitSeconds);
            abort_test();
            break;
        }
        usleep(TIMER_THREAD_USLEEP);
    }
    return 0;
}

static int signal_thread(void *arg)
{
    struct timespec timeout;

    pthread_detach(pthread_self());

    timeout.tv_sec = 0;
    timeout.tv_nsec = SIGWAIT_TIMEOUT_MS * 1000000;

    while (!g_aborted)
    {
        if (sigtimedwait(&g_sigset, NULL, &timeout) > 0)
        {
            abort_test();
            break;
        }
    }
    return 0;
}

static void process_deinterlace(qcarcam_test_input_t *input_ctxt, uint32_t id, QCarCamInterlaceField_e field_type, testutil_deinterlace_t di_method)
{
    test_util_sw_di_t di_info;

    di_info.qcarcam_window = input_ctxt->qcarcam_window[id];
    di_info.display_window = input_ctxt->display_window[id];
    di_info.source_buf_idx = input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt;
    di_info.field_type = field_type;

    switch (di_method) {
    case TESTUTIL_DEINTERLACE_SW_WEAVE:
        /* sw weave 30fps method deinterlacing */
        test_util_di_sw_weave_30fps(&di_info);
        break;
    case TESTUTIL_DEINTERLACE_SW_BOB:
        /* needn't process field into new buffer, display each field in post_window_buffer directly */
        break;
    default:
        /* unsupported deinterlacing method */
        QCARCAM_ERRORMSG("Unknown deinterlacing method");
        break;
    }
}

/**
 * Function to retrieve frame from qcarcam and increase frame_counter
 * @param input_ctxt
 * @return 0 on success, -1 on failure
 */
static int qcarcam_test_get_frame(qcarcam_test_input_t *input_ctxt, uint32_t bufferListId)
{
    QCarCamRet_e ret;
    QCarCamFrameInfo_t buffer_info;
    buffer_info.id = bufferListId;
    ret = QCarCamGetFrame(input_ctxt->qcarcam_hndl, &buffer_info, input_ctxt->frame_timeout, 0);
    if (ret == QCARCAM_RET_TIMEOUT)
    {
        QCARCAM_ERRORMSG("QCarCamGetFrame timeout context 0x%lx ret %d", input_ctxt->qcarcam_hndl, ret);
        input_ctxt->signal_lost = 1;
        return -1;
    }

    if (QCARCAM_RET_OK != ret)
    {
        QCARCAM_ERRORMSG("Get frame context 0x%lx ret %d", input_ctxt->qcarcam_hndl, ret);
        return -1;
    }

    if (buffer_info.bufferIndex >= input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].n_buffers)
    {
        QCARCAM_ERRORMSG("Get frame context 0x%lx ret invalid idx %d", input_ctxt->qcarcam_hndl, buffer_info.bufferIndex);
        return -1;
    }

    if (input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt == 0)
    {
        qcarcam_test_get_time(&input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].t_firstFrame);
        input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].hwt_firstFrame = buffer_info.sofTimestamp.timestamp;
        input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].hwt_prevFrame = buffer_info.sofTimestamp.timestamp;
        if (input_ctxt->is_first_start)
        {
            ais_log_kpi(AIS_EVENT_KPI_CLIENT_FIRST_FRAME);

            printf("Success - First Frame [%d:%d]\n", input_ctxt->idx, input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].bufferList_id);
            fflush(stdout);

            QCARCAM_ALWZMSG("[%u] First Frame buf_idx %d after : %llu ms (field type: %d)",
                    input_ctxt->idx, buffer_info.bufferIndex, (input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].t_firstFrame - gCtxt.t_start), buffer_info.fieldType);

            input_ctxt->is_first_start = FALSE;
        }
        else
        {
            QCARCAM_ALWZMSG("[%d:0x%lx] restart took %llu ms",
                    input_ctxt->idx, input_ctxt->qcarcam_hndl, (input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].t_firstFrame - input_ctxt->t_start));
        }

        if (gCtxt.enable_deinterlace)
        {
            input_ctxt->field_type_previous = QCARCAM_INTERLACE_FIELD_UNKNOWN;

            if (buffer_info.fieldType == QCARCAM_INTERLACE_FIELD_UNKNOWN)
                input_ctxt->skip_post_display = 1;
            else
                input_ctxt->field_type_previous = buffer_info.fieldType;
        }
    }
    else
    {
        input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].hwt_latestFrame = buffer_info.sofTimestamp.timestamp;
        if (gCtxt.enable_deinterlace && (input_ctxt->field_type_previous != buffer_info.fieldType))
        {
            input_ctxt->skip_post_display = 0;
            QCARCAM_ERRORMSG("Field type changed: %d -> %d @frame_%d", input_ctxt->field_type_previous, buffer_info.fieldType, buffer_info.seqNo);
            if (buffer_info.fieldType == QCARCAM_INTERLACE_FIELD_UNKNOWN)
                buffer_info.fieldType = input_ctxt->field_type_previous;
            else
                input_ctxt->field_type_previous = buffer_info.fieldType;
        }
    }

    input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].get_frame_buf_idx = buffer_info.bufferIndex;
    input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].get_frame_buf_idx] = QCARCAMTEST_BUFFER_STATE_GET_FRAME;

    input_ctxt->signal_lost = 0;

    input_ctxt->diag.frame_generate_time[TEST_PREV_BUFFER] = input_ctxt->diag.frame_generate_time[TEST_CUR_BUFFER];
    input_ctxt->diag.frame_generate_time[TEST_CUR_BUFFER] = buffer_info.timestamp * NS_TO_MS;

    QCARCAM_DBGMSG("[%d] frameCnt:%d (seqNo %u) bufId:%d qtime:%lu inputID:%u bufferListId:%u",
                   input_ctxt->idx, input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt, buffer_info.seqNo,
                    buffer_info.bufferIndex, buffer_info.sofTimestamp.timestamp, input_ctxt->input_param[bufferListId].inputId,
                     input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].bufferList_id);

    if (gCtxt.enable_deinterlace)
        process_deinterlace(input_ctxt, buffer_info.id, buffer_info.fieldType, gCtxt.enable_deinterlace);

    input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt++;

    return 0;
}
/**
 * Function to post new frame to display. May also do color conversion and frame dumps.
 * @param input_ctxt
 * @return 0 on success, -1 on failure
 */
static int qcarcam_test_post_to_display(qcarcam_test_input_t *input_ctxt, uint32_t bufferListId)
{
    QCarCamRet_e ret;
    /**********************
     * Composite to display
     ********************** */
    QCARCAM_DBGMSG("Post Frame before buf_idx %i", input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam);
    /**********************
     * Dump raw if necessary
     ********************** */
    if (input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].dumpNextFrame ||
        (input_ctxt->is_injection && input_ctxt->injectionSettings.n_dump_frames > 0) ||
        (gCtxt.dumpFrame && (0 == input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt % gCtxt.dumpFrame)))
    {
        snprintf(g_filename, sizeof(g_filename), DEFAULT_DUMP_LOCATION "frame_%d_%d_%i.raw", input_ctxt->idx, bufferListId, input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt);
#ifdef POST_PROCESS
        test_util_dump_window_buffer(input_ctxt->test_util_ctxt, input_ctxt->display_window[bufferListId], input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_disp, g_filename);
#else
        test_util_dump_window_buffer(input_ctxt->test_util_ctxt, input_ctxt->qcarcam_window[bufferListId], input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam, g_filename);
#endif

        input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].dumpNextFrame = FALSE;

        if (input_ctxt->is_injection)
        {
            input_ctxt->injectionSettings.n_dump_frames--;
        }
    }

    if (gCtxt.checkDualCsi)
    {
        test_util_buf_ptr_t buffer = {};
        uint32_t i = 0;
        uint32_t width = input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].width;
        uint32_t height = input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].height;
        uint32_t height_incr = 50; //check matching pixels every 50 lines
        uint32_t midpoint = width*3/2;
        uint32_t stride = 0;
        uint32_t err_cnt = 0;
        uint32_t check_cnt = 0;
        uint8_t *p_buf = NULL;

        buffer.buf_idx = input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam;
        test_util_get_buf_ptr(input_ctxt->qcarcam_window[bufferListId], &buffer);

        p_buf = (uint8_t *)buffer.p_va[0];
        stride = (uint32_t)buffer.stride[0];

        for (i = 0; i < height; i += height_incr)
        {

            if (p_buf[0] != p_buf[midpoint] ||
                p_buf[1] != p_buf[midpoint + 1] ||
                p_buf[2] != p_buf[midpoint + 2])
            {
                err_cnt++;

            }
            check_cnt++;

            p_buf += (stride * height_incr);
        }

        if (err_cnt)
        {
            QCARCAM_ERRORMSG("[FAIL] DUALCSI CHECK %u_%u FAILED %d/%d LINE CHECKS", input_ctxt->idx, input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt,
                err_cnt, check_cnt);
        }
    }

    /**********************
     * Color conversion if necessary
     ********************** */
#ifdef POST_PROCESS
    if (!(gCtxt.enable_c2d || gCtxt.enable_csc|| gCtxt.enable_post_processing))
#else
    if (!(gCtxt.enable_c2d || gCtxt.enable_csc))
#endif
    {
        /**********************
         * Post to screen
         ********************** */
        QCARCAM_DBGMSG("Post Frame %d", input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam);
        if (gCtxt.enable_deinterlace && (gCtxt.enable_deinterlace == TESTUTIL_DEINTERLACE_SW_WEAVE))
        {
            ret = test_util_post_window_buffer(input_ctxt->test_util_ctxt, input_ctxt->display_window[bufferListId], input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_disp, NULL, input_ctxt->field_type_previous);
            input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_disp++;
            input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_disp %= input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_DISPLAY].n_buffers;
            input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].release_buf_idx.push_back(input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam);
        }
        else
        {
            ret = test_util_post_window_buffer(input_ctxt->test_util_ctxt, input_ctxt->qcarcam_window[bufferListId], input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam, &input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].release_buf_idx, input_ctxt->field_type_previous);
            input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam] = QCARCAMTEST_BUFFER_STATE_POST_DISPLAY;
            input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].release_buf_idx.push_back(input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam);
        }
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("test_util_post_window_buffer failed");
        }
    }
#ifndef C2D_DISABLED
    else
    {
#ifdef POST_PROCESS
        if (!(gCtxt.enable_csc || gCtxt.enable_post_processing))
#else
        if (!(gCtxt.enable_csc))
#endif
        {
            //for now always go through c2d conversion instead of posting directly to  test since gles composition cannot handle
            // uyvy buffers for now.
            QCARCAM_DBGMSG("[%d] converting through c2d %d -> %d", input_ctxt->idx, input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam, input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_disp);

            C2D_STATUS c2d_status;
            C2D_OBJECT c2dObject;
            memset(&c2dObject, 0x0, sizeof(C2D_OBJECT));
            unsigned int target_id;
            ret = test_util_get_c2d_surface_id(input_ctxt->test_util_ctxt, input_ctxt->qcarcam_window[bufferListId], input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam, &c2dObject.surface_id);
            ret = test_util_get_c2d_surface_id(input_ctxt->test_util_ctxt, input_ctxt->display_window[bufferListId], input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_disp, &target_id);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("test_util_get_c2d_surface_id failed (%d)", ret);
            }

            pthread_mutex_lock(&gCtxt.mutex_c2d);
            c2d_status = c2dDraw(target_id, C2D_TARGET_ROTATE_0, 0x0, 0, 0, &c2dObject, 1);

            c2d_ts_handle c2d_timestamp;
            if (c2d_status == C2D_STATUS_OK)
            {
                c2d_status = c2dFlush(target_id, &c2d_timestamp);
            }
            pthread_mutex_unlock(&gCtxt.mutex_c2d);

            if (c2d_status == C2D_STATUS_OK)
            {
                c2d_status = c2dWaitTimestamp(c2d_timestamp);
            }

            QCARCAM_DBGMSG("c2d conversion finished");

            if (c2d_status != C2D_STATUS_OK)
            {
                QCARCAM_ERRORMSG("c2d conversion failed with error %d", c2d_status);
            }
        }
#ifdef ENABLE_CL_CONVERTER
        else
        {
            //Use open CL to convert
            csc_run(input_ctxt->g_converter, input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam, input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_disp, NULL);
            csc_wait(input_ctxt->g_converter, 0, NULL);
        }
#else
#ifdef POST_PROCESS
    else{
            if(gCtxt.enable_post_processing)
            {
                pp_job_t pp_job = {};
                pp_job.frame_id = input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam;
                pp_job.src_buf_idx[0] = input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam;
                pp_job.tgt_buf_idx[0] =  input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_disp;

                test_util_post_processing_process_frame(&gCtxt.post_processing_ctx[0], &pp_job);
            }
    }
#endif
#endif

        /**********************
         * Dump if necessary
         ********************** */
        if (0 != gCtxt.dumpFrame)
        {
            if (0 == input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt % gCtxt.dumpFrame)
            {
                snprintf(g_filename, sizeof(g_filename), DEFAULT_DUMP_LOCATION "frame_display_%d_%i.raw", input_ctxt->idx, 
                         input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt);
                test_util_dump_window_buffer(input_ctxt->test_util_ctxt, input_ctxt->display_window[bufferListId], input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_disp, g_filename);
            }
        }
        /**********************
         * Post to screen
         ********************** */
        QCARCAM_DBGMSG("Post Frame %d", input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_disp);
        ret = test_util_post_window_buffer(input_ctxt->test_util_ctxt,
                                           input_ctxt->display_window[bufferListId],
                                           input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_disp,
                                           &input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].release_buf_idx,
                                           input_ctxt->field_type_previous);

        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("test_util_post_window_buffer failed");
        }

        input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_disp++;
        input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_disp %= input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_DISPLAY].n_buffers;
#ifndef __INTEGRITY
//qcarcam buffer is already added to release queue in test_util_post_window_buffer().

        input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].release_buf_idx.push_back(input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam);
#endif
    }
#endif
    QCARCAM_DBGMSG("Post Frame after buf_idx %i", input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam);

    return 0;
}
/**
 * Release frame back to qcarcam
 * @param input_ctxt
 * @return 0 on success, -1 on failure
 */
static int qcarcam_test_release_frame(qcarcam_test_input_t *input_ctxt, uint32_t id)
{
    QCarCamRet_e ret;

    if (gCtxt.disable_display || input_ctxt->window_params[id].is_offscreen)
    {
        /* should release current buffer back to HW immediately if needn't display */
        ret = QCarCamReleaseFrame(input_ctxt->qcarcam_hndl, id, input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam);
        if (QCARCAM_RET_OK != ret)
        {
            QCARCAM_ERRORMSG("QCarCamReleaseFrame(%d) failed %d", input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam, ret);
            return -1;
        }

        input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam] = QCARCAMTEST_BUFFER_STATE_QCARCAM;
        input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].releaseframeCnt++;
    }
    else
    {
        while(!input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].release_buf_idx.empty())
        {
            uint32_t rel_idx = input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].release_buf_idx.front();
            input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].release_buf_idx.pop_front();

            if (rel_idx >= 0 &&
                rel_idx < input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].n_buffers)
            {
                if (QCARCAMTEST_BUFFER_STATE_GET_FRAME == (input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[rel_idx] & 0xF) ||
                    QCARCAMTEST_BUFFER_STATE_POST_DISPLAY == (input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[rel_idx] & 0xF))
                {
                    ret = QCarCamReleaseFrame(input_ctxt->qcarcam_hndl, id, rel_idx);
                    if (QCARCAM_RET_OK != ret)
                    {
                        QCARCAM_ERRORMSG("QCarCamReleaseFrame(%d) failed %d", rel_idx, ret);
                        return -1;
                    }
                    input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].releaseframeCnt++;
                    input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[rel_idx] = QCARCAMTEST_BUFFER_STATE_QCARCAM;
                }
                else
                {
                    QCARCAM_ERRORMSG("QCarCamReleaseFrame(%d) skipped since buffer bad state (%d)", rel_idx, input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[rel_idx]);
                }
            }
            else
            {
                QCARCAM_ERRORMSG("QCarCamReleaseFrame(%d) skipped", rel_idx);
            }
        }
    }

    return 0;
}

static void usr_process_cb(void *arg)
{
    timer_usr_data * usr_data = (timer_usr_data *)arg;
    qcarcam_test_input_t *input_ctxt = (qcarcam_test_input_t *)usr_data->p_data;
    uint32_t buf_idx = usr_data->buf_idx;

    pthread_mutex_lock(&input_ctxt->mutex);
    for (unsigned int stream_idx = 0; stream_idx < input_ctxt->num_stream_instance; stream_idx++)
    {
        if (input_ctxt->buffers_param[stream_idx][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[buf_idx] != QCARCAMTEST_BUFFER_STATE_USR_PROCESS)
        {
            QCARCAM_ERRORMSG("buf (%d) in error state %d", buf_idx, input_ctxt->buffers_param[stream_idx][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[buf_idx]);
        }

        input_ctxt->buffers_param[stream_idx][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[buf_idx] = QCARCAMTEST_BUFFER_STATE_USR_PROCESS_DONE;
    }
    input_ctxt->usr_process_buf_idx.push_back(buf_idx);
    input_ctxt->usr_process_frameCnt--;

    QCARCAM_DBGMSG("buf (%d) usr process done, usr_process_frameCnt %d", buf_idx, input_ctxt->usr_process_frameCnt);

    pthread_mutex_unlock(&input_ctxt->mutex);

}

/**
 * Function to handle routine of fetching, displaying, and releasing frames when one is available
 * @param input_ctxt
 * @return 0 on success, -1 on failure
 */
static int qcarcam_test_handle_new_frame(qcarcam_test_input_t *input_ctxt, uint32_t id)
{
#ifdef ACCESS_BUF_VA
    uint32_t *pbuf = NULL;
    test_util_buf_ptr_t buffer = {};
#endif

    if (qcarcam_test_get_frame(input_ctxt, id))
    {
        /*if we fail to get frame, we silently continue...*/
        return 0;
    }

    if (gCtxt.enablePauseResume && 0 == (input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt % gCtxt.enablePauseResume))
    {
        QCarCamRet_e ret = QCARCAM_RET_OK;

        if (input_ctxt->delay_time)
            qcarcam_test_clear_usr_process_list(input_ctxt);

        //release frame before pause
        qcarcam_test_release_frame(input_ctxt, id);

        pthread_mutex_lock(&input_ctxt->mutex);
        if (input_ctxt->state != QCARCAMTEST_STATE_CLOSED)
        {
            QCARCAM_INFOMSG("pause...");
            ret = qcarcam_input_pause(input_ctxt);
            if (ret == QCARCAM_RET_OK)
            {
                pthread_mutex_unlock(&input_ctxt->mutex);

                usleep(PAUSE_RESUME_USLEEP);

                pthread_mutex_lock(&input_ctxt->mutex);
                if (input_ctxt->state != QCARCAMTEST_STATE_CLOSED)
                {
                    QCARCAM_INFOMSG("resume...");
                    ret = qcarcam_input_resume(input_ctxt);
                }
            }
        }
        pthread_mutex_unlock(&input_ctxt->mutex);

        return ret;
    }
    else if (gCtxt.enableStartStop && 0 == (input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt % gCtxt.enableStartStop))
    {
        QCarCamRet_e ret = QCARCAM_RET_OK;

        if (input_ctxt->delay_time)
            qcarcam_test_clear_usr_process_list(input_ctxt);

        pthread_mutex_lock(&input_ctxt->mutex);
        if (input_ctxt->state != QCARCAMTEST_STATE_CLOSED)
        {
            QCARCAM_INFOMSG("stop...");
            ret = qcarcam_input_stop(input_ctxt);
            if (ret == QCARCAM_RET_OK)
            {
                pthread_mutex_unlock(&input_ctxt->mutex);

                usleep(START_STOP_USLEEP);

                pthread_mutex_lock(&input_ctxt->mutex);
                if (input_ctxt->state != QCARCAMTEST_STATE_CLOSED)
                {
                    QCARCAM_INFOMSG("re-start...");
                    ret = qcarcam_input_start(input_ctxt);
                }
            }
        }
        pthread_mutex_unlock(&input_ctxt->mutex);
        return ret;
    }
    if (input_ctxt->delay_time)
    {
        uint32_t idx = input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].get_frame_buf_idx;
        if (!input_ctxt->buf_timer[idx].ptimer)
        {
            input_ctxt->buf_timer[idx].usr_data.buf_idx = idx;
            input_ctxt->buf_timer[idx].usr_data.p_data = input_ctxt;

            if (OSAL_TimerCreate(FALSE, input_ctxt->delay_time,
                    usr_process_cb,
                    &input_ctxt->buf_timer[idx].usr_data,
                    "usr_process_cb",
                    QCARCAM_THRD_PRIO,
                    &input_ctxt->buf_timer[idx].ptimer))
            {
                QCARCAM_ERRORMSG("CameraCreateTimer failed for buf %d", idx);
                return -1;
            }
        }
        else
        {
            if (OSAL_TimerStart(input_ctxt->buf_timer[idx].ptimer, input_ctxt->delay_time))
            {
                QCARCAM_ERRORMSG("CameraUpdateTimer failed for buf %d", idx);
                return -1;
            }
        }

        pthread_mutex_lock(&input_ctxt->mutex);
        input_ctxt->usr_process_frameCnt++;
        input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[idx] = QCARCAMTEST_BUFFER_STATE_USR_PROCESS;
        pthread_mutex_unlock(&input_ctxt->mutex);

        return 0;
    }

    input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam = input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].get_frame_buf_idx;
#ifdef ACCESS_BUF_VA
    buffer.buf_idx = input_ctxt->buffers_param[id][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam;
    test_util_get_buf_ptr(input_ctxt->qcarcam_window, &buffer);
    pbuf = (uint32*)buffer.p_va[0];
    if (NULL == pbuf)
    {
        QCARCAM_ERRORMSG("NULL Pointer");
    }
    else
    {
        QCARCAM_ERRORMSG("Embedded frame sequence no = %d", pbuf[0]);
    }
#endif
    if (!input_ctxt->skip_post_display)
    {
        qcarcam_test_post_to_display(input_ctxt, id);
    }

    if (qcarcam_test_release_frame(input_ctxt, id))
    {
        return -1;
    }

    return 0;
}

static int inpt_ctxt_usr_process_thread(void *arg)
{
    qcarcam_test_input_t *input_ctxt = (qcarcam_test_input_t *)arg;
    pthread_detach(pthread_self());

    while (!g_aborted)
    {
        pthread_mutex_lock(&input_ctxt->mutex);
        if(!input_ctxt->usr_process_buf_idx.empty() && input_ctxt->state == QCARCAMTEST_STATE_START)
        {
            for (unsigned int idx = 0; idx < input_ctxt->num_stream_instance; idx++)
            {
                input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam = input_ctxt->usr_process_buf_idx.front();
            }
            input_ctxt->usr_process_buf_idx.pop_front();
            pthread_mutex_unlock(&input_ctxt->mutex);

            for (unsigned int idx = 0; idx < input_ctxt->num_stream_instance; idx++)
            {
                if (!input_ctxt->skip_post_display)
                    qcarcam_test_post_to_display(input_ctxt, idx);

                qcarcam_test_release_frame(input_ctxt, idx);
            }
            pthread_mutex_lock(&input_ctxt->mutex);
        }

        pthread_mutex_unlock(&input_ctxt->mutex);
        usleep(USR_PROCESS_THREAD_USLEEP);
    }
    return 0;
}

static int qcarcam_test_handle_input_signal(qcarcam_test_input_t *input_ctxt, QCarCamInputSignal_e signal_type)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;

    switch (signal_type) {
    case QCARCAM_INPUT_SIGNAL_LOST:
        QCARCAM_ERRORMSG("LOST: idx: %d, input handle :0x%lx", input_ctxt->idx, input_ctxt->qcarcam_hndl);

        /*TODO: offload this to other thread to handle restart recovery*/
        pthread_mutex_lock(&input_ctxt->mutex);
        if (input_ctxt->state == QCARCAMTEST_STATE_STOP) {
            QCARCAM_ERRORMSG("Input handle 0x%lx already stop, break", input_ctxt->qcarcam_hndl);
            pthread_mutex_unlock(&input_ctxt->mutex);
            break;
        }

        input_ctxt->signal_lost = 1;
        qcarcam_input_stop(input_ctxt);

        pthread_mutex_unlock(&input_ctxt->mutex);

        break;
    case QCARCAM_INPUT_SIGNAL_VALID:
        QCARCAM_ERRORMSG("VALID: idx: %d, Input handle 0x%lx", input_ctxt->idx, input_ctxt->qcarcam_hndl);

        pthread_mutex_lock(&input_ctxt->mutex);
        if (input_ctxt->state == QCARCAMTEST_STATE_START)
        {
            QCARCAM_ERRORMSG("Input handle 0x%lx already running, break", input_ctxt->qcarcam_hndl);
            pthread_mutex_unlock(&input_ctxt->mutex);
            break;
        }

        ret = qcarcam_input_start(input_ctxt);

        pthread_mutex_unlock(&input_ctxt->mutex);

        break;
    default:
        QCARCAM_ERRORMSG("Unknown Event type: %d", signal_type);
        break;
    }

    return ret;
}

static int qcarcam_test_handle_fatal_error(qcarcam_test_input_t *input_ctxt, boolean recover)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;

    QCARCAM_ERRORMSG("Fatal error: client idx - %d, Input handle 0x%lx", input_ctxt->idx, input_ctxt->qcarcam_hndl);

    pthread_mutex_lock(&input_ctxt->mutex);

    input_ctxt->signal_lost = 1;
    input_ctxt->fatal_err_cnt++;

    if (input_ctxt->state == QCARCAMTEST_STATE_ERROR || input_ctxt->state == QCARCAMTEST_STATE_STOP) {
        QCARCAM_ERRORMSG("Input handle 0x%lx already error state, return", input_ctxt->qcarcam_hndl);
        pthread_mutex_unlock(&input_ctxt->mutex);
        return ret;
    }

    if (recover)
    {
        ret = QCarCamStop(input_ctxt->qcarcam_hndl);
        if (ret == QCARCAM_RET_OK) {
            input_ctxt->state = QCARCAMTEST_STATE_STOP;
            QCARCAM_INFOMSG("Client %d Input handle 0x%lx QCarCamStop successfully", input_ctxt->idx, input_ctxt->qcarcam_hndl);
        }
        else
        {
            input_ctxt->state = QCARCAMTEST_STATE_ERROR;
            QCARCAM_ERRORMSG("Client %d Input handle 0x%lx QCarCamStop failed: %d !", input_ctxt->idx, input_ctxt->qcarcam_hndl, ret);
        }
    }
    else
    {
        input_ctxt->state = QCARCAMTEST_STATE_ERROR;
    }

    pthread_mutex_unlock(&input_ctxt->mutex);

    return ret;
}

static int qcarcam_test_handle_set_event_notification(qcarcam_test_input_t *input_ctxt, QCarCamParamType_e evnt_type)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;

    switch(evnt_type)
    {
    case QCARCAM_SENSOR_PARAM_EXPOSURE:
    case QCARCAM_SENSOR_PARAM_SATURATION:
    case QCARCAM_SENSOR_PARAM_HUE:
    case QCARCAM_STREAM_CONFIG_PARAM_FRAME_DROP_CONTROL:
    case QCARCAM_SENSOR_PARAM_GAMMA:
        QCARCAM_INFOMSG("Input_ctxt is notified for changed setting:%d Input handle 0x%lx client_id:%d",
            evnt_type, input_ctxt->qcarcam_hndl, input_ctxt->idx);
        break;
    case QCARCAM_STREAM_CONFIG_PARAM_MASTER:
        QCARCAM_INFOMSG("Master released Input handle 0x%lx client_id:%d",
            input_ctxt->qcarcam_hndl, input_ctxt->idx);
        break;
    default:
        QCARCAM_ERRORMSG("Input_ctxt is notified with unknown event");
        break;
    }
    return ret;
}

/**
 * Qcarcam event callback function
 * @param hndl
 * @param event_id
 * @param p_payload
 */
QCarCamRet_e qcarcam_test_event_cb(const QCarCamHndl_t hndl,
        const uint32_t eventId,
        const QCarCamEventPayload_t *pPayload,
        void  *pPrivateData)
{
    int result = 0;
    qcarcam_test_input_t *input_ctxt = (qcarcam_test_input_t*)pPrivateData;
    qcarcam_event_msg_t event_msg;

    if (!input_ctxt || hndl != input_ctxt->qcarcam_hndl)
    {
        QCARCAM_ERRORMSG("event_cb called with invalid qcarcam handle 0x%lx", hndl);
        return QCARCAM_RET_FAILED;
    }

    if (g_aborted)
    {
        QCARCAM_ERRORMSG("Test aborted");
        return QCARCAM_RET_OK;
    }

    event_msg.event_id = (uint32_t)eventId;

    memcpy(&event_msg.payload, pPayload, sizeof(QCarCamEventPayload_t));

    pthread_mutex_lock(&input_ctxt->queue_mutex);
    input_ctxt->eventqueue.push(event_msg);
    pthread_mutex_unlock(&input_ctxt->queue_mutex);

    result = CameraSetSignal(input_ctxt->m_eventHandlerSignal);
    if (result)
    {
        QCARCAM_ERRORMSG("Failed to signal event %d (%d)", eventId, result);
    }

    return QCARCAM_RET_OK;
}

static int injection_thread(void *arg)
{
    qcarcam_test_input_t *input_ctxt = (qcarcam_test_input_t *)arg;

    CameraWaitOnSignal(input_ctxt->m_injectionHandlerSignal, CAM_SIGNAL_WAIT_NO_TIMEOUT);
    while (!g_aborted)
    {
        uint32_t sleepTime = (1.0f / input_ctxt->injectionParams.framerate) * 1000000;
        OSAL_SleepMicroSecond(sleepTime);
        if (input_ctxt->is_injection)
        {
            boolean inject = FALSE;

            if (input_ctxt->injectionParams.repeat != -1)
            {
                if (input_ctxt->injectionSettings.n_total_frames > 0)
                {
                    input_ctxt->injectionSettings.n_total_frames--;
                    inject = TRUE;
                }
            }
            else
            {
                inject = TRUE;
            }

            if (inject)
            {
                QCarCamRequest_t  request = {};
                request.inputBufferIdx = input_ctxt->injectionSettings.buf_idx;
                request.requestId = input_ctxt->injectionSettings.request_id;

                //update buf_index and request id for next submission
                input_ctxt->injectionSettings.buf_idx =
                    (input_ctxt->injectionSettings.buf_idx + 1) % input_ctxt->injectionParams.n_buffers;
                input_ctxt->injectionSettings.request_id++;

                request.inputMetaIdx = input_ctxt->input_meta_idx;

                if(QCARCAMTEST_STATE_START == input_ctxt->state)
                {
                    QCarCamRet_e ret;

                    QCARCAM_DBGMSG("SUBMIT_REQUEST: ctx[0x%lx]: bufIdx %d reqId %d",
                            input_ctxt->qcarcam_hndl, request.inputBufferIdx, request.requestId);
                    ret = QCarCamSubmitRequest(input_ctxt->qcarcam_hndl, &request);
                    if (QCARCAM_RET_OK != ret)
                    {
                        QCARCAM_ERRORMSG("QCarCamSubmitRequest ctx[0x%lx]: bufIdx %d reqId %d failed %d",
                                input_ctxt->qcarcam_hndl, request.inputBufferIdx, request.requestId, ret);
                    }
                }
            }
            else
            {
                // Abort test once injection of all frames was done
                // Need to add a sleep of 1 sec because there are two
                // events that could occur simultaneously: buffer ready
                // and vendor params report.
                OSAL_SleepMicroSecond(1000000);

                abort_test();
            }
        }
    }

    return 0;
}

static int qcarcam_test_handle_request_mode(qcarcam_test_input_t *input_ctxt, QCarCamFrameInfo_t *pframeinfo)
{
    int ret = 0;
    QCarCamFrameInfo_t buffer_info = *pframeinfo;
    /* Use the number of buffers per request, to create the index to the request in the 
     * request pool, corresponding to the requestID received */
    int req_idx = buffer_info.requestId % input_ctxt->p_buffers_output[0].nBuffers;

    /* store the incoming frameinfo into its corresponding request in the pool */
    input_ctxt->submitrequestSettings.request_pool[req_idx].request[buffer_info.id].bufferlistId = buffer_info.id;
    input_ctxt->submitrequestSettings.request_pool[req_idx].request[buffer_info.id].bufferId = buffer_info.bufferIndex;
    /* Increment the number of streams available in the corresponding request*/
    input_ctxt->submitrequestSettings.request_pool[req_idx].streams_pending++;

    if (input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt == 0)
    {
        qcarcam_test_get_time(&input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].t_firstFrame);

        if (input_ctxt->is_first_start)
        {
            ais_log_kpi(AIS_EVENT_KPI_CLIENT_FIRST_FRAME);

            printf("Success - First Frame [%d]\n", input_ctxt->idx);
            fflush(stdout);

            QCARCAM_ALWZMSG("[%u] First Frame buf_idx %d after : %llu ms (field type: %d)",
                    input_ctxt->idx, buffer_info.bufferIndex, (input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].t_firstFrame - gCtxt.t_start), buffer_info.fieldType);

            input_ctxt->is_first_start = FALSE;
        }
        else
        {
            QCARCAM_ALWZMSG("[%d:0x%lx] restart took %llu ms",
                    input_ctxt->idx, input_ctxt->qcarcam_hndl, (input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].t_firstFrame - input_ctxt->t_start));
        }
    }

    input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].get_frame_buf_idx = buffer_info.bufferIndex;
    input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].get_frame_buf_idx] = QCARCAMTEST_BUFFER_STATE_GET_FRAME;
    input_ctxt->signal_lost = 0;

    input_ctxt->diag.frame_generate_time[TEST_PREV_BUFFER] = input_ctxt->diag.frame_generate_time[TEST_CUR_BUFFER];
    input_ctxt->diag.frame_generate_time[TEST_CUR_BUFFER] = buffer_info.timestamp * NS_TO_MS;

    QCARCAM_DBGMSG("[%d] framecnt:%d (seqNo %u) bufId:%d qtime:%lu inputID:%u  bufferListId:%u requsetid=%d",
            input_ctxt->idx, input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt, buffer_info.seqNo,
        buffer_info.bufferIndex, buffer_info.sofTimestamp.timestamp, input_ctxt->input_param[buffer_info.id].inputId, buffer_info.id, 
        buffer_info.requestId);

    input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt++;
    input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].releaseframeCnt++;

    input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam = input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].get_frame_buf_idx;
    if (!input_ctxt->skip_post_display)
    {
        qcarcam_test_post_to_display(input_ctxt, buffer_info.id);
    }

    if (gCtxt.disable_display || input_ctxt->window_params[buffer_info.id].is_offscreen)
    {
        /* Re-Submit the request if all the streams are available in the request*/
        if(input_ctxt->num_stream_instance == input_ctxt->submitrequestSettings.request_pool[req_idx].streams_pending)
        {
            QCarCamRequest_t request = {};
            request.requestId = input_ctxt->submitrequestSettings.request_id;
            request.inputMetaIdx = input_ctxt->input_meta_idx;
            request.numStreamRequests = input_ctxt->num_stream_instance;
            /* Populate the details of all the sreams required in the request */
            for(uint32_t stream = 0; stream < request.numStreamRequests; stream++)
            {
                request.streamRequests[stream].bufferlistId = input_ctxt->submitrequestSettings.request_pool[req_idx].request[stream].bufferlistId;
                request.streamRequests[stream].bufferId = input_ctxt->submitrequestSettings.request_pool[req_idx].request[stream].bufferId;
            }

            ret = QCarCamSubmitRequest(input_ctxt->qcarcam_hndl, &request);

            if (QCARCAM_RET_OK != ret)
            {
                QCARCAM_ERRORMSG("QCarCamSubmitRequest req id %d, failed %d", request.requestId, ret);
            }
            input_ctxt->submitrequestSettings.request_id++;
            input_ctxt->submitrequestSettings.request_pool[req_idx].streams_pending = 0;
            input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam] = QCARCAMTEST_BUFFER_STATE_QCARCAM;
        }
    }
    else
    {
        while(!input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].release_buf_idx.empty())
        {
            uint32_t rel_idx = input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].release_buf_idx.front();
            input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].release_buf_idx.pop_front();
            if (rel_idx >= 0 &&
                rel_idx < input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].n_buffers)
            {
                if (QCARCAMTEST_BUFFER_STATE_GET_FRAME == (input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[rel_idx] & 0xF) ||
                    QCARCAMTEST_BUFFER_STATE_POST_DISPLAY == (input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[rel_idx] & 0xF))
                {
                    /* TODO: Properly handle the display enabled scenario
                     * */
                    /* Re-Submit the request if all the streams are available in the request*/
                    if(input_ctxt->num_stream_instance ==input_ctxt->submitrequestSettings.request_pool[req_idx].streams_pending){
                        QCarCamRequest_t request = {};
                        request.requestId = input_ctxt->submitrequestSettings.request_id;
                        request.inputMetaIdx = input_ctxt->input_meta_idx;
                        request.numStreamRequests = input_ctxt->num_stream_instance;
                        /* Populate the details of all the sreams required in the request */
                        for(uint32_t stream = 0; stream < request.numStreamRequests; stream++)
                        {
                            request.streamRequests[stream].bufferlistId = input_ctxt->submitrequestSettings.request_pool[req_idx].request[stream].bufferlistId;
                            request.streamRequests[stream].bufferId = input_ctxt->submitrequestSettings.request_pool[req_idx].request[stream].bufferId;
                        }

                        ret = QCarCamSubmitRequest(input_ctxt->qcarcam_hndl, &request);

                        if (QCARCAM_RET_OK != ret)
                        {
                            QCARCAM_ERRORMSG("QCarCamSubmitRequest req id %d, failed %d", request.requestId, ret);
                        }
                        input_ctxt->submitrequestSettings.request_id++;
                        input_ctxt->submitrequestSettings.request_pool[req_idx].streams_pending = 0;
                        for(uint32_t stream = 0; stream < request.numStreamRequests; stream++)
                        {
                            unsigned int bufferIndex = input_ctxt->submitrequestSettings.request_pool[req_idx].request[stream].bufferlistId;
                            input_ctxt->buffers_param[bufferIndex][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[input_ctxt->buffers_param[bufferIndex][QCARCAM_TEST_BUFFERS_OUTPUT].buf_idx_qcarcam] = QCARCAMTEST_BUFFER_STATE_QCARCAM;
                        }
                    }
                }
                else
                {
                    QCARCAM_ERRORMSG("QCarCamSubmitRequest(%d) skipped since buffer bad state (%d)", rel_idx, input_ctxt->buffers_param[buffer_info.id][QCARCAM_TEST_BUFFERS_OUTPUT].buf_state[rel_idx]);
                }
            }
            else
            {
                QCARCAM_ERRORMSG("QCarCamSubmitRequest(%d) skipped", rel_idx);
            }
        }
    }

    return 0;
}


static int process_cb_event_thread(void *arg)
{
    qcarcam_test_input_t *input_ctxt = (qcarcam_test_input_t *)arg;
    qcarcam_event_msg_t event_msg;

    while (!g_aborted)
    {
        CameraWaitOnSignal(input_ctxt->m_eventHandlerSignal, CAM_SIGNAL_WAIT_NO_TIMEOUT);

        pthread_mutex_lock(&input_ctxt->queue_mutex);
        if (!input_ctxt->eventqueue.empty())
        {
            event_msg = input_ctxt->eventqueue.front();
            input_ctxt->eventqueue.pop();
        }
        else
        {
            QCARCAM_ERRORMSG("event queue is empty");
            pthread_mutex_unlock(&input_ctxt->queue_mutex);
            continue;
        }

        pthread_mutex_unlock(&input_ctxt->queue_mutex);

        switch (event_msg.event_id)
        {
            case QCARCAM_EVENT_FRAME_READY:
            {
                if (input_ctxt->state == QCARCAMTEST_STATE_START)
                {
                    QCARCAM_DBGMSG("%d received QCARCAM_EVENT_FRAME_READY", input_ctxt->idx);

                    if (!input_ctxt->request_mode)
                    {
                        qcarcam_test_handle_new_frame(input_ctxt, event_msg.payload.frameInfo.id);
                    }
                    else //test for request_mode
                    {
                        qcarcam_test_handle_request_mode(input_ctxt, &event_msg.payload.frameInfo);
                    }
                }
                break;
            }
            case QCARCAM_EVENT_INPUT_SIGNAL:
            {
                if (gCtxt.enableBridgeErrorDetect)
                {
                    qcarcam_test_handle_input_signal(input_ctxt, (QCarCamInputSignal_e)event_msg.payload.u32Data);
                }
                break;
            }
            case QCARCAM_EVENT_ERROR:
            {
                switch (event_msg.payload.errInfo.errorId)
                {
                case QCARCAM_ERROR_FATAL:
                    QCARCAM_ERRORMSG("FATAL error %d on id:%d Input handle 0x%lx",
                            event_msg.payload.errInfo.errorCode, input_ctxt->idx, input_ctxt->qcarcam_hndl);
                    qcarcam_test_handle_fatal_error(input_ctxt, gCtxt.enableFatalErrorRecover);
                    break;
                case QCARCAM_ERROR_WARNING:
                    QCARCAM_ERRORMSG("WARNING error %d on id:%d Input handle 0x%lx",
                            event_msg.payload.errInfo.errorCode, input_ctxt->idx, input_ctxt->qcarcam_hndl);
                    break;
                default:
                    break;
                }

                break;
            }
            case QCARCAM_EVENT_VENDOR:
            {
                QCARCAM_INFOMSG("Frame[%d]: receive QCARCAM_EVENT_VENDOR data[0]=%u data[1]=%u",
                input_ctxt->buffers_param[event_msg.payload.frameInfo.id][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt, event_msg.payload.vendorData.data[0], event_msg.payload.vendorData.data[1]);
                if (input_ctxt->is_injection)
                {
                    if (QCARCAM_ISP_USECASE_SHDR_PREPROCESS == input_ctxt->isp_config[0].usecaseId)
                    {
                        QCarCamExposureConfig_t* pHdrExposure =
                            (QCarCamExposureConfig_t*)(event_msg.payload.vendorData.data);
                        QCARCAM_INFOMSG("Frame[%d]: Injection: Set HDR exposure: mode %d exposure_time (%f,%f,%f) gain (%f,%f,%f)",
                            input_ctxt->buffers_param[event_msg.payload.frameInfo.id][QCARCAM_TEST_BUFFERS_OUTPUT].frameCnt, 
                            pHdrExposure->mode, pHdrExposure->exposureTime[0],
                            pHdrExposure->exposureTime[1], pHdrExposure->exposureTime[2],
                            pHdrExposure->gain[0], pHdrExposure->gain[1], pHdrExposure->gain[2]);
                    }
                }
                break;
            }
            case QCARCAM_EVENT_PROPERTY_NOTIFY:
                qcarcam_test_handle_set_event_notification(input_ctxt, (QCarCamParamType_e)event_msg.payload.u32Data);
                break;
            case QCARCAM_EVENT_FRAME_SOF:
                QCARCAM_INFOMSG("received QCARCAM_EVENT_FRAME_SOF for id:%d, qtimestamp:%lu",
                input_ctxt->idx, event_msg.payload.hwTimestamp.timestamp);
                break;
            case QCARCAM_EVENT_RECOVERY:
                QCARCAM_INFOMSG("Client%d QCARCAM_EVENT_ID_RECOVERY", input_ctxt->idx);

                switch (event_msg.payload.recovery.msg)
                {
                case QCARCAM_RECOVERY_STARTED:
                    QCARCAM_INFOMSG("Client%d RECOVERY START", input_ctxt->idx);
                    input_ctxt->state = QCARCAMTEST_STATE_RECOVERY;
                    break;
                case QCARCAM_RECOVERY_SUCCESS:
                    QCARCAM_INFOMSG("Client%d RECOVERY SUCCESS", input_ctxt->idx);
                    input_ctxt->state = QCARCAMTEST_STATE_START;
                    break;
                case QCARCAM_RECOVERY_FAILED:
                    QCARCAM_INFOMSG("Client%d RECOVERY FAILED", input_ctxt->idx);
                    input_ctxt->state = QCARCAMTEST_STATE_ERROR;
                    break;
                default:
                    break;
                }
                break;

            default:
                QCARCAM_ERRORMSG("%d received unsupported event %d", input_ctxt->idx, event_msg.event_id);
                break;
        }
    }

    return 0;
}



/**
 * Qcarcam gpio interrupt callback function for visibility mode
 */
static void qcarcam_test_gpio_interrupt_cb()
{
    int idx;
    unsigned long long t_before = 0;
    unsigned long long t_after = 0;

    gCtxt.vis_value = !gCtxt.vis_value;

    qcarcam_test_get_time(&t_before);

    for (idx = 0; idx < gCtxt.numInputs; ++idx)
    {
        qcarcam_test_input_t *input_ctxt = &gCtxt.inputs[idx];

        for (uint32_t i = 0 ; i < input_ctxt->num_stream_instance; i++)
        {
            if (input_ctxt->qcarcam_window[i] != NULL)
            {
                test_util_set_param(input_ctxt->qcarcam_window[i], TEST_UTIL_VISIBILITY, gCtxt.vis_value);
            }

            if (input_ctxt->display_window[i] != NULL)
            {
                test_util_set_param(input_ctxt->display_window[i], TEST_UTIL_VISIBILITY, gCtxt.vis_value);
            }

            input_ctxt->window_params[i].visibility = gCtxt.vis_value;
        }
    }

    qcarcam_test_get_time(&t_after);
    QCARCAM_PERFMSG(gCtxt.enableStats, "Time for visbility toggle : %llu ms", (t_after - t_before));
}

/**
 * Qcarcam gpio interrupt callback function for play/pause mode
 */
static void qcarcam_test_gpio_play_pause_cb()
{
    QCarCamRet_e ret = QCARCAM_RET_OK;
    int session_id;
    unsigned long long t_before = 0;
    unsigned long long t_after = 0;

    qcarcam_test_get_time(&t_before);

    for (session_id = 0; session_id < gCtxt.numInputs; ++session_id)
    {
        qcarcam_test_input_t *input_ctxt = &gCtxt.inputs[session_id];

        pthread_mutex_lock(&input_ctxt->mutex);

        if (input_ctxt->state == QCARCAMTEST_STATE_PAUSE)
        {
            ret = qcarcam_input_resume(input_ctxt);
        }
        else if (input_ctxt->state == QCARCAMTEST_STATE_START)
        {
            ret = qcarcam_input_pause(input_ctxt);
        }
        else
        {
            QCARCAM_ERRORMSG("bad state %d", input_ctxt->state);
            ret = QCARCAM_RET_BADSTATE;
        }

        pthread_mutex_unlock(&input_ctxt->mutex);

        if (ret)
        {
            QCARCAM_ERRORMSG("failed gpio toggle (%d)", input_ctxt->state);
        }
    }

    qcarcam_test_get_time(&t_after);
    QCARCAM_PERFMSG(gCtxt.enableStats, "Time for play/pause toggle : %llu ms",  (t_after - t_before));
}

/**
 * Qcarcam gpio interrupt callback function for stop/start mode
 */
static void qcarcam_test_gpio_start_stop_cb()
{
    QCarCamRet_e ret = QCARCAM_RET_OK;
    int session_id;
    unsigned long long t_before = 0;
    unsigned long long t_after = 0;

    qcarcam_test_get_time(&t_before);

    for (session_id = 0; session_id < gCtxt.numInputs; ++session_id)
    {
        qcarcam_test_input_t *input_ctxt = &gCtxt.inputs[session_id];

        pthread_mutex_lock(&input_ctxt->mutex);

        if (input_ctxt->state == QCARCAMTEST_STATE_STOP)
        {
            ret = qcarcam_input_start(input_ctxt);
        }
        else if (input_ctxt->state == QCARCAMTEST_STATE_START)
        {
            ret = qcarcam_input_stop(input_ctxt);
        }
        else
        {
            QCARCAM_ERRORMSG("bad state %d", input_ctxt->state);
            ret = QCARCAM_RET_BADSTATE;
        }

        pthread_mutex_unlock(&input_ctxt->mutex);

        if (ret)
        {
            QCARCAM_ERRORMSG("failed gpio toggle (%d)", input_ctxt->state);
        }
    }

    qcarcam_test_get_time(&t_after);
    QCARCAM_PERFMSG(gCtxt.enableStats, "Time for start/stop toggle : %llu ms", (t_after - t_before));
}

/**
 * Thread to setup and run qcarcam based on test input context
 *
 * @note For single threaded operation, this function only sets up qcarcam context.
 *      QCarCamStart and handling of frames is not executed.
 *
 * @param arg qcarcam_test_input_t* input_ctxt
 */
static int qcarcam_test_setup_input_ctxt_thread(void *arg)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;
    qcarcam_test_input_t *input_ctxt = (qcarcam_test_input_t *)arg;
    unsigned long long t_before = 0;
    unsigned long long t_after = 0;

    if (!input_ctxt)
        return -1;

    QCARCAM_INFOMSG("setup_input_ctxt_thread idx = %d, Input handle 0x%lx", input_ctxt->idx, input_ctxt->qcarcam_hndl);

    qcarcam_test_get_time(&t_before);

    QCarCamOpen_t openParams = {};

    openParams.opMode = input_ctxt->op_mode;
    openParams.numInputs = input_ctxt->num_inputs;

    for (unsigned int idx = 0; idx < input_ctxt->num_inputs; idx++)
    {
        openParams.inputs[idx].inputId = input_ctxt->input_param[idx].inputId;
        openParams.inputs[idx].srcId = input_ctxt->input_param[idx].srcId;
        openParams.inputs[idx].inputMode = input_ctxt->input_param[idx].inputMode;
    }

    if (input_ctxt->recovery)
    {
        openParams.flags = QCARCAM_OPEN_FLAGS_RECOVERY;
    }
    else if (input_ctxt->request_mode)
    {
        openParams.flags = QCARCAM_OPEN_FLAGS_REQUEST_MODE;
    }


    ret = QCarCamOpen(&openParams, &input_ctxt->qcarcam_hndl);
    if(ret != QCARCAM_RET_OK || input_ctxt->qcarcam_hndl == QCARCAM_HNDL_INVALID)
    {
        input_ctxt->state = QCARCAMTEST_STATE_ERROR;
        QCARCAM_ERRORMSG("QCarCamOpen() failed");
        goto qcarcam_thread_fail;
    }

    pthread_mutex_lock(&gCtxt.mutex_open_cnt);
    gCtxt.opened_stream_cnt++;
    pthread_mutex_unlock(&gCtxt.mutex_open_cnt);

    input_ctxt->state = QCARCAMTEST_STATE_OPEN;

    qcarcam_test_get_time(&t_after);
    QCARCAM_PERFMSG(gCtxt.enableStats, "QCarCamOpen (idx %u) : %llu ms", input_ctxt->idx, (t_after - t_before));
    t_before = t_after;

    QCARCAM_INFOMSG("render_thread idx = %d, Input handle 0x%lx context=%lx",
            input_ctxt->idx, input_ctxt->qcarcam_hndl, input_ctxt->qcarcam_hndl);
    for (unsigned int idx = 0; idx < input_ctxt->num_stream_instance; idx++)
    {
        input_ctxt->p_buffers_output[idx].id = input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].bufferList_id;
        ret = QCarCamSetBuffers(input_ctxt->qcarcam_hndl, &input_ctxt->p_buffers_output[idx]);
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("QCarCamSetBuffers() failed");
            goto qcarcam_thread_fail;
        }

        if (input_ctxt->request_mode)
        {
            QCARCAM_ALWZMSG(" set meta buffers list 0x%x", input_ctxt->meta_bufferList.id);
            ret = QCarCamSetBuffers(input_ctxt->qcarcam_hndl, &input_ctxt->meta_bufferList);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("QCarCamSetBuffers() input meta failed");
                goto qcarcam_thread_fail;
            }

            QCARCAM_ALWZMSG(" set output meta buffers list 0x%x", input_ctxt->meta_output_bufferList.id);
            ret = QCarCamSetBuffers(input_ctxt->qcarcam_hndl, &input_ctxt->meta_output_bufferList);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("QCarCamSetBuffers() output meta failed");
                goto qcarcam_thread_fail;
            }
        }

        uint32_t param = {};
        ret = QCarCamGetParam(input_ctxt->qcarcam_hndl, QCARCAM_SENSOR_PARAM_COLOR_SPACE, &param, sizeof(param));
        if (input_ctxt->qcarcam_window[idx] != NULL && ret == QCARCAM_RET_OK)
        {
            test_util_set_param(input_ctxt->qcarcam_window[idx], TEST_UTIL_COLOR_SPACE, param);
        }
        else
        {
            QCARCAM_ERRORMSG("Failed to set color space");
        }
    }

    if (input_ctxt->is_injection)
    {
        QCARCAM_ALWZMSG("read from input_file - set input buffers");

        ret = QCarCamSetBuffers(input_ctxt->qcarcam_hndl, &input_ctxt->p_buffers_input);
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("QCarCamSetBuffers() failed");
            goto qcarcam_thread_fail;
        }
    }

    qcarcam_test_get_time(&t_after);
    QCARCAM_PERFMSG(gCtxt.enableStats, "QCarCamSetBuffers (idx %u) : %llu ms", input_ctxt->idx, (t_after - t_before));
    t_before = t_after;

    QCARCAM_INFOMSG("QCarCamSetBuffers done, QCarCamStart ...");

    //event callback config
    if (input_ctxt->use_event_callback)
    {
        uint32_t param = 0;

        ret = QCarCamRegisterEventCallback(input_ctxt->qcarcam_hndl, &qcarcam_test_event_cb, input_ctxt);
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("QCarCamSetParam(QCARCAM_PARAM_EVENT_CB) failed");
            goto qcarcam_thread_fail;
        }

        if (input_ctxt->recovery)
        {
            param = QCARCAM_EVENT_FRAME_READY |
                QCARCAM_EVENT_INPUT_SIGNAL |
                QCARCAM_EVENT_ERROR | QCARCAM_EVENT_VENDOR |
                QCARCAM_EVENT_RECOVERY;
                // | QCARCAM_EVENT_FRAME_FREEZE;
        }
        else
        {
            param = QCARCAM_EVENT_FRAME_READY |
                QCARCAM_EVENT_INPUT_SIGNAL |
                QCARCAM_EVENT_ERROR | QCARCAM_EVENT_VENDOR;
                // | QCARCAM_EVENT_FRAME_FREEZE;
        }

        ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_STREAM_CONFIG_PARAM_EVENT_MASK, &param, sizeof(param));
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("QCarCamSetParam(QCARCAM_STREAM_CONFIG_PARAM_EVENT_MASK) failed");
            goto qcarcam_thread_fail;
        }

        qcarcam_test_get_time(&t_after);
        QCARCAM_PERFMSG(gCtxt.enableStats, "QCarCamSetParam (idx %u) : %llu ms", input_ctxt->idx, (t_after - t_before));
        t_before = t_after;
    }

    //frame drop control
    if (input_ctxt->frameDropConfig.frameDropPeriod != 0)
    {
        QCarCamFrameDropConfig_t* param = &input_ctxt->frameDropConfig;

        ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_STREAM_CONFIG_PARAM_FRAME_DROP_CONTROL, param, sizeof(*param));
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("QCarCamSetParam(QCARCAM_STREAM_CONFIG_PARAM_FRAME_DROP_CONTROL) failed");
            goto qcarcam_thread_fail;
        }

        qcarcam_test_get_time(&t_after);
        QCARCAM_PERFMSG(gCtxt.enableStats, "QCarCamSetParam fps (idx %u): %llu ms", input_ctxt->idx, (t_after - t_before));
        t_before = t_after;

        QCARCAM_INFOMSG("Provided QCARCAM_STREAM_CONFIG_PARAM_FRAME_DROP_CONTROL: period = %d, pattern = 0x%x",
            param->frameDropPeriod,
            param->frameDropPattern);
    }

    //batch mode config
    if (input_ctxt->num_batch_frames > 1)
    {
        QCarCamBatchConfig_t param = {};

        param.numBatchFrames = input_ctxt->num_batch_frames;
        param.detectFirstPhaseTimer = input_ctxt->detect_first_phase_timer;
        param.frameIncrement = input_ctxt->frame_increment;

        ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_STREAM_CONFIG_PARAM_BATCH_MODE, &param, sizeof(param));
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("QCarCamSetParam(QCARCAM_STREAM_CONFIG_PARAM_BATCH_MODE) failed");
            goto qcarcam_thread_fail;
        }

        QCARCAM_INFOMSG("Provided QCARCAM_STREAM_CONFIG_PARAM_BATCH_MODE: batch_frames = %d frame_increment %d",
                        param.numBatchFrames,
                        param.frameIncrement);
    }

    //ISP usecase config
    for (uint32_t idx = 0; idx < input_ctxt->num_isp_instances; idx++)
    {
        ret = QCarCamSetParam(input_ctxt->qcarcam_hndl,
                QCARCAM_STREAM_CONFIG_PARAM_ISP_USECASE,
                &input_ctxt->isp_config[idx],
                sizeof(input_ctxt->isp_config[idx]));
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("QCarCamSetParam(QCARCAM_STREAM_CONFIG_PARAM_ISP_USECASE) failed");
            goto qcarcam_thread_fail;
        }

        qcarcam_test_get_time(&t_after);
        QCARCAM_PERFMSG(gCtxt.enableStats, "QCarCamSetParam usecase (idx %u): %llu ms", input_ctxt->idx, (t_after - t_before));
        t_before = t_after;
    }

    if ((0 != input_ctxt->buffers_param[0][QCARCAM_TEST_BUFFERS_OUTPUT].cropRegion.width) &&
        (0 != input_ctxt->buffers_param[0][QCARCAM_TEST_BUFFERS_OUTPUT].cropRegion.height))
    {
        //TODO crop region per output buffer is not supported now.
        ret = QCarCamSetParam(input_ctxt->qcarcam_hndl,
                QCARCAM_STREAM_CONFIG_PARAM_SET_CROP,
                &input_ctxt->buffers_param[0][QCARCAM_TEST_BUFFERS_OUTPUT].cropRegion,
                sizeof(QCarCamRegion_t));
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("QCarCamSetParam(QCARCAM_STREAM_CONFIG_PARAM_SET_CROP) failed");
            goto qcarcam_thread_fail;
        }
    }

    /*single threaded handles frames outside this function*/
    if (gCtxt.multithreaded)
    {
        pthread_mutex_lock(&input_ctxt->mutex);

        input_ctxt->is_first_start = TRUE;
        ret = qcarcam_input_start(input_ctxt);

        pthread_mutex_unlock(&input_ctxt->mutex);

        if (input_ctxt->is_injection)
        {
            int result = 0;
            result = CameraSetSignal(input_ctxt->m_injectionHandlerSignal);
            if (result)
            {
                QCARCAM_ERRORMSG("Failed to signal injection thread (%d)", result);
            }
        }

        qcarcam_test_get_time(&t_after);
        QCARCAM_PERFMSG(gCtxt.enableStats, "QCarCamStart (idx %u) : %llu ms", input_ctxt->idx, (t_after - t_before));
        t_before = t_after;

        if (input_ctxt->manual_exposure != -1)
        {
            /* Set exposure configuration */
            QCarCamExposureConfig_t param = {};
            param.exposureTime[0] = input_ctxt->exp_time;
            param.gain[0] = input_ctxt->gain;
            param.mode = input_ctxt->manual_exposure ? QCARCAM_EXPOSURE_MANUAL : QCARCAM_EXPOSURE_AUTO;

            if (input_ctxt->manual_exposure)
            {
                QCARCAM_INFOMSG("Provided QCARCAM_PARAM_MANUAL_EXPOSURE : time =%f ms, gain = %f", input_ctxt->exp_time, input_ctxt->gain);
            }
            else
            {
                QCARCAM_INFOMSG("AUTO EXPOSURE MODE");
            }

            QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_SENSOR_PARAM_EXPOSURE, &param, sizeof(param));
        }
        else
        {
            QCARCAM_INFOMSG("Exposure is not configured, use default setting");
        }

        qcarcam_test_get_time(&t_after);
        QCARCAM_PERFMSG(gCtxt.enableStats, "qcarcam setexposure (idx %u) : %llu ms", input_ctxt->idx, (t_after - t_before));
        t_before = t_after;

        if (!input_ctxt->use_event_callback)
        {
            while (!g_aborted)
            {
                for (unsigned int idx = 0; idx < input_ctxt->num_stream_instance; idx++)
                {
                    if (qcarcam_test_handle_new_frame(input_ctxt, input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].bufferList_id))
                        break;
                }
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
    }

    QCARCAM_INFOMSG("exit setup_input_ctxt_thread idx = %d", input_ctxt->idx);
    return 0;

qcarcam_thread_fail:
    if (input_ctxt->qcarcam_hndl)
    {
        QCarCamClose(input_ctxt->qcarcam_hndl);
        input_ctxt->qcarcam_hndl = QCARCAM_HNDL_INVALID;

        input_ctxt->state = QCARCAMTEST_STATE_ERROR;
    }

    return -1;
}


QCarCamRet_e qcarcam_test_power_event_callback(test_util_pm_event_t power_event_id, void* p_usr_ctxt)
{
    QCarCamRet_e rc = QCARCAM_RET_OK;
    int i = 0;
    qcarcam_test_input_t *input_ctxt = NULL;
    qcarcam_test_ctxt_t* p_ctxt = (qcarcam_test_ctxt_t* )p_usr_ctxt;

    switch (power_event_id)
    {
    case TEST_UTIL_PM_SUSPEND:
    {
        for (i = 0; i < p_ctxt->numInputs; ++i)
        {
            input_ctxt = &p_ctxt->inputs[i];
            if (input_ctxt->qcarcam_hndl)
            {
                QCARCAM_ERRORMSG("suspend inputId: %d, Input handle 0x%lx\n", i, input_ctxt->qcarcam_hndl);
                pthread_mutex_lock(&input_ctxt->mutex);
                if (input_ctxt->state == QCARCAMTEST_STATE_START ||
                    input_ctxt->state == QCARCAMTEST_STATE_ERROR)
                {
                    rc = qcarcam_input_stop(input_ctxt);
                    if (rc == QCARCAM_RET_OK)
                    {
                        input_ctxt->state = QCARCAMTEST_STATE_SUSPEND;
                    }
                }
                pthread_mutex_unlock(&input_ctxt->mutex);
            }
        }

        break;
    }
    case TEST_UTIL_PM_RESUME:
    {
        for (i = 0; i < p_ctxt->numInputs; ++i)
        {
            input_ctxt = &p_ctxt->inputs[i];
            if (input_ctxt->state == QCARCAMTEST_STATE_SUSPEND)
            {
                QCARCAM_ERRORMSG("Input handle 0x%lx, idx = %d\n", input_ctxt->qcarcam_hndl, i);
                pthread_mutex_lock(&input_ctxt->mutex);
                rc = qcarcam_input_start(input_ctxt);
                pthread_mutex_unlock(&input_ctxt->mutex);
            }
        }
        break;
    }
    default:
        break;
    }

    return rc;
}


int main(int argc, char **argv)
{
#ifdef POST_PROCESS
    char* pLibName = (char*)"libpost_process_c2d_convert.so";
#endif
#if defined(__INTEGRITY)
    // Set priority of main thread
    // Task needs to be linked to an object defined in corresponding INT file
    Task mainTask = TaskObjectNumber(QCARCAM_OBJ_NUM);
    SetTaskPriority(mainTask, CameraTranslateThreadPriority(QCARCAM_THRD_PRIO), false);
#endif

    initialize_qcarcam_test_ctxt();

    unsigned long long t_before = 0;
    unsigned long long t_after = 0;

    qcarcam_test_get_time(&gCtxt.t_start);

    const char *tok = NULL;
    QCarCamRet_e ret = QCARCAM_RET_OK;

    int rval = EXIT_FAILURE;
    int rc = 0;
    int first_single_buf_input_idx = -1;

    QCXThread_t timer_thread_handle = QCXThreadInvalid;
    QCXThread_t signal_thread_handle = QCXThreadInvalid;
    QCXThread_t fps_thread_handle = QCXThreadInvalid;
    QCXThread_t signal_loss_thread_handle = QCXThreadInvalid;
    QCXThread_t csi_error_thread_handle = QCXThreadInvalid;
    char thread_name[64];
    int bprint_diag = 0;

    test_util_intr_thrd_args_t intr_thrd_args;

    int remote_attach_loop = 0; // no looping for remote process attach

    /* Should be placed before any forked thread */
    sigfillset(&g_sigset);
    for (int i = 0; exceptsigs[i] != -1; i++)
    {
        sigdelset(&g_sigset, exceptsigs[i]);
    }
    pthread_sigmask(SIG_BLOCK, &g_sigset, NULL);

    QCarCamInit_t qcarcam_init = {0};
    qcarcam_init.apiVersion = QCARCAM_VERSION;
    //qcarcam_init.debug_tag = (char *)"qcarcam_test";

    ret = QCarCamInitialize(&qcarcam_init);
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("QCarCamInitialize failed %d", ret);
        exit(-1);
    }

    vendor_tag_ops_t pMainVendorTagOps;
    ret = QCarCamMetadataGetVendorOps(&pMainVendorTagOps);
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("QCarCamMetadataGetVendorOps failed %d", ret);
        exit(-1);
    }

    rc = set_camera_metadata_vendor_ops(&pMainVendorTagOps);
    if (rc != 0)
    {
        QCARCAM_ERRORMSG("set_camera_metadata_vendor_ops failed %d", rc);
        exit(-1);
    }

    test_util_ctxt_t* test_util_ctxt = NULL;
    test_util_ctxt_params_t test_util_ctxt_params = {};

    test_util_set_power_callback(qcarcam_test_power_event_callback, &gCtxt);

    QCARCAM_DBGMSG("Arg parse Begin");

    for (int i = 1; i < argc; i++)
    {
        if (!strncmp(argv[i], "-config=", strlen("-config=")))
        {
            tok = argv[i] + strlen("-config=");
            snprintf(g_filename, sizeof(g_filename), "%s", tok);
        }
        else if (!strncmp(argv[i], "-dumpFrame=", strlen("-dumpFrame=")))
        {
            tok = argv[i] + strlen("-dumpFrame=");
            gCtxt.dumpFrame = atoi(tok);
        }
        else if (!strncmp(argv[i], "-startStop=", strlen("-startStop=")))
        {
            tok = argv[i] + strlen("-startStop=");
            gCtxt.enableStartStop = atoi(tok);
        }
        else if (!strncmp(argv[i], "-pauseResume=", strlen("-pauseResume=")))
        {
            tok = argv[i] + strlen("-pauseResume=");
            gCtxt.enablePauseResume = atoi(tok);
        }
        else if (!strncmp(argv[i], "-noDisplay", strlen("-noDisplay")))
        {
            gCtxt.disable_display = 1;
            test_util_ctxt_params.disable_display = 1;
        }
        else if (!strncmp(argv[i], "-singlethread", strlen("-singlethread")))
        {
            gCtxt.multithreaded = 0;
        }
        else if (!strncmp(argv[i], "-printfps", strlen("-printfps")))
        {
            tok = argv[i] + strlen("-printfps=");
            gCtxt.fps_print_delay = atoi(tok);
        }
        else if (!strncmp(argv[i], "-checkbuf=", strlen("-checkbuf=")))
        {
            tok = argv[i] + strlen("-checkbuf=");
            gCtxt.check_buffer_state = atoi(tok);
        }
        else if (!strncmp(argv[i], "-showStats=", strlen("-showStats=")))
        {
            tok = argv[i] + strlen("-showStats=");
            gCtxt.enableStats = atoi(tok);
        }
        else if (!strncmp(argv[i], "-c2d=", strlen("-c2d=")))
        {
            tok = argv[i] + strlen("-c2d=");
            gCtxt.enable_c2d = atoi(tok);
            test_util_ctxt_params.enable_c2d = gCtxt.enable_c2d;
        }
        else if (!strncmp(argv[i], "-cache", strlen("-cache")))
        {
            gCtxt.enable_cache = 1;
        }
#ifdef ENABLE_CL_CONVERTER
        else if (!strncmp(argv[i], "-csc=", strlen("-csc=")))
        {
            tok = argv[i] + strlen("-csc=");
            gCtxt.enable_csc = atoi(tok);
            test_util_ctxt_params.enable_csc = gCtxt.enable_csc;
        }
#endif
        else if (!strncmp(argv[i], "-di=", strlen("-di=")))
        {
            tok = argv[i] + strlen("-di=");
            gCtxt.enable_deinterlace = (testutil_deinterlace_t)atoi(tok);
            test_util_ctxt_params.enable_di = gCtxt.enable_deinterlace;
        }
        else if (!strncmp(argv[i], "-nonInteractive", strlen("-nonInteractive")) ||
                !strncmp(argv[i], "-nomenu", strlen("-nomenu")))
        {
            gCtxt.enableMenuMode = 0;
        }
        else if (!strncmp(argv[i], "-fatalErrorRecover", strlen("-fatalErrorRecover")))
        {
            /* only one of them enabled in real scenario */
            gCtxt.enableBridgeErrorDetect = 0;
            gCtxt.enableFatalErrorRecover = 1;
        }
        else if (!strncmp(argv[i], "-checkdualcsi", strlen("-checkdualcsi")))
        {
            gCtxt.checkDualCsi = 1;
        }
        else if (!strncmp(argv[i], "-printlatency", strlen("-printlatency")))
        {
            bprint_diag = 1;
        }
        else if (!strncmp(argv[i], "-seconds=", strlen("-seconds=")))
        {
            tok = argv[i] + strlen("-seconds=");
            gCtxt.exitSeconds = atoi(tok);
        }
        else if (!strncmp(argv[i], "-retry", strlen("-retry")))
        {
            /* Used to wait for query inputs to handoff early RVC */
            gCtxt.enableRetry = 1;
        }
        else if (!strncmp(argv[i], "-gpioNumber=", strlen("-gpioNumber=")))
        {
            tok = argv[i]+ strlen("-gpioNumber=");
            gCtxt.gpioNumber = atoi(tok);
            test_util_ctxt_params.enable_gpio_config = 1;
        }
        else if (!strncmp(argv[i], "-gpioMode=", strlen("-gpioMode=")))
        {
            /**
            *   Mode 0 : Visiblity toggle
            *   Mode 1 : Play/Pause toggle
            *   Mode 2 : Start/Stop toggle
            */
            tok = argv[i] + strlen("-gpioMode=");
            gCtxt.gpioMode = atoi(tok);
        }
        else if (!strncmp(argv[i], "-allocator=", strlen("-allocator=")))
        {
            tok = argv[i] + strlen("-allocator=");
            test_util_ctxt_params.offscreen_allocator = atoi(tok);
        }
#ifdef POST_PROCESS
        else if (!strncmp(argv[i], "-post_process=", strlen("-post_process=")))
        {
            tok = argv[i] + strlen("-post_process=");
            gCtxt.enable_post_processing = atoi(tok);
            test_util_ctxt_params.enable_post_processing = gCtxt.enable_post_processing;
            QCARCAM_ERRORMSG("enabled post processing");
        }

        else if (strncmp(argv[i], "-post_process_lib_name=", strlen("-post_process_lib_name=")) == 0) {
            pLibName = argv[i] + strlen("-post_process_lib_name=");
        }
#endif
        else if (!strncmp(argv[i], "-hwTimestampBasedFps", strlen("-hwTimestampBasedFps")))
        {
            gCtxt.bEnableHWTimeStampBasedFps = TRUE;
        }
        else
        {
            QCARCAM_ERRORMSG("Invalid argument, argv[%d]=%s", i, argv[i]);
            exit(-1);
        }
    }

    QCARCAM_DBGMSG("Arg parse End");

    qcarcam_test_get_time(&t_before);

    QCARCAM_DBGMSG("test_util_init");
    ret = test_util_init(&test_util_ctxt, &test_util_ctxt_params);
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("test_util_init failed");
        exit(-1);
    }

    qcarcam_test_get_time(&t_after);
    QCARCAM_PERFMSG(gCtxt.enableStats, "test_util_init : %llu ms", (t_after - t_before));
    t_before = t_after;

    /*parse xml*/
    test_util_xml_input_t *xml_inputs = (test_util_xml_input_t *)calloc(NUM_MAX_CAMERAS, sizeof(test_util_xml_input_t));
    if (!xml_inputs)
    {
        QCARCAM_ERRORMSG("Failed to allocate xml input struct");
        exit(-1);
    }

    /*If exposure is not set in XML file, set manual_exp to -1 to use default exposure setting*/
    for (int i = 0; i < NUM_MAX_CAMERAS; i++)
    {
        xml_inputs[i].exp_params.manual_exp = -1;
    }

    gCtxt.numInputs = test_util_parse_xml_config_file_v2(g_filename, xml_inputs, NUM_MAX_CAMERAS, &g_xml_cfg);
    if (gCtxt.numInputs <= 0)
    {
        QCARCAM_ERRORMSG("Failed to parse config file!");
        exit(-1);
    }

    qcarcam_test_get_time(&t_after);
    QCARCAM_PERFMSG(gCtxt.enableStats, "xml parsing : %llu ms", (t_after - t_before));
    t_before = t_after;


    /*query qcarcam*/
    QCarCamInput_t *pInputs;
    unsigned int queryNumInputs = 0, queryFilled = 0;
    /*retry used for early camera handoff for RVC*/
    do {
        ret = QCarCamQueryInputs(NULL, 0, &queryNumInputs);
        if (QCARCAM_RET_OK != ret || queryNumInputs == 0)
        {
            QCARCAM_ERRORMSG("Failed QCarCamQueryInputs number of inputs with ret %d", ret);
            if (gCtxt.enableRetry == 0)
            {
                exit(-1);
            }
        }
        else
        {
            break;
        }
        if (gCtxt.enableRetry == 1)
        {
            usleep(500000);
        }
    } while (gCtxt.enableRetry == 1);

    pInputs = (QCarCamInput_t *)calloc(queryNumInputs, sizeof(*pInputs));
    if (!pInputs)
    {
        QCARCAM_ERRORMSG("Failed to allocate pInputs");
        exit(-1);
    }

    ret = QCarCamQueryInputs(pInputs, queryNumInputs, &queryFilled);
    if (QCARCAM_RET_OK != ret || queryFilled != queryNumInputs)
    {
        QCARCAM_ERRORMSG("Failed QCarCamQueryInputs with ret %d %d %d", ret, queryFilled, queryNumInputs);
        exit(-1);
    }

    printf("--- QCarCam Queried Inputs ----\n");
    for (unsigned int inputIdx = 0; inputIdx < queryFilled; inputIdx++)
    {
        pInputs[inputIdx].numModes = 4;
        QCarCamMode_t *pModes = (QCarCamMode_t *)calloc(pInputs[inputIdx].numModes, sizeof(QCarCamMode_t));
        QCarCamInputModes_t *pQueryModes = &gCtxt.queryInputs[inputIdx].modesDesc;

        gCtxt.queryInputs[inputIdx].pInputDesc = &pInputs[inputIdx];
        pQueryModes->numModes = pInputs[inputIdx].numModes;
        pQueryModes->pModes = pModes;

        ret = QCarCamQueryInputModes(pInputs[inputIdx].inputId, pQueryModes);
        if (QCARCAM_RET_OK != ret)
        {
            QCARCAM_ERRORMSG("QCarCamQueryInputModes() failed %d", ret);
            continue;
        }

        printf("%d: input_id=%d, numModes=%d name=%s flags=0x%x currMode=0x%x\n",
                inputIdx, pInputs[inputIdx].inputId,
                pInputs[inputIdx].numModes, pInputs[inputIdx].inputName, pInputs[inputIdx].flags,
                pQueryModes->currentMode);

        for (unsigned int modeIdx = 0; modeIdx < pInputs[inputIdx].numModes; modeIdx++)
        {
            pModes[modeIdx].numSources = 1;
            printf("\t { modeId=%d  numSources=%d\n", modeIdx, pModes[modeIdx].numSources);

            for (unsigned int srcIdx = 0; srcIdx < pModes[modeIdx].numSources; srcIdx++)
            {
                printf("\t\t { srcId %d | %dx%d fmt=0x%08x fps=%.2f }\n",
                        pModes[modeIdx].sources[srcIdx].srcId,
                        pModes[modeIdx].sources[srcIdx].width, pModes[modeIdx].sources[srcIdx].height,
                        pModes[modeIdx].sources[srcIdx].colorFmt,
                        pModes[modeIdx].sources[srcIdx].fps);
            }
            printf("\t }\n");
        }
    }

    qcarcam_test_get_time(&t_after);
    QCARCAM_PERFMSG(gCtxt.enableStats, "query inputs : %llu ms", (t_after - t_before));
    printf("-------------------------------\n");

    while (remote_attach_loop)
    {
        volatile int i;
        for (i = 0; i < 1000; i++)
            usleep(100000);
    }

#ifndef C2D_DISABLED
    if (gCtxt.enable_c2d || gCtxt.enable_csc)
    {
        QCARCAM_DBGMSG("C2D Setup Begin");

        /*init C2D*/
        C2D_DRIVER_SETUP_INFO set_driver_op = {
            .max_surface_template_needed = (NUM_MAX_DISP_BUFS + QCARCAM_MAX_NUM_BUFFERS) * NUM_MAX_CAMERAS,
        };
        C2D_STATUS c2d_status = c2dDriverInit(&set_driver_op);
        if (c2d_status != C2D_STATUS_OK)
        {
            QCARCAM_ERRORMSG("c2dDriverInit failed");
            goto fail;
        }
    }
#endif

    /*signal handler thread*/
    snprintf(thread_name, sizeof(thread_name), "signal_thrd");
    rc = OSAL_ThreadCreate(QCARCAM_THRD_PRIO, &signal_thread, 0, 0, thread_name, &signal_thread_handle);
    if (rc)
    {
        QCARCAM_ERRORMSG("OSAL_ThreadCreate failed : %s", thread_name);
        goto fail;
    }

#ifdef __QNXNTO__
    snprintf(thread_name, sizeof(thread_name), "signal_loss_thrd");
    rc = OSAL_ThreadCreate(QCARCAM_THRD_PRIO, &check_signal_loss_thread, 0, 0, thread_name, &signal_loss_thread_handle);
    if (rc)
    {
        QCARCAM_ERRORMSG("OSAL_ThreadCreate failed : %s", thread_name);
        goto fail;
    }
#endif

    if (gCtxt.enableFatalErrorRecover)
    {
        snprintf(thread_name, sizeof(thread_name), "check_error_thread");
        rc = OSAL_ThreadCreate(QCARCAM_THRD_PRIO, &check_error_thread, 0, 0, thread_name, &csi_error_thread_handle);
        if (rc)
        {
            QCARCAM_ERRORMSG("OSAL_ThreadCreate failed : %s", thread_name);
            goto fail;
        }
    }

    /*if GPIO interrupts have been enabled*/
    if (gCtxt.gpioNumber > 0)
    {
        uint32_t irq;
        test_util_trigger_type_t trigger;

        switch (gCtxt.gpioMode)
        {
        case TESTUTIL_GPIO_MODE_VISIBILITY:
            // gpioMode 0: visibility toggle
            QCARCAM_INFOMSG("Selected GPIO Mode: Visibility toggle");
            trigger = TESTUTIL_GPIO_INTERRUPT_TRIGGER_EDGE;
            intr_thrd_args.cb_func = qcarcam_test_gpio_interrupt_cb;
            break;
        case TESTUTIL_GPIO_MODE_PLAYPAUSE:
            // gpioMode 1: play/pause toggle
            QCARCAM_INFOMSG("Selected GPIO Mode: Play/Pause toggle");
            trigger = TESTUTIL_GPIO_INTERRUPT_TRIGGER_FALLING;
            intr_thrd_args.cb_func = qcarcam_test_gpio_play_pause_cb;
            break;
        case TESTUTIL_GPIO_MODE_STARTSTOP:
            // gpioMode 2: start/stop toggle
            QCARCAM_INFOMSG("Selected GPIO Mode: Start/Stop toggle");
            trigger = TESTUTIL_GPIO_INTERRUPT_TRIGGER_FALLING;
            intr_thrd_args.cb_func = qcarcam_test_gpio_start_stop_cb;
            break;
        default:
            QCARCAM_ERRORMSG("Invalid mode");
            goto fail;
        }

        if (test_util_gpio_interrupt_config(&irq, gCtxt.gpioNumber, trigger) != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("test_util_gpio_interrupt_config failed");
            goto fail;
        }

        intr_thrd_args.irq = irq;

        if (test_util_interrupt_attach(&intr_thrd_args) != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("test_util_interrupt_attach failed");
            goto fail;
        }
    }

    /*thread used frame rate measurement*/
    if (gCtxt.fps_print_delay)
    {
        snprintf(thread_name, sizeof(thread_name), "framerate_thrd");
        rc = OSAL_ThreadCreate(QCARCAM_THRD_PRIO, &framerate_thread, 0, 0, thread_name, &fps_thread_handle);
        if (rc)
        {
            QCARCAM_ERRORMSG("OSAL_ThreadCreate failed : %s", thread_name);
            goto fail;
        }
    }

    for (int input_idx = 0; input_idx < gCtxt.numInputs; input_idx++)
    {
        qcarcam_test_input_t* input_ctxt = &gCtxt.inputs[input_idx];
        test_util_xml_input_t* p_xml_input = &xml_inputs[input_idx];
        QCarCamInputModes_t *pQueryModes = NULL;

        pthread_mutex_init(&input_ctxt->mutex, NULL);
        pthread_mutex_init(&input_ctxt->queue_mutex, NULL);
        input_ctxt->sessionId = input_idx;
        input_ctxt->state = QCARCAMTEST_STATE_INIT;

        input_ctxt->op_mode = p_xml_input->properties.op_mode;
        input_ctxt->use_event_callback = p_xml_input->properties.use_event_callback;
        input_ctxt->subscribe_parameter_change = p_xml_input->properties.subscribe_parameter_change;
        input_ctxt->delay_time = p_xml_input->properties.delay_time;
        input_ctxt->recovery = p_xml_input->properties.recovery;
        input_ctxt->request_mode = p_xml_input->properties.request_mode;
        input_ctxt->num_inputs = p_xml_input->properties.num_inputs;

        for (unsigned int idx = 0; idx < input_ctxt->num_inputs; idx++)
        {
            input_ctxt->input_param[idx].inputId = p_xml_input->properties.input_param[idx].inputId;
            input_ctxt->input_param[idx].srcId = p_xml_input->properties.input_param[idx].srcId;
            input_ctxt->input_param[idx].inputMode = p_xml_input->properties.input_param[idx].inputMode;
        }

        /*no timeout when using event callback as frame is ready as soon as we get callback*/
        if (input_ctxt->use_event_callback)
        {
            input_ctxt->frame_timeout = 0;
        }
        else
        {
            input_ctxt->frame_timeout = (p_xml_input->properties.frame_timeout == -1) ?
                QCARCAM_TEST_DEFAULT_GET_FRAME_TIMEOUT :
                p_xml_input->properties.frame_timeout * 1000 * 1000;
        }

        //Find input in list of queried inputs
        for (unsigned int idx = 0; idx < input_ctxt->num_inputs; idx++)
        {
            for (unsigned int i = 0; i < queryFilled; i++)
            {
                if (pInputs[i].inputId == input_ctxt->input_param[idx].inputId)
                {
                    input_ctxt->query_inputs_idx = i;
                }
            }
            if (queryFilled == input_ctxt->query_inputs_idx)
            {
                QCARCAM_ERRORMSG("Couldn't find inputId %d in queried input list...", input_ctxt->input_param[idx].inputId);
                goto fail;
            }
        }
        pQueryModes = &gCtxt.queryInputs[input_ctxt->query_inputs_idx].modesDesc;

        input_ctxt->num_isp_instances = p_xml_input->isp_params.num_isp_instances;
        for (unsigned int i = 0; i < input_ctxt->num_isp_instances; i++)
        {
            input_ctxt->isp_config[i].id =  p_xml_input->isp_params.isp_config[i].id;
            input_ctxt->isp_config[i].cameraId = p_xml_input->isp_params.isp_config[i].cameraId;
            input_ctxt->isp_config[i].usecaseId = p_xml_input->isp_params.isp_config[i].usecaseId;
        }

        //Set output buffer params
        input_ctxt->num_stream_instance = p_xml_input->stream_params.num_buffer_instances;
        for (unsigned int idx = 0; idx < input_ctxt->num_stream_instance; idx++)
        {        
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].bufferList_id = p_xml_input->stream_params.buffer_id[idx];
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].n_buffers = p_xml_input->stream_params.output_params[idx].n_buffers;
            if (p_xml_input->stream_params.output_params[idx].width == -1 || p_xml_input->stream_params.output_params[idx].height == -1)
            {
                input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].width = pQueryModes->pModes[0].sources[0].width;
                input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].height = pQueryModes->pModes[0].sources[0].height;
            }
            else
            {
                input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].width = p_xml_input->stream_params.output_params[idx].width;
                input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].height = p_xml_input->stream_params.output_params[idx].height;
            }

            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].cropRegion.x = p_xml_input->stream_params.output_params[idx].cropRegion.x;
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].cropRegion.y = p_xml_input->stream_params.output_params[idx].cropRegion.y;
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].cropRegion.width = p_xml_input->stream_params.output_params[idx].cropRegion.width;
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].cropRegion.height = p_xml_input->stream_params.output_params[idx].cropRegion.height;

            if (p_xml_input->stream_params.output_params[idx].format == -1)
            {
                input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].format =
                        pQueryModes->pModes[0].sources[0].colorFmt;
            }
            else
            {
                input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].format =
                    p_xml_input->stream_params.output_params[idx].format; 
            }

            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].flags = p_xml_input->stream_params.output_params[idx].buffer_flags;

            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_DISPLAY].n_buffers = p_xml_input->stream_params.window_params[idx].n_buffers_display;
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_DISPLAY].width =  p_xml_input->stream_params.output_params[idx].width;
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_DISPLAY].height =  p_xml_input->stream_params.output_params[idx].height;
            input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_DISPLAY].format = QCARCAM_FMT_RGB_888;

            input_ctxt->window_params[idx] = p_xml_input->stream_params.window_params[idx];
            input_ctxt->window_params[idx].buffer_size[0] = input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].width;
            input_ctxt->window_params[idx].buffer_size[1] = input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].height;
            input_ctxt->window_params[idx].visibility = 1;
            //set window debug name to XML input index
            snprintf(input_ctxt->window_params[idx].debug_name, sizeof(input_ctxt->window_params[idx].debug_name), "qcarcam_%d", input_idx);

            input_ctxt->window_params[idx].flags = pInputs[input_ctxt->query_inputs_idx].flags;
            input_ctxt->window_params[idx].is_imported_buffer = 0;

            //Capture exposure params if any
            input_ctxt->exp_time = p_xml_input->exp_params.exposure_time;
            input_ctxt->gain = p_xml_input->exp_params.gain;
            input_ctxt->manual_exposure = p_xml_input->exp_params.manual_exp;

            //Capture frame rate params if any
            input_ctxt->frameDropConfig = p_xml_input->stream_params.output_params[idx].frame_rate_config;
            input_ctxt->num_batch_frames = p_xml_input->stream_params.output_params[idx].num_batch_frames;
            input_ctxt->detect_first_phase_timer = p_xml_input->stream_params.output_params[idx].detect_first_phase_timer;
            input_ctxt->frame_increment = p_xml_input->stream_params.output_params[idx].frame_increment;

            if (gCtxt.enable_deinterlace && (gCtxt.enable_deinterlace == TESTUTIL_DEINTERLACE_SW_WEAVE))
            {
                if (gCtxt.enable_c2d || gCtxt.enable_csc)
                {
                    QCARCAM_ERRORMSG("c2d & deinterlace can't be used at the same time");
                    goto fail;
                }

                input_ctxt->window_params[idx].buffer_size[1] = DEINTERLACE_FIELD_HEIGHT * 2;
                input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_DISPLAY].height = DEINTERLACE_FIELD_HEIGHT * 2;
            }

            if (p_xml_input->inject_params.filename[0] != '\0')
            {
                size_t size;

                // Check validity of injection parameters
                if ((0 == p_xml_input->inject_params.n_buffers) ||
                    (0 == p_xml_input->inject_params.n_frames) ||
                    (0 == p_xml_input->inject_params.framerate))
                {
                    QCARCAM_ERRORMSG("Invalid injection params: n_buffers %d n_frames %d framerate %d",
                        p_xml_input->inject_params.n_buffers, p_xml_input->inject_params.n_frames,
                        p_xml_input->inject_params.framerate);
                    goto fail;
                }

                // Number of buffers should be greater than or equal to number of frames
                if (p_xml_input->inject_params.n_frames > p_xml_input->inject_params.n_buffers)
                {
                    QCARCAM_ERRORMSG("Invalid injection params: n_frames %d > n_buffers %d",
                        p_xml_input->inject_params.n_frames, p_xml_input->inject_params.n_buffers);
                    goto fail;
                }

                input_ctxt->injectionParams = p_xml_input->inject_params;
                input_ctxt->injectionSettings.buf_idx = 0;
                input_ctxt->injectionSettings.request_id = 1;

                // Calculate total number of frames to inject
                // when no continuous injection is needed
                if (input_ctxt->injectionParams.repeat != -1)
                {
                    input_ctxt->injectionSettings.n_total_frames =
                        input_ctxt->injectionParams.n_frames * input_ctxt->injectionParams.repeat;
                    input_ctxt->injectionSettings.n_dump_frames = input_ctxt->injectionParams.n_frames;
                }

                // Populate input buffer list parameters for injection
                //@todo change bufferlist id to QCARCAM_BUFFERLIST_ID_INPUT_0 once support is changed;
                //      for now it is expecting id=1 for injection
                input_ctxt->p_buffers_input.id          = 1;
                input_ctxt->p_buffers_input.flags       = QCARCAM_BUFFER_FLAG_OS_HNDL;
                input_ctxt->p_buffers_input.nBuffers    = p_xml_input->inject_params.n_buffers;
                input_ctxt->p_buffers_input.colorFmt    = (QCarCamColorFmt_e)QCARCAM_COLOR_FMT(
                                                                    p_xml_input->inject_params.pattern,
                                                                    p_xml_input->inject_params.bitdepth,
                                                                    p_xml_input->inject_params.pack);
                input_ctxt->p_buffers_input.pBuffers    = (QCarCamBuffer_t*)calloc(
                                                                    input_ctxt->p_buffers_input.nBuffers,
                                                                    sizeof(QCarCamBuffer_t));
                if (input_ctxt->p_buffers_input.pBuffers == 0)
                {
                    QCARCAM_ERRORMSG("Alloc qcarcam_buffer input buffers failed");
                    goto fail;
                }
                input_ctxt->p_buffers_input.pBuffers[0].numPlanes = 1;
                input_ctxt->p_buffers_input.pBuffers[0].planes[0].width = p_xml_input->inject_params.buffer_size[0];
                input_ctxt->p_buffers_input.pBuffers[0].planes[0].height = p_xml_input->inject_params.buffer_size[1];
                input_ctxt->p_buffers_input.pBuffers[0].planes[0].stride = p_xml_input->inject_params.stride;

                if ((0 == p_xml_input->inject_params.singlebuf) ||
                    (p_xml_input->inject_params.singlebuf && (-1 == first_single_buf_input_idx)))
                {
                    if (p_xml_input->inject_params.singlebuf)
                    {
                        first_single_buf_input_idx = input_idx;
                    }

                    ret = test_util_init_window(test_util_ctxt, &input_ctxt->injection_window);
                    if (ret != QCARCAM_RET_OK)
                    {
                        QCARCAM_ERRORMSG("test_util_init_window failed for injection_window");
                        goto fail;
                    }
                    
                    size = input_ctxt->p_buffers_input.pBuffers[0].planes[0].stride *
                        input_ctxt->p_buffers_input.pBuffers[0].planes[0].height;
                    ret = test_util_allocate_input_buffers(input_ctxt->injection_window,
                                                        &input_ctxt->p_buffers_input,
                                                        size);
                    if (ret != QCARCAM_RET_OK)
                    {
                        QCARCAM_ERRORMSG("test_util_alloc_input_buffers failed");
                        goto fail;
                    }
                    
                    ret = test_util_read_input_data(input_ctxt->injection_window,
                                                    p_xml_input->inject_params.filename,
                                                    p_xml_input->inject_params.n_frames,
                                                    size);
                    if (ret != QCARCAM_RET_OK)
                    {
                        QCARCAM_ERRORMSG("test_util_read_input_data failed");
                        goto fail;
                    }
                }
                else
                {
                    qcarcam_test_input_t* input0_ctxt = &gCtxt.inputs[first_single_buf_input_idx];
                    input_ctxt->p_buffers_input = input0_ctxt->p_buffers_input;
                }

                input_ctxt->is_injection = 1;
            }
        }

        //input metadata
        if (input_ctxt->request_mode)
        {
            QCARCAM_ALWZMSG("use initial %u meta data tags", p_xml_input->isp_params.num_tags);

            ret = test_util_init_window(test_util_ctxt, &input_ctxt->metabuffer_window);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("test_util_init_window failed for metabuffer_window");
                goto fail;
            }

            input_ctxt->meta_bufferList.id          = QCARCAM_BUFFERLIST_ID_INPUT_METADATA;
            input_ctxt->meta_bufferList.flags       = QCARCAM_BUFFER_FLAG_OS_HNDL;
            input_ctxt->meta_bufferList.nBuffers    = 3; // TODO update this number p_xml_input->output_params.n_buffers;
            input_ctxt->meta_bufferList.colorFmt    = QCARCAM_FMT_MAX;
            input_ctxt->meta_bufferList.pBuffers    = (QCarCamBuffer_t*)calloc(
                                                                input_ctxt->meta_bufferList.nBuffers,
                                                                sizeof(QCarCamBuffer_t));
            ret = test_util_allocate_metadata_buffers(input_ctxt->metabuffer_window,
                                                       &input_ctxt->meta_bufferList,
                                                       MAX_METADATA_TAG_NUM,
                                                       MAX_METADATA_TAG_DATA);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("test_util_allocate_metadata_buffers failed");
                goto fail;
            }

            test_util_initialize_metadata_buffers(input_ctxt->metabuffer_window,
                                                p_xml_input->isp_params.metadata_tags,
                                                p_xml_input->isp_params.num_tags);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("test_util_allocate_metadata_buffers failed");
                goto fail;
            }
        }

        // output metadata
        if (input_ctxt->request_mode)
        {
            QCARCAM_ALWZMSG("alloc output metadata buffers");
            ret = test_util_init_window(test_util_ctxt, &input_ctxt->metabuffer_window_output);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("test_util_init_window failed for metabuffer_window_output");
                goto fail;
            }
            input_ctxt->meta_output_bufferList.id          = QCARCAM_BUFFERLIST_ID_OUTPUT_METADATA;
            input_ctxt->meta_output_bufferList.flags       = QCARCAM_BUFFER_FLAG_OS_HNDL;
            input_ctxt->meta_output_bufferList.nBuffers    = 3; // TODO update this number p_xml_input->output_params.n_buffers;
            input_ctxt->meta_output_bufferList.colorFmt    = QCARCAM_FMT_MAX;
            input_ctxt->meta_output_bufferList.pBuffers    = (QCarCamBuffer_t*)calloc(
                                                                input_ctxt->meta_output_bufferList.nBuffers,
                                                                sizeof(QCarCamBuffer_t));
            ret = test_util_allocate_metadata_buffers(input_ctxt->metabuffer_window_output,
                                                       &input_ctxt->meta_output_bufferList,
                                                       MAX_METADATA_TAG_NUM,
                                                       MAX_METADATA_TAG_DATA);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("test_util_allocate_metadata_buffers failed");
                goto fail;
            }
        }
    }

    QCARCAM_DBGMSG("Buffer Setup Begin");

    for (int input_idx = 0; input_idx < gCtxt.numInputs; input_idx++)
    {
        qcarcam_test_input_t *input_ctxt = &gCtxt.inputs[input_idx];
        input_ctxt->test_util_ctxt = test_util_ctxt;
        unsigned int output_n_buffers = 0;

        for (unsigned int idx = 0; idx < input_ctxt->num_stream_instance; idx++)
        {
            // Allocate an additional buffer to be shown in case of signal loss
            output_n_buffers = input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].n_buffers + 1;
            input_ctxt->p_buffers_output[idx].nBuffers = output_n_buffers;

            input_ctxt->p_buffers_output[idx].colorFmt = input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].format;
            input_ctxt->p_buffers_output[idx].flags = input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].flags;
            input_ctxt->p_buffers_output[idx].pBuffers = (QCarCamBuffer_t *)calloc(input_ctxt->p_buffers_output[idx].nBuffers,
                                                                            sizeof(QCarCamBuffer_t));
            input_ctxt->diag.bprint = bprint_diag;
            if (!input_ctxt->p_buffers_output[idx].pBuffers)
            {
                QCARCAM_ERRORMSG("alloc qcarcam_buffer failed");
                goto fail;
            }

            qcarcam_test_get_time(&t_before);
#if 0 //@todo: secure buffers
            if (input_ctxt->window_params.flags & QCARCAM_INPUT_FLAG_CONTENT_PROTECTED)
            {
                input_ctxt->p_buffers_output[idx].flags = QCARCAM_BUFFER_FLAG_SECURE;
            }
#endif
            for (unsigned int i = 0; i < output_n_buffers; ++i)
            {
                input_ctxt->p_buffers_output[idx].pBuffers[i].numPlanes = 1;
                input_ctxt->p_buffers_output[idx].pBuffers[i].planes[0].width =
                    input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].width;
                input_ctxt->p_buffers_output[idx].pBuffers[i].planes[0].height =
                    input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].height *
                    input_ctxt->num_batch_frames;
            }

            ret = test_util_init_window(test_util_ctxt, &input_ctxt->qcarcam_window[idx]);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("test_util_init_window failed for qcarcam_window");
                goto fail;
            }

            test_util_set_diag(test_util_ctxt, input_ctxt->qcarcam_window[idx], &input_ctxt->diag);

            // set buffer to cacheable and offscreen
            if (gCtxt.enable_cache)
            {
                input_ctxt->window_params[idx].is_offscreen = 1;
                input_ctxt->p_buffers_output[idx].flags = QCARCAM_BUFFER_FLAG_CACHE;
            }

            if (gCtxt.enable_deinterlace && (gCtxt.enable_deinterlace == TESTUTIL_DEINTERLACE_SW_WEAVE))
            {
                ret = test_util_init_window(test_util_ctxt, &input_ctxt->display_window[idx]);
                if (ret != QCARCAM_RET_OK)
                {
                    QCARCAM_ERRORMSG("test_util_init_window failed for display_window");
                    goto fail;
                }
                test_util_set_diag(test_util_ctxt, input_ctxt->display_window[idx], &input_ctxt->diag);

                ret = test_util_set_window_param(test_util_ctxt, input_ctxt->display_window[idx], &input_ctxt->window_params[idx]);
            }
            else
            {
                ret = test_util_set_window_param(test_util_ctxt, input_ctxt->qcarcam_window[idx], &input_ctxt->window_params[idx]);
            }

            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("test_util_set_window_param failed");
                goto fail;
            }

            ret = test_util_init_window_buffers(test_util_ctxt, input_ctxt->qcarcam_window[idx], &input_ctxt->p_buffers_output[idx]);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("test_util_init_window_buffers failed");
                goto fail;
            }

            if (gCtxt.enable_deinterlace && (gCtxt.enable_deinterlace == TESTUTIL_DEINTERLACE_SW_WEAVE))
            {
                input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_DISPLAY].format = QCARCAM_FMT_UYVY_8;
                input_ctxt->p_buffers_disp[idx].colorFmt = (QCarCamColorFmt_e)input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_DISPLAY].format;
                input_ctxt->p_buffers_disp[idx].nBuffers = input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_DISPLAY].n_buffers;
                input_ctxt->p_buffers_disp[idx].pBuffers = (QCarCamBuffer_t *)calloc(input_ctxt->p_buffers_disp[idx].nBuffers, sizeof(*input_ctxt->p_buffers_disp[idx].pBuffers));
                if (!input_ctxt->p_buffers_disp[idx].pBuffers)
                {
                    QCARCAM_ERRORMSG("alloc qcarcam_buffer failed");
                    goto fail;
                }

                for (unsigned int i = 0; i < input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_DISPLAY].n_buffers; ++i)
                {
                    input_ctxt->p_buffers_disp[idx].pBuffers[i].numPlanes = 1;
                    input_ctxt->p_buffers_disp[idx].pBuffers[i].planes[0].width = input_ctxt->window_params[idx].buffer_size[0];
                    input_ctxt->p_buffers_disp[idx].pBuffers[i].planes[0].height = input_ctxt->window_params[idx].buffer_size[1];
                }

                ret = test_util_init_window_buffers(test_util_ctxt, input_ctxt->display_window[idx], &input_ctxt->p_buffers_disp[idx]);
                if (ret != QCARCAM_RET_OK)
                {
                    QCARCAM_ERRORMSG("test_util_init_window_buffers failed");
                    goto fail;
                }
            }

            qcarcam_test_get_time(&t_after);
            QCARCAM_PERFMSG(gCtxt.enableStats, "qcarcam window init (idx %d) : %llu ms", input_ctxt->idx, (t_after - t_before));
            t_before = t_after;

            input_ctxt->p_buffers_output[idx].nBuffers = input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].n_buffers;

#ifdef POST_PROCESS
            if (gCtxt.enable_c2d || gCtxt.enable_csc || gCtxt.enable_post_processing)
#else
            if (gCtxt.enable_c2d || gCtxt.enable_csc)
#endif
            {
                for (unsigned int i = 0; i < input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].n_buffers; ++i)
                {
                    test_util_create_c2d_surface(input_ctxt->test_util_ctxt, input_ctxt->qcarcam_window[idx], i);
                    if (ret != QCARCAM_RET_OK)
                    {
                        QCARCAM_ERRORMSG("test_util_create_c2d_surface failed");
                        goto fail;
                    }
                }

                qcarcam_test_get_time(&t_after);
                QCARCAM_PERFMSG(gCtxt.enableStats, "qcarcam create_c2d_surface (idx %d) : %llu ms", input_ctxt->idx, (t_after - t_before));
                t_before = t_after;

                /*display window*/
                input_ctxt->p_buffers_disp[idx].nBuffers = input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_DISPLAY].n_buffers;
                input_ctxt->p_buffers_disp[idx].colorFmt = input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_DISPLAY].format;
                input_ctxt->p_buffers_disp[idx].pBuffers = (QCarCamBuffer_t *)calloc(input_ctxt->p_buffers_disp[idx].nBuffers, sizeof(*input_ctxt->p_buffers_disp[idx].pBuffers));
                if (!input_ctxt->p_buffers_disp[idx].pBuffers)
                {
                    QCARCAM_ERRORMSG("alloc qcarcam_buffer failed");
                    goto fail;
                }

                for (unsigned int i = 0; i < input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_DISPLAY].n_buffers; ++i)
                {
                    input_ctxt->p_buffers_disp[idx].pBuffers[i].numPlanes = 1;
                    input_ctxt->p_buffers_disp[idx].pBuffers[i].planes[0].width = input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_DISPLAY].width;
                    input_ctxt->p_buffers_disp[idx].pBuffers[i].planes[0].height = input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_DISPLAY].height;
                }

                ret = test_util_init_window(test_util_ctxt, &input_ctxt->display_window[idx]);
                if (ret != QCARCAM_RET_OK)
                {
                    QCARCAM_ERRORMSG("test_util_init_window failed");
                    goto fail;
                }

                test_util_set_diag(test_util_ctxt, input_ctxt->display_window[idx], &input_ctxt->diag);

                // set display buffer to non-offscreen
                if (gCtxt.enable_cache)
                {
                    input_ctxt->window_params[idx].is_offscreen = 0;
                }


                ret = test_util_set_window_param(test_util_ctxt, input_ctxt->display_window[idx], &input_ctxt->window_params[idx]);
                if (ret != QCARCAM_RET_OK)
                {
                    QCARCAM_ERRORMSG("test_util_set_window_param failed");
                    goto fail;
                }

                ret = test_util_init_window_buffers(test_util_ctxt, input_ctxt->display_window[idx], &input_ctxt->p_buffers_disp[idx]);
                if (ret != QCARCAM_RET_OK)
                {
                    QCARCAM_ERRORMSG("test_util_init_window_buffers failed ret = %d",ret);
                    goto fail;
                }

                qcarcam_test_get_time(&t_after);
                QCARCAM_PERFMSG(gCtxt.enableStats, "display window init (idx %d) : %llu ms", input_ctxt->idx, (t_after - t_before));
                t_before = t_after;

                for (unsigned int i = 0; i < input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_DISPLAY].n_buffers; ++i)
                {
                    test_util_create_c2d_surface(input_ctxt->test_util_ctxt, input_ctxt->display_window[idx], i);
                    if (ret != QCARCAM_RET_OK)
                    {
                        QCARCAM_ERRORMSG("test_util_create_c2d_surface failed");
                        goto fail;
                    }
                }

                qcarcam_test_get_time(&t_after);
                QCARCAM_PERFMSG(gCtxt.enableStats, "display create_c2d_surface (idx %u) : %llu ms",
                        input_ctxt->idx, (t_after - t_before));
                t_before = t_after;

#ifdef ENABLE_CL_CONVERTER
                input_ctxt->g_converter = csc_create();

                ret = test_util_init_cl_converter(test_util_ctxt, input_ctxt->qcarcam_window[idx], input_ctxt->display_window[idx], input_ctxt->g_converter, &(input_ctxt->source_surface), &(input_ctxt->target_surface));
                if (ret != QCARCAM_RET_OK)
                {
                    QCARCAM_ERRORMSG("cl converter initialization failed");
                    goto fail;
                }
                qcarcam_test_get_time(&t_after);
                QCARCAM_PERFMSG(gCtxt.enableStats, "init cl convert : %llu ms", (t_after - t_before));
                t_before = t_after;
#endif
#ifdef POST_PROCESS
                if(gCtxt.enable_post_processing)
                {
                    gCtxt.post_processing_ctx[input_idx].pLibName = pLibName;
                    ret =  test_util_init_post_processing(&gCtxt.post_processing_ctx[input_idx],
                                                    test_util_ctxt,
                                                    input_ctxt->qcarcam_window[idx],
                                                    input_ctxt->display_window[idx]);
                    if (ret != QCARCAM_RET_OK)
                    {
                        QCARCAM_ERRORMSG("Post processing initialization failed");
                        goto fail;
                    }
                    qcarcam_test_get_time(&t_after);
                    QCARCAM_PERFMSG(gCtxt.enableStats, "init post processing: %llu ms", (t_after - t_before));
                    t_before = t_after;
                }
#endif

            }
        }
    }

    QCARCAM_DBGMSG("Buffer Setup End");
    QCARCAM_DBGMSG("Create qcarcam_hndl Begin");

    /*launch threads to do the work for multithreaded*/
    if (gCtxt.multithreaded)
    {
        for (int input_idx = 0; input_idx < gCtxt.numInputs; input_idx++)
        {
            qcarcam_test_input_t *input_ctxt = &gCtxt.inputs[input_idx];

            input_ctxt->idx = input_idx;
            snprintf(thread_name, sizeof(thread_name), "inpt_ctxt_%d", input_idx);

            rc = OSAL_ThreadCreate(QCARCAM_THRD_PRIO, &qcarcam_test_setup_input_ctxt_thread, &gCtxt.inputs[input_idx], 0, thread_name, &input_ctxt->thread_handle);
            if (rc)
            {
                QCARCAM_ERRORMSG("OSAL_ThreadCreate failed : %s", thread_name);
                goto fail;
            }

            rc = CameraCreateSignal(&gCtxt.inputs[input_idx].m_eventHandlerSignal);
            if (rc)
            {
                QCARCAM_ERRORMSG("CameraCreateSignal failed, rc=%d", rc);
                goto fail;
            }

            snprintf(thread_name, sizeof(thread_name), "process_cb_event_%d",input_idx);
            rc = OSAL_ThreadCreate(QCARCAM_THRD_PRIO, &process_cb_event_thread, &gCtxt.inputs[input_idx], 0, thread_name, &input_ctxt->process_cb_event_handle);
            if (rc)
            {
                QCARCAM_ERRORMSG("Create cb event process thread failed, rc=%d", rc);
                goto fail;
            }

            if (input_ctxt->is_injection)
            {
                rc = CameraCreateSignal(&gCtxt.inputs[input_idx].m_injectionHandlerSignal);
                if (rc)
                {
                    QCARCAM_ERRORMSG("CameraCreateSignal failed, rc=%d", rc);
                    goto fail;
                }

                snprintf(thread_name, sizeof(thread_name), "injection_%d",input_idx);
                rc = OSAL_ThreadCreate(QCARCAM_THRD_PRIO_HIGHREALTIME, &injection_thread, &gCtxt.inputs[input_idx], 0, thread_name, &input_ctxt->injection_handle);
                if (rc)
                {
                    QCARCAM_ERRORMSG("Create injection thread failed, rc=%d", rc);
                    goto fail;
                }
            }

            if (input_ctxt->delay_time)
            {
                snprintf(thread_name, sizeof(thread_name), "inpt_ctxt_usr_process_%d", input_idx);
                rc = OSAL_ThreadCreate(QCARCAM_THRD_PRIO, &inpt_ctxt_usr_process_thread, &gCtxt.inputs[input_idx], 0, thread_name, &input_ctxt->usr_process_thread_handle);
                if (rc)
                {
                    QCARCAM_ERRORMSG("OSAL_ThreadCreate failed : %s", thread_name);
                    goto fail;
                }
            }
        }

        if (gCtxt.exitSeconds)
        {
            snprintf(thread_name, sizeof(thread_name), "timer_thread");
            rc = OSAL_ThreadCreate(QCARCAM_THRD_PRIO, &timer_thread, 0, 0, thread_name, &timer_thread_handle);
            if (rc)
            {
                QCARCAM_ERRORMSG("OSAL_ThreadCreate failed, rc=%d", rc);
                goto fail;
            }
        }

        if (gCtxt.enableMenuMode)
        {
            qcarcam_test_menu();
        }
    }
    else
    {
        /*singlethreaded - init and start each camera sequentially*/
        for (int input_idx = 0; input_idx < gCtxt.numInputs; input_idx++)
        {
            qcarcam_test_input_t *input_ctxt = &gCtxt.inputs[input_idx];

            input_ctxt->idx = input_idx;
            if (0 != qcarcam_test_setup_input_ctxt_thread(&gCtxt.inputs[input_idx]))
            {
                QCARCAM_ERRORMSG("qcarcam_test_setup_input_ctxt_thread failed");
                goto fail;
            }
        }

        for (int input_idx = 0; input_idx < gCtxt.numInputs; input_idx++)
        {
            qcarcam_test_input_t *input_ctxt = &gCtxt.inputs[input_idx];

            input_ctxt->is_first_start = TRUE;
            ret = qcarcam_input_start(input_ctxt);
            if (ret != QCARCAM_RET_OK)
            {
                QCarCamClose(input_ctxt->qcarcam_hndl);
                input_ctxt->qcarcam_hndl = QCARCAM_HNDL_INVALID;
            }

            rc = CameraCreateSignal(&gCtxt.inputs[input_idx].m_eventHandlerSignal);
            if (rc)
            {
                QCARCAM_ERRORMSG("CameraCreateSignal failed, rc=%d", rc);
                goto fail;
            }

            snprintf(thread_name, sizeof(thread_name), "process_cb_event_%d",input_idx);
            rc = OSAL_ThreadCreate(QCARCAM_THRD_PRIO, &process_cb_event_thread, &gCtxt.inputs[input_idx], 0, thread_name, &input_ctxt->process_cb_event_handle);
            if (rc)
            {
                QCARCAM_ERRORMSG("Create cb event process thread failed, rc=%d", rc);
                goto fail;
            }

            if (input_ctxt->is_injection)
            {
                rc = CameraCreateSignal(&gCtxt.inputs[input_idx].m_injectionHandlerSignal);
                if (rc)
                {
                    QCARCAM_ERRORMSG("CameraCreateSignal failed, rc=%d", rc);
                    goto fail;
                }

                snprintf(thread_name, sizeof(thread_name), "injection_%d",input_idx);
                rc = OSAL_ThreadCreate(QCARCAM_THRD_PRIO_HIGHREALTIME, &injection_thread, &gCtxt.inputs[input_idx], 0, thread_name, &input_ctxt->injection_handle);
                if (rc)
                {
                    QCARCAM_ERRORMSG("Create injection thread failed, rc=%d", rc);
                    goto fail;
                }
            }
        }

        if (gCtxt.exitSeconds)
        {
            snprintf(thread_name, sizeof(thread_name), "timer_thread");
            rc = OSAL_ThreadCreate(QCARCAM_THRD_PRIO, &timer_thread, 0, 0, thread_name, &timer_thread_handle);
            if (rc)
            {
                QCARCAM_ERRORMSG("OSAL_ThreadCreate failed, rc=%d", rc);
                goto fail;
            }
        }

        while (!g_aborted)
        {
            for (int input_idx = 0; input_idx < gCtxt.numInputs; input_idx++)
            {
                qcarcam_test_input_t *input_ctxt = &gCtxt.inputs[input_idx];

                if (!input_ctxt->use_event_callback)
                {
                    for (unsigned int idx = 0; idx < input_ctxt->num_stream_instance; idx++)
                    {
                        if (qcarcam_test_handle_new_frame(input_ctxt, input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].bufferList_id))
                            break;
                    }
                }
                else
                {
                    sched_yield();
                }
            }
        }
    }

fail:

    /*Wait on all of them to join*/
    for (int input_idx = 0; input_idx < gCtxt.numInputs; input_idx++)
    {
        qcarcam_test_input_t *input_ctxt = &gCtxt.inputs[input_idx];

        if (input_ctxt->thread_handle)
        {
            OSAL_ThreadJoin(input_ctxt->thread_handle, NULL);
            OSAL_ThreadDestroy(input_ctxt->thread_handle, 0);
        }

        if (input_ctxt->m_eventHandlerSignal)
            CameraSetSignal(input_ctxt->m_eventHandlerSignal);

        if (input_ctxt->process_cb_event_handle)
        {
            OSAL_ThreadJoin(input_ctxt->process_cb_event_handle, NULL);
            OSAL_ThreadDestroy(input_ctxt->process_cb_event_handle, 0);
        }

        if (input_ctxt->injection_handle)
        {
            OSAL_ThreadJoin(input_ctxt->injection_handle, NULL);
            OSAL_ThreadDestroy(input_ctxt->injection_handle, 0);
        }

        if (input_ctxt->m_eventHandlerSignal)
        {
            CameraDestroySignal(input_ctxt->m_eventHandlerSignal);
        }

        if (input_ctxt->m_injectionHandlerSignal)
        {
            CameraDestroySignal(input_ctxt->m_injectionHandlerSignal);
        }

        input_ctxt->m_eventHandlerSignal= NULL;
        input_ctxt->m_injectionHandlerSignal = NULL;
    }

    // cleanup
    for (int input_idx = 0; input_idx < gCtxt.numInputs; input_idx++)
    {
        qcarcam_test_input_t *input_ctxt = &gCtxt.inputs[input_idx];

        if (input_ctxt->qcarcam_hndl != QCARCAM_HNDL_INVALID)
        {
            pthread_mutex_lock(&input_ctxt->mutex);
            (void)QCarCamStop(input_ctxt->qcarcam_hndl);
            (void)QCarCamRelease(input_ctxt->qcarcam_hndl);
            (void)QCarCamClose(input_ctxt->qcarcam_hndl);
            input_ctxt->state = QCARCAMTEST_STATE_CLOSED;
            pthread_mutex_unlock(&input_ctxt->mutex);
            input_ctxt->qcarcam_hndl = QCARCAM_HNDL_INVALID;
        }

        for (unsigned int idx = 0; idx < input_ctxt->num_stream_instance; idx++)
        {
            if (input_ctxt->qcarcam_window[idx])
            {
                test_util_deinit_window_buffer(test_util_ctxt, input_ctxt->qcarcam_window[idx]);
                test_util_deinit_window(test_util_ctxt, input_ctxt->qcarcam_window[idx]);
            }

            if (input_ctxt->injection_window)
            {
                test_util_free_input_buffers(input_ctxt->injection_window, &input_ctxt->p_buffers_input);
                test_util_deinit_window(test_util_ctxt, input_ctxt->injection_window);
            }

            if (input_ctxt->delay_time)
            {
                for (unsigned int idx = 0; idx < input_ctxt->p_buffers_output[idx].nBuffers; idx++)
                {
                    if (input_ctxt->buf_timer[idx].ptimer)
                    {
                        OSAL_TimerDestroy(input_ctxt->buf_timer[idx].ptimer);
                    }
                }
            }

#ifdef POST_PROCESS
            if (gCtxt.enable_post_processing || gCtxt.enable_csc || gCtxt.enable_c2d || (gCtxt.enable_deinterlace && (gCtxt.enable_deinterlace == TESTUTIL_DEINTERLACE_SW_WEAVE)))
#else
            if (gCtxt.enable_csc || gCtxt.enable_c2d || (gCtxt.enable_deinterlace && (gCtxt.enable_deinterlace == TESTUTIL_DEINTERLACE_SW_WEAVE)))
#endif
            {
                if (input_ctxt->display_window[idx])
                {
                    test_util_deinit_window_buffer(test_util_ctxt, input_ctxt->display_window[idx]);
                    test_util_deinit_window(test_util_ctxt, input_ctxt->display_window[idx]);
                }
            }
            if (input_ctxt->p_buffers_output[idx].pBuffers)
            {
                free(input_ctxt->p_buffers_output[idx].pBuffers);
                input_ctxt->p_buffers_output[idx].pBuffers = NULL;
            }

            if (input_ctxt->p_buffers_disp[idx].pBuffers)
            {
                free(input_ctxt->p_buffers_disp[idx].pBuffers);
                input_ctxt->p_buffers_disp[idx].pBuffers = NULL;
            }
        }
#ifdef ENABLE_CL_CONVERTER
        if (gCtxt.enable_csc)
        {
            test_util_deinit_cl_converter(input_ctxt->g_converter, &(input_ctxt->source_surface), &(input_ctxt->target_surface));
        }
#endif
#ifdef POST_PROCESS
        if (gCtxt.enable_post_processing)
        {
            test_util_deinit_post_processing(&gCtxt.post_processing_ctx[input_idx]);
        }
#endif
    }

    if (pInputs)
    {
        free(pInputs);
        pInputs = NULL;
    }

    if (xml_inputs)
    {
        free(xml_inputs);
        xml_inputs = NULL;
    }

    if (!g_aborted)
    {
        abort_test();
    }

    for (unsigned int inputIdx = 0; inputIdx < queryFilled; inputIdx++)
    {
        if (gCtxt.queryInputs[inputIdx].modesDesc.pModes)
        {
            free(gCtxt.queryInputs[inputIdx].modesDesc.pModes);
        }
    }

    test_util_deinit(test_util_ctxt);

    QCarCamUninitialize();

    return rval;
}

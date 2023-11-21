/* ===========================================================================
 * Copyright (c) 2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
=========================================================================== */

#include "qcarcam_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
    QCARCAM_TEST_MENU_FIRST_ITEM = 1,
    QCARCAM_TEST_MENU_STREAM_OPEN = QCARCAM_TEST_MENU_FIRST_ITEM,
    QCARCAM_TEST_MENU_STREAM_CLOSE,
    QCARCAM_TEST_MENU_STREAM_STOP,
    QCARCAM_TEST_MENU_STREAM_START,
    QCARCAM_TEST_MENU_STREAM_PAUSE,
    QCARCAM_TEST_MENU_STREAM_RESUME,
    QCARCAM_TEST_MENU_STREAM_STOP_ALL,
    QCARCAM_TEST_MENU_STREAM_START_ALL,
    QCARCAM_TEST_MENU_STREAM_PAUSE_ALL,
    QCARCAM_TEST_MENU_STREAM_RESUME_ALL,
    QCARCAM_TEST_MENU_STREAM_ENABLE_CALLBACK,
    QCARCAM_TEST_MENU_STREAM_DISABLE_CALLBACK,
    QCARCAM_TEST_MENU_STREAM_SET_FRAMERATE,
    QCARCAM_TEST_MENU_STREAM_SET_EXPOSURE,
    QCARCAM_TEST_MENU_STREAM_SET_SENSORMODE,
    QCARCAM_TEST_MENU_STREAM_GET_SENSORMODE,
    QCARCAM_TEST_MENU_STREAM_SET_COLOR_PARAM,
    QCARCAM_TEST_MENU_STREAM_GET_COLOR_PARAM,
    QCARCAM_TEST_MENU_STREAM_SET_GAMMA_PARAM,
    QCARCAM_TEST_MENU_STREAM_GET_GAMMA_PARAM,
    QCARCAM_TEST_MENU_STREAM_SET_METADATA_TAG,
    QCARCAM_TEST_MENU_STREAM_GET_METADATA_TAG,
    QCARCAM_TEST_MENU_DUMP_NEXT_FRAME,
    QCARCAM_TEST_MENU_CHECK_BUFFERS,
    QCARCAM_TEST_MENU_STREAM_SET_VENDOR_PARAM,
    QCARCAM_TEST_MENU_STREAM_GET_VENDOR_PARAM,
    QCARCAM_TEST_MENU_STREAM_SET_BRIGHTNESS,
    QCARCAM_TEST_MENU_STREAM_GET_BRIGHTNESS,
    QCARCAM_TEST_MENU_STREAM_SET_CONTRAST,
    QCARCAM_TEST_MENU_STREAM_GET_CONTRAST,
    QCARCAM_TEST_MENU_STREAM_SET_MIRRORING,
    QCARCAM_TEST_MENU_STREAM_GET_MIRRORING,
    QCARCAM_TEST_MENU_STREAM_SUBSCRIBE_CHANGE_EVENT,
    QCARCAM_TEST_MENU_STREAM_UNSUBSCRIBE_CHANGE_EVENT,
    QCARCAM_TEST_MENU_STREAM_SUBSCRIBE_SOF_EVENT,
    QCARCAM_TEST_MENU_STREAM_UNSUBSCRIBE_SOF_EVENT,
    QCARCAM_TEST_MENU_MASTER,
    QCARCAM_TEST_MENU_GET_SYSTEM_DIAGNOSTICS,
    QCARCAM_TEST_MENU_STREAM_GET_COLORSPACE,
    QCARCAM_TEST_MENU_STREAM_SET_ANTIBANDING_MODE,
    QCARCAM_TEST_MENU_MAX
}qcarcam_test_menu_option_t;

typedef struct
{
    qcarcam_test_menu_option_t id;
    const char* str;
}qcarcam_test_menu_t;

static qcarcam_test_menu_t g_qcarcam_menu[QCARCAM_TEST_MENU_MAX] =
{
    {},
    {QCARCAM_TEST_MENU_STREAM_OPEN,  "Open a stream"},
    {QCARCAM_TEST_MENU_STREAM_CLOSE, "Close a stream"},
    {QCARCAM_TEST_MENU_STREAM_STOP,  "Stop a stream"},
    {QCARCAM_TEST_MENU_STREAM_START, "Start a stream"},
    {QCARCAM_TEST_MENU_STREAM_PAUSE, "Pause a stream"},
    {QCARCAM_TEST_MENU_STREAM_RESUME, "Resume a stream"},
    {QCARCAM_TEST_MENU_STREAM_STOP_ALL, "Stop all streams"},
    {QCARCAM_TEST_MENU_STREAM_START_ALL, "Start all streams"},
    {QCARCAM_TEST_MENU_STREAM_PAUSE_ALL, "Pause all streams"},
    {QCARCAM_TEST_MENU_STREAM_RESUME_ALL, "Resume all streams"},
    {QCARCAM_TEST_MENU_STREAM_ENABLE_CALLBACK, "Enable callback"},
    {QCARCAM_TEST_MENU_STREAM_DISABLE_CALLBACK, "Disable callback"},
    {QCARCAM_TEST_MENU_STREAM_SET_FRAMERATE, "Set frame rate control"},
    {QCARCAM_TEST_MENU_STREAM_SET_EXPOSURE, "Set exposure"},
    {QCARCAM_TEST_MENU_STREAM_SET_SENSORMODE,"Set sensormode"},
    {QCARCAM_TEST_MENU_STREAM_GET_SENSORMODE,"Get sensormode"},
    {QCARCAM_TEST_MENU_STREAM_SET_COLOR_PARAM, "Set color param"},
    {QCARCAM_TEST_MENU_STREAM_GET_COLOR_PARAM, "Get color param"},
    {QCARCAM_TEST_MENU_STREAM_SET_GAMMA_PARAM, "Set Gamma Table"},
    {QCARCAM_TEST_MENU_STREAM_GET_GAMMA_PARAM, "Get Gamma Table"},
    {QCARCAM_TEST_MENU_STREAM_SET_METADATA_TAG, "Set Metadata Tag"},
    {QCARCAM_TEST_MENU_STREAM_GET_METADATA_TAG, "Get Metadata Tag"},
    {QCARCAM_TEST_MENU_DUMP_NEXT_FRAME, "Dump Next Frame"},
    {QCARCAM_TEST_MENU_CHECK_BUFFERS, "Check Buffers"},
    {QCARCAM_TEST_MENU_STREAM_SET_VENDOR_PARAM, "Set Vendor Param"},
    {QCARCAM_TEST_MENU_STREAM_GET_VENDOR_PARAM, "Get Vendor Param"},
    {QCARCAM_TEST_MENU_STREAM_SET_BRIGHTNESS, "Set Brightness"},
    {QCARCAM_TEST_MENU_STREAM_GET_BRIGHTNESS, "Get Brightness"},
    {QCARCAM_TEST_MENU_STREAM_SET_CONTRAST, "Set Contrast"},
    {QCARCAM_TEST_MENU_STREAM_GET_CONTRAST, "Get Contrast"},
    {QCARCAM_TEST_MENU_STREAM_SET_MIRRORING, "Set Mirroring"},
    {QCARCAM_TEST_MENU_STREAM_GET_MIRRORING, "Get Mirroring"},
    {QCARCAM_TEST_MENU_STREAM_SUBSCRIBE_CHANGE_EVENT, "Subscribe for an event"},
    {QCARCAM_TEST_MENU_STREAM_UNSUBSCRIBE_CHANGE_EVENT, "Unsubscribe for an event"},
    {QCARCAM_TEST_MENU_STREAM_SUBSCRIBE_SOF_EVENT, "Subscribe for SOF event"},
    {QCARCAM_TEST_MENU_STREAM_UNSUBSCRIBE_SOF_EVENT, "Unsubscribe for SOF event"},
    {QCARCAM_TEST_MENU_MASTER, "Set/Release a client as master"},
    {QCARCAM_TEST_MENU_GET_SYSTEM_DIAGNOSTICS, "Get System diagnostic info"},
    {QCARCAM_TEST_MENU_STREAM_GET_COLORSPACE, "Get color space"},
    {QCARCAM_TEST_MENU_STREAM_SET_ANTIBANDING_MODE, "Set Antibanding mode"},

};

///////////////////////////////
/// STATICS
///////////////////////////////

extern qcarcam_test_ctxt_t gCtxt;
extern volatile int g_aborted;


static void display_valid_session_ids()
{
    int i;
    qcarcam_test_input_t *input_ctxt = NULL;

    printf("Valid session ids are\n");
    printf("========================\n");
    printf("       Session id      \n");
    printf("========================\n");
    for (i = 0; i < gCtxt.numInputs; ++i)
    {
        input_ctxt = &gCtxt.inputs[i];
        if (input_ctxt->qcarcam_hndl != 0 )
        {
            printf("       %d       \n", input_ctxt->sessionId);
        }
    }
    printf("========================\n");
}

static void display_valid_closed_session_ids()
{
    int i;
    qcarcam_test_input_t *input_ctxt = NULL;

    printf("Valid closed session ids are\n");
    printf("============================\n");
    printf("         Session id        \n");
    printf("============================\n");
    for (i = 0; i < gCtxt.numInputs; ++i)
    {
        input_ctxt = &gCtxt.inputs[i];
        if (input_ctxt->state ==  QCARCAMTEST_STATE_CLOSED)
        {
            printf("        %d       \n", input_ctxt->sessionId);
        }
    }
    printf("===========================\n");
}

static void display_valid_bufferlist_ids(qcarcam_test_input_t *input_ctxt)
{
    printf("Valid bufferlist ids are\n");
    printf("========================\n");
    printf("     BufferList id     \n");
    printf("========================\n");
    if (input_ctxt->qcarcam_hndl != 0 )
    {
        for (uint32_t idx = 0; idx < input_ctxt->num_stream_instance; idx++)
        {
            printf("  %d  \n", input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].bufferList_id);
        }
    }
    printf("========================\n");
}

/**
 * Returns user entered stream idx
 */
static int get_closed_input_session_id()
{
    int session_id = gCtxt.numInputs;
    char buf[BUFSIZE];
    char *p = NULL;

    display_valid_closed_session_ids();

    printf("Enter session id\n");

    if (fgets(buf, sizeof(buf), stdin) != NULL)
    {
        session_id = strtol(buf, &p, 10);
    }

    return session_id;
}
/**
 * Returns user entered stream idx
 */
static int get_input_session_id()
{
    int session_id = gCtxt.numInputs;
    char buf[BUFSIZE];
    char *p = NULL;

    display_valid_session_ids();

    printf("Enter session id\n");

    if (fgets(buf, sizeof(buf), stdin) != NULL)
    {
        session_id = strtol(buf, &p, 10);
    }

    return session_id;
}

static int get_input_bufferlist_id(qcarcam_test_input_t *input_ctxt)
{
    int bufferlist_id = input_ctxt->num_stream_instance;
    char buf[BUFSIZE];
    char *p = NULL;

    display_valid_bufferlist_ids(input_ctxt);

    printf("Enter bufferlist id\n");

    if (fgets(buf, sizeof(buf), stdin) != NULL)
    {
        bufferlist_id = strtol(buf, &p, 10);
    }

    return bufferlist_id;
}

static void get_input_sensormode(uint32_t* sensormode_index)
{
    if (sensormode_index != NULL)
    {
        char buf[BUFSIZE];
        char *p = NULL;

        printf("Enter camera sensormode index:[0]30fps mode [3]15fps mode \n");
        if (fgets(buf, sizeof(buf), stdin) != NULL)
        {
            *sensormode_index = strtol(buf, &p, 10);
        }
    }
}

static void get_input_hdr_exposure(QCarCamExposureConfig_t* exposure_config)
{
    char buf[BUFSIZE];
    char *p = NULL;

    printf("numExposures?\n");
    if (fgets(buf, sizeof(buf), stdin) != NULL)
    {
        exposure_config->numExposures = strtol(buf, &p, 10);
    }

    for (uint32_t i = 0; i < exposure_config->numExposures; i++)
    {
        printf("exp%d\n", i);
        if (fgets(buf, sizeof(buf), stdin) != NULL)
        {
            exposure_config->exposureTime[i] = strtof(buf, NULL);
        }

        printf("gain%d\n", i);
        if (fgets(buf, sizeof(buf), stdin) != NULL)
        {
            exposure_config->gain[i] = strtof(buf, NULL);
        }
    }
}

static void get_input_framerate(QCarCamFrameDropConfig_t* frame_rate_config)
{
    if (frame_rate_config != NULL)
    {
        int frame_drop_period = 0;
        int frame_drop_pattern = 0;

        char buf[BUFSIZE];

        printf("Enter period value, maximal 32\n");
        if (fgets(buf, sizeof(buf), stdin) != NULL)
        {
            frame_drop_period = strtol(buf, NULL, 10);
        }
        printf("Enter pattern hex value, e.g. 0x23\n");
        if (fgets(buf, sizeof(buf), stdin) != NULL)
        {
            frame_drop_pattern = strtol(buf, NULL, 16);
        }

        frame_rate_config->frameDropPeriod = (unsigned char)frame_drop_period;
        frame_rate_config->frameDropPattern = frame_drop_pattern;
    }
}

static int get_input_set_flag()
{
    printf("Enter set[1] release[0]");
    char buf[BUFSIZE];
    int flag = 0;
    char *p = NULL;
    if (fgets(buf, sizeof(buf), stdin) != NULL)
    {
        flag = strtol(buf, &p, 10);
    }
    return flag;
}

static void get_metadata_data(test_util_metadata_tag_t* pTag)
{
    char buf[BUFSIZE];
    char* p = NULL;

    if (fgets(buf, sizeof(buf), stdin) != NULL)
    {
        int32_t data = strtol(buf, &p, 10);
        int32_t* pData = (int32_t*)pTag->data;
        *pData = data;
    }

    pTag->count = 1;
}

static void get_metadata_tag(test_util_metadata_tag_t* pTag)
{
    char buf[BUFSIZE];
    char *p = NULL;

    printf("isVendorTag?\n");
    if (fgets(buf, sizeof(buf), stdin) != NULL)
    {
        pTag->isVendorTag = strtol(buf, &p, 10);
    }

    printf("tagId?\n");
    if (fgets(buf, sizeof(buf), stdin) != NULL)
    {
        if (pTag->isVendorTag)
        {
            pTag->qccId = strtol(buf, &p, 10);
        }
        else
        {
            pTag->tagId = strtol(buf, &p, 10);
        }
    }

    printf("data?\n");
    if (fgets(buf, sizeof(buf), stdin) != NULL)
    {
        int32_t data = strtol(buf, &p, 10);
        int32_t* pData = (int32_t*)pTag->data;
        *pData = data;
    }

    pTag->count = 1;
}



/**
 * API to set/release a client as master
 *
 *
 * @param qcarcam_test_input_t* input_ctxt
 * @param unsigned int flag to set(1)/release(0) master
 */

static void qcarcam_test_set_master(qcarcam_test_input_t *input_ctxt, unsigned int flag)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;

    uint32_t param = flag;
    ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_STREAM_CONFIG_PARAM_MASTER, &param, sizeof(param));
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("This client can't be set(1)/release(0) master:%d, it's not fatal", flag);
    }
    else
    {
        if (flag == 1)
        {
            input_ctxt->is_master = true;
            printf("Client [Stream ID %u] has been set as master \n",
                    input_ctxt->idx);
        }
        else
        {
            input_ctxt->is_master = false;
            printf("Client [Stream ID %u] has been released \n",
                    input_ctxt->idx);
        }
    }
}

/**
 * API to subscribe for events to get notifications
 *
 *
 * @param qcarcam_test_input_t* input_ctxt
 * @param change_events  Bitmask of events
 */

static int qcarcam_test_subscribe_input_params_change(qcarcam_test_input_t *input_ctxt, uint64_t change_events)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;

    uint64_t param = change_events;

    ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_STREAM_CONFIG_PARAM_EVENT_CHANGE_SUBSCRIBE, &param, sizeof(param));
    if(ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("QCarCamSetParam(QCARCAM_PARAM_EVENT_SUBSCRIBE) failed for events:%lu with ret:%d", change_events, ret);
        return -1;
    }
    return 0;
}

/**
 * API to unsubscribe for events to get notifications
 *
 *
 * @param qcarcam_test_input_t* input_ctxt
 * @param change_events  Bitmask of events
 */

static int qcarcam_test_unsubscribe_input_params_change(qcarcam_test_input_t *input_ctxt, uint64_t change_events)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;

    uint64_t param = change_events;

    ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_STREAM_CONFIG_PARAM_EVENT_CHANGE_UNSUBSCRIBE, &param, sizeof(param));
    if(ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("QCarCamSetParam(QCARCAM_PARAM_EVENT_UNSUBSCRIBE) failed");
        return -1;
    }
    return 0;
}

/**
 * API to subscribe for SOF event notification
 *
 *
 * @param qcarcam_test_input_t* input_ctxt
 */

static int qcarcam_test_subscribe_SOF_event(qcarcam_test_input_t *input_ctxt)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;

    uint32_t param = 0;

    /* Get the applied mask. */
    ret = QCarCamGetParam(input_ctxt->qcarcam_hndl, QCARCAM_STREAM_CONFIG_PARAM_EVENT_MASK, &param, sizeof(param));
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("QCarCamGetParam failed for Input handle 0x%lx, ret = %d",input_ctxt->qcarcam_hndl, ret);
        return -1;
    }

    /* Set the SOF mask. */
    param |= QCARCAM_EVENT_FRAME_SOF;
    ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_STREAM_CONFIG_PARAM_EVENT_MASK, &param, sizeof(param));
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("QCarCamSetParam failed for Input handle 0x%lx, ret = %d",input_ctxt->qcarcam_hndl, ret);
        return -1;
    }

    return 0;
}

/**
 * API to unsubscribe for SOF event notification
 *
 *
 * @param qcarcam_test_input_t* input_ctxt
 */

static int qcarcam_test_unsubscribe_SOF_event(qcarcam_test_input_t *input_ctxt)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;

    uint32_t param = 0;

    /* Get the applied mask. */
    ret = QCarCamGetParam(input_ctxt->qcarcam_hndl, QCARCAM_STREAM_CONFIG_PARAM_EVENT_MASK, &param, sizeof(param));
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("QCarCamGetParam failed for Input handle 0x%lx, ret = %d",input_ctxt->qcarcam_hndl, ret);
        return -1;
    }

    /* Unset the SOF mask. */
    param &= ~QCARCAM_EVENT_FRAME_SOF;
    ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_STREAM_CONFIG_PARAM_EVENT_MASK, &param, sizeof(param));
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("QCarCamSetParam failed for Input handle 0x%lx, ret = %d",input_ctxt->qcarcam_hndl, ret);
        return -1;
    }

    return 0;
}


static void display_sensormode_settings(uint32_t param)
{
    printf("sensormode = %d\n", param);
}

static void display_color_space_settings(uint32_t param)
{
    printf("color space = %d\n",param);
}


void display_menu(void)
{
    int i = 0;

    printf("\n ========================================================== \n");

    for (i = QCARCAM_TEST_MENU_FIRST_ITEM; i < QCARCAM_TEST_MENU_MAX; i++)
    {
        printf("'%d'....%s \t\t", g_qcarcam_menu[i].id, g_qcarcam_menu[i].str);

        i++;
        if (i < QCARCAM_TEST_MENU_MAX)
        {
            printf("'%d'....%s\n", g_qcarcam_menu[i].id, g_qcarcam_menu[i].str);
        }
        else
        {
            printf("\n");
            break;
        }
    }

    printf(" 'h'.....display menu \n");
    printf(" 's'.....Dump Image \n");
    printf(" 'e'.....Exit \n");
    printf("\n =========================================================== \n");
    printf(" Enter your choice\n");
}

static qcarcam_test_input_t* get_input_ctxt(int session_id)
{
    if (session_id >= 0 && session_id < gCtxt.numInputs)
    {
        return &gCtxt.inputs[session_id];
    }

    printf("Wrong stream id entered, please check the input xml\n");

    return NULL;
}

void parse_diagnostics(QCarCamDiagInfo* diagnosticInfo)
{
    if(!diagnosticInfo)
        return;
    //parsing client Diagnostic Info
    QCarCamDiagClientInfo* pDiagClientInfo = diagnosticInfo->aisDiagClientInfo;
    for (uint32_t i = 0; i < MAX_USR_CLIENTS; i++)
    {
        qcarcam_test_input_t *input_ctxt = NULL;
        if (!pDiagClientInfo[i].usrHdl)
        {
            break;
        }
        for (uint32_t j = 0; j < (uint32_t)gCtxt.numInputs; j++)
        {
            if (pDiagClientInfo[i].usrHdl == gCtxt.inputs[j].qcarcam_hndl)
            {
                input_ctxt = &gCtxt.inputs[j];
                break;
            }
        }

        if (!input_ctxt)
        {
            QCARCAM_INFOMSG("This handle is invalid 0x%lx", pDiagClientInfo[i].usrHdl);
        }
        else
        {
            QCarCamDiagClientInfo *usrInfo = &pDiagClientInfo[i];

            QCARCAM_INFOMSG("usrHdl:0x%lx usr_state:%d inputId:%d opMode:%d inputDevId:%d"
                "csiPhyDevId:%d ifeDevId:%d rdiId:%d timeStampStart:%llu sofCounter:%llu frameCounter:%llu",
                usrInfo->usrHdl, usrInfo->state, usrInfo->inputId, usrInfo->opMode,
                usrInfo->inputDevId, usrInfo->csiphyDevId, usrInfo->ifeDevId, usrInfo->rdiId,
                usrInfo->timeStampStart, usrInfo->sofCounter, usrInfo->frameCounter);
            for (int j = 0; j < QCARCAM_MAX_NUM_BUFFERS; j++)
            {
                QCarCamDiagBufferInfo *bufInfo = &usrInfo->bufInfo[j];
                QCARCAM_INFOMSG("bufId:%d bufStatus:%d", bufInfo->bufId, bufInfo->bufStatus);
            }
        }
    }

    //parsing Input device statistics info
    /*
    QCarCamDiagInputDevInfo* pDiagInputInfo = diagnosticInfo->aisDiagInputDevInfo;
    for(uint32_t i = 0; i < MAX_NUM_INPUT_DEVICES; i++)
    {
        QCarCamDiagInputDevInfo* inputInfo = &pDiagInputInfo[i];

        QCARCAM_INFOMSG("DevId:%d numSensors:%d cciDevId:%d cciPortId:%d state:%d srcIdEnableMask:%d", inputInfo->inputDevId, inputInfo->numSensors,
            inputInfo->cciMap.cciDevId, inputInfo->cciMap.cciPortId, inputInfo->state, inputInfo->srcIdEnableMask);

        for (uint32_t j = 0; j < (uint32_t)inputInfo->numSensors; j++)
        {
            QCarCamDiagInputSrcInfo* sourceInfo = &inputInfo->inputSourceInfo[j];

            QCARCAM_INFOMSG("inputSrcId:%d status:%d fps:%.2f width:%d height:%d format:%d",
                sourceInfo->inputSrcId, sourceInfo->status,
                sourceInfo->sensorMode.fps, sourceInfo->sensorMode.res.width,
                sourceInfo->sensorMode.res.height, sourceInfo->sensorMode.colorFmt);

        }
    }
    */
    //parse csiphy device info
    QCarCamDiagCsiDevInfo* pDiagCsiInfo = diagnosticInfo->aisDiagCsiDevInfo;
    for(uint32_t i = 0; i < MAX_NUM_CSIPHY_DEVICES; i++)
    {
        QCarCamDiagCsiDevInfo* csiInfo = &pDiagCsiInfo[i];

        QCARCAM_INFOMSG("DevId:%d laneMapping:%x numIfeMap:%d ifeMap:%x", csiInfo->csiphyDevId, csiInfo->csiLaneMapping,
            csiInfo->numIfeMap, csiInfo->ifeMap);
    }

    //parse ife device info
    QCarCamDiagIfeDevInfo* pDiagIfeInfo = diagnosticInfo->aisDiagIfeDevInfo;
    for (uint32_t i = 0; i < MAX_NUM_IFE_DEVICES; i++)
    {
        QCarCamDiagIfeDevInfo* ifeInfo = &pDiagIfeInfo[i];

        QCARCAM_INFOMSG("DevId:%d csiDevId:%d numRdi:%d csidPktsReceived:%llu", ifeInfo->ifeDevId, ifeInfo->csiDevId,
            ifeInfo->numRdi, ifeInfo->csidPktsRcvd);

        for(uint32_t j = 0; j < (uint32_t)ifeInfo->numRdi; j++)
        {
            QCarCamDiagRdiInfo *rdiInfo = &ifeInfo->rdiInfo[j];

            QCARCAM_INFOMSG("rdiId:%d rdiStatus:%d", rdiInfo->rdiId, rdiInfo->rdiStatus);
        }
    }

    //parse Error Info
    QCarCamDiagErrorInfo* pDiagErrorInfo = diagnosticInfo->aisDiagErrInfo;
    for(uint32_t i = 0; i < MAX_ERR_QUEUE_SIZE; i++)
    {
        QCarCamDiagErrorInfo* errorInfo = &pDiagErrorInfo[i];
        QCARCAM_INFOMSG("errorType:%d errorStatus:%d usrHdl:0x%lx inputSrcId:%d inputDevId:%d csiphyId:%d"
            "ifeDevId:%d rdiId:%d errorTimeStamp:%llu", errorInfo->errorType, errorInfo->payload[0],
            errorInfo->usrHdl, errorInfo->inputSrcId, errorInfo->inputDevId, errorInfo->csiphyDevId,
            errorInfo->ifeDevId, errorInfo->rdiId, errorInfo->errorTimeStamp);
    }
}

static void process_cmds(uint32_t option)
{
    int ret = 0;
    int session_id;
    qcarcam_test_input_t *input_ctxt = NULL;

    switch (option)
    {
    case QCARCAM_TEST_MENU_STREAM_OPEN:
    {
        session_id = get_closed_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt)
        {
            pthread_mutex_lock(&input_ctxt->mutex);
            ret = qcarcam_input_open(input_ctxt);
            pthread_mutex_unlock(&input_ctxt->mutex);
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_CLOSE:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt)
        {
            pthread_mutex_lock(&input_ctxt->mutex);
            if (input_ctxt->state == QCARCAMTEST_STATE_STOP)
            {
                ret = qcarcam_input_close(input_ctxt);
            }
            pthread_mutex_unlock(&input_ctxt->mutex);
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_START:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt)
        {
            pthread_mutex_lock(&input_ctxt->mutex);
            if (input_ctxt->state == QCARCAMTEST_STATE_STOP || input_ctxt->state == QCARCAMTEST_STATE_OPEN)
            {
                ret = qcarcam_input_start(input_ctxt);
            }
            pthread_mutex_unlock(&input_ctxt->mutex);
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_START_ALL:
    {
        for (session_id = 0; session_id < gCtxt.numInputs; ++session_id)
        {
            input_ctxt = &gCtxt.inputs[session_id];
            pthread_mutex_lock(&input_ctxt->mutex);
            if (input_ctxt->state == QCARCAMTEST_STATE_STOP)
            {
                ret = qcarcam_input_start(input_ctxt);
            }
            pthread_mutex_unlock(&input_ctxt->mutex);
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_STOP:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt)
        {
            pthread_mutex_lock(&input_ctxt->mutex);
            if (input_ctxt->state == QCARCAMTEST_STATE_START ||
                input_ctxt->state == QCARCAMTEST_STATE_ERROR)
            {
                ret = qcarcam_input_stop(input_ctxt);
            }
            pthread_mutex_unlock(&input_ctxt->mutex);
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_STOP_ALL:
    {
        for (session_id = 0; session_id < gCtxt.numInputs; ++session_id)
        {
            input_ctxt = &gCtxt.inputs[session_id];

            pthread_mutex_lock(&input_ctxt->mutex);
            if (input_ctxt->state == QCARCAMTEST_STATE_START ||
                input_ctxt->state == QCARCAMTEST_STATE_ERROR)
            {
                ret = qcarcam_input_stop(input_ctxt);
            }
            pthread_mutex_unlock(&input_ctxt->mutex);
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_PAUSE:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt)
        {
            pthread_mutex_lock(&input_ctxt->mutex);
            if (input_ctxt->state == QCARCAMTEST_STATE_START)
            {
                ret = qcarcam_input_pause(input_ctxt);
            }
            pthread_mutex_unlock(&input_ctxt->mutex);
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_PAUSE_ALL:
    {
        for (session_id = 0; session_id < gCtxt.numInputs; ++session_id)
        {
            input_ctxt = &gCtxt.inputs[session_id];

            pthread_mutex_lock(&input_ctxt->mutex);
            if (input_ctxt->state == QCARCAMTEST_STATE_START)
            {
                ret = qcarcam_input_pause(input_ctxt);
            }
            pthread_mutex_unlock(&input_ctxt->mutex);
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_RESUME:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt)
        {
            pthread_mutex_lock(&input_ctxt->mutex);
            if (input_ctxt->state == QCARCAMTEST_STATE_PAUSE)
            {
                ret = qcarcam_input_resume(input_ctxt);
            }
            pthread_mutex_unlock(&input_ctxt->mutex);
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_RESUME_ALL:
    {
        for (session_id = 0; session_id < gCtxt.numInputs; ++session_id)
        {
            input_ctxt = &gCtxt.inputs[session_id];
            pthread_mutex_lock(&input_ctxt->mutex);
            if (input_ctxt->state == QCARCAMTEST_STATE_PAUSE)
            {
                ret = qcarcam_input_resume(input_ctxt);
            }
            pthread_mutex_unlock(&input_ctxt->mutex);
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_SET_EXPOSURE:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt && input_ctxt->qcarcam_hndl)
        {
            char buf[BUFSIZE];
            char *p = NULL;
            int param_option = 0;

            printf("Select exposure type: [0]Manual, [2]Auto\n");
            if (fgets(buf, sizeof(buf), stdin) != NULL)
            {
                /* 10 indicates decimal number*/
                param_option = strtol(buf, &p, 10);
            }

            switch (param_option)
            {
                case 0:
                {
                    QCarCamExposureConfig_t param = {};
                    get_input_hdr_exposure(&param);
                    param.mode = QCARCAM_EXPOSURE_MANUAL;
                    ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_SENSOR_PARAM_EXPOSURE, &param, sizeof(param));
                    break;
                }
                case 1:
                {
                    QCarCamExposureConfig_t param = {};
                    param.mode = QCARCAM_EXPOSURE_AUTO;
                    ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_SENSOR_PARAM_EXPOSURE, &param, sizeof(param));
                    break;
                }
                default:
                {
                    printf("Invalid input");
                    break;
                }
            }

            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("QCarCamSetParam (%d) failed for Input handle 0x%lx, ret = %d", param_option, input_ctxt->qcarcam_hndl, ret);
            }
            else
            {
                QCARCAM_INFOMSG("QCarCamSetParam (%d) success for Input handle 0x%lx", param_option, input_ctxt->qcarcam_hndl);
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_SET_SENSORMODE:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt && input_ctxt->qcarcam_hndl)
        {
            uint32_t param = 0;
            get_input_sensormode(&param);
            ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_STREAM_CONFIG_PARAM_INPUT_MODE, &param, sizeof(param));
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("QCarCamSetParam (%d) failed for Input handle 0x%lx, ret = %d", (int)QCARCAM_TEST_MENU_STREAM_SET_SENSORMODE, input_ctxt->qcarcam_hndl, ret);
            }
            else
            {
                QCARCAM_INFOMSG("QCarCamSetParam (%d) success for Input handle 0x%lx", (int)QCARCAM_TEST_MENU_STREAM_SET_SENSORMODE, input_ctxt->qcarcam_hndl);
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_GET_SENSORMODE:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt && input_ctxt->qcarcam_hndl)
        {
            uint32_t param = 0;

            ret = QCarCamGetParam(input_ctxt->qcarcam_hndl, QCARCAM_STREAM_CONFIG_PARAM_INPUT_MODE, &param, sizeof(param));
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("QCarCamGetParam (%d) failed for Input handle 0x%lx, ret = %d", (int)QCARCAM_TEST_MENU_STREAM_GET_SENSORMODE, input_ctxt->qcarcam_hndl, ret);
            }
            else
            {
                display_sensormode_settings(param);
            }

        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_GET_COLORSPACE:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt && input_ctxt->qcarcam_hndl)
        {
            QCarCamColorSpace_e param = QCARCAM_COLOR_SPACE_UNCORRECTED;
            ret = QCarCamGetParam(input_ctxt->qcarcam_hndl, QCARCAM_SENSOR_PARAM_COLOR_SPACE, &param, sizeof(param));

            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("qcarcam_g_param (%d) failed for Input handle 0x%lx, ret = %d", (int)QCARCAM_TEST_MENU_STREAM_GET_COLORSPACE, input_ctxt->qcarcam_hndl, ret);
            }
            else
            {
                QCARCAM_ALWZMSG("Color space for the Input handle 0x%lx = %d", input_ctxt->qcarcam_hndl, param);
                display_color_space_settings(param);
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_ENABLE_CALLBACK:
    {
        for (session_id = 0; session_id < gCtxt.numInputs; ++session_id)
        {
            input_ctxt = &gCtxt.inputs[session_id];
            if (input_ctxt->qcarcam_hndl && input_ctxt->use_event_callback)
            {
                uint32_t param = QCARCAM_EVENT_FRAME_READY | QCARCAM_EVENT_INPUT_SIGNAL | QCARCAM_EVENT_ERROR;

                ret = QCarCamRegisterEventCallback(input_ctxt->qcarcam_hndl, &qcarcam_test_event_cb, input_ctxt);
                if (ret != QCARCAM_RET_OK)
                {
                    QCARCAM_ERRORMSG("QCarCamSetParam failed for Input handle 0x%lx, ret = %d",input_ctxt->qcarcam_hndl, ret);
                    break;
                }

                ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_STREAM_CONFIG_PARAM_EVENT_MASK, &param, sizeof(param));
                if (ret != QCARCAM_RET_OK)
                {
                    QCARCAM_ERRORMSG("QCarCamSetParam failed for Input handle 0x%lx, ret = %d",input_ctxt->qcarcam_hndl, ret);
                }
                else
                {
                    QCARCAM_INFOMSG("QCarCamSetParam success for Input handle 0x%lx",input_ctxt->qcarcam_hndl);
                }
            }
            else
            {
                QCARCAM_ERRORMSG("Callback is disabled in xml, please check the input xml");
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_DISABLE_CALLBACK:
    {
        for (session_id = 0; session_id < gCtxt.numInputs; ++session_id)
        {
            input_ctxt = &gCtxt.inputs[session_id];
            if (input_ctxt->qcarcam_hndl && input_ctxt->use_event_callback)
            {
                uint32_t param = 0x0;

                ret = QCarCamUnregisterEventCallback(input_ctxt->qcarcam_hndl);
                if (ret != QCARCAM_RET_OK)
                {
                    QCARCAM_ERRORMSG("QCarCamSetParam failed for Input handle 0x%lx, ret = %d",input_ctxt->qcarcam_hndl, ret);
                    break;
                }

                ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_STREAM_CONFIG_PARAM_EVENT_MASK, &param, sizeof(param));
                if (ret != QCARCAM_RET_OK)
                {
                    QCARCAM_ERRORMSG("QCarCamSetParam failed for Input handle 0x%lx, ret = %d",input_ctxt->qcarcam_hndl, ret);
                }
                else
                {
                    QCARCAM_INFOMSG("QCarCamSetParam success for Input handle 0x%lx",input_ctxt->qcarcam_hndl);
                }
            }
            else
            {
                QCARCAM_ERRORMSG("Callback is disabled in xml, please check the input xml");
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_SET_FRAMERATE:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt && input_ctxt->qcarcam_hndl)
        {
            QCarCamFrameDropConfig_t param = {};
            get_input_framerate(&param);
            ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_STREAM_CONFIG_PARAM_FRAME_DROP_CONTROL, &param, sizeof(param));
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("QCarCamSetParam framerate failed for Input handle 0x%lx, ret = %d",input_ctxt->qcarcam_hndl, ret);
            }
            else
            {
                QCARCAM_INFOMSG("QCarCamSetParam framerate success for Input handle 0x%lx",input_ctxt->qcarcam_hndl);
            }
            QCARCAM_INFOMSG("Provided QCARCAM_STREAM_CONFIG_PARAM_FRAME_DROP_CONTROL: period = %d, pattern = 0x%x",
                param.frameDropPeriod,
                param.frameDropPattern);
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_SET_COLOR_PARAM:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt && input_ctxt->qcarcam_hndl)
        {
            char buf[BUFSIZE];
            char *p = NULL;
            int param_option = 0;
            float param_value = 0.0f;
            float param = 0.0f;

            printf("Select param: [0] Saturation, [1] Hue.\n");
            if (fgets(buf, sizeof(buf), stdin) != NULL)
            {
                /* 10 indicates decimal number*/
                param_option = strtol(buf, &p, 10);
            }

            switch (param_option)
            {
                case 0:
                    printf("Enter saturation value. Valid range (-1.0 to 1.0)\n");
                    if (fgets(buf, sizeof(buf), stdin) != NULL)
                    {
                        param_value = strtof(buf, NULL);
                    }

                    param = param_value;
                    ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_SENSOR_PARAM_SATURATION, &param, sizeof(param));
                    if (ret != QCARCAM_RET_OK)
                    {
                        QCARCAM_ERRORMSG("QCarCamSetParam failed for Input handle 0x%lx ret = %d\n",
                        input_ctxt->qcarcam_hndl, ret);
                    }
                    break;
                case 1:
                    printf("Enter hue value. Valid range (-1.0 to 1.0)\n");
                    if (fgets(buf, sizeof(buf), stdin) != NULL)
                    {
                        param_value = strtof(buf, NULL);
                    }

                    param = param_value;
                    ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_SENSOR_PARAM_HUE, &param, sizeof(param));
                    if (ret != QCARCAM_RET_OK)
                    {
                        QCARCAM_ERRORMSG("QCarCamSetParam failed for Input handle 0x%lx ret = %d\n",
                        input_ctxt->qcarcam_hndl, ret);
                    }
                    break;
                default:
                    printf("Param not available\n");
                    break;
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_GET_COLOR_PARAM:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt && input_ctxt->qcarcam_hndl)
        {
            char buf[BUFSIZE];
            char *p = NULL;
            int param_option = 0;
            float param = 0.0f;

            printf("Select param: [0] Saturation, [1] Hue.\n");
            if (fgets(buf, sizeof(buf), stdin) != NULL)
            {
                /* 10 indicates decimal number*/
                param_option = strtol(buf, &p, 10);
            }

            switch (param_option)
            {
                case 0:
                    ret = QCarCamGetParam(input_ctxt->qcarcam_hndl, QCARCAM_SENSOR_PARAM_SATURATION, &param, sizeof(param));
                    if (ret != QCARCAM_RET_OK)
                    {
                        QCARCAM_ERRORMSG("QCarCamGetParam failed for Input handle 0x%lx ret = %d",
                        input_ctxt->qcarcam_hndl, ret);
                    }
                    break;
                case 1:
                    ret = QCarCamGetParam(input_ctxt->qcarcam_hndl, QCARCAM_SENSOR_PARAM_HUE, &param, sizeof(param));
                    if (ret != QCARCAM_RET_OK)
                    {
                        QCARCAM_ERRORMSG("QCarCamGetParam failed for Input handle 0x%lx ret = %d",
                        input_ctxt->qcarcam_hndl, ret);
                    }
                    break;
                default:
                    printf("Param not available\n");
                    ret = -1;
                    break;
            }

            if (ret == 0)
            {
                printf("Param value returned = %.2f\n", param);
                QCARCAM_ALWZMSG("QCarCamGetParam value returned = %.2f", param);
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_DUMP_NEXT_FRAME:
    {
        char buf[BUFSIZE];
        char *p = NULL;
        int dump_option = 0;

        printf("Select dump option: \n [0] Dump frames of all bufferlists.\n [1] Dump frame for selected bufferlist.\n");
        if (fgets(buf, sizeof(buf), stdin) != NULL)
        {
            /* 10 indicates decimal number*/
            dump_option = strtol(buf, &p, 10);
        }

        if (0 == dump_option)
        {
            for (session_id = 0; session_id < gCtxt.numInputs; ++session_id)
            {
                input_ctxt = &gCtxt.inputs[session_id];
                for (unsigned int idx = 0; idx < input_ctxt->num_stream_instance; idx++)
                {
                    unsigned int bufferListId;
                    bufferListId = input_ctxt->buffers_param[idx][QCARCAM_TEST_BUFFERS_OUTPUT].bufferList_id;
                    input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].dumpNextFrame = TRUE;
                }
            }
        }
        else
        {
            unsigned int bufferListId;
            session_id = get_input_session_id();
            input_ctxt = get_input_ctxt(session_id);
            bufferListId = get_input_bufferlist_id(input_ctxt);
            input_ctxt->buffers_param[bufferListId][QCARCAM_TEST_BUFFERS_OUTPUT].dumpNextFrame = TRUE;
        }
        break;
    }
    case QCARCAM_TEST_MENU_CHECK_BUFFERS:
        gCtxt.check_buffer_state = !gCtxt.check_buffer_state;
        break;
    case QCARCAM_TEST_MENU_STREAM_SET_VENDOR_PARAM:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt && input_ctxt->qcarcam_hndl)
        {
            QCarCamVendorParam_t param = {};
#ifndef USE_VENDOR_EXT_PARAMS
            uint32_t* pData = (uint32_t*)param.data;
            pData[0] = 0x12345678;
            pData[1] = 0xfedcba98;
#else
            vendor_ext_property_t *p_isp_prop = (vendor_ext_property_t*)&(param.data[0]);
            p_isp_prop->type = VENDOR_EXT_PROP_TEST;
            p_isp_prop->value.uint_val = 0x12345678;
#endif

            ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_VENDOR_PARAM, &param, sizeof(param));
            if (QCARCAM_RET_OK != ret)
            {
                QCARCAM_ERRORMSG("QCarCamSetParam QCARCAM_VENDOR_PARAM failed for Input handle 0x%lx ret = %d",
                    input_ctxt->qcarcam_hndl, ret);
            }
            else
            {
#ifndef USE_VENDOR_EXT_PARAMS
                QCARCAM_INFOMSG("QCarCamSetParam QCARCAM_VENDOR_PARAM succeed for Input handle 0x%lx: data[0]=%u data[1]=%u",
                    input_ctxt->qcarcam_hndl, param.data[0],
                    param.data[1]);
#else
                QCARCAM_INFOMSG("QCarCamSetParam QCARCAM_VENDOR_PARAM succeed for Input handle 0x%lx: isp prop type=%u,val=%u",
                    input_ctxt->qcarcam_hndl, p_isp_prop->type ,
                    p_isp_prop->value.uint_val);
#endif
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_GET_VENDOR_PARAM:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt && input_ctxt->qcarcam_hndl)
        {
            QCarCamVendorParam_t param = {};
#ifndef USE_VENDOR_EXT_PARAMS
            param.data[0] = 0;
#else
            vendor_ext_property_t *p_isp_prop = (vendor_ext_property_t*)&(param.data[0]);
            p_isp_prop->type =  VENDOR_EXT_PROP_TEST;
#endif
            ret = QCarCamGetParam(input_ctxt->qcarcam_hndl, QCARCAM_VENDOR_PARAM, &param, sizeof(param));
            if (QCARCAM_RET_OK != ret)
            {
                QCARCAM_ERRORMSG("QCarCamGetParam QCARCAM_VENDOR_PARAM failed for Input handle 0x%lx ret = %d",
                    input_ctxt->qcarcam_hndl, ret);
            }
            else
            {
                QCARCAM_INFOMSG("QCarCamGetParam QCARCAM_VENDOR_PARAM succeeds");
#ifdef USE_VENDOR_EXT_PARAMS
                QCARCAM_INFOMSG("QCarCamGetParam QCARCAM_VENDOR_PARAM succeed for Input handle 0x%lx: isp prop type=%u,val=%u",
                    input_ctxt->qcarcam_hndl, p_isp_prop->type,
                    p_isp_prop->value.uint_val);
                printf("ISP param type = %u, value returned = %u\n", p_isp_prop->type,
                    p_isp_prop->value.uint_val);
#endif
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_SET_BRIGHTNESS:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt && input_ctxt->qcarcam_hndl)
        {
            char buf[BUFSIZE];
            float param_value = 0.0f;
            float param = 0.0f;

            printf("Enter brightness value. Valid range (-1.0 to 1.0)\n");
            if (fgets(buf, sizeof(buf), stdin) != NULL)
            {
                param_value = strtof(buf, NULL);
            }

            param = param_value;

            ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_SENSOR_PARAM_BRIGHTNESS, &param, sizeof(param));
            if (QCARCAM_RET_OK != ret)
            {
                QCARCAM_INFOMSG("QCarCamSetParam QCARCAM_SENSOR_PARAM_BRIGHTNESS succeed for Input handle 0x%lx: brightness=%d",
                    input_ctxt->qcarcam_hndl, ret);
            }
            else
            {
                QCARCAM_INFOMSG("QCarCamSetParam QCARCAM_SENSOR_PARAM_BRIGHTNESS succeed for Input handle 0x%lx: brightness=%f",
                    input_ctxt->qcarcam_hndl, param);
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_GET_BRIGHTNESS:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt && input_ctxt->qcarcam_hndl)
        {
            float param = 0.0f;

            ret = QCarCamGetParam(input_ctxt->qcarcam_hndl, QCARCAM_SENSOR_PARAM_BRIGHTNESS, &param, sizeof(param));
            if (QCARCAM_RET_OK != ret)
            {
                QCARCAM_ERRORMSG("QCarCamGetParam QCARCAM_SENSOR_PARAM_BRIGHTNESS failed for Input handle 0x%lx ret = %d",
                    input_ctxt->qcarcam_hndl, ret);
            }
            else
            {
                printf("Param value returned = %.2f\n", param);
                QCARCAM_INFOMSG("QCarCamGetParam QCARCAM_SENSOR_PARAM_BRIGHTNESS succeeded for Input handle 0x%lx: brightness=%f",
                    input_ctxt->qcarcam_hndl, param);
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_SET_CONTRAST:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt && input_ctxt->qcarcam_hndl)
        {
            char buf[BUFSIZE];
            float param_value = 0.0f;
            float param = 0.0f;

            printf("Enter contrast value. Valid range (-1.0 to 1.0)\n");
            if (fgets(buf, sizeof(buf), stdin) != NULL)
            {
                param_value = strtof(buf, NULL);
            }

            param = param_value;

            ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_SENSOR_PARAM_CONTRAST, &param, sizeof(param));
            if (QCARCAM_RET_OK != ret)
            {
                QCARCAM_ERRORMSG("QCarCamSetParam QCARCAM_SENSOR_PARAM_CONTRAST failed for Input handle 0x%lx ret = %d",
                    input_ctxt->qcarcam_hndl, ret);
            }
            else
            {
                QCARCAM_INFOMSG("QCarCamSetParam QCARCAM_SENSOR_PARAM_CONTRAST succeed for Input handle 0x%lx: contrast=%f",
                    input_ctxt->qcarcam_hndl, param);
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_GET_CONTRAST:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt && input_ctxt->qcarcam_hndl)
        {
            float param = 0.0f;

            ret = QCarCamGetParam(input_ctxt->qcarcam_hndl, QCARCAM_SENSOR_PARAM_CONTRAST, &param, sizeof(param));
            if (QCARCAM_RET_OK != ret)
            {
                QCARCAM_ERRORMSG("QCarCamGetParam QCARCAM_SENSOR_PARAM_CONTRAST failed for Input handle 0x%lx ret = %d",
                    input_ctxt->qcarcam_hndl, ret);
            }
            else
            {
                printf("Param value returned = %.2f\n", param);
                QCARCAM_INFOMSG("QCarCamGetParam QCARCAM_SENSOR_PARAM_CONTRAST succeeded for Input handle 0x%lx contrast=%f",
                    input_ctxt->qcarcam_hndl, param);
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_SET_MIRRORING:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt && input_ctxt->qcarcam_hndl)
        {
            char buf[BUFSIZE];
            char *p = NULL;
            int param_option = 0;
            uint32_t param_value = 0;
            uint32_t param = 0;

            printf("Select mirroring type: [0] Horizontal, [1] Vertical.\n");
            if (fgets(buf, sizeof(buf), stdin) != NULL)
            {
                /* 10 indicates decimal number*/
                param_option = strtol(buf, &p, 10);
            }

            switch (param_option)
            {
                case 0:
                    printf("Enter value. [1] Enable \n");
                    if (fgets(buf, sizeof(buf), stdin) != NULL)
                    {
                        param_value = strtoul(buf, NULL,10);
                    }

                    param = param_value;
                    ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_SENSOR_PARAM_MIRROR_H, &param, sizeof(param));
                    if (QCARCAM_RET_OK != ret)
                    {
                        QCARCAM_ERRORMSG("QCarCamSetParam failed for Input handle 0x%lx ret = %d\n",
                        input_ctxt->qcarcam_hndl, ret);
                    }
                    break;
                case 1:
                     printf("Enter value. [1] Enable \n");
                    if (fgets(buf, sizeof(buf), stdin) != NULL)
                    {
                        param_value = strtoul(buf, NULL,10);
                    }

                    param = param_value;
                    ret = QCarCamSetParam(input_ctxt->qcarcam_hndl, QCARCAM_SENSOR_PARAM_MIRROR_V, &param, sizeof(param));
                    if (QCARCAM_RET_OK != ret)
                    {
                        QCARCAM_ERRORMSG("QCarCamSetParam failed for Input handle 0x%lx ret = %d\n",
                        input_ctxt->qcarcam_hndl, ret);
                    }
                    break;
                default:
                    printf("Param not available\n");
                    break;
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_GET_MIRRORING:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt && input_ctxt->qcarcam_hndl)
        {
            char buf[BUFSIZE];
            char *p = NULL;
            int param_option = 0;
            uint32_t param = 0;

            printf("Select mirroring type: [0] Horizontal, [1] Vertical.\n");
            if (fgets(buf, sizeof(buf), stdin) != NULL)
            {
                /* 10 indicates decimal number*/
                param_option = strtol(buf, &p, 10);
            }

            switch (param_option)
            {
                case 0:
                    ret = QCarCamGetParam(input_ctxt->qcarcam_hndl, QCARCAM_SENSOR_PARAM_MIRROR_H, &param, sizeof(param));
                    if (QCARCAM_RET_OK != ret)
                    {
                        QCARCAM_ERRORMSG("QCarCamGetParam failed for Input handle 0x%lx ret = %d",
                        input_ctxt->qcarcam_hndl, ret);
                    }
                    break;
                case 1:
                    ret = QCarCamGetParam(input_ctxt->qcarcam_hndl, QCARCAM_SENSOR_PARAM_MIRROR_V, &param, sizeof(param));
                    if (QCARCAM_RET_OK != ret)
                    {
                        QCARCAM_ERRORMSG("QCarCamGetParam failed for Input handle 0x%lx ret = %d",
                        input_ctxt->qcarcam_hndl, ret);
                    }
                    break;
                default:
                    printf("Param not available\n");
                    ret = -1;
                    break;
            }

            if (ret == 0)
            {
                printf("Param value returned = %u\n", param);
                QCARCAM_ALWZMSG("QCarCamGetParam value returned = %u", param);
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_SUBSCRIBE_CHANGE_EVENT:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);

        if(input_ctxt && input_ctxt->qcarcam_hndl)
        {
            char buf[BUFSIZE];
            char *p = NULL;
            uint64_t param_option = 0;
            printf("Select setting in hexadecimal (1<<qcarcam_param_t)\n"
                "EXPOSURE [(1<<17) -- 0x20000] HDR EXPOSURE [1<<20 -- 0x100000]\n"
                "SATURATION [1<<19 -- 0x80000] HUE [1<<18 -- 0x40000]\n"
                "FRAME RATE [1<<9 -- 0x200] GAMMA [1<<21 -- 0x200000]\n");
            if (fgets(buf, sizeof(buf), stdin) != NULL)
            {
                param_option = strtoul(buf, &p, 0);
                qcarcam_test_subscribe_input_params_change(input_ctxt, param_option);
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_UNSUBSCRIBE_CHANGE_EVENT:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);

        if(input_ctxt && input_ctxt->qcarcam_hndl)
        {
            char buf[BUFSIZE];
            char *p = NULL;
            uint64_t param_option = 0;
            printf("Select setting in hexadecimal (1<<qcarcam_param_t)\n"
                "EXPOSURE [(1<<17) -- 0x20000] HDR EXPOSURE [1<<20 -- 0x100000]\n"
                "SATURATION [1<<19 -- 0x80000] HUE [1<<18 -- 0x40000]\n"
                "FRAME RATE [1<<9 -- 0x200] GAMMA [1<<21 -- 0x200000]\n");
            if (fgets(buf, sizeof(buf), stdin) != NULL)
            {
                param_option = strtoul(buf, &p, 0);
                qcarcam_test_unsubscribe_input_params_change(input_ctxt, param_option);
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_SUBSCRIBE_SOF_EVENT:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);

        if(input_ctxt && input_ctxt->qcarcam_hndl)
        {
            qcarcam_test_subscribe_SOF_event(input_ctxt);
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_UNSUBSCRIBE_SOF_EVENT:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);

        if(input_ctxt && input_ctxt->qcarcam_hndl)
        {
            qcarcam_test_unsubscribe_SOF_event(input_ctxt);
        }
        break;
    }
    case QCARCAM_TEST_MENU_MASTER:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if(input_ctxt && input_ctxt->qcarcam_hndl)
        {
            unsigned int flag = get_input_set_flag();
            if (flag == 1)
            {
                // to prevent setting a master stream as master again
                if(input_ctxt->is_master)
                {
                    QCARCAM_INFOMSG("This client is already set as the master");
                }
                else
                {
                    qcarcam_test_set_master(input_ctxt, flag);
                    if(!input_ctxt->is_master)
                    {
                        QCARCAM_INFOMSG("There is already another client set as master");
                    }
                }

            }
            else if (flag == 0)
            {
                if(!input_ctxt->is_master)
                {
                    QCARCAM_INFOMSG("This client is not master in order to release, session_id:%d", session_id);
                }
                else
                {
                    qcarcam_test_set_master(input_ctxt, flag);
                }
            }
            else
            {
                QCARCAM_ERRORMSG("Invalid option (%d)", flag);
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_GET_SYSTEM_DIAGNOSTICS:
    {
        QCarCamDiagInfo* pDiagnosticInfo = (QCarCamDiagInfo*)calloc(1, sizeof(QCarCamDiagInfo));
        if (!pDiagnosticInfo)
        {
            QCARCAM_ERRORMSG("Failed to allocate diagnosticInfo");
            break;
        }

        ret = QCarCamQueryDiagnostics(pDiagnosticInfo);
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("QCarCamQueryDiagnostics failed with ret = %d", ret);
        }
        else
        {
            parse_diagnostics(pDiagnosticInfo);
            QCARCAM_INFOMSG("QCarCamQueryDiagnostics is success and parsed");
        }

        free(pDiagnosticInfo);
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_SET_ANTIBANDING_MODE:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if (input_ctxt && input_ctxt->qcarcam_hndl)
        {
            test_util_metadata_tag_t tag = {};
            tag.tagId = ANDROID_CONTROL_AE_ANTIBANDING_MODE;
            printf("data ?");
            printf("\n0, ...0FF\n1, ...50HZ\n2, ...60Hz\n3, ...AUTO\n");
            get_metadata_data(&tag);

            uint32_t next_meta_idx = (input_ctxt->input_meta_idx + 1) % input_ctxt->meta_bufferList.nBuffers;

            ret = test_util_append_metadata_tag(input_ctxt->metabuffer_window, &tag);

            if (!ret)
            {
                printf("Will apply metadata index %d next frame\n", next_meta_idx);
                input_ctxt->input_meta_idx = next_meta_idx;
            }
            else
            {
                printf("failed to append tag - ret %d\n", ret);
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_SET_METADATA_TAG:
    {
        session_id = get_input_session_id();
        input_ctxt = get_input_ctxt(session_id);
        if(input_ctxt && input_ctxt->qcarcam_hndl)
        {
            test_util_metadata_tag_t tag = {};
            get_metadata_tag(&tag);

            uint32_t next_meta_idx = (input_ctxt->input_meta_idx + 1) % input_ctxt->meta_bufferList.nBuffers;

            ret = test_util_append_metadata_tag(input_ctxt->metabuffer_window, &tag);
            if (!ret)
            {
                printf("Will apply metadata index %d next frame\n", next_meta_idx);
                input_ctxt->input_meta_idx = next_meta_idx;
            }
            else
            {
                printf("failed to append tag - ret %d\n", ret);
            }
        }
        break;
    }
    case QCARCAM_TEST_MENU_STREAM_GET_METADATA_TAG:
    default:
        QCARCAM_ERRORMSG("Invalid option (%d)", option);
        break;
    }
}

//
void qcarcam_test_menu(void)
{
    // Wait till all streams starts
    while (!g_aborted && gCtxt.opened_stream_cnt < gCtxt.numInputs)
    {
        usleep(10000);
    }
    display_menu();

    while (!g_aborted)
    {
        usleep(100000);

        char buf[BUFSIZE] = "";
        const char* res = fgets(buf, sizeof(buf)-1, stdin);

        if (res == NULL)
        {
            continue;
        }
        char option = buf[0];

        switch (option)
        {
        case 'e':
        abort_test();
        continue;
        case 's':
            process_cmds(QCARCAM_TEST_MENU_DUMP_NEXT_FRAME);
            continue;
        case 'h':
            display_menu();
            continue;
        default:
            break;
        };

        int NumericCommand = 0;
        sscanf(buf, "%3d", & NumericCommand);

        process_cmds(NumericCommand); // if can't parse anything, it is ok. process_cmds(0) will do nothing
    }
}

/* ===========================================================================
 * Copyright (c) 2017-2022 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
=========================================================================== */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/keycodes.h>
#include <time.h>
#include <screen/screen.h>
#include <screen/screen_ext.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/resmgr.h>
#include <dcmd_pm.h>

#include "test_util.h"
#include "test_util_qnx.h"
#ifdef POST_PROCESS
#include <dlfcn.h>
#include "post_process.h"
#endif
#include "pmem.h"
#include "camera_metadata.h"
#include "qcarcam_metadata.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <gpio_devctl.h>
#include "gpio_client.h"

#ifdef __cplusplus
}
#endif

#define CAMERA_POWER_PREPARE_PULSE  (_PULSE_CODE_MINAVAIL + 1)
#define CAMERA_POWER_SUSPEND_PULSE  (_PULSE_CODE_MINAVAIL + 2)
#define CAMERA_POWER_RESUME_PULSE   (_PULSE_CODE_MINAVAIL + 3)
#define CAMERA_POWER_COMPLETE_PULSE (_PULSE_CODE_MINAVAIL + 4)
#define CAMERA_POWER_BREAKOUT_PULSE (_PULSE_CODE_MINAVAIL + 10)

typedef enum
{
    TEST_UTIL_PATTERN_BLACK = 0
}test_util_pattern_t;


static int g_aborted = 0;
static int g_fd_gpio;

//power manager resource

typedef struct
{
    int fd;
    QCXThread_t thread_id;
    int channel_id;
    int connect_id;
    void* event_client_data;
    power_event_callable p_power_event_callback;
}test_util_pm_handle_t;

static test_util_pm_handle_t g_pm_handle;

#ifdef POST_PROCESS
int test_util_get_num_planes(test_util_color_fmt_t fmt)
{
    int num_planes = 1;

    switch (fmt)
    {
    case TESTUTIL_FMT_RGB_888:
    case TESTUTIL_FMT_MIPIRAW_8:
    case TESTUTIL_FMT_MIPIRAW_10:
    case TESTUTIL_FMT_MIPIRAW_12:
    case TESTUTIL_FMT_UYVY_8:
    case TESTUTIL_FMT_UYVY_10:
    case TESTUTIL_FMT_YU12:
    case TESTUTIL_FMT_BGRX1010102:
    case TESTUTIL_FMT_RGBX1010102:
    case TESTUTIL_FMT_UYVY422_10BITS:
        break;
    case TESTUTIL_FMT_NV12:
    case TESTUTIL_FMT_NV21:
    case TESTUTIL_FMT_P010:
    case TESTUTIL_FMT_P01210:
    case TESTUTIL_FMT_P01208:
    case TESTUTIL_FMT_P010_LSB:
    case TESTUTIL_FMT_P01210_LSB:
    case TESTUTIL_FMT_P01208_LSB:
        num_planes = 2;
        break;
    case TESTUTIL_FMT_R8_G8_B8:
        num_planes = 3;
        break;
    default:
        break;
    }
    return num_planes;
}
#endif

static const QCarCamUBWCTileInfo QCarCamSupportedUBWCTileInfo[] =
{
    { 48, 64, 4, 256, 16, 4, 3 }, // UBWC_TP10-Y

    { 64, 64, 4, 256, 16, 1, 1 }, // UBWC_NV12-4R-Y

    { 32, 32, 8, 128, 32, 1, 1 }, // UBWC_NV12-Y

    { 32, 64, 4, 256, 16, 2, 1 }, // UBWC_P010
};

#define CEILING_POS(X) ((X-(int)(X)) > 0 ? (int)(X+1) : (int)(X))
#define CEILING_NEG(X) ((X-(int)(X)) < 0 ? (int)(X-1) : (int)(X))
#define ceil(X) ( ((X) > 0) ? CEILING_POS(X) : CEILING_NEG(X) )

void QCarCamSetupUBWCPlanes(QCarCamPlane_t* pPlane, const QCarCamUBWCTileInfo* pTileInfo)
{
        uint localWidth         = CAM_ALIGN_SIZE(pPlane->width, pTileInfo->widthPixels);
        uint localStride = (localWidth / pTileInfo->BPPDenominator) * pTileInfo->BPPNumerator;
        float local1            = static_cast<float>((static_cast<float>(pPlane->width) / pTileInfo->widthBytes)) / 64;
        uint32_t metadataStride  = static_cast<UINT32>(ceil(local1)) * 1024;
        float local2            = static_cast<float>((static_cast<float>(pPlane->height) / pTileInfo->height)) / 16;
        uint  localMetaSize     = static_cast<UINT32>(ceil(local2)) * metadataStride;
        uint32_t metadataSize    = CAM_ALIGN_SIZE(localMetaSize, 4096);
        //uint32_t metadataHeight  = metadataSize / metadataStride;
        pPlane->stride         =   CAM_ALIGN_SIZE(localStride, pTileInfo->widthMacroTile);
        uint32_t sliceHeight     = CAM_ALIGN_SIZE(pPlane->height, pTileInfo->heightMacroTile);
        uint32_t imageplaneSize  = CAM_ALIGN_SIZE((pPlane->stride * sliceHeight), 4096);
        pPlane->size           = metadataSize + imageplaneSize;

}

static void test_util_fill_planes(QCarCamBuffer_t* p_buffer, QCarCamColorFmt_e fmt)
{
    const QCarCamUBWCTileInfo*  pTileInfo = NULL;
    switch (fmt)
    {
    case QCARCAM_FMT_RGB_888:
        p_buffer->planes[0].stride = CAM_ALIGN_SIZE(p_buffer->planes[0].width * 3, 16);
        p_buffer->planes[0].size = p_buffer->planes[0].stride * p_buffer->planes[0].height;
        break;
    case QCARCAM_FMT_R8_G8_B8:
        p_buffer->numPlanes = 3;

        p_buffer->planes[0].stride = CAM_ALIGN_SIZE(p_buffer->planes[0].width, 256);

        //plane 2
        p_buffer->planes[1].width = p_buffer->planes[0].width;
        p_buffer->planes[1].height = p_buffer->planes[0].height;
        p_buffer->planes[1].stride = CAM_ALIGN_SIZE(p_buffer->planes[1].width, 256);

        //plane 3
        p_buffer->planes[2].width = p_buffer->planes[0].width;
        p_buffer->planes[2].height = p_buffer->planes[0].height;
        p_buffer->planes[2].stride = CAM_ALIGN_SIZE(p_buffer->planes[2].width, 256);

        p_buffer->planes[0].size = p_buffer->planes[0].stride * CAM_ALIGN_SIZE(p_buffer->planes[0].height, 32);
        p_buffer->planes[1].size = p_buffer->planes[1].stride * CAM_ALIGN_SIZE(p_buffer->planes[1].height, 32);
        p_buffer->planes[2].size = p_buffer->planes[2].stride * CAM_ALIGN_SIZE(p_buffer->planes[2].height, 32);
        break;
    case QCARCAM_FMT_MIPIRAW_8:
        p_buffer->planes[0].stride = CAM_ALIGN_SIZE(p_buffer->planes[0].width, 16);
        p_buffer->planes[0].size = p_buffer->planes[0].stride * p_buffer->planes[0].height;
        break;
    case QCARCAM_FMT_MIPIRAW_10:
        if (0 == (p_buffer->planes[0].width % 4))
        {
            p_buffer->planes[0].stride = CAM_ALIGN_SIZE((p_buffer->planes[0].width * 5 / 4), 16);
        }
        p_buffer->planes[0].size = p_buffer->planes[0].stride * p_buffer->planes[0].height;
        break;
    case QCARCAM_FMT_MIPIRAW_12:
        if (0 == (p_buffer->planes[0].width % 2))
        {
            p_buffer->planes[0].stride = CAM_ALIGN_SIZE((p_buffer->planes[0].width * 3 / 2), 16);
        }
        p_buffer->planes[0].size = p_buffer->planes[0].stride * p_buffer->planes[0].height;
        break;
    case QCARCAM_FMT_MIPIRAW_14:
        if (0 == (p_buffer->planes[0].width % 4))
        {
            p_buffer->planes[0].stride = CAM_ALIGN_SIZE((p_buffer->planes[0].width * 7 / 4), 16);
        }
        p_buffer->planes[0].size = p_buffer->planes[0].stride * p_buffer->planes[0].height;
        break;
    case QCARCAM_FMT_MIPIRAW_16:
        p_buffer->planes[0].stride = CAM_ALIGN_SIZE((p_buffer->planes[0].width * 2), 16);
        p_buffer->planes[0].size = p_buffer->planes[0].stride * p_buffer->planes[0].height;
        break;
    case QCARCAM_FMT_UYVY_8:
        p_buffer->planes[0].stride = CAM_ALIGN_SIZE((p_buffer->planes[0].width * 2), 16);
        p_buffer->planes[0].size = p_buffer->planes[0].stride * p_buffer->planes[0].height;
        break;
    case QCARCAM_FMT_UYVY_10:
        p_buffer->planes[0].stride = CAM_ALIGN_SIZE((p_buffer->planes[0].width * 4), 16);
        p_buffer->planes[0].size = p_buffer->planes[0].stride * p_buffer->planes[0].height;
        break;
    case QCARCAM_FMT_MIPIUYVY_10:
        p_buffer->planes[0].stride = CAM_ALIGN_SIZE((p_buffer->planes[0].width * 5 / 2), 16);
        p_buffer->planes[0].size = p_buffer->planes[0].stride * p_buffer->planes[0].height;
        break;
    case QCARCAM_FMT_UYVY_12:
        if (0 == (p_buffer->planes[0].width % 2))
        {
            p_buffer->planes[0].stride = CAM_ALIGN_SIZE(p_buffer->planes[0].width * 2 * 3 / 2, 16);
        }
        p_buffer->planes[0].size = p_buffer->planes[0].stride * p_buffer->planes[0].height;
        break;
    case QCARCAM_FMT_NV12:
    case QCARCAM_FMT_NV21:
        p_buffer->numPlanes = 2;

        p_buffer->planes[0].stride = CAM_ALIGN_SIZE(p_buffer->planes[0].width, 128);

        //plane 2
        p_buffer->planes[1].width = p_buffer->planes[0].width;
        p_buffer->planes[1].height = p_buffer->planes[0].height / 2;
        p_buffer->planes[1].stride = CAM_ALIGN_SIZE(p_buffer->planes[0].width, 128);

        p_buffer->planes[0].size = p_buffer->planes[0].stride * CAM_ALIGN_SIZE(p_buffer->planes[0].height, 32);
        p_buffer->planes[1].size = p_buffer->planes[1].stride * CAM_ALIGN_SIZE(p_buffer->planes[1].height, 32);
        break;
    case QCARCAM_FMT_UBWC_NV12:
        /*TODO: untested. Need to updated with tile settings*/
        p_buffer->numPlanes = 2;

        p_buffer->planes[0].stride = CAM_ALIGN_SIZE(p_buffer->planes[0].width, 256);

        //plane 2
        p_buffer->planes[1].width = p_buffer->planes[0].width;
        p_buffer->planes[1].height = p_buffer->planes[0].height / 2;
        p_buffer->planes[1].stride = CAM_ALIGN_SIZE(p_buffer->planes[0].width, 256);

        p_buffer->planes[0].size = p_buffer->planes[0].stride * CAM_ALIGN_SIZE(p_buffer->planes[0].height, 32);
        p_buffer->planes[1].size = p_buffer->planes[1].stride * CAM_ALIGN_SIZE(p_buffer->planes[1].height, 32);
        break;
    case QCARCAM_FMT_UBWC_TP10:

        pTileInfo = &(QCarCamSupportedUBWCTileInfo[0]);
        p_buffer->numPlanes = 2;
        p_buffer->planes[1].width = p_buffer->planes[0].width;
        p_buffer->planes[1].height = p_buffer->planes[0].height / 2;
        
        QCarCamSetupUBWCPlanes(&(p_buffer->planes[0]),pTileInfo);
        QCARCAM_ERRORMSG("qcarcam buffer : plane 0 size: %d", p_buffer->planes[0].size);
        QCarCamSetupUBWCPlanes(&(p_buffer->planes[1]),pTileInfo);
        QCARCAM_ERRORMSG("qcarcam buffer : plane 1 size: %d", p_buffer->planes[1].size);
        break;
    case QCARCAM_FMT_PLAIN16_10:
    case QCARCAM_FMT_PLAIN16_12:
    case QCARCAM_FMT_PLAIN16_14:
    case QCARCAM_FMT_PLAIN16_16:
        p_buffer->planes[0].stride = CAM_ALIGN_SIZE((p_buffer->planes[0].width * 2), 16);
        p_buffer->planes[0].size = p_buffer->planes[0].stride * p_buffer->planes[0].height;
        break;
    case QCARCAM_FMT_P010LSB:
    case QCARCAM_FMT_P01210LSB:
    case QCARCAM_FMT_P010:
    case QCARCAM_FMT_P01210:
    {
        // Y_Stride : Width * 2 aligned to 256
        // UV_Stride : Width * 2 aligned to 256
        // Y_Scanlines : Height aligned to 32
        // UV_Scanlines : Height / 2 aligned to 16
        // Total size = align((Y_Stride * Y_Scanlines
        //
        uint32_t width_align = 256;
        uint32_t height_align = 32;

        //plane 2
        p_buffer->numPlanes = 2;
        p_buffer->planes[1].height = p_buffer->planes[0].height / 2;
        p_buffer->planes[1].width = p_buffer->planes[0].width;
        p_buffer->planes[0].stride = CAM_ALIGN_SIZE(p_buffer->planes[0].width*2, width_align);
        p_buffer->planes[1].stride = p_buffer->planes[0].stride;

        p_buffer->planes[0].size = p_buffer->planes[0].stride * CAM_ALIGN_SIZE(p_buffer->planes[0].height, height_align);
        p_buffer->planes[1].size = p_buffer->planes[1].stride * p_buffer->planes[1].height;
        p_buffer->planes[0].size = CAM_ALIGN_SIZE(p_buffer->planes[0].size, 4096);
        break;
    }
    case QCARCAM_FMT_P01208LSB:
    case QCARCAM_FMT_P01208:
    {
        // Y_Stride : Width * 2 aligned to 256
        // UV_Stride : Width * 2 aligned to 256
        // Y_Scanlines : Height aligned to 32
        // UV_Scanlines : Height / 2 aligned to 16
        // Total size = align((Y_Stride * Y_Scanlines
        //
        uint32_t width_align = 256;
        uint32_t height_align = 32;

        //plane 2
        p_buffer->numPlanes = 2;
        p_buffer->planes[1].height = p_buffer->planes[0].height / 2;
        p_buffer->planes[1].width = p_buffer->planes[0].width;
        p_buffer->planes[0].stride = CAM_ALIGN_SIZE(p_buffer->planes[0].width*2, width_align);
        p_buffer->planes[1].stride = CAM_ALIGN_SIZE(p_buffer->planes[0].width, width_align);

        p_buffer->planes[0].size = p_buffer->planes[0].stride * CAM_ALIGN_SIZE(p_buffer->planes[0].height, height_align);
        p_buffer->planes[1].size = p_buffer->planes[1].stride * p_buffer->planes[1].height;
        p_buffer->planes[0].size = CAM_ALIGN_SIZE(p_buffer->planes[0].size, 4096);
        break;
    }
    case QCARCAM_FMT_BGRX_1010102:
    case QCARCAM_FMT_RGBX_1010102:
        p_buffer->numPlanes = 1;
        p_buffer->planes[0].stride = CAM_ALIGN_SIZE(p_buffer->planes[0].width*4, 16);
        p_buffer->planes[1].stride = 0;
        p_buffer->planes[0].size = p_buffer->planes[0].stride * p_buffer->planes[0].height;
        break;
     case QCARCAM_FMT_RGBX_8888:
     case QCARCAM_FMT_BGRX_8888:
        p_buffer->numPlanes = 1;
        p_buffer->planes[0].stride = CAM_ALIGN_SIZE(p_buffer->planes[0].width*4, 16);
        p_buffer->planes[0].size = p_buffer->planes[0].stride * p_buffer->planes[0].height;
        break;
    default:
    {
        QCarCamColorPack_e pack = QCARCAM_COLOR_GET_PACK(fmt);
        QCarCamColorPattern_e pattern = QCARCAM_COLOR_GET_PATTERN(fmt);

        if ((pack == QCARCAM_PACK_PLAIN16 || pack == QCARCAM_PACK_PLAIN16_LSB) &&
            pattern >= QCARCAM_YUV_YUYV && pattern <= QCARCAM_YUV_VYUY)
        {
            //2bytes per channel and 2channels per pixel
            p_buffer->planes[0].stride = CAM_ALIGN_SIZE((p_buffer->planes[0].width * 2 * 2), 16);
        }
        else
        {
            QCARCAM_ERRORMSG("non implemented format 0x%x", fmt);
        }
    }
        p_buffer->planes[0].size = p_buffer->planes[0].stride * p_buffer->planes[0].height;
        break;
    }
    QCARCAM_DBGMSG("plane0 size = %d  plane1 size = %d",p_buffer->planes[0].size,p_buffer->planes[1].size);
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_pm_event_handle
///
/// @brief power manager event handle
///
/// @param NULL
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
static int test_util_pm_event_handle(void* arg)
{
    int rc = 0;
    struct pm_ack_s ack = {.rc = QCARCAM_RET_OK};
    struct _pulse pulse;

    while(!g_aborted)
    {
        rc = MsgReceivePulse(g_pm_handle.channel_id, &pulse, sizeof(struct _pulse), NULL) ;
        if (rc != QCARCAM_RET_OK)
        {
           QCARCAM_ERRORMSG("MsgReceivePulse failed %d", rc);
           break;
        }
        switch (pulse.code)
        {
        case CAMERA_POWER_PREPARE_PULSE:
            QCARCAM_ERRORMSG("PM_STATE_PREPARE received)");
            // Send ACK to power-manager
            ack.state = PM_STATE_PREPARE;
            ack.rc = rc;
            rc = devctl(g_pm_handle.fd, DCMD_PM_ACK, &ack, sizeof(struct pm_ack_s), NULL);
            if (rc != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("devctl(DCMD_PM_ACK) failed, rc =%d", rc);
            }
            break;
        case CAMERA_POWER_SUSPEND_PULSE:
            QCARCAM_ERRORMSG("PM_STATE_SUSPEND received");
            rc = g_pm_handle.p_power_event_callback(TEST_UTIL_PM_SUSPEND, g_pm_handle.event_client_data);
            // Send ACK to power-manager
            ack.state = PM_STATE_SUSPEND;
            ack.rc = rc;
            rc = devctl(g_pm_handle.fd, DCMD_PM_ACK, &ack, sizeof(struct pm_ack_s), NULL);
            if (rc != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("devctl(DCMD_PM_ACK) failed, rc =%d", rc);
            }
            break;
        case CAMERA_POWER_RESUME_PULSE:
            QCARCAM_INFOMSG("PM_STATE_RESUME received)");
            rc = g_pm_handle.p_power_event_callback(TEST_UTIL_PM_RESUME, g_pm_handle.event_client_data);
            // Send ACK to power-manager
            ack.state = PM_STATE_RESUME;
            ack.rc = rc;
            rc = devctl(g_pm_handle.fd, DCMD_PM_ACK, &ack, sizeof(struct pm_ack_s), NULL);
            if (rc != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("devctl(DCMD_PM_ACK) failed, rc =%d", rc);
            }
            break;
        case CAMERA_POWER_COMPLETE_PULSE:
            QCARCAM_INFOMSG("PM_STATE_COMPLETE received)");
            // Send ACK to power-manager
            ack.state = PM_STATE_COMPLETE;
            ack.rc = rc;
            rc = devctl(g_pm_handle.fd, DCMD_PM_ACK, &ack, sizeof(struct pm_ack_s), NULL);
            if (rc != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("devctl(DCMD_PM_ACK) failed, rc =%d", rc);
            }
            break;
        case _PULSE_CODE_DISCONNECT:
            QCARCAM_ERRORMSG("PM has disconnected\n");
            goto out;
        case CAMERA_POWER_BREAKOUT_PULSE:
            QCARCAM_ERRORMSG("Deregistering pm client");
            goto out;
        default:
            QCARCAM_ERRORMSG("Unknown pulse received, code=%d\n", pulse.code);
            break;
        }
    }
out:
    if (g_pm_handle.connect_id != -1)
    {
        ConnectDetach(g_pm_handle.connect_id);
    }
    if (g_pm_handle.channel_id != -1)
    {
        ChannelDestroy(g_pm_handle.channel_id);
    }
    if (g_pm_handle.fd > 0)
    {
        close(g_pm_handle.fd);
    }
    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_register_pm_resource
///
/// @brief create power manager channel, register power resource
///
/// @param NULL
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
int test_util_register_pm_resource(void)
{
    int rc = QCARCAM_RET_OK;
    struct pm_register_s pm_register = {};
    char name[64];
    QCARCAM_INFOMSG("enter ");
    g_pm_handle.fd = open("/dev/pm", O_RDWR | O_CLOEXEC);
    if (g_pm_handle.fd < 0)
    {
        QCARCAM_ERRORMSG("Power Manager does not exist, err=%d\n", errno);
        return rc;
    }

    // Create a channel that can be used to receive messages and pulses.
    // Once created, the channel is owned by the process and isn't bound to the creating thread.
    g_pm_handle.channel_id = ChannelCreate(_NTO_CHF_DISCONNECT | _NTO_CHF_UNBLOCK);
    if(g_pm_handle.channel_id == -1)
    {
        QCARCAM_ERRORMSG("ChannelCreate failed (%s)", strerror(errno));
        goto out;
    }

    g_pm_handle.connect_id = ConnectAttach(0 , 0, g_pm_handle.channel_id,
              _NTO_SIDE_CHANNEL, _NTO_COF_CLOEXEC);
    if (g_pm_handle.connect_id == -1) {
        QCARCAM_ERRORMSG("ConnectAttach() failed (%s)", strerror(errno));
        goto out;
    }

    //INIT_PM_REGISTER_STRUCT(&pm_register);

    strlcpy(pm_register.name, "camera_client_pm", sizeof(pm_register.name));
    pm_register.pulse_codes[PM_STATE_PREPARE]  = CAMERA_POWER_PREPARE_PULSE;
    pm_register.pulse_codes[PM_STATE_SUSPEND]  = CAMERA_POWER_SUSPEND_PULSE;
    pm_register.pulse_codes[PM_STATE_RESUME]   = CAMERA_POWER_RESUME_PULSE;
    pm_register.pulse_codes[PM_STATE_COMPLETE] = CAMERA_POWER_COMPLETE_PULSE;
    pm_register.pulse_codes[PM_STATE_VOLT_NOK] = -1;
    pm_register.pulse_codes[PM_STATE_VOLT_OK]  = -1;
    pm_register.priority = PM_PRIO_LEVEL_1;
    pm_register.flags    = 0;
    pm_register.chid     = g_pm_handle.channel_id;
    snprintf(name, sizeof(name), "test_util_pm_event_handle");

    rc = devctl(g_pm_handle.fd, DCMD_PM_REGISTER, &pm_register, sizeof(struct pm_register_s), NULL);
    if (rc != QCARCAM_RET_OK) {
        QCARCAM_ERRORMSG("devctl(DCMD_PM_REGISTER) failed, rc =%d", rc);
        goto out;
    }

    rc = OSAL_ThreadCreate(QCARCAM_THRD_PRIO,
                            &test_util_pm_event_handle,
                            NULL,
                            0,
                            name,
                            &g_pm_handle.thread_id);

    QCARCAM_INFOMSG("exit ");
    return rc;

out:
    if (g_pm_handle.connect_id != -1)
    {
        ConnectDetach(g_pm_handle.connect_id);
    }

    if (g_pm_handle.channel_id != -1)
    {
        ChannelDestroy(g_pm_handle.channel_id);
    }
    if (g_pm_handle.fd > 0)
    {
        close(g_pm_handle.fd);
    }
    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_init
///
/// @brief Initialize context that is to be used to display content on the screen.
///
/// @param ctxt   Pointer to context to be initialized
/// @param params Parameters to init ctxt
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_init(test_util_ctxt_t **pp_ctxt, test_util_ctxt_params_t *p_params)
{
    int rc, i;
    test_util_ctxt_t* pCtxt;

    *pp_ctxt = NULL;

    if (!p_params)
    {
        return QCARCAM_RET_BADPARAM;
    }

    pCtxt = (test_util_ctxt_t*)calloc(1, sizeof(struct test_util_ctxt_t));
    if (!pCtxt)
    {
        return QCARCAM_RET_NOMEM;
    }

    pCtxt->params = *p_params;
    pCtxt->allocator = (test_util_allocator_t)pCtxt->params.offscreen_allocator;

    if (pCtxt->params.enable_gpio_config)
    {
        g_fd_gpio = gpio_open(NULL);
        if (g_fd_gpio == -1)
        {
            QCARCAM_ERRORMSG("gpio_open() failed");
            goto fail;
        }
    }

    if(!pCtxt->params.disable_display)
    {
        rc = screen_create_context(&pCtxt->screen_ctxt, 0);
        if (rc)
        {
            perror("screen_context_create");
            goto fail;
        }

        /*query displays*/
        rc = screen_get_context_property_iv(pCtxt->screen_ctxt, SCREEN_PROPERTY_DISPLAY_COUNT,
                                            &pCtxt->screen_ndisplays);
        if (rc)
        {
            perror("screen_get_context_property_iv(SCREEN_PROPERTY_DISPLAY_COUNT)");
            goto fail;
        }

        pCtxt->screen_display = (screen_display_t*)calloc(pCtxt->screen_ndisplays, sizeof(*pCtxt->screen_display));
        if (pCtxt->screen_display == NULL)
        {
            perror("could not allocate memory for display list");
            goto fail;
        }

        pCtxt->display_property = (test_util_display_prop_t*)calloc(pCtxt->screen_ndisplays, sizeof(*pCtxt->display_property));
        if (pCtxt->display_property == NULL)
        {
            perror("could not allocate memory for display list");
            goto fail;
        }

        rc = screen_get_context_property_pv(pCtxt->screen_ctxt, SCREEN_PROPERTY_DISPLAYS,
                                            (void **)pCtxt->screen_display);
        if (rc)
        {
            perror("screen_get_context_property_ptr(SCREEN_PROPERTY_DISPLAYS)");
            goto fail;
        }

        for (i = 0; i < pCtxt->screen_ndisplays; ++i)
        {
            rc = screen_get_display_property_iv(pCtxt->screen_display[i],
                                                SCREEN_PROPERTY_ID, &pCtxt->display_property[i].display_id);
            if (rc)
            {
                perror("screen_get_display_property_iv(SCREEN_PROPERTY_ID)");
                goto fail;
            }

            rc = screen_get_display_property_iv(pCtxt->screen_display[i],
                                                SCREEN_PROPERTY_SIZE, pCtxt->display_property[i].size);
            if (rc)
            {
                perror("screen_get_display_property_iv(SCREEN_PROPERTY_SIZE)");
                goto fail;
            }
        }
    }

    test_util_register_pm_resource();

    *pp_ctxt = pCtxt;

    return QCARCAM_RET_OK;

fail:
    if (pCtxt->display_property)
        free(pCtxt->display_property);
    if (pCtxt->screen_display)
        free(pCtxt->screen_display);
    free(pCtxt);
    return QCARCAM_RET_FAILED;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_init_window
///
/// @brief Initialize new window
///
/// @param ctxt             Pointer to util context
/// @param user_ctxt        Pointer to new window to be initialized
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_init_window(test_util_ctxt_t *p_ctxt, test_util_window_t **pp_window)
{
    *pp_window = NULL;

    test_util_window_t* p_window = (test_util_window_t*)calloc(1, sizeof(struct test_util_window_t));
    if (!p_window)
    {
        return QCARCAM_RET_NOMEM;
    }

    p_window->prev_post_idx = -1;

    *pp_window = p_window;

    return QCARCAM_RET_OK;
}

static int test_util_get_screen_format(test_util_color_fmt_t fmt)
{
    int ret;
    switch (fmt)
    {
    case TESTUTIL_FMT_UYVY_8:
        ret = SCREEN_FORMAT_UYVY;
        break;
    case TESTUTIL_FMT_RGB_888:
    case TESTUTIL_FMT_R8_G8_B8: //@todo for now treat same as packed rgb888
        ret = SCREEN_FORMAT_RGB888;
        break;
    case TESTUTIL_FMT_RGBX_8888:
        ret = SCREEN_FORMAT_RGBX8888;
        break;
    case TESTUTIL_FMT_NV12:
        ret = SCREEN_FORMAT_NV12;
        break;
    case TESTUTIL_FMT_P010:
    case TESTUTIL_FMT_P01210: //@todo for now treat same as p010
    case TESTUTIL_FMT_P01208:  //@todo for now treat same as p010
    case TESTUTIL_FMT_P010_LSB: //@todo for now treat same as p010
    case TESTUTIL_FMT_P01210_LSB: //@todo for now treat same as p010
    case TESTUTIL_FMT_P01208_LSB:  //@todo for now treat same as p010
        ret = SCREEN_FORMAT_P010;
        break;
    case TESTUTIL_FMT_UBWC_NV12:
        ret = SCREEN_FORMAT_NV12;
        break;
    case TESTUTIL_FMT_UBWC_TP10:
        ret = SCREEN_FORMAT_QC_TP10;
        break;
    case TESTUTIL_FMT_RGB_565:
    default:
        ret = SCREEN_FORMAT_RGB565;
        break;
    }
    return ret;
}

#ifndef C2D_DISABLED
static int test_util_get_c2d_format(int fmt)
{
    int ret;
    switch (fmt)
    {
    case SCREEN_FORMAT_RGBX8888:
        ret = C2D_COLOR_FORMAT_8888_ARGB;
        break;
    case SCREEN_FORMAT_RGB888:
        ret = C2D_COLOR_FORMAT_888_RGB;
        break;
    default:
        ret = C2D_COLOR_FORMAT_565_RGB;
        break;
    }
    return ret;
}
#endif

static int test_util_get_screen_colorspace_format(uint32_t colorSpace)
{
    switch(colorSpace)
    {
        case QCARCAM_COLOR_SPACE_UNCORRECTED:
            return SCREEN_COLOR_SPACE_UNCORRECTED;
        case QCARCAM_COLOR_SPACE_SRGB:
            return SCREEN_COLOR_SPACE_SRGB;
        case QCARCAM_COLOR_SPACE_LRGB:
            return SCREEN_COLOR_SPACE_LRGB;
        case QCARCAM_COLOR_SPACE_BT601:
            return SCREEN_COLOR_SPACE_BT601;
        case QCARCAM_COLOR_SPACE_BT601_FULL:
            return SCREEN_COLOR_SPACE_BT601_FULL;
        case QCARCAM_COLOR_SPACE_BT709:
            return SCREEN_COLOR_SPACE_BT709;
        case QCARCAM_COLOR_SPACE_BT709_FULL:
            return SCREEN_COLOR_SPACE_BT709_FULL;
        default: QCARCAM_ERRORMSG("Unsupported color space format, default set to SCREEN_COLOR_SPACE_BT601");
                 return SCREEN_COLOR_SPACE_BT601;
    }
}

static void test_util_fill_buffer(test_util_buffer_t* p_buffer, test_util_pattern_t pattern, test_util_color_fmt_t format)
{
    if (format == TESTUTIL_FMT_UYVY_8)
    {
        //grey
        memset(p_buffer->ptr[0], 0x80, p_buffer->size[0]);
    }
    else if ((format == TESTUTIL_FMT_NV12) || (format == TESTUTIL_FMT_P010) ||
             (TESTUTIL_FMT_P01210 == format) || (TESTUTIL_FMT_P01208 == format) ||
             (format == TESTUTIL_FMT_P010_LSB) || (TESTUTIL_FMT_P01210_LSB == format) ||
             (TESTUTIL_FMT_P01208_LSB == format))
    {
        //black
        memset(p_buffer->ptr[0], 0x0, p_buffer->size[0]);
        memset(p_buffer->ptr[1], 0x80, p_buffer->size[1]);
    }
    else
    {
        memset(p_buffer->ptr[0], 0x0, p_buffer->size[0]);
    }
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_init_window_buffers
///
/// @brief Initialize buffers for display
///
/// @param ctxt             Pointer to display context
/// @param user_ctxt        Pointer to structure containing window parameters
/// @param buffers          Pointer to qcarcam buffers
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_init_window_buffers(test_util_ctxt_t *p_ctxt, test_util_window_t *p_window, QCarCamBufferList_t *p_buffers)
{
    int i, rc;

    p_window->n_buffers = p_buffers->nBuffers;
    p_window->buffers = (test_util_buffer_t*)calloc(p_window->n_buffers, sizeof(*p_window->buffers));
    if (!p_window->buffers)
    {
        QCARCAM_ERRORMSG("Failed to allocate buffers structure");
        return QCARCAM_RET_FAILED;
    }

    p_window->buffer_size[0] = p_buffers->pBuffers[0].planes[0].width;
    p_window->buffer_size[1] = p_buffers->pBuffers[0].planes[0].height;

    //set flag for pmem v2
    if (p_ctxt->allocator == TESTUTIL_ALLOCATOR_PMEM_V2)
    {
        p_buffers->flags |= QCARCAM_BUFFER_FLAG_OS_HNDL;
    }

    if (!p_ctxt->params.disable_display && !p_window->is_offscreen)
    {
        rc = screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_FORMAT, &p_window->screen_format);
        if (rc)
        {
            perror("screen_set_window_property_iv(SCREEN_PROPERTY_FORMAT)");
            return QCARCAM_RET_FAILED;
        }

        rc = screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_BUFFER_SIZE, p_window->buffer_size);
        if (rc)
        {
            perror("screen_set_window_property_iv(SCREEN_PROPERTY_BUFFER_SIZE)");
            return QCARCAM_RET_FAILED;
        }

        QCARCAM_DBGMSG("screen_win = %p screen format = %d w= %d h = %d",
                p_window->screen_win,p_window->screen_format,p_window->buffer_size[0],p_window->buffer_size[1]);
        rc = screen_create_window_buffers(p_window->screen_win, p_window->n_buffers);
        if (rc)
        {
            perror("screen_create_window_buffers");
            return QCARCAM_RET_FAILED;
        }

        rc = screen_get_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_RENDER_BUFFER_COUNT, &p_window->n_pointers);
        if (rc)
        {
            perror("screen_get_window_property_iv(SCREEN_PROPERTY_RENDER_BUFFER_COUNT)");
            return QCARCAM_RET_FAILED;
        }

        p_window->screen_bufs = (screen_buffer_t*)calloc(p_window->n_pointers, sizeof(*p_window->screen_bufs));
        if (!p_window->screen_bufs)
        {
            QCARCAM_ERRORMSG("Failed to allocate screen buffers structure");
            return QCARCAM_RET_FAILED;
        }

        rc = screen_get_window_property_pv(p_window->screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)p_window->screen_bufs);
        if (rc)
        {
            perror("screen_get_window_property_pv(SCREEN_PROPERTY_RENDER_BUFFERS)");
            return QCARCAM_RET_FAILED;
        }

        rc = screen_get_buffer_property_iv(p_window->screen_bufs[0], SCREEN_PROPERTY_STRIDE, &p_window->stride[0]);
        if (rc)
        {
            perror("screen_get_buffer_property_iv(SCREEN_PROPERTY_STRIDE)");
            return QCARCAM_RET_FAILED;
        }

        // offset for each plane from start of buffer
        rc = screen_get_buffer_property_iv(p_window->screen_bufs[0], SCREEN_PROPERTY_PLANAR_OFFSETS, &p_window->offset[0]);
        if (rc)
        {
            perror("screen_get_buffer_property_iv(SCREEN_PROPERTY_PLANAR_OFFSETS)");
            return QCARCAM_RET_FAILED;
        }

        for (i = 0; i < p_window->n_pointers; i++)
        {
            // Get pmem handle from screen buffer
            rc = screen_get_buffer_property_pv(p_window->screen_bufs[i], SCREEN_PROPERTY_EGL_HANDLE, &p_window->buffers[i].mem_handle);
            if (rc)
            {
                perror("screen_get_window_property_pv(SCREEN_PROPERTY_EGL_HANDLE)");
                return QCARCAM_RET_FAILED;
            }

            // obtain the pointer of the buffers, for the capture use
            rc = screen_get_buffer_property_pv(p_window->screen_bufs[i], SCREEN_PROPERTY_POINTER, &p_window->buffers[i].ptr[0]);
            if (rc)
            {
                perror("screen_get_window_property_pv(SCREEN_PROPERTY_POINTER)");
                return QCARCAM_RET_FAILED;
            }
            else
            {
                QCARCAM_DBGMSG("screen pointer[%d] = 0x%p", i, p_window->buffers[i].ptr[0]);
            }

            rc = screen_get_buffer_property_llv(p_window->screen_bufs[i], SCREEN_PROPERTY_PHYSICAL_ADDRESS, &p_window->buffers[i].phys_addr);
            if (rc)
            {
                perror("screen_get_window_property_pv(SCREEN_PROPERTY_PHYSICAL_ADDRESS)");
                return QCARCAM_RET_FAILED;
            }

            // For V2 we us pmem handle, for V1 we use virtual address
            if (p_ctxt->allocator == TESTUTIL_ALLOCATOR_PMEM_V2)
            {
                p_buffers->pBuffers[i].planes[0].memHndl = (uint64_t)p_window->buffers[i].mem_handle;
            }
            else
            {
                p_buffers->pBuffers[i].planes[0].memHndl = (uint64_t)p_window->buffers[i].ptr[0];
            }

            QCARCAM_DBGMSG("allocate: [%d] 0x%lx", i, p_buffers->pBuffers[i].planes[0].memHndl);

            p_buffers->pBuffers[i].numPlanes = 1;
            p_buffers->pBuffers[i].planes[0].stride = p_window->stride[0];
            p_buffers->pBuffers[i].planes[0].size = p_window->stride[0] * p_buffers->pBuffers[i].planes[0].height;
            p_window->buffers[i].size[0] = p_buffers->pBuffers[i].planes[0].size;
            p_buffers->pBuffers[i].planes[0].offset = p_window->offset[0];

            QCARCAM_DBGMSG("\t [0] 0x%lx %dx%d %d, offset:0x%x format:0x%x",
                p_buffers->pBuffers[i].planes[0].memHndl,
                p_buffers->pBuffers[i].planes[0].width,
                p_buffers->pBuffers[i].planes[0].height,
                p_buffers->pBuffers[i].planes[0].stride,
                p_window->offset[0],
                p_window->format);

            if ((p_window->format == TESTUTIL_FMT_NV12) || (p_window->format == TESTUTIL_FMT_P010) ||
                (TESTUTIL_FMT_P01210 == p_window->format) || (TESTUTIL_FMT_P01208 == p_window->format) ||
                (p_window->format == TESTUTIL_FMT_P010_LSB) || (TESTUTIL_FMT_P01210_LSB == p_window->format) ||
                (TESTUTIL_FMT_P01208_LSB == p_window->format))
            {
                // Offset[1] is number of bytes to get to second plane (i.e. size of first plane)
                p_buffers->pBuffers[i].numPlanes = 2;
                p_buffers->pBuffers[i].planes[0].size = p_window->offset[1];
                p_window->buffers[i].size[0] = p_window->offset[1];

                p_window->buffers[i].ptr[1] = (void*)((uintptr_t)(p_window->buffers[i].ptr[0]) + p_window->offset[1]);

                if (p_ctxt->allocator != TESTUTIL_ALLOCATOR_PMEM_V2)
                {
                    p_buffers->pBuffers[i].planes[1].memHndl = (uint64_t)p_window->buffers[i].ptr[1];
                }

                p_buffers->pBuffers[i].planes[1].width = p_buffers->pBuffers[i].planes[0].width;
                p_buffers->pBuffers[i].planes[1].height = p_buffers->pBuffers[i].planes[0].height / 2;

                //stride is same as plane 1
                p_window->stride[1] = p_window->stride[0];
                p_buffers->pBuffers[i].planes[1].stride = p_window->stride[1];

                p_buffers->pBuffers[i].planes[1].size = p_window->stride[1] * p_buffers->pBuffers[i].planes[1].height;
                p_window->buffers[i].size[1] = p_buffers->pBuffers[i].planes[1].size;
                p_buffers->pBuffers[i].planes[1].offset = p_window->offset[1];

                QCARCAM_DBGMSG("\t [1] 0x%lx %dx%d %d, offset:0x%x",
                    p_buffers->pBuffers[i].planes[1].memHndl,
                    p_buffers->pBuffers[i].planes[1].width,
                    p_buffers->pBuffers[i].planes[1].height,
                    p_buffers->pBuffers[i].planes[1].stride,
                    p_window->offset[1]);
            }
        }

        // Fill last buffer with 0 pattern. Some applications allocate one extra buffer
        // to send it constantly to display in the event of loss of signal from the sensor
        // instead of the last captured frame.
        test_util_fill_buffer(&p_window->buffers[p_window->n_pointers-1], TEST_UTIL_PATTERN_BLACK, p_window->format);
    }
    else
    {
        uint32_t flags;
        QCARCAM_DBGMSG("init_window buffers start allocator = %d",p_ctxt->allocator);

        if (p_ctxt->allocator == TESTUTIL_ALLOCATOR_PMEM_V2)
        {
            flags = PMEM_FLAGS_SHMEM | PMEM_FLAGS_PHYS_NON_CONTIG | PMEM_FLAGS_CACHE_NONE;

            if (QCARCAM_BUFFER_FLAG_CACHE & p_buffers->flags)
            {
                flags = PMEM_FLAGS_SHMEM | PMEM_FLAGS_PHYS_NON_CONTIG | PMEM_FLAGS_CACHE_WT_NWA;
            }
        }
        else
        {
            flags = PMEM_FLAGS_PHYS_NON_CONTIG;

            if (QCARCAM_BUFFER_FLAG_CACHE & p_buffers->flags)
            {
                flags |= PMEM_FLAGS_CACHE_WT_NWA;
            }
        }

        for (i = 0; i < p_window->n_buffers; i++)
        {
            test_util_fill_planes(&p_buffers->pBuffers[i], p_buffers->colorFmt);
            p_window->stride[0] = p_buffers->pBuffers[i].planes[0].stride;
            p_window->stride[1] = p_buffers->pBuffers[i].planes[1].stride;
            p_window->stride[2] = p_buffers->pBuffers[i].planes[2].stride;
            p_window->buffers[i].size[0] = p_buffers->pBuffers[i].planes[0].size;
            p_window->buffers[i].size[1] = p_buffers->pBuffers[i].planes[1].size;
            p_window->buffers[i].size[2] = p_buffers->pBuffers[i].planes[2].size;
            p_window->offset[1] = p_buffers->pBuffers[i].planes[0].size;
            p_window->offset[2] = p_buffers->pBuffers[i].planes[0].size + p_buffers->pBuffers[i].planes[1].size;

            if (p_ctxt->allocator == TESTUTIL_ALLOCATOR_PMEM_V2)
            {
                p_window->buffers[i].ptr[0] = pmem_malloc_ext_v2(
                        p_buffers->pBuffers[i].planes[0].size + p_buffers->pBuffers[i].planes[1].size + p_buffers->pBuffers[i].planes[2].size,
                        PMEM_CAMERA_ID,
                        flags,
                        PMEM_ALIGNMENT_4K,
                        0,
                        (pmem_handle_t*)&p_window->buffers[i].mem_handle,
                        NULL);

                if (p_window->buffers[i].ptr[0] == NULL)
                {
                    QCARCAM_ERRORMSG("test_util_init_window_buffers: pmem_alloc failed size = %d  %d",
                            p_buffers->pBuffers[i].planes[0].size , p_buffers->pBuffers[i].planes[1].size);
                    return QCARCAM_RET_FAILED;
                }
                else
                {
                    QCARCAM_DBGMSG("p_window->buffers[i].ptr[0] = %p p_window->buffers[i].mem_handle = %p",p_window->buffers[i].ptr[0],p_window->buffers[i].mem_handle);
                    QCARCAM_DBGMSG("p_window->buffers[i].ptr[1] = %p p_window->buffers[i].mem_handle = %p",p_window->buffers[i].ptr[1],p_window->buffers[i].mem_handle);
                }
                p_window->buffers[i].ptr[1] = (void*)((uintptr_t)(p_window->buffers[i].ptr[0]) + p_window->offset[1]);
                p_window->buffers[i].ptr[2] = (void*)((uintptr_t)(p_window->buffers[i].ptr[0]) + p_window->offset[2]);
                p_buffers->pBuffers[i].planes[0].memHndl = (uint64_t)p_window->buffers[i].mem_handle;
            }
            else
            {
                p_window->buffers[i].ptr[0] = pmem_malloc_ext(
                        p_buffers->pBuffers[i].planes[0].size,
                        PMEM_CAMERA_ID,
                        flags,
                        PMEM_ALIGNMENT_4K);
                if (p_window->buffers[i].ptr[0] == NULL)
                {
                    QCARCAM_ERRORMSG("test_util_init_window_buffers: pmem_alloc failed size = %d",
                            p_buffers->pBuffers[i].planes[0].size);
                    return QCARCAM_RET_FAILED;
                }
                p_window->buffers[i].ptr[1] = (void*)((uintptr_t)(p_window->buffers[i].ptr[0]) + p_window->offset[1]);
                p_buffers->pBuffers[i].planes[0].memHndl = (uint64_t)p_window->buffers[i].ptr[0];
                p_buffers->pBuffers[i].planes[1].memHndl = (uint64_t)p_window->buffers[i].ptr[1];
            }

            p_window->buffers[i].phys_addr = (long long)pmem_get_phys_addr(p_window->buffers[i].ptr[0]);
        }
    }

    // Prefill all buffers as black
    for (i = 0; i < p_window->n_pointers; i++)
    {
        test_util_fill_buffer(&p_window->buffers[i], TEST_UTIL_PATTERN_BLACK, p_window->format);
    }

    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_post_window_buffer
///
/// @brief Send frame to display
///
/// @param ctxt             Pointer to display context
/// @param user_ctxt        Pointer to structure containing window parameters
/// @param idx              Frame ID number
/// @param p_rel_buf_idx    List to fill with buffers ready to release
/// @param field_type       Field type in current frame buffer if interlaced
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_post_window_buffer(test_util_ctxt_t *p_ctxt, test_util_window_t *p_window,
        unsigned int idx, std::list<uint32>* p_rel_buf_idx, QCarCamInterlaceField_e field_type)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;

    if (!p_ctxt->params.disable_display && !p_window->is_offscreen)
    {
        int rc = 0;
        int rect[4] = {p_window->params.spos[0], p_window->params.spos[1], p_window->params.ssize[0], p_window->params.ssize[1]};

        QCARCAM_DBGMSG("%s:%d %d %d", __func__, __LINE__, p_ctxt->params.enable_di, field_type);

        if (p_ctxt->params.enable_di == TESTUTIL_DEINTERLACE_SW_BOB)
        {
            if (field_type == QCARCAM_INTERLACE_FIELD_ODD)
            {
                int param[2] = {p_window->params.ssize[0], DEINTERLACE_FIELD_HEIGHT};
                rc = screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_SOURCE_SIZE, param);
                if (rc)
                {
                    return QCARCAM_RET_FAILED;
                }

                param[0] = p_window->params.spos[0];
                param[1] = DEINTERLACE_ODD_HEADER_HEIGHT;

                rect[1] = DEINTERLACE_ODD_HEADER_HEIGHT;
                rect[3] = DEINTERLACE_FIELD_HEIGHT;

                rc = screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_SOURCE_POSITION, param);
                if (rc)
                {
                    return QCARCAM_RET_FAILED;
                }

                rc = screen_post_window(p_window->screen_win, p_window->screen_bufs[idx], 1, rect, SCREEN_WAIT_IDLE);
                if (rc)
                {
                    return QCARCAM_RET_FAILED;
                }
            }
            else if (field_type == QCARCAM_INTERLACE_FIELD_EVEN)
            {
                int param[2] = {p_window->params.ssize[0], DEINTERLACE_FIELD_HEIGHT};
                rc = screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_SOURCE_SIZE, param);
                if (rc)
                {
                    return QCARCAM_RET_FAILED;
                }

                param[0] = p_window->params.spos[0];
                param[1] = DEINTERLACE_EVEN_HEADER_HEIGHT;

                rect[1] = DEINTERLACE_EVEN_HEADER_HEIGHT;
                rect[3] = DEINTERLACE_FIELD_HEIGHT;

                rc = screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_SOURCE_POSITION, param);
                if (rc)
                {
                    return QCARCAM_RET_FAILED;
                }

                rc = screen_post_window(p_window->screen_win, p_window->screen_bufs[idx], 1, rect, SCREEN_WAIT_IDLE);
                if (rc)
                {
                    return QCARCAM_RET_FAILED;
                }
            }
            else if (field_type == QCARCAM_INTERLACE_FIELD_ODD_EVEN)
            {
                int param[2] = {p_window->params.ssize[0], DEINTERLACE_FIELD_HEIGHT};
                rc = screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_SOURCE_SIZE, param);
                if (rc)
                {
                    return QCARCAM_RET_FAILED;
                }

                param[0] = p_window->params.spos[0];
                param[1] = DEINTERLACE_ODD_HEADER_HEIGHT;

                rect[1] = DEINTERLACE_ODD_HEADER_HEIGHT;
                rect[3] = DEINTERLACE_FIELD_HEIGHT;

                rc = screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_SOURCE_POSITION, param);
                if (rc)
                {
                    return QCARCAM_RET_FAILED;
                }

                rc = screen_post_window(p_window->screen_win, p_window->screen_bufs[idx], 1, rect, SCREEN_WAIT_IDLE);
                if (rc)
                {
                    return QCARCAM_RET_FAILED;
                }

                //offset to even field
                param[1] = DEINTERLACE_ODD_HEIGHT + DEINTERLACE_EVEN_HEADER_HEIGHT;
                rect[1] = param[1];

                rc = screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_SOURCE_POSITION, param);
                if (rc)
                {
                    return QCARCAM_RET_FAILED;
                }

                rc = screen_post_window(p_window->screen_win, p_window->screen_bufs[idx], 1, rect, SCREEN_WAIT_IDLE);
                if (rc)
                {
                    return QCARCAM_RET_FAILED;
                }
            }
            else if (field_type == QCARCAM_INTERLACE_FIELD_EVEN_ODD)
            {
                int param[2] = {p_window->params.ssize[0], DEINTERLACE_FIELD_HEIGHT};
                rc = screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_SOURCE_SIZE, param);
                if (rc)
                {
                    return QCARCAM_RET_FAILED;
                }

                param[0] = p_window->params.spos[0];
                param[1] = DEINTERLACE_EVEN_HEADER_HEIGHT;

                rect[1] = DEINTERLACE_EVEN_HEADER_HEIGHT;
                rect[3] = DEINTERLACE_FIELD_HEIGHT;

                rc = screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_SOURCE_POSITION, param);
                if (rc)
                {
                    return QCARCAM_RET_FAILED;
                }

                rc = screen_post_window(p_window->screen_win, p_window->screen_bufs[idx], 1, rect, SCREEN_WAIT_IDLE);
                if (rc)
                {
                    return QCARCAM_RET_FAILED;
                }

                //offset to odd field
                param[1] = DEINTERLACE_EVEN_HEIGHT + DEINTERLACE_ODD_HEADER_HEIGHT;
                rect[1] = param[1];

                rc = screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_SOURCE_POSITION, param);
                if (rc)
                {
                    return QCARCAM_RET_FAILED;
                }

                rc = screen_post_window(p_window->screen_win, p_window->screen_bufs[idx], 1, rect, SCREEN_WAIT_IDLE);
                if (rc)
                {
                    return QCARCAM_RET_FAILED;
                }
            }
            else
            {
                rc = screen_post_window(p_window->screen_win, p_window->screen_bufs[idx], 1, rect, SCREEN_WAIT_IDLE);
                if (rc)
                {
                    return QCARCAM_RET_FAILED;
                }
            }
        }
        else
        {
            rc = screen_post_window(p_window->screen_win, p_window->screen_bufs[idx], 1, rect, SCREEN_WAIT_IDLE);
            if (rc)
            {
                QCARCAM_ERRORMSG("Failed to post buffer to screen with rc = %d", rc);
                ret = QCARCAM_RET_FAILED;
            }
        }

        if (-1 != p_window->prev_post_idx)
        {
            if (NULL != p_rel_buf_idx)
            {
                p_rel_buf_idx->push_back(p_window->prev_post_idx);
            }
        }
        p_window->prev_post_idx = idx;
    }

    return ret;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_dump_window_buffer
///
/// @brief Dump frame to a file
///
/// @param ctxt             Pointer to display context
/// @param user_ctxt        Pointer to structure containing window parameters
/// @param idx              Frame ID number
/// @param filename         Char pointer to file name to be dumped
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_dump_window_buffer(test_util_ctxt_t *p_ctxt, test_util_window_t *p_window, unsigned int idx, const char *filename)
{
    FILE *fp;
    size_t numBytesWritten = 0;
    size_t numByteToWrite = 0;

    fp = fopen(filename, "w+");

    QCARCAM_ERRORMSG("dumping qcarcam frame %s", filename);

    if (0 != fp)
    {
        test_util_buffer_t* buffer = &p_window->buffers[idx];

        numByteToWrite = buffer->size[0];
        numBytesWritten = fwrite(buffer->ptr[0], 1, buffer->size[0], fp);

        if ((p_window->format == TESTUTIL_FMT_NV12) || (p_window->format == TESTUTIL_FMT_P010) ||
                (TESTUTIL_FMT_P01210 == p_window->format) || (TESTUTIL_FMT_P01208 == p_window->format) ||
                (p_window->format == TESTUTIL_FMT_P010_LSB) || (TESTUTIL_FMT_P01210_LSB == p_window->format) ||
                (TESTUTIL_FMT_P01208_LSB == p_window->format) || (TESTUTIL_FMT_UBWC_TP10 == p_window->format) ||
                (TESTUTIL_FMT_UBWC_NV12 == p_window->format) || (p_window->format == TESTUTIL_FMT_R8_G8_B8))
        {
            numByteToWrite += buffer->size[1];
            numBytesWritten += fwrite(buffer->ptr[1], 1, buffer->size[1], fp);
        }

        if(p_window->format == TESTUTIL_FMT_R8_G8_B8)
        {
            numByteToWrite += buffer->size[2];
            numBytesWritten += fwrite(buffer->ptr[2], 1, buffer->size[2], fp);            
        }

        if (numBytesWritten != numByteToWrite)
        {
            QCARCAM_ERRORMSG("error no data written to file");
        }

        fclose(fp);
    }
    else
    {
        QCARCAM_ERRORMSG("failed to open file");
        return QCARCAM_RET_FAILED;
    }
    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_get_buf_ptr
///
/// @brief Get buffer virtual address
///
/// @param p_window       window
/// @param p_buf          pointer to buffer structure to be filled
///
/// @return Void
///////////////////////////////////////////////////////////////////////////////
void test_util_get_buf_ptr(test_util_window_t *p_window, test_util_buf_ptr_t *p_buf)
{

    int idx = p_buf->buf_idx % p_window->n_buffers;

    p_buf->p_va[0] = (unsigned char *)p_window->buffers[idx].ptr[0];
    p_buf->p_va[1] = (unsigned char *)p_window->buffers[idx].ptr[1];


    QCARCAM_DBGMSG("p_buf->p_va[0] = %p",p_buf->p_va[0]);
    QCARCAM_DBGMSG("p_buf->p_va[1] = %p",p_buf->p_va[1]);


    p_buf->stride[0] = p_window->stride[0];
    p_buf->stride[1] = p_window->stride[1];
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_allocate_input_buffers
///
/// @brief Allocate buffers for injection as input to qcarcam
///
/// @param p_window         Pointer to window
/// @param p_buffer_list    Pointer to buffer list
/// @param size             size to allocate
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_allocate_input_buffers(test_util_window_t*     p_window,
                                               QCarCamBufferList_t*    p_buffer_list,
                                               unsigned int             size)
{
    QCarCamRet_e rc = QCARCAM_RET_OK;

    if ((NULL == p_window) || (NULL == p_buffer_list) || (0 == size))
    {
        QCARCAM_ERRORMSG("Invalid params: p_window %p p_buffer_list %p size %d",
            p_window, p_buffer_list, size);
        return QCARCAM_RET_BADPARAM;
    }

    p_window->n_buffers = p_buffer_list->nBuffers;
    p_window->buffers = (test_util_buffer_t*)calloc(p_window->n_buffers, sizeof(*p_window->buffers));
    if (!p_window->buffers)
    {
        QCARCAM_ERRORMSG("Failed to allocate buffers structure");
        return QCARCAM_RET_FAILED;
    }

    for (int i = 0; i < p_window->n_buffers; i++)
    {
        uint32_t pmem_flags = PMEM_FLAGS_SHMEM;
        p_window->buffers[i].ptr[0] = pmem_malloc_ext_v2(
                                            (size_t)size,
                                            PMEM_CAMERA_ID,
                                            pmem_flags,
                                            PMEM_ALIGNMENT_4K,
                                            0,
                                            (pmem_handle_t*)&(p_window->buffers[i].mem_handle),
                                            NULL);
        if (NULL == p_window->buffers[i].ptr[0])
        {
            QCARCAM_ERRORMSG("pmem_malloc failed");
            rc = QCARCAM_RET_FAILED;
            break;
        }

        p_window->buffers[i].size[0] = size;

        p_buffer_list->pBuffers[i].numPlanes = 1;
        p_buffer_list->pBuffers[i].planes[0].width = p_buffer_list->pBuffers[0].planes[0].width;
        p_buffer_list->pBuffers[i].planes[0].height = p_buffer_list->pBuffers[0].planes[0].height;
        p_buffer_list->pBuffers[i].planes[0].stride = p_buffer_list->pBuffers[0].planes[0].stride;
        p_buffer_list->pBuffers[i].planes[0].size = p_window->buffers[i].size[0];
        p_buffer_list->pBuffers[i].planes[0].memHndl = (uint64_t)(p_window->buffers[i].mem_handle);
        QCARCAM_DBGMSG("Alloc buf[%d]: ptr 0x%p hndl 0x%lx size %u",
            i, p_window->buffers[i].ptr[0], p_buffer_list->pBuffers[i].planes[0].memHndl,
            p_buffer_list->pBuffers[i].planes[0].size);
    }

    if (rc != QCARCAM_RET_OK)
    {
        /* allocation failed-free any partially allocated buffers*/
        for (int i = 0; i < p_window->n_buffers; i++)
        {
            if (p_window->buffers[i].ptr[0])
            {
                pmem_free(p_window->buffers[i].ptr[0]);
            }
        }
    }

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_free_input_buffers
///
/// @brief Free buffers allocated for injection as input to qcarcam
///
/// @param p_window         Pointer to window
/// @param p_buffer_list    Pointer to buffer list
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_free_input_buffers(test_util_window_t*              p_window,
                                          QCarCamBufferList_t*             p_buffer_list)
{
    QCarCamRet_e rc = QCARCAM_RET_OK;

    if ((NULL == p_window) || (NULL == p_buffer_list))
    {
        QCARCAM_ERRORMSG("Invalid params: p_window %p p_buffer_list %p",
            p_window, p_buffer_list);
        return QCARCAM_RET_BADPARAM;
    }

    for (int i = 0; i < p_window->n_buffers; i++)
    {
        if (p_window->buffers[i].ptr[0])
        {
            QCARCAM_DBGMSG("Free buf[%d]: ptr 0x%p hndl 0x%lx size %u",
                i, p_window->buffers[i].ptr[0], p_buffer_list->pBuffers[i].planes[0].memHndl,
                p_buffer_list->pBuffers[i].planes[0].size);
            pmem_free(p_window->buffers[i].ptr[0]);
            p_window->buffers[i].ptr[0] = NULL;
            p_window->buffers[i].size[0] = 0;
            p_window->buffers[i].mem_handle = NULL;
            p_buffer_list->pBuffers[i].planes[0].size = p_window->buffers[i].size[0];
            p_buffer_list->pBuffers[i].planes[0].memHndl = (uint64_t)(p_window->buffers[i].mem_handle);
        }
    }

    if (p_window->buffers)
        free(p_window->buffers);

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_allocate_metadata_buffers
///
/// @brief Allocate buffers for metadata for qcarcam
///
/// @param p_window         Pointer to window
/// @param p_buffer_list    Pointer to buffer list
/// @param entry_capacity   Max number of tag entries
/// @param data_capacity    Max number of tag data
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_allocate_metadata_buffers(test_util_window_t*        p_window,
                                                 QCarCamBufferList_t*       p_buffer_list,
                                                 unsigned int entry_capacity,
                                                 unsigned int data_capacity)
{
    QCarCamRet_e rc = QCARCAM_RET_OK;
    if (NULL == p_window || NULL == p_buffer_list)
    {
        QCARCAM_ERRORMSG("Invalid params: p_window %p p_buffer_list %p", p_window, p_buffer_list);
        return QCARCAM_RET_BADPARAM;
    }

    p_window->n_buffers = p_buffer_list->nBuffers;
    p_window->buffers = (test_util_buffer_t*)calloc(p_window->n_buffers, sizeof(*p_window->buffers));
    if (!p_window->buffers)
    {
        QCARCAM_ERRORMSG("Failed to allocate buffers structure");
        return QCARCAM_RET_FAILED;
    }

    for (int i = 0; i < p_window->n_buffers; i++)
    {
        size_t memSize = calculate_camera_metadata_size(entry_capacity, data_capacity);
        uint32 pmem_flags = PMEM_FLAGS_SHMEM;
        p_window->buffers[i].ptr[0] = pmem_malloc_ext_v2(
                                            (size_t)memSize,
                                            PMEM_CAMERA_ID,
                                            pmem_flags,
                                            PMEM_ALIGNMENT_4K,
                                            0,
                                            (pmem_handle_t*)&(p_window->buffers[i].mem_handle),
                                            NULL);
        if (NULL == p_window->buffers[i].ptr[0])
        {
            QCARCAM_ERRORMSG("pmem_malloc failed");
            rc = QCARCAM_RET_FAILED;
            break;
        }
        p_window->buffers[i].size[0] = memSize;
        p_buffer_list->pBuffers[i].numPlanes = 1;
        p_buffer_list->pBuffers[i].planes[0].memHndl = (uint64_t)(p_window->buffers[i].mem_handle);
        p_buffer_list->pBuffers[i].planes[0].size = memSize;
        p_buffer_list->pBuffers[i].planes[0].offset = 0;
        QCARCAM_ERRORMSG("Alloc meta buf[%d]: ptr 0x%p hndl 0x%lx size %u",
                i, p_window->buffers[i].ptr[0], p_buffer_list->pBuffers[i].planes[0].memHndl,
                p_buffer_list->pBuffers[i].planes[0].size);

        camera_metadata_t* meta = place_camera_metadata(p_window->buffers[i].ptr[0],
                memSize, entry_capacity, data_capacity);
        if (!meta)
        {
            QCARCAM_ERRORMSG("place_camera_metadata failed");
            rc = QCARCAM_RET_FAILED;
            break;
        }
    }

    if (rc != QCARCAM_RET_OK)
    {
        /* allocation failed-free any partially allocated buffers*/
        for (int i = 0; i < p_window->n_buffers; i++)
        {
            if (p_window->buffers[i].ptr[0])
            {
                pmem_free(p_window->buffers[i].ptr[0]);
            }
        }
    }
    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_initialize_metadata_buffers
///
/// @brief Add metadata entries to all buffers
///
/// @param p_window         Pointer to window
/// @param metadata_table   Table of metadata entries
/// @param metadata_count   number of entries from table
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_initialize_metadata_buffers(test_util_window_t*         p_window,
                                                   test_util_metadata_tag_t*  metadata_table,
                                                   unsigned int metadata_count)
{
    QCarCamRet_e rc = QCARCAM_RET_OK;
    if (NULL == p_window || (NULL == metadata_table && metadata_count > 0))
    {
        QCARCAM_ERRORMSG("Invalid params: p_window %p metadata_table %p", p_window, metadata_table);
        return QCARCAM_RET_BADPARAM;
    }

    for (int i = 0; i < p_window->n_buffers; i++)
    {
        camera_metadata_t* meta = (camera_metadata_t*)p_window->buffers[i].ptr[0];
        if (meta)
        {
            for (unsigned int metaIdx = 0;metaIdx < metadata_count; ++metaIdx)
            {
                test_util_metadata_tag_t& tagData = metadata_table[metaIdx];
                if (tagData.isVendorTag)
                {
                    QCarCamGetMetaDataTagId((QCarCamMetadataTagId_e)tagData.qccId, &tagData.tagId);
                }

                int ret = add_camera_metadata_entry(meta, tagData.tagId, &tagData.data, tagData.count);
                if (ret != 0)
                {
                    rc = QCARCAM_RET_FAILED;
                    QCARCAM_ERRORMSG("Error adding camera metadata (result:%d)", ret);
                }
                else
                {
                    QCARCAM_DBGMSG("Add meta metadata entry success (count:%ld)",
                            get_camera_metadata_entry_count(meta));
                }
            }
        }
    }

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_append_metadata_tag
///
/// @brief Append metadata entry to all buffers
///
/// @param p_window         Pointer to window
/// @param p_tag            Tag entry to append
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_append_metadata_tag(test_util_window_t*         p_window,
                                           test_util_metadata_tag_t*   p_tag)
{
    QCarCamRet_e rc = QCARCAM_RET_OK;
    if (NULL == p_window || NULL == p_tag)
    {
        QCARCAM_ERRORMSG("Invalid params: p_window %p p_tag %p", p_window, p_tag);
        return QCARCAM_RET_BADPARAM;
    }

    for (int i = 0; i < p_window->n_buffers; i++)
    {
        camera_metadata_t* meta = (camera_metadata_t*)p_window->buffers[i].ptr[0];
        if (meta)
        {
            if (p_tag->isVendorTag)
            {
                rc = QCarCamGetMetaDataTagId((QCarCamMetadataTagId_e)p_tag->qccId, &p_tag->tagId);
                QCARCAM_DBGMSG("QCarCamGetMetaDataTagId %u  ret %d", p_tag->tagId, rc);
            }

            if (QCARCAM_RET_OK == rc)
            {
                camera_metadata_entry_t entry = {};

                int ret = find_camera_metadata_entry(meta, p_tag->tagId, &entry);
                if (0 != ret)
                {
                    ret = add_camera_metadata_entry(meta,
                            p_tag->tagId,
                            p_tag->data,
                            p_tag->count);
                    QCARCAM_DBGMSG("add_camera_metadata_entry %u  ret %d", p_tag->tagId, ret);
                }
                else
                {
                    ret = update_camera_metadata_entry(meta,
                            entry.index,
                            p_tag->data,
                            p_tag->count,
                            NULL);
                    QCARCAM_DBGMSG("update_camera_metadata_entry %zu  ret %d", entry.index, ret);
                }

                QCARCAM_ALWZMSG("qccId:%d tagId:0x%x val:%d  (ret %d)",
                        p_tag->qccId, p_tag->tagId, (int32_t)p_tag->data[0], ret)

                if (0 != ret)
                {
                    rc = QCARCAM_RET_FAILED;
                }
            }
        }
    }

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_free_metadata_buffers
///
/// @brief Allocate buffers for meta data tag as input to qcarcam
///
/// @param p_window         Pointer to window
/// @param p_buffer_list    Pointer to buffer list
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_free_metadata_buffers(test_util_window_t*              p_window,
                                                           QCarCamBufferList_t*     p_buffer_list)
{
    QCarCamRet_e rc = QCARCAM_RET_OK;
    if ((NULL == p_window) || (NULL == p_buffer_list))
    {
        QCARCAM_ERRORMSG("Invalid params: p_window %p p_buffer_list %p",
            p_window, p_buffer_list);
        return QCARCAM_RET_BADPARAM;
    }
    for (int i = 0; i < p_window->n_buffers; i++)
    {
        if (p_window->buffers[i].ptr[0])
        {
            QCARCAM_DBGMSG("Free buf[%d]: ptr 0x%p hndl 0x%lx size %u",
                i, p_window->buffers[i].ptr[0], p_buffer_list->pBuffers[i].planes[0].memHndl,
                p_buffer_list->pBuffers[i].planes[0].size);
            pmem_free(p_window->buffers[i].ptr[0]);
            p_window->buffers[i].ptr[0] = NULL;
            p_window->buffers[i].size[0] = 0;
            p_window->buffers[i].mem_handle = NULL;
            p_buffer_list->pBuffers[i].planes[0].size = p_window->buffers[i].size[0];
            p_buffer_list->pBuffers[i].planes[0].memHndl = (uint64_t)(p_window->buffers[i].mem_handle);
        }
    }
    if (p_window->buffers)
        free(p_window->buffers);
    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_read_input_data
///
/// @brief Read input data into buffer list
///
/// @param p_window         Pointer to window
/// @param nframes          Number of frames stored in the file
/// @param filename         Path to data file to be read
/// @param size             Size of each frame
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_read_input_data(test_util_window_t*                 p_window,
                                                 const char*                filename,
                                                 int                        nframes,
                                                 size_t                     size)
{
    QCarCamRet_e rc = QCARCAM_RET_OK;
    FILE *fp;
    size_t numByteToRead = size * nframes;
    size_t fileSize;

    if ((NULL == p_window) || (NULL == filename) || (0 == nframes) || (0 == size))
    {
        QCARCAM_ERRORMSG("Invalid params: p_window %p filename %p nframes %d size %lu",
            p_window, filename, nframes, size);
        return QCARCAM_RET_BADPARAM;
    }

    fp = fopen(filename, "r");
    if (NULL == fp)
    {
        QCARCAM_ERRORMSG("Failed to open file %s, err=%d",
            filename, errno);
        rc = QCARCAM_RET_FAILED;
    }

    if (QCARCAM_RET_OK == rc)
    {
        if (fseek(fp, 0, SEEK_END) != 0)
        {
            QCARCAM_ERRORMSG("fseek failed, err=%d", errno);
            rc = QCARCAM_RET_FAILED;
        }
    }

    if (QCARCAM_RET_OK == rc)
    {
        fileSize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
    }

    if (QCARCAM_RET_OK == rc)
    {
        if (numByteToRead > fileSize)
        {
            QCARCAM_ERRORMSG("fileSize %lu is smaller than numByteToRead %lu",
                fileSize, numByteToRead);
            rc = QCARCAM_RET_FAILED;
        }
    }

    if (QCARCAM_RET_OK == rc)
    {
        for (int i = 0; i < p_window->n_buffers; i++)
        {
            if ((1 == nframes) || ((i != 0) && (!(i % nframes))))
            {
                rewind(fp);
            }

            size_t bytesRead;
            void* pBuf = p_window->buffers[i].ptr[0];
            memset(pBuf, 0, size);
            if ((bytesRead = fread(pBuf, 1, size, fp)) != size)
            {
                QCARCAM_ERRORMSG("fread failed, bytesRead=%lu, size=%lu\n",
                    bytesRead, size);
                rc = QCARCAM_RET_FAILED;
                break;
            }
        }
        fclose(fp);
    }

    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_deinit_window_buffer
///
/// @brief Destroy window buffers
///
/// @param ctxt             Pointer to display context
/// @param user_ctxt        Pointer to structure containing window parameters
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_deinit_window_buffer(test_util_ctxt_t *p_ctxt, test_util_window_t *p_window)
{
    int i;

    if(p_ctxt == NULL || p_window == NULL)
    {
        return QCARCAM_RET_BADPARAM;
    }

    if(p_ctxt->params.disable_display || p_window->is_offscreen)
    {
        /* De allocate buffers if created directly from PMEM */
        for (i = 0; i < p_window->n_buffers; i++)
        {
            if (p_window->buffers[i].ptr[0])
            {
                int rc = 0;
                if (0 != (rc = pmem_free(p_window->buffers[i].ptr[0])))
                {
                    QCARCAM_ERRORMSG("pmem_free failed: rc=%d, n_buf=%d", rc, i);
                }
            }
        }
    }

    if(p_window->screen_bufs != NULL)
    {
        free(p_window->screen_bufs);
        p_window->screen_bufs = NULL;
    }

    if(p_window->buffers != NULL)
    {
#ifndef C2D_DISABLED
        if (p_ctxt->params.enable_c2d)
        {
            for (i = 0; i < p_window->n_pointers; i++)
            {
                c2dDestroySurface(p_window->buffers[i].c2d_surface_id);
                p_window->buffers[i].c2d_surface_id = 0;
            }
        }
#endif

        free(p_window->buffers);
        p_window->buffers = NULL;
    }

    return QCARCAM_RET_OK;
}


///////////////////////////////////////////////////////////////////////////////
/// test_util_release_pm_resource
///
/// @brief close power manager channel, fd, release thread
///
/// @param NULL
///
/// @return NULL
///////////////////////////////////////////////////////////////////////////////
void test_util_release_pm_resource(void)
{
    MsgSendPulse(g_pm_handle.connect_id, -1, CAMERA_POWER_BREAKOUT_PULSE, 0);

    if (g_pm_handle.connect_id != -1)
    {
        ConnectDetach(g_pm_handle.connect_id);
    }

    if (g_pm_handle.channel_id != -1)
    {
        ChannelDestroy(g_pm_handle.channel_id);
    }
    if (g_pm_handle.fd > 0)
    {
        close(g_pm_handle.fd);
    }
    OSAL_ThreadJoin(g_pm_handle.thread_id, NULL);
    OSAL_ThreadDestroy(g_pm_handle.thread_id, 0);
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_deinit_window
///
/// @brief Destroy window
///
/// @param ctxt             Pointer to display context
/// @param user_ctxt        Pointer to structure containing window parameters
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_deinit_window(test_util_ctxt_t *p_ctxt, test_util_window_t *p_window)
{
    (void)p_ctxt;

    if (p_window)
    {
        if (p_window->screen_win != NULL)
        {
            screen_destroy_window(p_window->screen_win);
            p_window->screen_win = NULL;
        }
        free(p_window);
    }
    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_deinit
///
/// @brief Destroy context and free memory.
///
/// @param ctxt   Pointer to context to be destroyed
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_deinit(test_util_ctxt_t *p_ctxt)
{
    QCarCamRet_e rc = QCARCAM_RET_OK;

    g_aborted = 1;

    if (p_ctxt)
    {
        if (g_pm_handle.thread_id)
        {
            test_util_release_pm_resource();
        }

        if (p_ctxt->screen_ctxt != NULL)
        {
            screen_destroy_context(p_ctxt->screen_ctxt);
            p_ctxt->screen_ctxt = NULL;
        }
        if (p_ctxt->screen_display)
        {
            free(p_ctxt->screen_display);
            p_ctxt->screen_display = NULL;
        }
        if (p_ctxt->display_property)
        {
            free(p_ctxt->display_property);
            p_ctxt->display_property = NULL;
        }

        if (p_ctxt->params.enable_gpio_config)
        {
            g_fd_gpio = gpio_close(NULL);
            if (g_fd_gpio == -1)
            {
                QCARCAM_ERRORMSG("gpio_close() failed!");
                rc = QCARCAM_RET_FAILED;
            }
        }

        free(p_ctxt);
    }

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_create_c2d_surface
///
/// @brief Create a C2D surface
///
/// @param ctxt             Pointer to display context
/// @param user_ctxt        Pointer to structure containing window parameters
/// @param idx              Frame ID number
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_create_c2d_surface(test_util_ctxt_t *p_ctxt, test_util_window_t *p_window, unsigned int idx)
{
#ifndef C2D_DISABLED
    if(!p_ctxt->params.disable_display && !p_window->is_offscreen)
    {
        if (SCREEN_FORMAT_UYVY == p_window->screen_format)
        {
            C2D_STATUS c2d_status;
            C2D_YUV_SURFACE_DEF c2d_yuv_surface_def;
            c2d_yuv_surface_def.format = C2D_COLOR_FORMAT_422_UYVY;
            c2d_yuv_surface_def.width = p_window->buffer_size[0];
            c2d_yuv_surface_def.height = p_window->buffer_size[1];
            c2d_yuv_surface_def.stride0 = p_window->stride[0];
            c2d_yuv_surface_def.plane0 = p_window->buffers[idx].ptr[0];
            c2d_yuv_surface_def.phys0 = (void *)p_window->buffers[idx].phys_addr;
            c2d_status = c2dCreateSurface(&p_window->buffers[idx].c2d_surface_id,
                                        C2D_SOURCE | C2D_TARGET,
                                        (C2D_SURFACE_TYPE)(C2D_SURFACE_YUV_HOST | C2D_SURFACE_WITH_PHYS),
                                        &c2d_yuv_surface_def);
            if (c2d_status != C2D_STATUS_OK)
            {
                QCARCAM_ERRORMSG("c2dCreateSurface %d buf %d failed %d", 0, idx, c2d_status);
                return QCARCAM_RET_FAILED;
            }
        }
        else
        {
            C2D_STATUS c2d_status;
            C2D_RGB_SURFACE_DEF c2d_rgb_surface_def;
            c2d_rgb_surface_def.format = test_util_get_c2d_format(p_window->screen_format);
            c2d_rgb_surface_def.width = p_window->buffer_size[0];
            c2d_rgb_surface_def.height = p_window->buffer_size[1];
            c2d_rgb_surface_def.stride = p_window->stride[0];
            c2d_rgb_surface_def.buffer = p_window->buffers[idx].ptr[0];
            c2d_rgb_surface_def.phys = (void *)p_window->buffers[idx].phys_addr;
            c2d_status = c2dCreateSurface(&p_window->buffers[idx].c2d_surface_id,
                                        C2D_SOURCE | C2D_TARGET,
                                        (C2D_SURFACE_TYPE)(C2D_SURFACE_RGB_HOST | C2D_SURFACE_WITH_PHYS),
                                        &c2d_rgb_surface_def);

            if (c2d_status != C2D_STATUS_OK)
            {
                QCARCAM_ERRORMSG("c2dCreateSurface %d buf %d failed %d", 0, idx, c2d_status);
                return QCARCAM_RET_FAILED;
            }
        }
    }
#endif
    return QCARCAM_RET_OK;
}


QCarCamColorFmt_e test_util_get_qcarcam_format(test_util_color_fmt_t fmt)
{
    QCarCamColorFmt_e test_format = QCARCAM_FMT_RGBX_8888;
    switch (fmt)
    {
        case TESTUTIL_FMT_RGBX_8888:
            test_format = QCARCAM_FMT_RGBX_8888;
            break;
        case TESTUTIL_FMT_RGB_565:
            test_format = QCARCAM_FMT_RGB_565;
            break;
        case TESTUTIL_FMT_P010:
            test_format = QCARCAM_FMT_P010;
            break;
        case TESTUTIL_FMT_P01210:
            test_format = QCARCAM_FMT_P01210;
            break;
        case TESTUTIL_FMT_P01208:
            test_format = QCARCAM_FMT_P01208;
            break;
        case TESTUTIL_FMT_P010_LSB:
            test_format = QCARCAM_FMT_P010LSB;
            break;
        case TESTUTIL_FMT_P01210_LSB:
            test_format = QCARCAM_FMT_P01210LSB;
            break;
        case TESTUTIL_FMT_P01208_LSB:
            test_format = QCARCAM_FMT_P01208LSB;
            break;
        case TESTUTIL_FMT_RGB_888:
            test_format = QCARCAM_FMT_RGB_888;
            break;
        case TESTUTIL_FMT_MIPIRAW_8:
            test_format = QCARCAM_FMT_MIPIRAW_8;
            break;
        case TESTUTIL_FMT_MIPIRAW_10:
            test_format = QCARCAM_FMT_MIPIRAW_10;
            break;
        case TESTUTIL_FMT_MIPIRAW_12:
            test_format = QCARCAM_FMT_MIPIRAW_12;
            break;
        case TESTUTIL_FMT_MIPIRAW_14:
            test_format = QCARCAM_FMT_MIPIRAW_14;
            break;
        case TESTUTIL_FMT_PLAIN_10:
            test_format = QCARCAM_FMT_PLAIN16_10;
            break;
        case TESTUTIL_FMT_PLAIN_12:
            test_format = QCARCAM_FMT_PLAIN16_12;
            break;
        case TESTUTIL_FMT_PLAIN_14:
            test_format = QCARCAM_FMT_PLAIN16_14;
            break;
        case TESTUTIL_FMT_PLAIN_16:
            test_format = QCARCAM_FMT_PLAIN16_16;
            break;
        case TESTUTIL_FMT_UYVY_10:
            test_format = QCARCAM_FMT_UYVY_10;
            break;
        case TESTUTIL_FMT_YU12:
            test_format = QCARCAM_FMT_UYVY_12;
            break;
        case TESTUTIL_FMT_UYVY_8:
            test_format = QCARCAM_FMT_UYVY_8;
            break;
        case TESTUTIL_FMT_NV12:
            test_format = QCARCAM_FMT_NV12;
            break;
        case TESTUTIL_FMT_NV21:
            test_format = QCARCAM_FMT_NV21;
            break;
        case TESTUTIL_FMT_BGRX1010102:
            test_format = QCARCAM_FMT_BGRX_1010102;
            break;
        case TESTUTIL_FMT_RGBX1010102:
            test_format = QCARCAM_FMT_RGBX_1010102;
            break;
        case TESTUTIL_FMT_BGRX8888:
            test_format = QCARCAM_FMT_BGRX_8888;
            break;
        case TESTUTIL_FMT_UBWC_NV12:
            test_format = QCARCAM_FMT_UBWC_NV12;
            break;
        case TESTUTIL_FMT_UBWC_TP10:
            test_format = QCARCAM_FMT_UBWC_TP10;
            break;
        default:
            break;
    }

    return test_format;
}
#ifdef POST_PROCESS
struct {
    QCarCamColorFmt_e  format;
    const char        *qcarcam_fmt_string;
} g_qcarcam_fmt_string_lookup[] = {
    {QCARCAM_FMT_MIPIRAW_8, "QCARCAM_FMT_MIPIRAW_8"},
    {QCARCAM_FMT_MIPIRAW_10, "QCARCAM_FMT_MIPIRAW_10"},
    {QCARCAM_FMT_MIPIRAW_12, "QCARCAM_FMT_MIPIRAW_12"},
    {QCARCAM_FMT_MIPIRAW_14, "QCARCAM_FMT_MIPIRAW_14"},
    {QCARCAM_FMT_MIPIRAW_16,"QCARCAM_FMT_MIPIRAW_16"},
    {QCARCAM_FMT_MIPIRAW_20,"QCARCAM_FMT_MIPIRAW_20"},
    {QCARCAM_FMT_PLAIN16_10, "QCARCAM_FMT_PLAIN16_10"},
    {QCARCAM_FMT_PLAIN16_12, "QCARCAM_FMT_PLAIN16_12"},
    {QCARCAM_FMT_PLAIN16_14, "QCARCAM_FMT_PLAIN16_14"},
    {QCARCAM_FMT_PLAIN16_16, "QCARCAM_FMT_PLAIN16_16"},
    {QCARCAM_FMT_PLAIN32_20,"QCARCAM_FMT_PLAIN32_20"},
    {QCARCAM_FMT_RGB_888,"QCARCAM_FMT_RGB_888"},
    {QCARCAM_FMT_R8_G8_B8,"QCARCAM_FMT_R8_G8_B8"},
    {QCARCAM_FMT_BGR_888,"QCARCAM_FMT_BGR_888"},
    {QCARCAM_FMT_RGBX_8888,"QCARCAM_FMT_RGBX_8888"},
    {QCARCAM_FMT_BGRX_8888,"QCARCAM_FMT_BGRX_8888"},
    {QCARCAM_FMT_RGB_565,"QCARCAM_FMT_RGB_565"},
    {QCARCAM_FMT_UYVY_8,"QCARCAM_FMT_UYVY_8"},
    {QCARCAM_FMT_UYVY_10, "QCARCAM_FMT_UYVY_10"},
    {QCARCAM_FMT_UYVY_12, "QCARCAM_FMT_UYVY_12"},
    {QCARCAM_FMT_YUYV_8,"QCARCAM_FMT_YUYV_8"},
    {QCARCAM_FMT_YUYV_10,"QCARCAM_FMT_YUYV_10"},
    {QCARCAM_FMT_YUYV_12,"QCARCAM_FMT_YUYV_12"},
    {QCARCAM_FMT_RGBX_1010102,"QCARCAM_FMT_RGBX_1010102"},
    {QCARCAM_FMT_BGRX_1010102,"QCARCAM_FMT_BGRX_1010102"},
    {QCARCAM_FMT_NV12,"QCARCAM_FMT_NV12"},
    {QCARCAM_FMT_NV21,"QCARCAM_FMT_NV21"},
    {QCARCAM_FMT_YU12,"QCARCAM_FMT_YU12"},
    {QCARCAM_FMT_YV12,"QCARCAM_FMT_YV12"},
    {QCARCAM_FMT_MIPIUYVY_10,"QCARCAM_FMT_MIPIUYVY_10"},
    {QCARCAM_FMT_P010,"QCARCAM_FMT_P010"},
    {QCARCAM_FMT_P01210,"QCARCAM_FMT_P01210"},
    {QCARCAM_FMT_P01208,"QCARCAM_FMT_P01208"},
    {QCARCAM_FMT_P010LSB,"QCARCAM_FMT_P010LSB"},
    {QCARCAM_FMT_P01210LSB,"QCARCAM_FMT_P01210LSB"},
    {QCARCAM_FMT_P01208LSB,"QCARCAM_FMT_P01208LSB"}
};

QCarCamColorFmt_e test_util_get_color_format_from_name(char *pString)
{
    QCarCamColorFmt_e fmt = QCARCAM_FMT_PLAIN32_20;
    int i;
    int table_size = sizeof(g_qcarcam_fmt_string_lookup)/sizeof(g_qcarcam_fmt_string_lookup[0]);

    QCARCAM_DBGMSG("input string = %s table size = %d",pString,table_size);
    if (pString != NULL)
    {
        for(i = 0 ;i < table_size;i++)
        {
            if (strcmp(g_qcarcam_fmt_string_lookup[i].qcarcam_fmt_string, pString) == 0)
            {
                QCARCAM_DBGMSG("match found fmt = %x",g_qcarcam_fmt_string_lookup[i].format);
                return g_qcarcam_fmt_string_lookup[i].format;
            }
        }
    }
    return fmt;
}

test_util_color_fmt_t test_util_get_test_format(QCarCamColorFmt_e fmt)
{
    test_util_color_fmt_t test_format = TESTUTIL_FMT_RGBX_8888;
    switch (fmt)
    {
        case QCARCAM_FMT_RGBX_8888:
            test_format = TESTUTIL_FMT_RGBX_8888;
            break;
        case QCARCAM_FMT_RGB_565:
            test_format = TESTUTIL_FMT_RGB_565;
            break;
        case QCARCAM_FMT_P010:
            test_format = TESTUTIL_FMT_P010;
            break;
        case QCARCAM_FMT_P01210:
            test_format = TESTUTIL_FMT_P01210;
            break;
        case QCARCAM_FMT_P01208:
            test_format = TESTUTIL_FMT_P01208;
            break;
        case QCARCAM_FMT_P010LSB:
            test_format = TESTUTIL_FMT_P010_LSB;
            break;
        case QCARCAM_FMT_P01210LSB:
            test_format = TESTUTIL_FMT_P01210_LSB;
            break;
        case QCARCAM_FMT_P01208LSB:
            test_format = TESTUTIL_FMT_P01208_LSB;
            break;
        case QCARCAM_FMT_RGB_888:
            test_format = TESTUTIL_FMT_RGB_888;
            break;
        case QCARCAM_FMT_R8_G8_B8:
            test_format = TESTUTIL_FMT_R8_G8_B8;
            break;
        case QCARCAM_FMT_MIPIRAW_8:
            test_format = TESTUTIL_FMT_MIPIRAW_8;
            break;
        case QCARCAM_FMT_MIPIRAW_10:
            test_format = TESTUTIL_FMT_MIPIRAW_10;
            break;
        case QCARCAM_FMT_MIPIRAW_12:
            test_format = TESTUTIL_FMT_MIPIRAW_12;
            break;
        case QCARCAM_FMT_PLAIN16_10:
            test_format = TESTUTIL_FMT_PLAIN_10;
            break;
        case QCARCAM_FMT_PLAIN16_12:
            test_format = TESTUTIL_FMT_PLAIN_12;
            break;
        case QCARCAM_FMT_PLAIN16_14:
            test_format = TESTUTIL_FMT_PLAIN_14;
            break;
        case QCARCAM_FMT_PLAIN16_16:
            test_format = TESTUTIL_FMT_PLAIN_16;
            break;
        case QCARCAM_FMT_UYVY_10:
            test_format = TESTUTIL_FMT_UYVY_10;
            break;
        case QCARCAM_FMT_UYVY_12:
            test_format = TESTUTIL_FMT_YU12;
            break;
        case QCARCAM_FMT_UYVY_8:
            test_format = TESTUTIL_FMT_UYVY_8;
            break;
        case QCARCAM_FMT_NV12:
            test_format = TESTUTIL_FMT_NV12;
            break;
        case QCARCAM_FMT_NV21:
            test_format = TESTUTIL_FMT_NV21;
            break;
        case QCARCAM_FMT_BGRX_1010102:
            test_format = TESTUTIL_FMT_BGRX1010102;
            break;
        case QCARCAM_FMT_RGBX_1010102:
            test_format = TESTUTIL_FMT_RGBX1010102;
            break;
        case QCARCAM_FMT_BGRX_8888:
            test_format = TESTUTIL_FMT_BGRX8888;
            break;
        default:
            break;
    }

    return test_format;
}

QCarCamRet_e test_util_post_processing_process_frame(test_util_pp_ctxt_t *pProc, pp_job_t *pJob)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;

    pProc->pInterface->qcarcam_post_process_frame(pProc->pp_ctxt, pJob);

    return ret;
}

QCarCamRet_e test_util_init_post_processing(test_util_pp_ctxt_t *pProc,
                                          test_util_ctxt_t *p_ctxt,
                                          test_util_window_t *in_user_ctxt,
                                          test_util_window_t *out_user_ctxt)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;
    pp_init_t pp_init = {};
    int idx = 0;
    int j = 0;
    int status;

    QCARCAM_DBGMSG("Enter init %s", pProc->pLibName);

    if (!in_user_ctxt)
    {
        QCARCAM_ERRORMSG("input user context is NULL");
        ret = QCARCAM_RET_FAILED;
        return ret;
    }

    if (!out_user_ctxt)
    {
        QCARCAM_ERRORMSG("output user context is NULL");
        ret = QCARCAM_RET_FAILED;
        return ret;
    }

    if (0 == (pProc->hPPLib = dlopen(pProc->pLibName, RTLD_NOW|RTLD_GLOBAL)))
    {
        QCARCAM_ERRORMSG("'%s' Loading failed '%s'", pProc->pLibName, dlerror());
        ret = QCARCAM_RET_FAILED;
        return ret;
    }
    else if (0 == (pProc->pGetPPInterface = (GetPostProcessingInterfaceType)
            dlsym(pProc->hPPLib, GET_POST_PROCESSING_INTERFACE)))
    {
        QCARCAM_ERRORMSG("'%s' Interface not found '%s'", pProc->pLibName, dlerror());
        ret = QCARCAM_RET_FAILED;
        dlclose(pProc->hPPLib);
        return ret;
    }
    else
    {
        pProc->pInterface = pProc->pGetPPInterface();

        if (!pProc->pInterface)
        {
            QCARCAM_ERRORMSG("'%s' invalid interface", pProc->pLibName);
            ret = QCARCAM_RET_FAILED;
        }
        else if (POST_PROCESS_VERSION != pProc->pInterface->version ||
                 !pProc->pInterface->qcarcam_post_process_open ||
                 !pProc->pInterface->qcarcam_post_process_close ||
                 !pProc->pInterface->qcarcam_post_process_init ||
                 !pProc->pInterface->qcarcam_post_process_deinit ||
                 !pProc->pInterface->qcarcam_post_process_frame)
        {
            QCARCAM_ERRORMSG("'%s' invalid interfaces", pProc->pLibName);
            ret = QCARCAM_RET_FAILED;
        }
        else
        {
            QCARCAM_DBGMSG("Loaded interface successfully", pProc->pInterface);

            pProc->pp_ctxt = pProc->pInterface->qcarcam_post_process_open();
            if (!pProc->pp_ctxt)
            {
                QCARCAM_ERRORMSG("'%s' failed to open", pProc->pLibName);
                ret = QCARCAM_RET_FAILED;
            }
        }

        if (ret)
        {
            dlclose(pProc->hPPLib);
            return ret;
        }
    }


    int num_input_plane = test_util_get_num_planes(in_user_ctxt->format);;
    int num_output_plane = test_util_get_num_planes(out_user_ctxt->format);

    QCARCAM_DBGMSG("Enter test util 3 n_in_plane = %d n_buf = %d n_out_plane =%d w= %d h = %d",
            num_input_plane, in_user_ctxt->n_buffers,
            num_output_plane, in_user_ctxt->buffer_size[0], in_user_ctxt->buffer_size[1]);

    QCARCAM_DBGMSG("Physical=%p pbuf=%p ",(void *)in_user_ctxt->buffers[0].phys_addr,
        in_user_ctxt->buffers[0].ptr[0]);

    pp_init.src_bufferlists[0].color_format = test_util_get_qcarcam_format(in_user_ctxt->format);
    pp_init.src_bufferlists[0].n_buffers = in_user_ctxt->n_buffers;
    pp_init.src_bufferlists[0].num_planes = num_input_plane;
    pp_init.src_bufferlists[0].width = in_user_ctxt->buffer_size[0];
    pp_init.src_bufferlists[0].height = in_user_ctxt->buffer_size[1];
    pp_init.src_bufferlists[0].stride[0] = in_user_ctxt->stride[0];
    pp_init.src_bufferlists[0].stride[1] = in_user_ctxt->stride[1];
    pp_init.src_bufferlists[0].stride[2] = in_user_ctxt->stride[2];
    pp_init.src_bufferlists[0].offset[0] = in_user_ctxt->offset[0];
    pp_init.src_bufferlists[0].offset[1] = in_user_ctxt->offset[1];
    pp_init.src_bufferlists[0].offset[2] = in_user_ctxt->offset[2];
    pp_init.src_bufferlists[0].size[0] = in_user_ctxt->buffers[0].size[0];
    pp_init.src_bufferlists[0].size[1] = in_user_ctxt->buffers[0].size[1];
    pp_init.src_bufferlists[0].size[2] = in_user_ctxt->buffers[0].size[2];
    for (idx = 0; idx < in_user_ctxt->n_buffers; idx++)
    {
        pp_init.src_bufferlists[0].buffers[idx].mem_handle = (unsigned long long)in_user_ctxt->buffers[idx].mem_handle;
        pp_init.src_bufferlists[0].buffers[idx].ptr = in_user_ctxt->buffers[idx].ptr[0];
    }
    pp_init.tgt_bufferlists[0].color_format = test_util_get_qcarcam_format(out_user_ctxt->format);
    pp_init.tgt_bufferlists[0].n_buffers = out_user_ctxt->n_buffers;
    pp_init.tgt_bufferlists[0].num_planes = num_output_plane;
    pp_init.tgt_bufferlists[0].width = out_user_ctxt->buffer_size[0];
    pp_init.tgt_bufferlists[0].height = out_user_ctxt->buffer_size[1];
    pp_init.tgt_bufferlists[0].stride[0] = out_user_ctxt->stride[0];
    pp_init.tgt_bufferlists[0].stride[1] = out_user_ctxt->stride[1];
    pp_init.tgt_bufferlists[0].stride[2] = out_user_ctxt->stride[2];
    pp_init.tgt_bufferlists[0].offset[0] = out_user_ctxt->offset[0];
    pp_init.tgt_bufferlists[0].offset[1] = out_user_ctxt->offset[1];
    pp_init.tgt_bufferlists[0].offset[2] = out_user_ctxt->offset[2];
    pp_init.tgt_bufferlists[0].size[0] = out_user_ctxt->buffers[0].size[0];
    pp_init.tgt_bufferlists[0].size[1] = out_user_ctxt->buffers[0].size[1];
    pp_init.tgt_bufferlists[0].size[2] = out_user_ctxt->buffers[0].size[2];

    for (idx = 0; idx < out_user_ctxt->n_buffers; idx++)
    {
        pp_init.tgt_bufferlists[0].buffers[idx].mem_handle = (unsigned long long)out_user_ctxt->buffers[idx].mem_handle;
        pp_init.tgt_bufferlists[0].buffers[idx].ptr = out_user_ctxt->buffers[idx].ptr[0];

    }

    status = pProc->pInterface->qcarcam_post_process_init(pProc->pp_ctxt, &pp_init);

    if (status != 0)
    {
        QCARCAM_ERRORMSG("Failed to init post processing", idx, j);
        ret = QCARCAM_RET_FAILED;
    }

    return ret;

}

QCarCamRet_e test_util_deinit_post_processing(test_util_pp_ctxt_t *pProc)
{
    if (pProc && pProc->pp_ctxt)
    {
        pProc->pInterface->qcarcam_post_process_deinit(pProc->pp_ctxt);

        pProc->pInterface->qcarcam_post_process_close(pProc->pp_ctxt);
        pProc->pp_ctxt = NULL;
    }

    return QCARCAM_RET_OK;
}
#endif

#ifdef ENABLE_CL_CONVERTER

///////////////////////////////////////////////////////////////////////////////
/// test_util_init_cl_converter
///
/// @brief test utility init function for cl based conversion
///
/// @param ctxt             Pointer to display context
/// @param in_user_ctxt     input window
/// @param out_user_ctxt    output window
/// @param param            Pointer to CL converter
/// @param value            Pointer to source converter surface structure
/// @param value            Pointer to target converter surface structure
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_init_cl_converter(test_util_ctxt_t *p_ctxt,test_util_window_t *in_user_ctxt,
                                          test_util_window_t *out_user_ctxt,
                                          void* pConverter,
                                          ClConverter_surface_t* source_surface,
                                          ClConverter_surface_t* target_surface)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;

    CSC_COLOR_FORMAT_t input_format = CSC_COLOR_FORMAT_UYVY422_10BITS;
#ifdef ENABLE_RGBX1010102
    CSC_COLOR_FORMAT_t output_format = CSC_COLOR_FORMAT_BGRX1010102;
#else
    CSC_COLOR_FORMAT_t output_format = CSC_COLOR_FORMAT_BGRX8888;
#endif

   if (!in_user_ctxt)
   {
       QCARCAM_ERRORMSG("input user context is NULL");
       ret = QCARCAM_RET_FAILED;
       return ret;
    }

    if (!out_user_ctxt)
    {
        QCARCAM_ERRORMSG("output user context is NULL");
        ret = QCARCAM_RET_FAILED;
        return ret;
    }

    if (!pConverter)
    {
        QCARCAM_ERRORMSG("csc converter ref is NULL");
        ret = QCARCAM_RET_FAILED;
        return ret;
    }

    int width = 1664; //Converter requires input to be 64 byte aligned, this default being set will be updated later.
    int height = in_user_ctxt->buffer_size[1];
    int stride_in = width;
    int stride_out = width;
    int num_buffers = in_user_ctxt->n_buffers;

    if(!p_ctxt->params.enable_csc)
    {
        width = in_user_ctxt->buffer_size[0];
        height = in_user_ctxt->buffer_size[1];
        stride_in = in_user_ctxt->stride[0];
        stride_out = out_user_ctxt->stride[0];
    }

    int num_input_plane = 1; //get_num_plane(input_format);
    int num_output_plane = 1; //get_num_plane(output_format);
    int idx = 0;
    int j = 0;
    void** input_va = NULL;
    void** output_va = NULL;
    int status = 0;

    input_va = new void*[num_input_plane * num_buffers];
    output_va = new void*[num_output_plane * num_buffers];
    if ((NULL == input_va) || (NULL == output_va))
    {
        QCARCAM_ERRORMSG("failed to allocate pointer array");
        ret = QCARCAM_RET_FAILED;
        goto exit;
    }

    for (idx = 0; idx < num_buffers; idx++)
    {
        for (j = 0; j < num_input_plane; j++)
        {
            source_surface->size_plane[j] = in_user_ctxt->buffers[idx].size[j];
            input_va[idx*num_input_plane+j] = in_user_ctxt->buffers[idx].ptr[j];
            QCARCAM_DBGMSG("input_va[%d] 0x%x size %d", idx*num_input_plane+j, input_va[idx*num_input_plane+j], source_surface->size_plane[j]);
            if (input_va[idx*num_input_plane+j] == NULL)
            {
                QCARCAM_ERRORMSG("invalid input pointer idx %d plane %d", idx, j);
                ret = QCARCAM_RET_FAILED;
                goto exit;
            }
        }
    }

    for (idx = 0; idx < num_buffers-1; idx++)
    {
        for (j = 0; j < num_output_plane; j++)
        {
            target_surface->size_plane[j] = out_user_ctxt->buffers[idx].size[j];
            output_va[idx*num_output_plane+j] = out_user_ctxt->buffers[idx].ptr[j];
            QCARCAM_DBGMSG("output_va[%d] 0x%x size %d", idx*num_output_plane+j, output_va[idx*num_input_plane+j], target_surface->size_plane[j]);
            if (output_va[idx*num_input_plane+j] == NULL)
            {
                QCARCAM_ERRORMSG("invalid output pointer idx %d plane %d", idx, j);
                ret = QCARCAM_RET_FAILED;
                goto exit;
            }
        }
    }

    source_surface->format = input_format;
    source_surface->num_buffers = num_buffers;
    source_surface->num_planes = num_input_plane;
    source_surface->stride_plane[0] = stride_in * csc_get_bpp(input_format, 0);
    source_surface->stride_plane[1] = stride_in * csc_get_bpp(input_format, 1);
    source_surface->stride_plane[2] = stride_in * csc_get_bpp(input_format, 2);;
    source_surface->hostptr = input_va;

    target_surface->format = output_format;
    target_surface->num_buffers = num_buffers-1;
    target_surface->num_planes = num_output_plane;
    target_surface->stride_plane[0] = stride_out * csc_get_bpp(output_format, 0);;
    target_surface->stride_plane[1] = stride_out * csc_get_bpp(output_format, 1);;
    target_surface->stride_plane[2] = stride_out * csc_get_bpp(output_format, 2);;
    target_surface->hostptr = output_va;

    status = csc_init(pConverter, width, height, source_surface, target_surface);
    if (status != 0)
    {
        QCARCAM_ERRORMSG("failed to init converter", idx, j);
        ret = QCARCAM_RET_FAILED;
    }

    exit:
    QCARCAM_ALWZMSG("exitng now ...");
    return ret;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_deinit_cl_converter
///
/// @brief test utility deinit function for cl based conversion
///
/// @param csc_handle       Handle to csc call
/// @param value            Pointer to source converter surface structure
/// @param value            Pointer to target converter surface structure
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_deinit_cl_converter(void* csc_handle, ClConverter_surface_t* source_surface, ClConverter_surface_t* target_surface)
{
    QCarCamRet_e ret = QCARCAM_RET_OK;
    int rc = 0;
    if (NULL != csc_handle)
    {
        rc = csc_deinit(csc_handle);
        if (0 != rc)
        {
            ret = QCARCAM_RET_FAILED;
            QCARCAM_ERRORMSG("csc_deinit call failed %d", rc);
        }
    }
    else
    {
        ret = QCARCAM_RET_FAILED;
        QCARCAM_ERRORMSG("csc handle is not valid");
    }

    if (source_surface->hostptr)
    {
        delete[] source_surface->hostptr;
        source_surface->hostptr = NULL;
    }

    if (target_surface->hostptr)
    {
        delete[] target_surface->hostptr;
        target_surface->hostptr = NULL;
    }

    return ret;
}

#endif

///////////////////////////////////////////////////////////////////////////////
/// test_util_get_c2d_surface_id
///
/// @brief Get the ID from a C2D surface
///
/// @param ctxt             Pointer to display context
/// @param user_ctxt        Pointer to structure containing window parameters
/// @param idx              Frame ID number
/// @param surface_id       Pointer to C2D sruface ID number
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_get_c2d_surface_id(test_util_ctxt_t *p_ctxt, test_util_window_t *p_window, unsigned int idx, unsigned int *p_surface_id)
{
    if (!p_surface_id)
        return QCARCAM_RET_BADPARAM;

    *p_surface_id = p_window->buffers[idx].c2d_surface_id;

    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_set_window_param
///
/// @brief Send window parameters to display
///
/// @param ctxt             Pointer to display context
/// @param user_ctxt        Pointer to structure containing window parameters
/// @param window_params    Pointer to structure with window properties
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_set_window_param(test_util_ctxt_t *p_ctxt, test_util_window_t *p_window, test_util_window_param_t *p_window_params)
{
    int rc;
    int disp_idx = p_window_params->display_id;
    int val = SCREEN_USAGE_WRITE | SCREEN_USAGE_VIDEO | SCREEN_USAGE_CAPTURE;

    p_window->format = p_window_params->format;
    p_window->is_offscreen = p_window_params->is_offscreen;

    if (!p_ctxt->params.disable_display && !p_window->is_offscreen)
    {
        rc = screen_create_window(&p_window->screen_win, p_ctxt->screen_ctxt);
        if (rc)
        {
            perror("screen_create_window");
            return QCARCAM_RET_FAILED;
        }

        if (disp_idx >= p_ctxt->screen_ndisplays)
        {
            QCARCAM_ERRORMSG("display idx %d exceeds max number of displays[%d]",
            disp_idx, p_ctxt->screen_ndisplays);
            return QCARCAM_RET_FAILED;
        }

        // Set ID string for debugging via /dev/screen.
        rc = screen_set_window_property_cv(p_window->screen_win, SCREEN_PROPERTY_ID_STRING,
                                        strlen(p_window_params->debug_name), p_window_params->debug_name);
        if (rc)
        {
            perror("screen_set_window_property_cv(SCREEN_PROPERTY_ID_STRING)");
            return QCARCAM_RET_FAILED;
        }

        if (p_window_params->pipeline_id != -1)
        {
            QCARCAM_ERRORMSG("USING SCREEN_USAGE_OVERLAY!!!!");
            val |= SCREEN_USAGE_OVERLAY;
        }

        // Enable UBWC
        if ((p_window->format == TESTUTIL_FMT_UBWC_NV12) | (p_window->format == TESTUTIL_FMT_UBWC_TP10))
        {
            val |= SCREEN_USAGE_COMPRESSION;
        }

        rc = screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_USAGE, &val);
        if (rc)
        {
            perror("screen_set_window_property_iv(SCREEN_PROPERTY_USAGE)");
            return QCARCAM_RET_FAILED;
        }

        p_window->screen_format = test_util_get_screen_format(p_window->format);

        p_window->buffer_size[0] = p_window_params->buffer_size[0];
        p_window->buffer_size[1] = p_window_params->buffer_size[1];

        p_window->params.size[0] = p_window_params->window_size[0] * p_ctxt->display_property[disp_idx].size[0];
        p_window->params.size[1] = p_window_params->window_size[1] * p_ctxt->display_property[disp_idx].size[1];
        p_window->params.pos[0] = p_window_params->window_pos[0] * p_ctxt->display_property[disp_idx].size[0];
        p_window->params.pos[1] = p_window_params->window_pos[1] * p_ctxt->display_property[disp_idx].size[1];
        p_window->params.ssize[0] = p_window_params->window_source_size[0] * p_window->buffer_size[0];
        p_window->params.ssize[1] = p_window_params->window_source_size[1] * p_window->buffer_size[1];
        p_window->params.spos[0] = p_window_params->window_source_pos[0] * p_window->buffer_size[0];
        p_window->params.spos[1] = p_window_params->window_source_pos[1] * p_window->buffer_size[1];

        p_window->params.visibility = p_window_params->visibility;

        /*associate window with display */
        rc = screen_set_window_property_pv(p_window->screen_win, SCREEN_PROPERTY_DISPLAY,
                                        (void **)&p_ctxt->screen_display[disp_idx]);
        if (rc)
        {
            perror("screen_set_window_property_ptr(SCREEN_PROPERTY_DISPLAY)");
            free(p_ctxt->screen_display);
            return QCARCAM_RET_FAILED;
        }

        /*display size*/
        if (p_window->params.size[0] == -1 || p_window->params.size[1] == -1)
        {
            rc = screen_get_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_SIZE, p_window->params.size);
            if (rc)
            {
                perror("screen_get_window_property_iv(SCREEN_PROPERTY_SIZE)");
                return QCARCAM_RET_FAILED;
            }
        }

        rc = screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_SIZE, p_window->params.size);
        if (rc)
        {
            perror("screen_set_window_property_iv(SCREEN_PROPERTY_SIZE)");
            return QCARCAM_RET_FAILED;
        }

        /*display position*/
        if (p_window->params.pos[0] != 0 || p_window->params.pos[1] != 0)
        {
            rc = screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_POSITION, p_window->params.pos);

            if (rc)
            {
                perror("screen_set_window_property_iv(SCREEN_PROPERTY_POSITION)");
                return QCARCAM_RET_FAILED;
            }
        }

        if (p_window_params->pipeline_id != -1)
        {
            rc = screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_PIPELINE, &p_window_params->pipeline_id);
            if (rc)
            {
                perror("screen_set_window_property_iv(SCREEN_PROPERTY_PIPELINE)");
                return QCARCAM_RET_FAILED;
            }
        }

        if (p_window_params->zorder != -1)
        {
            rc = screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_ZORDER, &p_window_params->zorder);
            if (rc)
            {
                perror("screen_set_window_property_iv(SCREEN_PROPERTY_ZORDER)");
                return QCARCAM_RET_FAILED;
            }
        }

        if (p_window->params.ssize[0] == -1 || p_window->params.ssize[1] == -1 ||
            (p_window->params.ssize[0] + p_window->params.spos[0]) > p_window->buffer_size[0] ||
            (p_window->params.ssize[1] + p_window->params.spos[1]) > p_window->buffer_size[1])
        {
            QCARCAM_INFOMSG("adjusting viewport size from %d x %d ", p_window->params.ssize[0], p_window->params.ssize[1]);

            p_window->params.ssize[0] = p_window->buffer_size[0] - p_window->params.spos[0];
            p_window->params.ssize[1] = p_window->buffer_size[1] - p_window->params.spos[1];

            QCARCAM_INFOMSG("to %d x %d\n", p_window->params.ssize[0], p_window->params.ssize[1]);
        }

        rc = screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_SOURCE_SIZE, p_window->params.ssize);
        if (rc)
        {
            perror("screen_set_window_property_iv(SCREEN_PROPERTY_SOURCE_SIZE)");
            return QCARCAM_RET_FAILED;
        }

        QCARCAM_INFOMSG("window_source_position %d x %d\n", p_window->params.spos[0], p_window->params.spos[1]);

        if (p_window->params.spos[0] != 0 || p_window->params.spos[1] != 0)
        {
            rc = screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_SOURCE_POSITION, p_window->params.spos);
            if (rc)
            {
                perror("screen_set_window_property_iv(SCREEN_PROPERTY_SOURCE_POSITION)");
                return QCARCAM_RET_FAILED;
            }
        }
    }

    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_set_diag
///
/// @brief set the diagnostic structure to test_util_window_t
///
/// @param ctxt             Pointer to display context
/// @param user_ctxt        Pointer to structure containing window parameters
/// @param diag             diagnostic structure
///
/// @return Void
///////////////////////////////////////////////////////////////////////////////
void test_util_set_diag(test_util_ctxt_t *p_ctxt, test_util_window_t *p_window, test_util_diag_t* p_diag)
{
    (void)p_ctxt;
    (void)p_window;
    (void)p_diag;
}

///////////////////////////////////////////////////////////////////////////////
/// gpio_interrupt_thread
///
/// @brief thread which toggles the visibility when gpio interrupt detected
///
/// @param arguments                arguments for the thread to handle
///
/// @return 0 when thread has finished running
///////////////////////////////////////////////////////////////////////////////
static int gpio_interrupt_thread(void *p_data)
{
    pthread_detach(pthread_self());

    test_util_intr_thrd_args_t *p_args = (test_util_intr_thrd_args_t*)p_data;
    uint32_t irq = p_args->irq;

    struct sigevent int_event;

    // Attach Event ISR
    SIGEV_INTR_INIT(&int_event);
    int interrupt_id = InterruptAttachEvent(irq, &int_event, _NTO_INTR_FLAGS_TRK_MSK);

    if (interrupt_id == -1)
    {
        QCARCAM_ERRORMSG("InterruptAttach failed!");
        goto exit_interrupt_thread;
    }

    int status;
    while (!g_aborted)
    {
        status = InterruptWait_r(0, NULL);

        if (status != EOK)
        {
            QCARCAM_ERRORMSG("InterruptWait_r failed with error %d", status);
            break;
        }

        if (InterruptUnmask(irq, interrupt_id) == -1)
        {
            QCARCAM_ERRORMSG("InterruptUnmask failed!");
            break;
        }

        p_args->cb_func();
    }

exit_interrupt_thread:
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_gpio_interrupt_config
///
/// @brief enable IO privileges, configure the gpio and set it up for interrupts
///
/// @param intr             Pointer for the IRQ to be stored
/// @param gpio_number      Specific gpio that is being utilized
/// @param trigger          Instance of the signal which shall causes the interrupt
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_gpio_interrupt_config(uint32_t *p_intr, int gpio_number, test_util_trigger_type_t trigger)
{
    if (ThreadCtl(_NTO_TCTL_IO, 0) == -1)
    {
        QCARCAM_ERRORMSG("Failed to get IO privileges!");
        return QCARCAM_RET_FAILED;
    }

    uint32_t cfg;
    uint32_t irq;

    if (gpio_number <= 0)
    {
        QCARCAM_ERRORMSG("Bad GPIO input param  gpio=%d", gpio_number);
        return QCARCAM_RET_FAILED;
    }

    cfg = 0x0;
    if (GPIO_SUCCESS != gpio_set_config(g_fd_gpio, gpio_number, 0, cfg))
    {
        QCARCAM_ERRORMSG("gpio_set_config failed for gpio gpio_number %d", gpio_number);
        return QCARCAM_RET_FAILED;
    }

    if (GPIO_SUCCESS != gpio_set_interrupt_cfg(g_fd_gpio, gpio_number, trigger, NULL))
    {
        QCARCAM_ERRORMSG("Failed to setup detect pin interrupt");
        return QCARCAM_RET_FAILED;
    }

    if (GPIO_SUCCESS != gpio_get_interrupt_cfg(g_fd_gpio, gpio_number, &irq))
    {
        QCARCAM_ERRORMSG("Failed to get irq corresponding to gpio %d", gpio_number);
        return QCARCAM_RET_FAILED;
    }
    else
    {
        *p_intr = irq;
        QCARCAM_INFOMSG("irq corresponding to gpio %d is %d", gpio_number, irq);
    }

    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_interrupt_attach
///
/// @brief create a thread to handle the interrupt
///
/// @param arguments    arguments to pass to the newly created thread
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_interrupt_attach(test_util_intr_thrd_args_t *p_args)
{
    char thread_name[64];
    QCXThread_t interrupt_thread_handle;
    int rc;

    snprintf(thread_name, sizeof(thread_name), "gpio_interrupt_thrd");
    rc = OSAL_ThreadCreate(QCARCAM_THRD_PRIO, &gpio_interrupt_thread, p_args, 0, thread_name, &interrupt_thread_handle);
    if (rc)
    {
        QCARCAM_ERRORMSG("CameraCreateThread failed : %s", thread_name);
        return QCARCAM_RET_FAILED;
    }

    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_interrupt_wait_and_unmask
///
/// @brief wait for a GPIO interrupt and then unmask it
///
/// @param irq              IRQ to unmask
/// @param interrupt_id     interrupt id to unmask
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_interrupt_wait_and_unmask(uint32_t irq, int interrupt_id)
{
    int status = InterruptWait_r(0, NULL);

    if (status != EOK)
    {
        QCARCAM_ERRORMSG("InterruptWait_r failed with error %d", status);
        return QCARCAM_RET_FAILED;
    }

    if (InterruptUnmask(irq, interrupt_id) == -1)
    {
        QCARCAM_ERRORMSG("InterruptUnmask failed!");
        return QCARCAM_RET_FAILED;
    }

    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_get_param
///
/// @brief get the value of the window parameter of the window
///
/// @param user_ctxt        window we want to use
/// @param param            window parameter you are trying to access
/// @param value            value of parameter will be stored here
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_get_param(test_util_window_t *p_window, test_util_params_t param, int *value)
{
    switch (param)
    {
    case TEST_UTIL_VISIBILITY:
        *value = p_window->params.visibility;
        break;
    default:
        QCARCAM_ERRORMSG("Param not supported");
        return QCARCAM_RET_FAILED;
    }

    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_set_param
///
/// @brief set the value of the window parameter
///
/// @param user_ctxt        window we want to use
/// @param param            window parameter you want to change
/// @param value            value you want to set the param to
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
QCarCamRet_e test_util_set_param(test_util_window_t *p_window, test_util_params_t param, int value)
{
    switch (param)
    {
    case TEST_UTIL_VISIBILITY:
        {
            p_window->params.visibility = value;
            screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_VISIBLE, &(value));
        }
        break;
    case TEST_UTIL_COLOR_SPACE:
        {
            int colorSpace = test_util_get_screen_colorspace_format(value);
            screen_set_window_property_iv(p_window->screen_win, SCREEN_PROPERTY_COLOR_SPACE, &(colorSpace));
            QCARCAM_ALWZMSG("Set color_space value = %d to screen", value);
        }
        break;
    default:
        QCARCAM_ERRORMSG("Param not supported");
        return QCARCAM_RET_FAILED;
    }

    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_set_power_callback
///
/// @brief set power event callback
///
/// @return NULL
///////////////////////////////////////////////////////////////////////////////
void test_util_set_power_callback(power_event_callable pm_event_callback, void* p_usr_ctxt)
{
    g_pm_handle.p_power_event_callback = pm_event_callback;
    g_pm_handle.event_client_data      = p_usr_ctxt;
}

/**================================================================================================

 @file
 qcx_ccidbgr.c

 @brief
  This file implements the cci application that can R/W cci devices.

 Copyright (c) 2022 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.

 ================================================================================================**/
 
/**================================================================================================
 INCLUDE FILES FOR MODULE
 ================================================================================================**/
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <getopt.h> 
#include "qcx_ccidbgr.h"
#include "qcxosal.h"
#include "qcxstatus.h"
#include "platformccidbgr.h"

/**================================================================================================
 ** Constant and Macros
 ================================================================================================**/

/**================================================================================================
 ** Typedefs
 ================================================================================================**/

/**================================================================================================
 ** Variables
 ================================================================================================**/
  
// Initialize the global context
static CCIDbgrCtxt_t g_CCIDbgrCtxt = {0};

static struct option cciDbgrParameterList[] = {
    {"standalone",       no_argument,        0,  CCIDBGR_PARAMETER_STANDALONE },
    {"slaveaddresscheck",no_argument,        0,  CCIDBGR_PARAMETER_SLAVEADDRCHECK },
    {"cmdreadfromfile",  required_argument,  0,  CCIDBGR_PARAMETER_RDFRFILE },
    {"delay",            required_argument,  0,  CCIDBGR_PARAMETER_CMD_DELAY },
    {"help",             no_argument,        0,  CCIDBGR_PARAMETER_HELP },
    { 0 }
};


static uint8_t burstWriteArray[16384*2];
static uint8_t burstReadArray[16384*2];

/**================================================================================================
 DEFINITIONS AND DECLARATIONS
 ================================================================================================**/
static int CCIDbgr_RdWrtStandalone(void);
static int CCIDbgr_RdWrtWithQCX(void);
static int CCIDbgr_UpdateCmdOp(char *pCmdOp);
static int CCIDbgr_ParseCmd(int argCnt, char *argv[]);
static int CCIDbgr_ParseCmdInfoFrmFile(char *pStr, int len);
static int CCIDbgr_CmdReadFrmFile(void);
static int CCIDbgr_SlaveAddrCheck(void);
static int CCIDbgr_StandaloneDeinit(void);
static int CCIDbgr_StandaloneInit(void);
static int CCIDbgr_CmdToServer(CCIDbgrCtxt_t *pClientCtxt);
static int CCIDbgr_Deinit(void);
static int CCIDbgr_ChannelCreate(void);
static int CCIDbgr_CreateClientCtxt(void);
static int CCIDbgr_ParseCmdLineParameter(int argCnt, char *arg[]);

static int CCIDbgr_CreateClientCtxt(void)
{
    int ret = CAMERA_EFAILED;
    // Get pid
    g_CCIDbgrCtxt.clientInfo.pid = (uint32_t)getpid();
    g_CCIDbgrCtxt.clientInfo.pid &= 0x00FFFFFFU;
    g_CCIDbgrCtxt.msg.clientId = (g_CCIDbgrCtxt.clientInfo.pid << 8);
    
    // Create client context to get data from the camera service
    ret = OSAL_IPCCreate(g_CCIDbgrCtxt.msg.clientId, &g_CCIDbgrCtxt.clientIpcHndl);
    if (CAMERA_SUCCESS != ret)
    {
        QCX_LOG(PLATFORM, ERROR,
                "Server IPC channel creation failed (rc = 0x%X)", ret);
    }

    return ret;
}

static int CCIDbgr_ParseCmdLineParameter(int argCnt, char *arg[])
{
    int ret = CAMERA_SUCCESS;
    int cmd;
    int optIndex;

    while(-1 != (cmd = getopt_long_only(argCnt, arg, "", cciDbgrParameterList, &optIndex)))
    {
        switch (cmd)
        {
            // Parse the file name for reading the comand from file
            case CCIDBGR_PARAMETER_RDFRFILE:
                snprintf(g_CCIDbgrCtxt.ccidbgrConfig.fileName,
                        sizeof(g_CCIDbgrCtxt.ccidbgrConfig.fileName), "%s",
                        optarg);
                g_CCIDbgrCtxt.idx++;
                g_CCIDbgrCtxt.ccidbgrConfig.bCmdFrmFile = TRUE;
                break;
            // If camera service is not running, update the CCIDbgrStandalone as true
            case CCIDBGR_PARAMETER_STANDALONE:
                g_CCIDbgrCtxt.ccidbgrConfig.bStandalone = TRUE;
                g_CCIDbgrCtxt.idx++;
                break;
            // For check the active slave address on particular I2C line
            case CCIDBGR_PARAMETER_SLAVEADDRCHECK:
                g_CCIDbgrCtxt.ccidbgrConfig.bSlaveAddrCheck = TRUE;
                g_CCIDbgrCtxt.idx++;
                break;
            // Delay info for I2C Read/write
            case CCIDBGR_PARAMETER_CMD_DELAY:
                g_CCIDbgrCtxt.msg.i2cReadWrite.delay = atoi(optarg);
                g_CCIDbgrCtxt.idx++;
                break;
            case CCIDBGR_PARAMETER_HELP:
                fprintf(stderr, 
                        "*****Command to run the CCIDbgr with qcx_server ************** \n");
                fprintf(stderr, 
                        "1.   qcx_server need to run with -enableCCIDbgr parameter \n");
                fprintf(stderr, 
                        "2    qcx_ccidbgr $CCIDevNode $slaveAddr $CMD_OP $reg [$val] -delay=0 \n");
                fprintf(stderr, 
                        "*****Command to run the CCIDbgr with qcx_server and read command from file \n");
                fprintf(stderr, 
                        "3    qcx_ccidbgr -cmdreadfromfile=$PATH_OF_FILE \n");
                fprintf(stderr, 
                        "*****Command to run the CCIDbgr standalone *************************** \n");
                fprintf(stderr, 
                        "4    qcx_server should not be running \n");
                fprintf(stderr, 
                        "5    qcx_ccidbgr $CCIDevNode $slaveAddr $CMD_OP   $reg [$val] -delay=0 -standalone \n");
                fprintf(stderr, 
                        "*****Command to run the CCIDbgr standalone and read command from file \n");
                fprintf(stderr, 
                        "6    qcx_ccidbgr -cmdreadfromfile=$PATH_OF_FILE -standalone \n");
                fprintf(stderr, 
                        "7    CMD_OP read/write      for 1 Byte register address and value \n");
                fprintf(stderr, 
                        "8   CMD_OP read21/write21  for 2 Byte register address and 1 byte register value \n");
                fprintf(stderr, 
                        "9   CMD_OP read22/write22  for 2 Byte register address and value \n");
                fprintf(stderr, 
                        "10   CMD_OP burstread/burstread21/burstread22 cci-id slaveaddr num_bytes outputfile for burst read \n");
                fprintf(stderr, 
                        "11   CMD_OP burstwrite/burstwrite21/burstwrite22 cci-id slaveaddr num_bytes inputfile for burst write\n");
                fprintf(stderr, 
                        "12   CMD_OP burstwriteread/burstwriteread21/burstwriteread22 cci-id slaveaddr num_bytes inputfile num_bytes outputfile \n");							
                fprintf(stderr, 
                        "13.  Note : -delay and [$val] for read is optional \n");
                // Upadate the command help flag as true
                g_CCIDbgrCtxt.ccidbgrConfig.bCmdHelp = TRUE;
                break;
            default:
                fprintf(stderr, "Invalid parameter provided while parseCmdLineParameter \n");
                ret = CAMERA_EFAILED;
                break;
        }
        if (CAMERA_SUCCESS != ret)
        {
            fprintf(stderr, "Command line argument parsing failed \n");
            // Exitc from the while loop if parameter is not valid
            break; 
        }
    }
    
    return ret;
}

static int CCIDbgr_StandaloneInit(void)
{
    int ret = CAMERA_SUCCESS;

    ret = PLM_Init();
    if (CAMERA_SUCCESS != ret)
    {
       fprintf(stderr,"PLM init failed %d\n", ret);
    }
    return ret;
}

static int CCIDbgr_StandaloneDeinit(void)
{
    int ret = CAMERA_EFAILED;

    ret = PLM_Deinit();
    if (CAMERA_SUCCESS != ret)
    {
        fprintf(stderr,"PLM_Deinit failed %d\n", ret);
    }

    return ret;
}

static int CCIDbgr_Deinit(void)
{
    int ret = CAMERA_EFAILED;

    if (TRUE == g_CCIDbgrCtxt.ccidbgrConfig.bStandalone)
    {
        // Deinit the Clock, CCI and GPIO etc.
        ret = CCIDbgr_StandaloneDeinit();
        if (ret != CAMERA_SUCCESS)
        {
            fprintf(stderr,"CCIDbgr_StandaloneInit failed %d\n", ret);
        }
    }
    else
    {
        // Disconnect from the server before exit
        CCIDbgrCtxt_t *pClientCtxt = &g_CCIDbgrCtxt;
        pClientCtxt->msg.cmd = CCIDBGR_DISCONNECT_CMD;
        ret = CCIDbgr_CmdToServer(pClientCtxt);
        if (ret != CAMERA_SUCCESS)
        {
            fprintf(stderr,"Disconnect command to server failed %d \n", ret);
        }
    }
    
    return ret;
}

static int CCIDbgr_CmdToServer(CCIDbgrCtxt_t *pClientCtxt)
{
    int ret = CAMERA_EFAILED;
    size_t msgSize = sizeof(CCIDbgrMsg_t);

    ret = OSAL_IPCSendAsync(pClientCtxt->serverIpcHndl,
                            &(pClientCtxt->msg),
                            msgSize,
                            0);

    if (ret == CAMERA_SUCCESS)
    {
        // For disconnect command expected not to receive respone
        if (pClientCtxt->msg.cmd != CCIDBGR_DISCONNECT_CMD) 
        {            
            // Listen for incoming messages from client. Blocking call
            ret = OSAL_IPCRecvAsync(pClientCtxt->clientIpcHndl, &(pClientCtxt->msg), 
                                    &msgSize, 1000);
            if (CAMERA_SUCCESS != ret)
            {
                fprintf(stderr, "Command response failed ret %d cmdStatus %u\n", 
                        ret, pClientCtxt->msg.cmdStatus);
            }
            else
            {
                switch(pClientCtxt->msg.cmd)
                {
                    case CCIDBGR_CONNECT_CMD:
                        if (CCIDBGR_CONNECTED != pClientCtxt->msg.cmdStatus)
                        {
                            fprintf(stderr, "Connect with qcx_server failed \n");
                        }
                        break;
                    case CCIDBGR_I2C_READ_CMD:
                    case CCIDBGR_I2C_READ21_CMD:
                    case CCIDBGR_I2C_READ22_CMD:
                    case CCIDBGR_I2C_READ_BURST_CMD:
                        if (CCIDBGR_READ_WRITE_FAILURE == pClientCtxt->msg.cmdStatus)
                        {
                            fprintf(stderr, "Read failed for the command \n");
                        }
                        break;
                    case CCIDBGR_I2C_WRITE_CMD:
                    case CCIDBGR_I2C_WRITE21_CMD:
                    case CCIDBGR_I2C_WRITE22_CMD:
                    case CCIDBGR_I2C_WRITE_BURST_CMD:
                        if (CCIDBGR_READ_WRITE_FAILURE == pClientCtxt->msg.cmdStatus)
                        {
                            fprintf(stderr, "Write failed for the command \n");
                        }
                        break;
                    case CCIDBGR_I2C_WRITE_THEN_READ_BURST_CMD:
                        if (CCIDBGR_READ_WRITE_FAILURE == pClientCtxt->msg.cmdStatus)
                        {
                            fprintf(stderr, "Write then read failed for the command \n");
                        }
                        break;
                    default:
                        fprintf(stderr, "Invalid command\n");
                        break;
                }
            }
        }
    }
    else
    {
        fprintf(stderr,"IPC send message failed %d\n", ret);
    }

    return ret;  
}

static int CCIDbgr_ChannelCreate(void)
{
    int ret = CAMERA_EFAILED;
    CCIDbgrCtxt_t *pClientCtxt = &g_CCIDbgrCtxt;
    
    if (TRUE == g_CCIDbgrCtxt.ccidbgrConfig.bStandalone)
    {
        // Init the Clock, CCI and GPIO etc.
        ret = CCIDbgr_StandaloneInit();
        if (ret != CAMERA_SUCCESS)
        {
            fprintf(stderr,"CCIDbgr_StandaloneInit failed %d \n", ret);
        }
    }
    else
    {
        // Check for the IPC channel 
        ret = access("/dev/name/local/QCX_debfcaff",F_OK);
        if (CAMERA_SUCCESS == ret)
        {
            // Connect over the IPC channel
            ret = OSAL_IPCConnect(CCIDBGR_SERVER_IPC_CHANNEL_ID,
                                     &pClientCtxt->serverIpcHndl);
            if (CAMERA_SUCCESS != ret)
            {
                fprintf(stderr,"IPC connect over the channel %u failed \n", 
                        CCIDBGR_SERVER_IPC_CHANNEL_ID);
            }
            else
            {
                // Create the IPC channel for client context to get data 
                ret = CCIDbgr_CreateClientCtxt();
                if (CAMERA_SUCCESS != ret)
                {
                    fprintf(stderr, "create client context failed %d \n", ret);
                }
                else
                {
                    // update the message command for handshake b/w server and client 
                    pClientCtxt->msg.cmd = CCIDBGR_CONNECT_CMD;
                    // send connect command to server 
                    ret = CCIDbgr_CmdToServer(pClientCtxt);
                    if (CAMERA_SUCCESS != ret)
                    {
                        fprintf(stderr, "connect command to server failed %d \n", ret);
                    }
                }
            }
                    
        }
        else
        {
               fprintf(stderr,"IPC Channel not created from qcx_server \n");
        }
    }

    return ret;
}

static int CCIDbgr_RdWrtStandalone(void)
{
    int ret = CAMERA_EFAILED;
    CCIDbgrMsg_t *pCciMsg = &g_CCIDbgrCtxt.msg;
    CameraI2CTransact_t i2cTrans;

    switch(pCciMsg->cmd)
    {                
        case CCIDBGR_I2C_READ_CMD:
        case CCIDBGR_I2C_READ21_CMD:
        case CCIDBGR_I2C_READ22_CMD:
            ret = PLM_I2CRead(&pCciMsg->i2cDev, &pCciMsg->settings,(CameraI2CReg_t *)(&pCciMsg->i2cReadWrite), 1);
            fprintf(stderr, "devId %d, i2cIdx %d i2cAddr 0x%x clockmode %d regAddrType %d dataType %d endian type %d regAddr%d, regData %d, delay %d", 
                      pCciMsg->i2cDev.devId, pCciMsg->i2cDev.i2cIdx, pCciMsg->i2cDev.i2cAddr, (int)pCciMsg->i2cDev.clockMode,
                      pCciMsg->settings.regAddrType, pCciMsg->settings.dataType, pCciMsg->settings.dataEndian, pCciMsg->i2cReadWrite.regAddr, 
                      pCciMsg->i2cReadWrite.regData, pCciMsg->i2cReadWrite.delay);
            if (CAMERA_SUCCESS != ret)
            {
                fprintf(stderr,"PLM_I2C Read failed %d \n", ret);
            }
            else
            {
                fprintf(stderr, "0x%x -> 0x%x\n", pCciMsg->i2cReadWrite.regAddr, pCciMsg->i2cReadWrite.regData);
            }
            break;
        case CCIDBGR_I2C_WRITE_CMD:
        case CCIDBGR_I2C_WRITE21_CMD:
        case CCIDBGR_I2C_WRITE22_CMD:
            ret = PLM_I2CWrite(&pCciMsg->i2cDev, &pCciMsg->settings,(CameraI2CReg_t *)(&pCciMsg->i2cReadWrite), 1);
            if (CAMERA_SUCCESS != ret)
            {
                fprintf(stderr,"PLM_I2C write failed %d", ret);
            }
            else
            {
                fprintf(stderr, "0x%x -> 0x%x \n", pCciMsg->i2cReadWrite.regAddr, pCciMsg->i2cReadWrite.regData);
            }
            break;

    case CCIDBGR_I2C_READ_BURST_CMD:
            i2cTrans.readOp.regAddr = pCciMsg->i2cTransact.regAddr;
            i2cTrans.readOp.regData = (uint8_t *)&burstReadArray[0];
            i2cTrans.readOp.regDataStore = sizeof(uint8_t);
            i2cTrans.readOp.size = pCciMsg->i2cTransact.readLen;
            i2cTrans.readOp.i2cSetting.regAddrType = pCciMsg->settings.regAddrType;
            i2cTrans.readOp.i2cSetting.dataType = pCciMsg->settings.dataType;
            i2cTrans.readOp.i2cSetting.dataEndian = pCciMsg->settings.dataEndian;
            i2cTrans.transactType = CAMERA_I2C_READ_BULK;
            QCX_LOG(PLATFORM, HIGH,"Burst Read Started");
            ret = PLM_I2CTransact(&pCciMsg->i2cDev,&i2cTrans);
            writeOutputFile(&burstReadArray[0], pCciMsg->i2cTransact.readLen, 1, pCciMsg->i2cTransact.burstOutFile);
            if (CAMERA_SUCCESS != ret)
            {
                fprintf(stderr,"PLM_I2CTransact failed %d", ret);
            }
            else
            {
                fprintf(stderr, "readlen 0x%x \n", pCciMsg->i2cTransact.readLen);
            } 
            break;

    case CCIDBGR_I2C_WRITE_BURST_CMD:
            i2cTrans.writeOp.regAddr = pCciMsg->i2cTransact.regAddr;
            i2cTrans.writeOp.regData = (uint8_t *)&burstWriteArray[0];
            i2cTrans.writeOp.regDataStore = sizeof(uint8_t);
            i2cTrans.writeOp.size = pCciMsg->i2cTransact.writeLen;
            i2cTrans.writeOp.i2cSetting.regAddrType = pCciMsg->settings.regAddrType;
            i2cTrans.writeOp.i2cSetting.dataType = pCciMsg->settings.dataType;
            i2cTrans.writeOp.i2cSetting.dataEndian = pCciMsg->settings.dataEndian;
            i2cTrans.transactType = CAMERA_I2C_WRITE_BULK;
            parseInputFile(&burstWriteArray[0],pCciMsg->i2cTransact.writeLen, 1, pCciMsg->i2cTransact.burstInFile);
            QCX_LOG(PLATFORM, HIGH,"Burst WRITE Started");
            ret = PLM_I2CTransact(&pCciMsg->i2cDev, &i2cTrans);
            if (CAMERA_SUCCESS != ret)
            {
                fprintf(stderr,"PLM_I2CTransact failed %d", ret);
            }
            else
            {
                fprintf(stderr, "write len0x%x \n", pCciMsg->i2cTransact.writeLen);
            }
            break;

    case CCIDBGR_I2C_WRITE_THEN_READ_BURST_CMD:
            i2cTrans.readOp.regAddr = pCciMsg->i2cTransact.regAddr;
            i2cTrans.readOp.regData = (uint8_t *)&burstReadArray[0];
            i2cTrans.readOp.regDataStore = sizeof(uint8_t);
            i2cTrans.readOp.size = pCciMsg->i2cTransact.readLen;
            i2cTrans.readOp.i2cSetting.regAddrType = pCciMsg->settings.regAddrType;
            i2cTrans.readOp.i2cSetting.dataType = pCciMsg->settings.dataType;
            i2cTrans.readOp.i2cSetting.dataEndian = pCciMsg->settings.dataEndian;

            i2cTrans.writeOp.regAddr = pCciMsg->i2cTransact.regAddr;
            i2cTrans.writeOp.regData = (uint8_t *)&burstWriteArray[0];
            i2cTrans.writeOp.regDataStore = sizeof(uint8_t);
            i2cTrans.writeOp.size = pCciMsg->i2cTransact.writeLen;
            i2cTrans.writeOp.i2cSetting.regAddrType = pCciMsg->settings.regAddrType;
            i2cTrans.writeOp.i2cSetting.dataType = pCciMsg->settings.dataType;
            i2cTrans.writeOp.i2cSetting.dataEndian = pCciMsg->settings.dataEndian;

            i2cTrans.transactType = CAMERA_I2C_WRITE_THEN_READ_BULK;

            parseInputFile(&burstWriteArray[0],pCciMsg->i2cTransact.writeLen, 1, pCciMsg->i2cTransact.burstInFile);
            ret = PLM_I2CTransact(&pCciMsg->i2cDev, &i2cTrans);
            writeOutputFile(&burstReadArray[0],pCciMsg->i2cTransact.readLen, 1, pCciMsg->i2cTransact.burstOutFile);
            if (CAMERA_SUCCESS != ret)
            {
                fprintf(stderr,"PLM_I2CTransact failed %d", ret);
            }
            else
            {
                fprintf(stderr, "0x%x -> 0x%x \n", pCciMsg->i2cTransact.readLen, pCciMsg->i2cTransact.writeLen);
            }
            break;

        default:
            fprintf(stderr, "message command not supported %d\n", g_CCIDbgrCtxt.msg.cmd);
            ret = CAMERA_EFAILED;
            break;
    }

    return ret;
}

static int CCIDbgr_RdWrtWithQCX(void)
{
    int ret = CAMERA_EFAILED;
    CCIDbgrCtxt_t *pClientCtxt = &g_CCIDbgrCtxt;

    // Send and receive message to camera service using IPC
    ret = CCIDbgr_CmdToServer(pClientCtxt);
    if (CAMERA_SUCCESS == ret)
    {
        if((pClientCtxt->msg.cmd !=  CCIDBGR_I2C_READ_BURST_CMD) &&
           (pClientCtxt->msg.cmd !=  CCIDBGR_I2C_WRITE_BURST_CMD) &&
           (pClientCtxt->msg.cmd !=  CCIDBGR_I2C_WRITE_THEN_READ_BURST_CMD))
           {
                fprintf(stderr, "0x%x -> 0x%x\n", pClientCtxt->msg.i2cReadWrite.regAddr,
                       pClientCtxt->msg.i2cReadWrite.regData);
           }
    }
    else
    {
        fprintf(stderr, "IPC message send/receive failed %d", ret);
    }

    return ret;
}

static int CCIDbgr_UpdateCmdOp(char *pCmdOp)
{
    int ret = CAMERA_SUCCESS;
    CCIDbgrMsg_t *pMsg = &g_CCIDbgrCtxt.msg;
    // ccidbgr command for read 1byte register address and value 
    if (!strcmp(pCmdOp, "read"))
    {
        pMsg->cmd = CCIDBGR_I2C_READ_CMD;
        pMsg->settings.regAddrType = CAMERA_I2C_BYTE_DATA;
        pMsg->settings.dataType = CAMERA_I2C_BYTE_DATA;
    }
    // ccidbgr command for read 2byte register address and 1 byte register value 
    else if (!strcmp(pCmdOp, "read21"))
    {
        pMsg->cmd = CCIDBGR_I2C_READ21_CMD;
        pMsg->settings.regAddrType = CAMERA_I2C_WORD_DATA;
        pMsg->settings.dataType = CAMERA_I2C_BYTE_DATA;
    }
    // ccidbgr command for read 2byte register address and value 
    else if (!strcmp(pCmdOp, "read22"))
    {
        pMsg->cmd = CCIDBGR_I2C_READ22_CMD;
        pMsg->settings.regAddrType = CAMERA_I2C_WORD_DATA;
        pMsg->settings.dataType = CAMERA_I2C_WORD_DATA;
    }
    // ccidbgr command for write 1byte register address and value 
    else if (!strcmp(pCmdOp, "write"))
    {
        pMsg->cmd = CCIDBGR_I2C_WRITE_CMD;
        pMsg->settings.regAddrType = CAMERA_I2C_BYTE_DATA;
        pMsg->settings.dataType = CAMERA_I2C_BYTE_DATA;
    }
    // ccidbgr command for write 2byte register address and 1 byte register value 
    else if (!strcmp(pCmdOp, "write21"))
    {
        pMsg->cmd = CCIDBGR_I2C_WRITE21_CMD;
        pMsg->settings.regAddrType = CAMERA_I2C_WORD_DATA;
        pMsg->settings.dataType = CAMERA_I2C_BYTE_DATA;
    }
    // ccidbgr command for write 2byte register address and value 
    else if (!strcmp(pCmdOp, "write22"))
    {
        pMsg->cmd = CCIDBGR_I2C_WRITE22_CMD;
        pMsg->settings.regAddrType = CAMERA_I2C_WORD_DATA;
        pMsg->settings.dataType = CAMERA_I2C_WORD_DATA;
    }
    else if (!strcmp(pCmdOp, "burstread"))
    {
        pMsg->cmd = CCIDBGR_I2C_READ_BURST_CMD;
        pMsg->settings.regAddrType = CAMERA_I2C_BYTE_DATA;
        pMsg->settings.dataType = CAMERA_I2C_BYTE_DATA;
    }
    else if (!strcmp(pCmdOp, "burstread21"))
    {
        pMsg->cmd = CCIDBGR_I2C_READ_BURST_CMD;
        pMsg->settings.regAddrType = CAMERA_I2C_WORD_DATA;
        pMsg->settings.dataType = CAMERA_I2C_BYTE_DATA;
    }
    else if (!strcmp(pCmdOp, "burstread22"))
    {
        pMsg->cmd = CCIDBGR_I2C_READ_BURST_CMD;
        pMsg->settings.regAddrType = CAMERA_I2C_WORD_DATA;
        pMsg->settings.dataType = CAMERA_I2C_WORD_DATA;
    }
    else if (!strcmp(pCmdOp, "burstwrite"))
    {
        pMsg->cmd = CCIDBGR_I2C_WRITE_BURST_CMD;
        pMsg->settings.regAddrType = CAMERA_I2C_BYTE_DATA;
        pMsg->settings.dataType = CAMERA_I2C_BYTE_DATA;
    }
    else if (!strcmp(pCmdOp, "burstwrite21"))
    {
        pMsg->cmd = CCIDBGR_I2C_WRITE_BURST_CMD;
        pMsg->settings.regAddrType = CAMERA_I2C_WORD_DATA;
        pMsg->settings.dataType = CAMERA_I2C_BYTE_DATA;
    }
    else if (!strcmp(pCmdOp, "burstwrite22"))
    {
        pMsg->cmd = CCIDBGR_I2C_WRITE_BURST_CMD;
        pMsg->settings.regAddrType = CAMERA_I2C_WORD_DATA;
        pMsg->settings.dataType = CAMERA_I2C_WORD_DATA;
    }
    else if (!strcmp(pCmdOp, "burstwriteread"))
    {
        pMsg->cmd = CCIDBGR_I2C_WRITE_THEN_READ_BURST_CMD;
        pMsg->settings.regAddrType = CAMERA_I2C_BYTE_DATA;
        pMsg->settings.dataType = CAMERA_I2C_BYTE_DATA;
    }
    else if (!strcmp(pCmdOp, "burstwriteread21"))
    {
        pMsg->cmd = CCIDBGR_I2C_WRITE_THEN_READ_BURST_CMD;
        pMsg->settings.regAddrType = CAMERA_I2C_WORD_DATA;
        pMsg->settings.dataType = CAMERA_I2C_BYTE_DATA;
    }
    else if (!strcmp(pCmdOp, "burstwriteread22"))
    {
        pMsg->cmd = CCIDBGR_I2C_WRITE_THEN_READ_BURST_CMD;
        pMsg->settings.regAddrType = CAMERA_I2C_WORD_DATA;
        pMsg->settings.dataType = CAMERA_I2C_WORD_DATA;
    }    
    else 
    {
        fprintf(stderr, "Invalid command provided during command operation \n");
        ret = CAMERA_EFAILED;
    }
        
    return ret;    
}

static int CCIDbgr_ParseCmdInfoFrmFile(char *pStr, int len)
{
    int ret = CAMERA_EFAILED;
    char cmd[CCIDBGR_CMD_SIZE] = {0};
    uint32_t idx = 0;
    uint32_t cmdPos = 0;
    uint32_t cmdValidate = 0;
    
    CCIDbgrMsg_t *const pMsg = &g_CCIDbgrCtxt.msg;
    // Initialize the i2c dev field with zero    
    pMsg->i2cDev.devId = 0; 
    pMsg->i2cDev.i2cAddr = 0;
    pMsg->i2cReadWrite.regAddr = 0;
    pMsg->i2cReadWrite.regData = 0;
    pMsg->i2cReadWrite.delay = 0;
    
    for (int i = 0; i <= len; i++)
    {
        // check for command speration from string
        if (*pStr != ' ' && *pStr != '\0' && *pStr != '=' && *pStr != '\t')
        {
            cmd[idx] = *pStr; // get the command in array
            idx++;         // increment the array indx
        }
        // check if any dealy provided in command
        else if (*pStr == '=')
        {
            idx = 0;     // initialize the array index from starting 
            cmdPos = 6;  // updting the value for delay
        }
        // Skip if any tab or extra space provided in command
        else if (((*pStr == ' ') || (*pStr == '\t' )) && (0 == idx))
        {
            // Do nothing just Skip the tab and blank space if there
        }
        else
        {
            // check if starting index itself is blank
            if (0 != i && 0 != idx)
            {             
                switch (cmdPos)
                {
                    // update the CCI id
                    case 0:
                        pMsg->i2cDev.devId = strtol(cmd, NULL, 16);
                        cmdPos++;
                        break; 
                    // update the slave address for register read/write
                    case 1:
                        pMsg->i2cDev.i2cAddr = strtol(cmd, NULL, 16);
                        cmdPos++;
                        break;
                    // update the command operation eg : read/write
                    case 2:
                        ret = CCIDbgr_UpdateCmdOp(cmd);
                        cmdPos++;
                        break;
                    // update the register address for read/write
                    case 3:
                        pMsg->i2cReadWrite.regAddr = strtol(cmd, NULL, 16);
                        cmdValidate = cmdPos;
                        cmdPos++;
                        break;
                    // update the regiater value for I2C write, 
                    // for read get the number of register for continuous read
                    case 4:
                        pMsg->i2cReadWrite.regData = strtol(cmd, NULL, 16);
                        cmdValidate = cmdPos;
                        cmdPos++;
                        break;
                    // update delay info if provided
                    case 6:                      
                        pMsg->i2cReadWrite.delay = strtol(cmd, NULL, 0);
                        cmdPos++;
                        break;
                    default:
                        fprintf(stderr, "Provide command is invalid \n");
                        ret = CAMERA_EFAILED;
                        break;
                }
            }
            idx = 0;
            (void)memset(cmd, 0, sizeof(cmd));
        }
        pStr++;
    }
    if (CAMERA_SUCCESS == ret)
    {
        // validate the parse command from the file
        if ((CCIDBGR_I2C_READ_CMD <= pMsg->cmd) && (CCIDBGR_I2C_READ22_CMD >= pMsg->cmd))
        {
            if (3 <= cmdValidate)
            {
                ret = CAMERA_SUCCESS;
            }
            else
            {
                fprintf(stderr, "bad command\n");
                ret = CAMERA_EFAILED;
            }
        }
        else if ((CCIDBGR_I2C_WRITE_CMD <= pMsg->cmd) && (CCIDBGR_I2C_WRITE22_CMD >= pMsg->cmd))
        {
            if (4 <= cmdValidate)
            {
                ret = CAMERA_SUCCESS;
            }
            else
            {
                fprintf(stderr, "bad command\n");
                ret = CAMERA_EFAILED;
                
            }
        }
        else
        {
            fprintf(stderr, "bad command\n");
            ret = CAMERA_EFAILED;
        }
    }

    if (CAMERA_SUCCESS == ret)
    {
        // update the I2C clock mode
         pMsg->i2cDev.clockMode = CAMERA_I2C_MODE_FAST;
        // update the operation mode to disable the error notification to diagnostic manager
        //pMsg->settings.opMode = 1;
    }

    return ret;
}

static int CCIDbgr_CmdReadFrmFile(void)
{
    int ret        = CAMERA_EFAILED;
    char *pStr     = NULL;
    size_t len     = 0;
    uint32_t count = 1;
    FILE *fp;
    ssize_t read;
    
    CCIDbgrMsg_t *pMsg = &g_CCIDbgrCtxt.msg;
    pStr=(char *)malloc(CCIDBGR_CMD_SIZE);

    ret = access (g_CCIDbgrCtxt.ccidbgrConfig.fileName, F_OK);
    if (CAMERA_SUCCESS == ret)
    {
        // open the file in read only mode
        fp = fopen(g_CCIDbgrCtxt.ccidbgrConfig.fileName, "r");
        if (fp == NULL)
        {
            fprintf(stderr, "File open pointer is NULL\n");
            ret = CAMERA_EFAILED;
        }
    }
    
    if (CAMERA_SUCCESS == ret)
    {
        // read the file line by line
        while ((read = getline(&pStr, &len, fp)) != -1) 
        {
            // parse the command from the read line
            ret = CCIDbgr_ParseCmdInfoFrmFile(pStr, read);
            if (CAMERA_SUCCESS != ret)
            {
                fprintf(stderr,"parse command from file is failed %d\n", ret);
                break;
            }
            else
            {
                // check for continuous read based on the
                if (((CCIDBGR_I2C_READ_CMD == pMsg->cmd) || (CCIDBGR_I2C_READ21_CMD == pMsg->cmd)
                   || (CCIDBGR_I2C_READ22_CMD == pMsg->cmd)) && (0 != pMsg->i2cReadWrite.regData))
                {
                    count = pMsg->i2cReadWrite.regData;
                }
                else
                {
                    // If continuous read is not enabled then
                    // atleast run one time to excute the current command
                    count = 1;
                }

                while(count > 0)
                {                
                    if (TRUE == g_CCIDbgrCtxt.ccidbgrConfig.bStandalone)
                    {
                        QCX_LOG(PLATFORM,HIGH,"Read/Write with Standalone \n");
                        ret = CCIDbgr_RdWrtStandalone();
                        if (CAMERA_SUCCESS != ret)
                        {
                            fprintf(stderr, "ccidbgr standalone read/write failed %d\n", ret);
                            break;
                        }
                    }
                    else
                    {
                        QCX_LOG(PLATFORM,HIGH,"Read/Write with QCX \n");
                        ret = CCIDbgr_RdWrtWithQCX();
                        if (CAMERA_SUCCESS != ret)
                        {
                            fprintf(stderr, 
                                   "ccidbgr read/write failed failed with qcx_server %d\n", ret);
                            break;
                        }
                    }
                    count--;
                    
                    if (count > 0)
                    {
                        // Increment the register address by one if continuous read enabled
                        pMsg->i2cReadWrite.regAddr = pMsg->i2cReadWrite.regAddr + 1;
                    }
                }
            }
        }
    }

    return ret;
}

static int CCIDbgr_ParseCmd(int argCnt, char *argv[])
{
    int ret = CAMERA_EFAILED;
    uint32_t idx = g_CCIDbgrCtxt.idx;
    uint8_t cciID;
    
    // check number of argument count before accessing
    if (argCnt < (idx + IDX_REG))
    {
        fprintf(stderr, "%s: bad no. of parameters.\n", argv[0]);
        ret = CAMERA_EFAILED;
    }
    else
    {
        CCIDbgrMsg_t *const pMsg = &g_CCIDbgrCtxt.msg;

        ret = CCIDbgr_UpdateCmdOp(argv[idx + IDX_OP]);
        // Get the CCI device ID
        cciID = strtol(argv[idx + IDX_CCI], NULL, 0); //0-7
        pMsg->i2cDev.devId = cciID/2;               /**< Sensor lib device ID */
        pMsg->i2cDev.i2cIdx = cciID%2;   
        // Get the device slave address
        pMsg->i2cDev.i2cAddr= strtol(argv[idx + IDX_SLAVE_ADDR], NULL, 0);
        // Get the register to read/write
         if(pMsg->cmd == CCIDBGR_I2C_WRITE_BURST_CMD)
         {
             pMsg->i2cTransact.regAddr = strtol(argv[idx + IDX_BURST_WRITE_LEN], NULL, 0);
             pMsg->i2cTransact.writeLen = strtol(argv[idx + IDX_BURST_WRITE_LEN+1], NULL, 0);
             OSAL_Strlcpy((char *)pMsg->i2cTransact.burstInFile, argv[idx + IDX_BURST_WRITE_LEN+2],FILE_NAME_LEN);
             QCX_LOG(PLATFORM,HIGH,"Burstwrite regAddr %d \n", pMsg->i2cTransact.regAddr);
         }
         else if(pMsg->cmd == CCIDBGR_I2C_READ_BURST_CMD)
         {
            pMsg->i2cTransact.regAddr = strtol(argv[idx + IDX_BURST_READ_LEN], NULL, 0);
            pMsg->i2cTransact.readLen = strtol(argv[idx + IDX_BURST_READ_LEN+1], NULL, 0);
            OSAL_Strlcpy((char *)pMsg->i2cTransact.burstOutFile, argv[idx + IDX_BURST_READ_LEN+2],FILE_NAME_LEN);
            QCX_LOG(PLATFORM,HIGH,"BurstRead regAddr %d \n", pMsg->i2cTransact.regAddr);
         }
         else if(pMsg->cmd == CCIDBGR_I2C_WRITE_THEN_READ_BURST_CMD)
         {
            pMsg->i2cTransact.regAddr = strtol(argv[idx + IDX_BURST_READ_LEN], NULL, 0);
            pMsg->i2cTransact.readLen = strtol(argv[idx + IDX_BURST_READ_LEN+1], NULL, 0);
            OSAL_Strlcpy((char *)pMsg->i2cTransact.burstInFile, argv[idx + IDX_BURST_READ_LEN+2],FILE_NAME_LEN);
            pMsg->i2cTransact.writeLen= pMsg->i2cTransact.readLen;
            OSAL_Strlcpy((char *)pMsg->i2cTransact.burstOutFile, argv[idx + IDX_BURST_READ_LEN+3],FILE_NAME_LEN);
         }
         else
         {
            //read/write
            pMsg->i2cReadWrite.regAddr=strtol(argv[idx + IDX_REG], NULL, 0);
            // Get the register value for write or number of register to read
            if ((idx + IDX_VAL + 1) == argCnt)
            {
                pMsg->i2cReadWrite.regData = strtol(argv[idx + IDX_VAL], NULL, 0);
            }
         }
        // update the I2C clock mode
        pMsg->i2cDev.clockMode = CAMERA_I2C_MODE_FAST;
        pMsg->settings.dataEndian = CAMERA_I2C_ENDIAN_BIG;
        pMsg->i2cReadWrite.delay = 0;
        // update the operation mode to disable the error notification to diagnostic manager
        //TBD
        //pMsg->settings.opMode = 1;
        if (CAMERA_SUCCESS != ret)
        {
            fprintf(stderr, "update command operation during parseCmd failed %d", ret);
        }
    }

    return ret;
}

static int CCIDbgr_SlaveAddrCheck(void)
{
    // ToDo
    return 0;
}


int main(int argc, char *argv[])
{
    int ret            = CAMERA_EFAILED;
    CCIDbgrMsg_t* pMsg = &g_CCIDbgrCtxt.msg;
    uint32_t count = 0;

    ret = QcxLogInit();
    if (CAMERA_SUCCESS != ret)
    {
        fprintf(stderr, "QCXLogInit failed %d\n", ret);
    }
    ret = CCIDbgr_ParseCmdLineParameter(argc, argv);
    if (CAMERA_SUCCESS != ret)
    {
        fprintf(stderr, "Processing Command line arugment failed %d\n", ret);
    }

    if ((CAMERA_SUCCESS == ret) && (TRUE != g_CCIDbgrCtxt.ccidbgrConfig.bCmdHelp))
    {
        ret = CCIDbgr_ChannelCreate();
        if (CAMERA_SUCCESS != ret)
        {
            fprintf(stderr, "ccidbgr channel configuration failed %d\n", ret);
        }
    }
    
    if ((CAMERA_SUCCESS == ret) && (TRUE != g_CCIDbgrCtxt.ccidbgrConfig.bCmdHelp))
    {
        if (TRUE == g_CCIDbgrCtxt.ccidbgrConfig.bSlaveAddrCheck)
        {
            // Check all the active slave address on cci bus
            ret = CCIDbgr_SlaveAddrCheck();
            if (CAMERA_SUCCESS != ret)
            {
                fprintf(stderr, "check active slave address failed %d\n", ret);
            }
        }
        else if (TRUE == g_CCIDbgrCtxt.ccidbgrConfig.bCmdFrmFile)
        {
            //CCI command read from file
            ret = CCIDbgr_CmdReadFrmFile();
            if (CAMERA_SUCCESS != ret)
            {
                fprintf(stderr, "Read and excute command from file failed %d\n", ret);
            }
        }
        else
        {
            // Parse complete command form the provided command line argument 
            ret = CCIDbgr_ParseCmd(argc, argv);
            if (CAMERA_SUCCESS != ret)
            {
                fprintf(stderr, "parsing command line failed %d\n", ret);
            }
            else
            {
                // check for continuous read based on the command
                if (((CCIDBGR_I2C_READ_CMD == pMsg->cmd) || (CCIDBGR_I2C_READ21_CMD == pMsg->cmd)
                   || (CCIDBGR_I2C_READ22_CMD == pMsg->cmd)) && (0 != pMsg->i2cReadWrite.regData))
                {
                    count = g_CCIDbgrCtxt.msg.i2cReadWrite.regData;
                }
                else
                {
                    // If continuous read is not enabled then
                    // atleast run one time to excute the current command
                    count = 1;
                }
                
                while(count > 0)
                {
                    if (TRUE == g_CCIDbgrCtxt.ccidbgrConfig.bStandalone)
                    {
                        ret = CCIDbgr_RdWrtStandalone();
                        if (CAMERA_SUCCESS != ret)
                        {
                            fprintf(stderr, "ccidbgr standalone read/write failed %d\n", ret);
                            break;
                        }
                    }
                    else
                    {
                        ret = CCIDbgr_RdWrtWithQCX();
                        if (CAMERA_SUCCESS != ret)
                        {
                            fprintf(stderr, 
                                    "ccidbgr read/write failed failed with qcx_server %d\n", ret);
                            break;
                        }
                    }
                    count--;
                    
                    if (count > 0)
                    {
                        // Increment the register address by one if continuous read enabled
                        pMsg->i2cReadWrite.regAddr = pMsg->i2cReadWrite.regAddr + 1;
                    }
                }
            }
        }
    }

    // Skip De-init if cmdline parameter is help
    if (TRUE != g_CCIDbgrCtxt.ccidbgrConfig.bCmdHelp)
    {
        //cleanup the initialized resource
        ret = CCIDbgr_Deinit();
        if (CAMERA_SUCCESS != ret)
        {
            fprintf(stderr, "ccidbgr De_init failed %d \n",ret);
        }
    }

    return ret;
}


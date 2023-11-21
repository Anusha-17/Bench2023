#ifndef QCX_CCIDBGR_H
#define QCX_CCIDBGR_H
/**================================================================================================

 @file
 qcx_ccidbgr.h

 @brief
 This file declares functions for qcd_ccidbgr.

 Copyright (c) 2022 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.

 ================================================================================================**/

/**================================================================================================
 INCLUDE FILES FOR MODULE
 ================================================================================================**/
//#include "sensorsdk.h"
#include "qcxplatform.h"
#include "qcxplatformcommon.h"
#include "qcxstatus.h"
#include "qcxplatformi2c.h"
#include "qcxlog.h"
#include "qcxosal.h"
#include "qcxosalipc.h"
#include "qcxosalthread.h"
#include "platformccidbgr.h"

#ifdef __cplusplus
extern "C" {
#endif 

/**================================================================================================
 ** Constant and Macros
 ================================================================================================**/
//parameter index in the command line
#define IDX_CCI                     1U      /* CCI Id for read/write             */
#define IDX_SLAVE_ADDR              2U      /* Slave address                     */
#define IDX_OP                      3U      /* Operation for read/write          */
#define IDX_REG                     4U      /* Register to read/write            */
#define IDX_VAL                     5U      /* write: value to write, 
                                               read: optional no. of consecutive
                                               registers to read                 */
#define IDX_BURST_READ_LEN          4U
#define IDX_BURST_WRITE_LEN         4U
#define IDX_BURST_WRITE_READ_LEN    6U
                                              
#define CCIDBGR_CMD_SIZE            128U    /* CCI debugger command size         */

/**================================================================================================
 ** Typedefs
 ================================================================================================**/

/**********************************************************************************************//**
@brief
    Enum for CCIDbgr command line parameter
**************************************************************************************************/
typedef enum
{
    CCIDBGR_PARAMETER_FIRST = 0,             /**< Command line first parameter             */      
    CCIDBGR_PARAMETER_STANDALONE = CCIDBGR_PARAMETER_FIRST,                              
                                             /**< Command for cci debugger standalone      */ 
    CCIDBGR_PARAMETER_RDFRFILE,              /**< Command for read/write from file         */
    CCIDBGR_PARAMETER_SLAVEADDRCHECK,        /**< Command to check the active slave addr   */
    CCIDBGR_PARAMETER_CMD_DELAY,             /**< Command to provide the dealy             */
    CCIDBGR_PARAMETER_HELP,                  /**< Commadn to print the info for read/write */
    CCIDBGR_PARAMETER_MAX
} CCIDbgrParameter_e;

/**********************************************************************************************//**
@brief
    CCIDbgr config stucture for different mode
**************************************************************************************************/
typedef struct
{
    char        fileName[CCIDBGR_CMD_SIZE];  /**< file name to parse the read/write command */
    bool        bStandalone;                 /**< Set if ccidbgr run in standalone mode     */    
    bool        bSlaveAddrCheck;             /**< To check the active I2C device            */
    bool        bCmdFrmFile;                 /**< For read from the file                    */
    bool        bCmdHelp;                    /**< Provide command info for help             */
} CCIDbgrConfig_t;

/**********************************************************************************************//**
@brief
    CCIDbgr PID for client
**************************************************************************************************/
typedef struct
{
    uint32_t pid;                         /**< PID of process which initialized client lib */
} ClientProcInfo_t;

/**********************************************************************************************//**
@brief
    CCIDbgr stucture send from the client with client ID, serverand message information for 
    CCI read/write.
**************************************************************************************************/
typedef struct
{
    uint32_t            clientId;         /**< Client Id for IPC              */
    QCXIPCHdl_t        clientIpcHndl;    /**< Client IPC handle              */
    QCXIPCHdl_t        serverIpcHndl;    /**< Server IPC handle              */
    CCIDbgrConfig_t     ccidbgrConfig;    /**< Configuration for opertaion    */
    ClientProcInfo_t    clientInfo;       /**< Client PID info                */
    CCIDbgrMsg_t        msg;              /**< Message to send to server      */
    uint32_t            idx;              /**< idx for command line parameter */
} CCIDbgrCtxt_t;

/**================================================================================================
 ** Variables
 ================================================================================================**/

/**================================================================================================
 DEFINITIONS AND DECLARATIONS
 ================================================================================================**/

#ifdef __cplusplus
}
#endif
#endif // QCX_CCIDBGR_H

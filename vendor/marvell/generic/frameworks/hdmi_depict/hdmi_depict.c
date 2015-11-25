
/*******************************************************************************
*                Copyright 2012, MARVELL SEMICONDUCTOR, LTD.                   *
* THIS CODE CONTAINS CONFIDENTIAL INFORMATION OF MARVELL.                      *
* NO RIGHTS ARE GRANTED HEREIN UNDER ANY PATENT, MASK WORK RIGHT OR COPYRIGHT  *
* OF MARVELL OR ANY THIRD PARTY. MARVELL RESERVES THE RIGHT AT ITS SOLE        *
* DISCRETION TO REQUEST THAT THIS CODE BE IMMEDIATELY RETURNED TO MARVELL.     *
* THIS CODE IS PROVIDED "AS IS". MARVELL MAKES NO WARRANTIES, EXPRESSED,       *
* IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY, COMPLETENESS OR PERFORMANCE.   *
*                                                                              *
* MARVELL COMPRISES MARVELL TECHNOLOGY GROUP LTD. (MTGL) AND ITS SUBSIDIARIES, *
* MARVELL INTERNATIONAL LTD. (MIL), MARVELL TECHNOLOGY, INC. (MTI), MARVELL    *
* SEMICONDUCTOR, INC. (MSI), MARVELL ASIA PTE LTD. (MAPL), MARVELL JAPAN K.K.  *
* (MJKK), MARVELL ISRAEL LTD. (MSIL).                                          *
*******************************************************************************/


#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "amp_client.h"
#include "amp_client_support.h"


/*Global varible*/
AMP_DISP gDisp;
HANDLE hListener;

static CHAR *client_argv[] =
    {"client", "iiop:1.0//127.0.0.1:999/AMP::FACTORY/factory"};



static int cmd_handler_hdmi_help(char *cmdname)
{
    printf("Usage: %s on:  Turn on  HDMI\n", cmdname);
    printf("       %s off: Turn off HDMI\n", cmdname);
    return 0;
}

static int cmd_handler_hdmi(int argc, char *argv[])
{
    HRESULT ret;


    if (strcmp(argv[1], "on") == 0) {
        AMP_RPC(ret, AMP_DISP_SetPortPower, gDisp, AMP_DISP_PORT_HDMI, 1);
        AMP_RPC(ret, AMP_DISP_OUT_SetMute, gDisp, AMP_DISP_OUT_HDMI, 0);
    }else if (strcmp(argv[1], "off") == 0)
    {
        AMP_RPC(ret, AMP_DISP_OUT_SetMute, gDisp, AMP_DISP_OUT_HDMI, 1);
        AMP_RPC(ret, AMP_DISP_SetPortPower, gDisp, AMP_DISP_PORT_HDMI, 0);
    }
    
    return 0;
}



int main(int argc, char **argv)
{

    if (argc < 2){
        cmd_handler_hdmi_help(argv[0]);
        return -1;
    }


    if (strcmp(argv[1], "on") != 0 
        && strcmp(argv[1], "off") != 0 ) {
        cmd_handler_hdmi_help(argv[0]);
        return -1;
    }


    unsigned int Ret;
    AMP_FACTORY hFactory;

    AMP_LOG_Initialize(AMP_LOG_FATAL, AMP_LOG_CONSOLE);
    AMP_LOG_Control(AMP_LOG_CMD_SETLEVEL, MODULE_GENERIC,
                    ((1 << AMP_LOG_USER1) - 1), NULL);
    MV_OSAL_Init();


    /* Get factory */
    do{
        Ret = AMP_Initialize(2, client_argv, &hFactory);
        if( Ret == SUCCESS) {
            break;
        }
        printf("%s,%s: Waiting for AMP initialize!\n", __FILE__, __FUNCTION__);
        usleep(200000);
    }while(1);

    assert(AMP_GetFactory(&hFactory) == SUCCESS);

    /* Create disp service */
    AMP_RPC(Ret, AMP_FACTORY_CreateDisplayService, hFactory, &gDisp);
    if( Ret == SUCCESS ) {
        printf("Create disp service OK\n");
    } else {
        printf("Create disp service Failed\n");
    }

    
    cmd_handler_hdmi(argc, argv);

    AMP_FACTORY_Release(hFactory);

    MV_OSAL_Exit();

    return 0;
}



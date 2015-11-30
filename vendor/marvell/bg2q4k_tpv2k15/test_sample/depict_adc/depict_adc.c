
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "com_type.h"
#include "sm_common.h"
#include "sm_agent.h"

static int usage(const char *app_name)
{
    printf("Usage:\n"
           "  %s <channel> <sample>\n", app_name);
 
    return 0;
}



int read_adc(int channel, int sample)
{    
	int module_id = 5;
	int msg_id = 0xee;
	int val = -1;
	
	INT32 *p_msg = NULL;
	INT32 len = 0;
	int ret = 0;
	MV_SM_Message sm_msg_send;
	MV_SM_Message sm_msg_recv;

	MV_SM_Agent_Init();

	memset(&sm_msg_send, 0, sizeof(MV_SM_Message));
	memset(&sm_msg_recv, 0, sizeof(MV_SM_Message));

	sm_msg_send.m_iModuleID = module_id;
	sm_msg_recv.m_iModuleID = module_id;

	p_msg = (INT32 *)(sm_msg_send.m_pucMsgBody);
	p_msg[0] = msg_id;
	p_msg[1] = channel;
	p_msg[2] = sample;

	sm_msg_send.m_iMsgLen = 3 * sizeof(INT32);

	ret = MV_SM_Agent_Write_Msg(sm_msg_send.m_iModuleID, sm_msg_send.m_pucMsgBody,
					sm_msg_send.m_iMsgLen);
	if (ret < 0) {
		printf("[%s] write msg error: %d!\n", __func__, ret);
		return -1;
	}


	if (MV_SM_Agent_Read_Msg_Ext(sm_msg_recv.m_iModuleID, &sm_msg_recv, &len, 1) == S_OK) {
		val = sm_msg_recv.m_pucMsgBody[0];
	}else {
                printf("[%s] read msg error!\n", __func__);
                return -1;
        }


	MV_SM_Agent_Close();

	return val;

}


int main(int argc, const char *argv[])
{

    if (argc < 3) {
        usage(argv[0]);
        return -1;
    }
    
    int channel = atoi(argv[1]);
    int sample = atoi(argv[2]);
    
    int val = read_adc(channel, sample);
    
    printf("adc channel %d val: %d\n", channel, val);
    
    return 0;
}


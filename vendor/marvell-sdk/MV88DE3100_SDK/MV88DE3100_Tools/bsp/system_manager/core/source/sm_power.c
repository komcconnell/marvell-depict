#include "com_type.h"
#include "sys_mgr.h"
#include "sm_common.h"
#include "sm.h"

#include "platform_config.h"

#include "sm_apb_gpio_defs.h"
#include "sm_comm.h"

#include "sm_state.h"
#include "sm_printf.h"
#include "sm_timer.h"
#include "sm_mem.h"
#include "sm_debug.h"

#include "sm_state.h"
#include "sm_def.h"
#include "sm_gpio.h"
#include "sm_adc.h"
#include "sm_temperature.h"
#include "sm_power.h"
#include "sm_ir_key_def.h"

#ifdef UNIT_TEST
#define static
#endif

BOOL bFastboot = FALSE;
BOOL bFastboot_Standby = FALSE;
BOOL bRequest_Poweron = FALSE;


/*-------------------------------*/

#define MS (SM_SYSTEM_HZ * 1000)
#define POWEROFF2POWERON 300
#define POWERON2OKTIME 60
#define ASSERT2DEASSERTTIME 100 //this time should be longer

static UINT32 power_mystatus = STA_INVALID;
static UINT32 power_myflow = FLOW_IDLE;

#ifndef UART_WORKAROUND
const static char* flow_name[] = {
	"---Active to Lowpower Standby---",
	"---Active to Normal Standby---",
	"---Active to Suspend---",

	"---Lowpower Standby to Active---",
	"---Normal Standby to Active---",
	"---Suspend to Active---",

	"---Sysreset to Lowpower Standby---",
	"---Sysreset to Normal Standby---",
	"---Sysreset to Active---",

	"---Cold to Lowpower Standby---",

	"---Idle---",
};
#endif

static INT32 power_lasttime = 0;
static BOOL is_get_rsp = FALSE;

static BOOL is_linux_ready = FALSE;

#define TIMEOUT_POWERFLOW (10 * 1000 * 1000)  //10S
static INT32 timer_id = -1;
static INT32 wmdn_timer_enable = 0, wmup_timer_enable = 0;
static INT32 cur_wakeup_time = 0;
#ifdef LTIMER_ENABLE
static timer_long_t wmdn_timer_begin, wmup_timer_begin;
static UINT32 wmdn_timer_interval, wmup_timer_interval;
#else /* LTIMER_ENABLE */
static INT32 wmdn_timer_begin, wmdn_timer_interval, wmup_timer_begin, wmup_timer_interval;
#endif /*!LTIMER_ENABLE*/
volatile static BOOL is_wakeup = FALSE;
static MSG_LINUX_2_SM standby_type = DEFAULT_STANDBY_REQUEST_TYPE;


volatile UINT32	linux_resume_addr = 0xFFFFFFFF;

static volatile MV_SM_WAKEUP_SOURCE_TYPE wakeup_source = MV_SM_WAKEUP_SOURCE_INVALID;

extern INT32 chip_subver;
extern INT32 chip_ver;

extern inline void board_peripheral_assert(void);
extern inline void board_peripheral_deassert(void);
extern inline void board_soc_poweron(void);
extern inline void board_soc_poweroff(void);
extern inline INT32 board_soc_power_status(void);

static inline BOOL waittime(INT32 interval)
{
	INT32 curr = mv_sm_timer_gettimems();
	if((curr - power_lasttime) >= interval) {
		power_lasttime = curr;
		return TRUE;
	}
	return FALSE;
}

#ifdef UART_WORKAROUND
void apb_uart_reset(void);
void apb_uart_enable(void);
#endif

static hresult soc_assert(void)
{
	T32smSysCtl_SM_RST_CTRL reg;
	//reset the SOC by set the SM_RST_CTRL register
	reg.u32=MV_SM_READ_REG32(SM_SM_SYS_CTRL_REG_BASE + RA_smSysCtl_SM_RST_CTRL);
	reg.uSM_RST_CTRL_SOC_RST_GO = 0;
	MV_SM_WRITE_REG32(SM_SM_SYS_CTRL_REG_BASE + RA_smSysCtl_SM_RST_CTRL, reg.u32);
#ifndef UART_WORKAROUND
	PRT_INFO("%s done\n", __func__);
#endif

	return S_OK;
}

static hresult soc_deassert(void)
{
#ifdef UART_WORKAROUND
	if (chip_ver == BERLIN_BG2_Q && chip_subver == 0xA1D)
		apb_uart_reset();	/* reset UART to workaroud UART access conflict issue */
#endif
	T32smSysCtl_SM_RST_CTRL reg;

	//deasset the SOC by set the SM_RST_CTRL register
	reg.u32=MV_SM_READ_REG32(SM_SM_SYS_CTRL_REG_BASE + RA_smSysCtl_SM_RST_CTRL);
	reg.uSM_RST_CTRL_SOC_RST_GO = 1;
	MV_SM_WRITE_REG32(SM_SM_SYS_CTRL_REG_BASE + RA_smSysCtl_SM_RST_CTRL, reg.u32);
#ifndef UART_WORKAROUND
	PRT_INFO("%s done\n", __func__);
#endif

	return S_OK;
}

static hresult peripheral_assert(void)
{
	board_peripheral_assert();
#ifndef UART_WORKAROUND
	PRT_INFO("%s done\n", __func__);
#endif
	return S_OK;
}

static hresult peripheral_deassert(void)
{
	board_peripheral_deassert();
#ifndef UART_WORKAROUND
	PRT_INFO("%s done\n", __func__);
#endif
	return S_OK;
}

BOOL mv_sm_get_linux_state(void)
{
	return is_linux_ready;
}

hresult mv_sm_power_socpoweron(void)
{
	board_soc_poweron();
#ifndef UART_WORKAROUND
	PRT_INFO("%s done\n", __func__);
#endif
	return S_OK;
}

hresult mv_sm_power_socpoweroff(void)
{
	peripheral_assert();
	soc_assert();
	board_soc_poweroff();
#ifndef UART_WORKAROUND
	PRT_INFO("%s done\n", __func__);
#endif
	return S_OK;
}

INT32 mv_sm_power_soc_power_status(void)
{
	return board_soc_power_status();
}


/* broadcast flow type and status. */
static void mv_sm_power_broadcast_power_state()
{
	UINT32 msg[3];

	msg[0] = MV_SM_ID_POWER;
	msg[1] = power_myflow;
	msg[2] = power_mystatus;

	mv_sm_send_internal_msg(BOARDCASTMSG, (void *)msg, (3 * sizeof(UINT32)));
}


hresult mv_sm_power_enterflow(SM_POWER_FLOW flow)
{
	if((power_myflow != FLOW_IDLE) || (power_mystatus != STA_INVALID)) {
		PRT_ERROR("Last flow didn't finish. flow = %d, status = %d\n",
			power_myflow, power_mystatus);
		return S_FALSE;
	}

#ifndef UART_WORKAROUND
	PRT_INFO("******enter flow %s\n", flow_name[flow]);
#endif

	is_linux_ready = FALSE;
	power_mystatus = STA_ENTERFLOW;
	power_myflow = flow;
	power_lasttime = mv_sm_timer_gettimems();
	return S_OK;
}

static void mv_sm_power_up_ontime(void)
{
#ifdef LTIMER_ENABLE
	static INT32 one_hour_time = 3600000;
	INT32 ret = S_OK;
	UINT32 tmp_time, duration = 0;
	INT32 state = mv_sm_get_state();

	timer_long_t current_time;

	if(wmup_timer_enable == 0)
		return;

	ret = mv_sm_timer_getlongtimems(&current_time);
	if (ret == S_OK)
	{
		if (current_time.low >= wmup_timer_begin.low)
		{
			// only count 32bits duration (max about 48 days)
			duration = current_time.low - wmup_timer_begin.low;
		}
		else
		{
			duration = (MAX_TIMER_MS - wmup_timer_begin.low) + current_time.low + 1000;
		}

		if (duration > wmup_timer_interval)
		{
			cur_wakeup_time = 0;
			PRT_INFO("power up time out. time begin is %u:%u, interval is %u current is %u:%u, duration is %u\n",
					wmup_timer_begin.high, wmup_timer_begin.low, wmup_timer_interval, current_time.high, current_time.low, duration);
			wmup_timer_enable = 0;
			if(state == MV_SM_STATE_ACTIVE)
			{
				PRT_INFO("system is in ACTIVE state so ignore wakeup!!");
				return;
			}
			mv_sm_power_setwakeupsource(MV_SM_WAKEUP_SOURCE_TIMER);
			mv_sm_power_enterflow_bysmstate();
		}
		else
		{
			cur_wakeup_time = (INT32)((wmup_timer_interval - duration)/1000);
			tmp_time = duration % one_hour_time; //3600000
			if (tmp_time == 0)
			{
				PRT_INFO("current power up time: time begin is %u:%u, interval is %u current is %u:%u diff is %u\n",
						wmup_timer_begin.high, wmup_timer_begin.low, wmup_timer_interval,
						current_time.high, current_time.low, duration);
			}
		}
	}
	else
	{
		PRT_INFO("cannot get current long time value!!!!\n");
	}
#else /* LTIMER_ENABLE */
	INT32 state = mv_sm_get_state();

	INT32 current_time;
	INT32 duration = 0;
	static INT32 one_hour_time = 3600000;
	INT32 tmp_time;

	if(wmup_timer_enable == 0)
		return;
	current_time = mv_sm_timer_gettimems();
	duration = current_time - wmup_timer_begin;

	if(duration > wmup_timer_interval)
	{

		cur_wakeup_time = 0;
		PRT_INFO("power up time out. time begin is %d, interval is %d current is %d\n", wmup_timer_begin, wmup_timer_interval, current_time);
		wmup_timer_enable = 0;
		if(state == MV_SM_STATE_ACTIVE)
			return;
		mv_sm_power_setwakeupsource(MV_SM_WAKEUP_SOURCE_TIMER);
		mv_sm_power_enterflow_bysmstate();

	}
	else
	{
		cur_wakeup_time = (wmup_timer_interval - duration)/1000;
		tmp_time = current_time % one_hour_time; //3600000

		if (tmp_time == 0)
		{
			PRT_INFO("current power up time: time begin is %d, interval is %d current is %d diff is %d\n",
					wmup_timer_begin, wmup_timer_interval, current_time, (current_time - wmup_timer_begin));
		}
	}
#endif /*!LTIMER_ENABLE */
}

static void mv_sm_power_down_ontime(void)
{
#ifdef LTIMER_ENABLE
	static INT32 one_hour_time = 3600000;
	UINT32 uiKey, duration, tmp_time;
	INT32 ret = S_OK;
	INT32 state = mv_sm_get_state();
	timer_long_t current_time;

	if(wmdn_timer_enable == 0)
		return;

	ret = mv_sm_timer_getlongtimems(&current_time);
	if (ret == S_OK)
	{
		if (current_time.low >= wmdn_timer_begin.low)
		{
			// only count 32bits duration (max about 48 days)
			duration = current_time.low - wmdn_timer_begin.low;
		}
		else
		{
			duration = (MAX_TIMER_MS - wmdn_timer_begin.low) + current_time.low + 1000;
		}

		if (duration > wmdn_timer_interval)
		{
			PRT_INFO("power down time out. time begin is %u:%u, interval is %u current is %u:%u, duration:%u\n",
					wmdn_timer_begin.high, wmdn_timer_begin.low, wmdn_timer_interval, current_time.high, current_time.low, duration);
			wmdn_timer_enable = 0;
			if(state == MV_SM_STATE_ACTIVE)
			{
				uiKey = MV_IR_KEY_POWER;
				mv_sm_send_msg(MV_SM_ID_IR, &uiKey, sizeof(uiKey));
				uiKey = MV_IR_KEY2UPKEY(uiKey);
				mv_sm_send_msg(MV_SM_ID_IR, &uiKey, sizeof(uiKey));
			}
		}
		else
		{
			tmp_time = duration % one_hour_time; //3600000
			if (tmp_time == 0)
			{
				PRT_INFO("current power up time: time begin is %u:%u, interval is %u current is %u:%u diff is %u\n",
						wmdn_timer_begin.high, wmdn_timer_begin.low, wmdn_timer_interval,
						current_time.high, current_time.low, duration);
			}
		}
	}
#else /*LTIMER_ENABLE */
	UINT32 uiKey;
	INT32 state = mv_sm_get_state();
	INT32 current_time;

	if(wmdn_timer_enable == 0)
		return;

	current_time = mv_sm_timer_gettimems();

	if(current_time - wmdn_timer_begin > wmdn_timer_interval)
	{
		PRT_INFO("power down time out. time interval is %d\n", wmdn_timer_interval);
		wmdn_timer_enable = 0;
		if(state == MV_SM_STATE_ACTIVE)
		{
			uiKey = MV_IR_KEY_POWER;
			mv_sm_send_msg(MV_SM_ID_IR, &uiKey, sizeof(uiKey));
			uiKey = MV_IR_KEY2UPKEY(uiKey);
			mv_sm_send_msg(MV_SM_ID_IR, &uiKey, sizeof(uiKey));
		}
	}
#endif /* !LTIMER_ENABLE */
}

/*FIXME*/
hresult mv_sm_power_enterflow_bysmstate()
{
	INT32 state = mv_sm_get_state();
	SM_POWER_FLOW flow = FLOW_IDLE;
	switch(state) {
	case MV_SM_STATE_LOWPOWERSTANDBY:
		flow = FLOW_LOWPOWERSTANDBY_2_ACTIVE;
		break;
	case MV_SM_STATE_NORMALSTANDBY:
		flow = FLOW_NORMALSTANDBY_2_ACTIVE;
		break;
	case MV_SM_STATE_SUSPEND:
		flow = FLOW_SUSPEND_2_ACTIVE;
		break;
	default:
		PRT_INFO("don't support 0x%x\n", state);
		break;
	}
	return mv_sm_power_enterflow(flow);
}


void mv_sm_power_leaveflow(void)
{
#ifndef UART_WORKAROUND
	PRT_INFO("******leave flow %s\n", flow_name[power_myflow]);
#endif
	power_mystatus = STA_INVALID;
	power_myflow = FLOW_IDLE;
}

static hresult mv_sm_power_soc_poweron_2_powerok()
{
	switch(power_mystatus) {
	case STA_POWERON:
		if(waittime(POWEROFF2POWERON)) {
			mv_sm_power_broadcast_power_state();
			mv_sm_power_socpoweron();
			power_mystatus = STA_POWEROK;
		}
		break;
	case STA_POWEROK:
		if(waittime(POWERON2OKTIME)) {
			if(mv_sm_power_soc_power_status() != S_OK) {
				/* reset time and wait again. */
				power_lasttime = mv_sm_timer_gettimems();
			} else {
				mv_sm_power_broadcast_power_state();
				power_mystatus = STA_ASSERT;
			}
		}
		break;
	default:
		PRT_ERROR("should not come here. flow = %d, status = %d\n",
			power_myflow, power_mystatus);
		return S_FALSE;
	}
	return S_OK;
}

static hresult mv_sm_power_soc_assert_2_deassert()
{
	hresult ret = S_FALSE;
	switch(power_mystatus) {
	case STA_ASSERT:
		mv_sm_power_broadcast_power_state();
		peripheral_assert();
		soc_assert();
		power_mystatus = STA_DEASSERT;
		break;
	case STA_DEASSERT:
		/* hold assert 100ms */
		if(waittime(ASSERT2DEASSERTTIME)) {
			peripheral_deassert();
			soc_deassert();
			mv_sm_power_broadcast_power_state();
			power_mystatus = STA_WAITRSP;

			timer_id = mv_sm_timer_request();
		}
		break;
	case STA_WAITRSP:
		/* just loop here to wait rsp msg. do nothing */
		if(is_get_rsp) {
			is_get_rsp = FALSE;
			ret = S_OK;
		}
		break;
	default:
		PRT_ERROR("should not come here. flow = %d, status = %d\n",
			power_myflow, power_mystatus);
		break;
	}
	return ret;
}

/* FIXME: how to trace */

/* there are three different warmdown type
  * 1. lowpower standby; 2. normal standby; 3. suspend
*/
static hresult mv_sm_power_flow_warmdown(SM_POWER_FLOW flow)
{
	if(flow != power_myflow) {
		PRT_ERROR("flow doens't match. %d != %d\n", flow, power_myflow);
		power_myflow = FLOW_IDLE;
		return S_FALSE;
	}

	switch(power_mystatus) {
	case STA_ENTERFLOW:
		mv_sm_power_broadcast_power_state();
		switch(flow) {
		case FLOW_ACTIVE_2_LOWPOWERSTANDBY:
			mv_sm_set_state(MV_SM_STATE_ACTIVE_2_LOWPOWERSTANDBY);
			/* FIXME */
			//mv_sm_set_boot_flag(MV_SOC_STATE_ACTIVE_2_LOWPOWERSTANDBY);
			mv_sm_set_boot_flag(MV_SOC_STATE_ACTIVE_2_NORMALSTANDBY);
			set_linux_startaddr(0xFFFFFFFF);
			power_mystatus = STA_POWEROFF;
			break;
		case FLOW_ACTIVE_2_NORMALSTANDBY:
			mv_sm_set_state(MV_SM_STATE_ACTIVE_2_NORMALSTANDBY);
			mv_sm_set_boot_flag(MV_SOC_STATE_ACTIVE_2_NORMALSTANDBY);
			set_linux_startaddr(0xFFFFFFFF);
			power_mystatus = STA_POWEROFF;
			break;
		case FLOW_ACTIVE_2_SUSPEND:
			mv_sm_set_state(MV_SM_STATE_ACTIVE_2_SUSPEND);
			mv_sm_set_boot_flag(MV_SOC_STATE_ACTIVE_2_SUSPEND);
			/*
			 * Assert & Deassert are skipped in suspend flow,
			 * since they would make SoC to do some re-init, such
			 * as memory controller, it could result in data loss.
			 */
			power_mystatus = STA_WAITRSP;
			break;
		default:
			break;
		}
		break;
	case STA_ASSERT:
	case STA_DEASSERT:
	case STA_WAITRSP:
		if(S_OK == mv_sm_power_soc_assert_2_deassert()) {
			power_mystatus = STA_POWEROFF;
		}
		break;
	case STA_POWEROFF:
		mv_sm_power_socpoweroff();
		power_mystatus = STA_LEAVEFLOW;
		mv_sm_power_broadcast_power_state();
		break;
	case STA_LEAVEFLOW:
		switch(power_myflow) {
		case FLOW_ACTIVE_2_LOWPOWERSTANDBY:
			mv_sm_set_state(MV_SM_STATE_LOWPOWERSTANDBY);
			break;
		case FLOW_ACTIVE_2_NORMALSTANDBY:
			mv_sm_set_state(MV_SM_STATE_NORMALSTANDBY);
			break;
		case FLOW_ACTIVE_2_SUSPEND:
			mv_sm_set_state(MV_SM_STATE_SUSPEND);
			break;
		default:
			PRT_ERROR("should not come here. flow = %d\n",
				power_myflow);
			break;
		}
		mv_sm_power_leaveflow();
		break;
	default:
		PRT_ERROR("should not come here. flow = %d, status = %d\n",
			power_myflow, power_mystatus);
		break;
	}

	return S_OK;
}

/* by default, cold down will turn into lowpower standby */
static hresult mv_sm_power_flow_colddown(SM_POWER_FLOW flow)
{
	if(flow != power_myflow) {
		PRT_ERROR("flow doens't match. %d != %d\n", flow, power_myflow);
		power_myflow = FLOW_IDLE;
		return S_FALSE;
	}

	switch(power_mystatus) {
	case STA_ENTERFLOW:
		mv_sm_power_broadcast_power_state();
		power_mystatus = STA_POWEROFF;
		break;
	case STA_POWEROFF:
		mv_sm_power_socpoweroff();
		power_mystatus = STA_LEAVEFLOW;
		break;
	case STA_LEAVEFLOW:
		mv_sm_set_state(MV_SM_STATE_LOWPOWERSTANDBY);
		mv_sm_power_broadcast_power_state();
		mv_sm_power_leaveflow();
		if (bRequest_Poweron) {
			PRT_DEBUG("Enter active soon\n");
			mv_sm_power_setwakeupsource(MV_SM_WAKEUP_SOURCE_INVALID);
			mv_sm_power_enterflow_bysmstate();
			bRequest_Poweron = FALSE;
		}
		break;
	default:
		PRT_ERROR("should not come here. flow = %d, status = %d\n",
			power_myflow, power_mystatus);
		break;
	}

	return S_OK;
}

/* Accordingly, there are three different warmup type */
static hresult mv_sm_power_flow_warmup(SM_POWER_FLOW flow)
{
	if(flow != power_myflow) {
		PRT_ERROR("flow doens't match. %d != %d\n", flow, power_myflow);
		power_myflow = FLOW_IDLE;
		return S_FALSE;
	}

	switch(power_mystatus) {
	case STA_ENTERFLOW:
		mv_sm_power_broadcast_power_state();
		mv_sm_set_state(MV_SM_STATE_STANDBY_2_ACTIVE);
		power_mystatus = STA_POWERON;
		switch(flow) {
		case FLOW_LOWPOWERSTANDBY_2_ACTIVE:
			mv_sm_set_boot_flag(MV_SOC_STATE_LOWPOWERSTANDBY_2_ACTIVE);
			break;
		case FLOW_NORMALSTANDBY_2_ACTIVE:
			/*
			 * FIXME: After bootloader side for Normal Standby
			 *        feature completed, we should update this line
			 */
			mv_sm_set_boot_flag(MV_SOC_STATE_LOWPOWERSTANDBY_2_ACTIVE);
			break;
		case FLOW_SUSPEND_2_ACTIVE:
			mv_sm_set_boot_flag(MV_SOC_STATE_SUSPEND_2_ACTIVE);
			break;
		default:
			break;
		}
		break;
	case STA_POWERON:
	case STA_POWEROK:
		mv_sm_power_soc_poweron_2_powerok();
		break;
	case STA_ASSERT:
		/* assert is not needed while warm up */
		power_mystatus = STA_DEASSERT;
		break;
	case STA_DEASSERT:
	case STA_WAITRSP:
		if(S_OK == mv_sm_power_soc_assert_2_deassert()) {
			power_mystatus = STA_LEAVEFLOW;
			mv_sm_power_broadcast_power_state();
		}
		break;
	case STA_LEAVEFLOW:
		mv_sm_set_state(MV_SM_STATE_ACTIVE);
		mv_sm_set_boot_flag(MV_SOC_STATE_POWERON);
		mv_sm_power_leaveflow();
		break;
	default:
		PRT_ERROR("should not come here. flow = %d, status = %d\n",
			power_myflow, power_mystatus);
		break;
	}

	return S_OK;
}

static hresult mv_sm_power_flow_sysreset(SM_POWER_FLOW flow)
{
	if(flow != power_myflow) {
		PRT_ERROR("flow doens't match. %d != %d\n", flow, power_myflow);
		power_myflow = FLOW_IDLE;
		return S_FALSE;
	}

	mv_sm_power_broadcast_power_state();

	switch(power_mystatus) {
	case STA_ENTERFLOW:
		power_mystatus = STA_POWEROFF;
		switch(flow) {
		case FLOW_SYSRESET_2_ACTIVE:
			mv_sm_set_boot_flag(MV_SOC_STATE_SYSRESET_2_ACTIVE);
			break;
		case FLOW_SYSRESET_2_LOWPOWERSTANDBY:
			mv_sm_set_boot_flag(MV_SOC_STATE_SYSRESET_2_LOWPOWERSTANDBY);
			break;
		case FLOW_SYSRESET_2_NORMALSTANDBY:
			mv_sm_set_boot_flag(MV_SOC_STATE_SYSRESET_2_NORMALSTANDBY);
			break;
		default:
			break;
		}
		break;
	case STA_POWEROFF:
		mv_sm_power_broadcast_power_state();
		mv_sm_power_socpoweroff();
		power_lasttime = mv_sm_timer_gettimems();
		power_mystatus = STA_POWERON;
		break;
	case STA_POWERON:
	case STA_POWEROK:
		mv_sm_power_soc_poweron_2_powerok();
		break;
	case STA_ASSERT:
	case STA_DEASSERT:
	case STA_WAITRSP:
		if(S_OK == mv_sm_power_soc_assert_2_deassert()) {
			power_mystatus = STA_LEAVEFLOW;
		}
		break;
	case STA_LEAVEFLOW:
		mv_sm_power_broadcast_power_state();
		mv_sm_power_leaveflow();
		break;
	default:
		PRT_ERROR("should not come here. flow = %d, status = %d\n",
			power_myflow, power_mystatus);
		break;
	}

	return S_OK;
}

static hresult mv_sm_power_flow_idle()
{
	INT32 state = mv_sm_get_state();

	if( (state == MV_SM_STATE_LOWPOWERSTANDBY) ||
		(state == MV_SM_STATE_NORMALSTANDBY) ||
		(state == MV_SM_STATE_SUSPEND)) {
		if(is_wakeup) {
			mv_sm_power_setwakeupsource(MV_SM_WAKEUP_SOURCE_WIFI_BT);
			mv_sm_power_enterflow_bysmstate();
			is_wakeup = FALSE;
		}
	}

	if(wmup_timer_enable)
		mv_sm_power_up_ontime();

	if(wmdn_timer_enable)
		mv_sm_power_down_ontime();

	return S_OK;
}

hresult mv_sm_power_task(void * data)
{
	switch(power_myflow) {
	case FLOW_COLD_2_LOWPOWERSTANDBY:
		mv_sm_power_flow_colddown(power_myflow);
		break;
	case FLOW_ACTIVE_2_LOWPOWERSTANDBY:
	case FLOW_ACTIVE_2_NORMALSTANDBY:
	case FLOW_ACTIVE_2_SUSPEND:
		mv_sm_power_flow_warmdown(power_myflow);
		break;
	case FLOW_LOWPOWERSTANDBY_2_ACTIVE:
	case FLOW_NORMALSTANDBY_2_ACTIVE:
	case FLOW_SUSPEND_2_ACTIVE:
#ifdef UART_WORKAROUND
		if (chip_ver == BERLIN_BG2_Q && chip_subver == 0xA1D)
			apb_uart_enable();
#endif
		mv_sm_power_flow_warmup(power_myflow);
		break;
	case FLOW_SYSRESET_2_ACTIVE:
		mv_sm_power_flow_sysreset(power_myflow);
		break;
	case FLOW_IDLE:
		mv_sm_power_flow_idle();
		break;
	default:
		PRT_ERROR("Unknow power flow: flow = %d, status = %d\n", power_myflow, power_mystatus);
		break;
	}

	return S_OK;
}


hresult mv_sm_power_setwakeupsource(INT32 ws)
{
	wakeup_source = (MV_SM_WAKEUP_SOURCE_TYPE)ws;

	switch (wakeup_source)
	{
	case MV_SM_WAKEUP_SOURCE_INVALID:
		PRT_INFO("%s: INVALID\n", __func__);
		break;
	case MV_SM_WAKEUP_SOURCE_IR:
		PRT_INFO("%s: IR\n", __func__);
		break;
	case MV_SM_WAKEUP_SOURCE_WIFI_BT:
		PRT_INFO("%s: WIFI/BT\n", __func__);
		break;
	case MV_SM_WAKEUP_SOURCE_WOL:
		PRT_INFO("%s: WOL\n", __func__);
		break;
	case MV_SM_WAKEUP_SOURCE_VGA:
		PRT_INFO("%s: VGA\n", __func__);
		break;
	case MV_SM_WAKEUP_SOURCE_CEC:
		PRT_INFO("%s: CEC\n", __func__);
		break;
	case MV_SM_WAKEUP_SOURCE_TIMER:
		PRT_INFO("%s: TIMER\n", __func__);
		break;
	default:
		PRT_INFO("Unknow wake up source (%d)!\n", wakeup_source);
		break;
	}

	return S_OK;
}

hresult mv_sm_process_power_msg(void * data, INT32 len)
{
	INT32 isysstate, iMsgType,iMsgContent;
	isysstate = mv_sm_get_state();
	iMsgType = *(INT32*)data;
	iMsgContent=*(((INT32 *)data) + 1);
#ifdef SAFETY_SHUTDOWN
	INT32 main_volt_raw, core_volt_raw, main_volt, core_volt;
	INT32 probe_msg[2] = {0};
#endif

	PRT_DEBUG("sysstate = 0x%x, type = 0x%x, content = 0x%x\n", isysstate, iMsgType, iMsgContent);

	switch(isysstate)
	{
	case MV_SM_STATE_COLD:
		if (iMsgType == MV_SoC_STATE_COLDBOOT)
			mv_sm_power_enterflow(FLOW_COLD_2_LOWPOWERSTANDBY);
		else if (iMsgType == MV_SoC_STATE_SYSPOWERON)
			bRequest_Poweron = TRUE;
		break;
	case MV_SM_STATE_ACTIVE:
		// Only process warm down request
		switch(iMsgType)
		{
		case MV_SM_IR_Linuxready:
			{
				is_linux_ready = TRUE;
				is_wakeup = FALSE;
				break;
			}
		case MV_SM_POWER_LOWPOWERSTANDBY_REQUEST:
			{
				standby_type = MV_SM_POWER_LOWPOWERSTANDBY_REQUEST;
				break;
			}
		case MV_SM_POWER_NORMALSTANDBY_REQUEST:
			{
				standby_type = MV_SM_POWER_NORMALSTANDBY_REQUEST;
				break;
			}
		case MV_SM_POWER_STANDBY_REQUEST:
			{
				if(standby_type == MV_SM_POWER_LOWPOWERSTANDBY_REQUEST)
					mv_sm_power_enterflow(FLOW_ACTIVE_2_LOWPOWERSTANDBY);
				else
					mv_sm_power_enterflow(FLOW_ACTIVE_2_NORMALSTANDBY);
				break;
			}
		case MV_SM_POWER_SUSPEND_REQUEST:
			{
				linux_resume_addr = iMsgContent ;
				PRT_INFO("receive MV_SM_POWER_WARMDOWN_REQUEST_2 from Linux: %x\n", iMsgContent);
				mv_sm_power_enterflow(FLOW_ACTIVE_2_SUSPEND);
				break;
			}
		case MV_SM_POWER_WAKEUP_SOURCE_REQUEST:
			{
				mv_sm_send_msg(MV_SM_ID_POWER , (void *)&wakeup_source, sizeof(wakeup_source));
				break;
			}
		case MV_SM_POWER_WARMDOWN_TIME://warmdown on time
			{
				//FIXME: start a timer
				//DownTimerTot = iMsgContent;
				//DownTimerElaps = 0;

		#ifdef LTIMER_ENABLE
				if(0 == iMsgContent && wmdn_timer_enable)
				{
					wmdn_timer_enable = 0; // if 0,cancel the previous timer
				}
				else
				{
					wmdn_timer_enable = 1;
					mv_sm_timer_getlongtimems(&wmdn_timer_begin);
					wmdn_timer_interval = iMsgContent*1000;
				}
				PRT_INFO("warm down after %u seconds\n",iMsgContent);
				break;
		#else /* LTIMER_ENABLE */
				if(0 == iMsgContent && wmdn_timer_enable)
				{
					wmdn_timer_enable = 0; // if 0,cancel the previous timer
				}
				else
				{
					wmdn_timer_enable = 1;
					wmdn_timer_begin = mv_sm_timer_gettimems();
					wmdn_timer_interval = iMsgContent*1000;
				}
				PRT_INFO("warm down after %d seconds\n",iMsgContent);
				break;
		#endif /* !LTIMER_ENABLE */
			}
		case MV_SM_POWER_WARMUP_TIME_REQUEST://get warmup remaining time
			{
				PRT_INFO("remain wakeup time is :%d seconds\n",cur_wakeup_time);
				mv_sm_send_msg(MV_SM_ID_POWER, &cur_wakeup_time, sizeof(cur_wakeup_time));
				break;
			}
		case MV_SM_POWER_WARMUP_TIME://warmup on time
			{
		#ifdef LTIMER_ENABLE
				if(0 == iMsgContent && wmup_timer_enable)
				{
					wmup_timer_enable = 0;
					cur_wakeup_time = 0;
				}
				else
				{
					wmup_timer_enable = 1;
					cur_wakeup_time = iMsgContent;
					mv_sm_timer_getlongtimems(&wmup_timer_begin);
					wmup_timer_interval = iMsgContent*1000;
				}

		// timer overflow test
				PRT_INFO("warm up after %u seconds, timer begain:%u:%u\n",
				iMsgContent, wmup_timer_begin.high, wmup_timer_begin.low);
				break;
		#else /* LTIMER_ENABLE */
				if(0 == iMsgContent && wmup_timer_enable)
				{
					wmup_timer_enable = 0;
					cur_wakeup_time = 0;
				}
				else
				{
					wmup_timer_enable = 1;
					cur_wakeup_time = iMsgContent;
					wmup_timer_begin = mv_sm_timer_gettimems();
					wmup_timer_interval = iMsgContent*1000;
				}
				PRT_INFO("warm up after %d seconds\n",iMsgContent);
				break;
		#endif /* !LTIMER_ENABLE */
			}
		case MV_SM_POWER_SYS_RESET://reset whole system
			{
				PRT_INFO("%%%%%%%%SM:Msg reset system%%%%%%%%\n");

				mv_sm_power_enterflow(FLOW_SYSRESET_2_ACTIVE);
				break;
			}
		case MV_SM_POWER_FASTBOOT_ENABLE://fast boot mode enter
			{
				PRT_INFO("%%%%%%%%SM:Enter fast boot mode%%%%%%%%\n");
				bFastboot = TRUE;
				break;
			}
		case MV_SM_POWER_FASTBOOT_DISABLE://fast boot mode exit
			{
				PRT_INFO("%%%%%%%%SM:Exit fast boot mode%%%%%%%%\n");
				bFastboot = FALSE;
				break;
			}
#ifdef SAFETY_SHUTDOWN
		/*
		 * saftey shutdown feature
		 *	main_volt_raw = mv_sm_adc_read(1, 4);
		 *	main_volt = (main_volt_raw*ADC_REF)/ADC_FULL_RANGE * FACTOR;
		 *	probe_msg[0] = main_volt_raw >> 2;
		 */
		case MV_SM_POWER_INSPECT_REQUEST:
			main_volt_raw = core_volt_raw = main_volt = core_volt = 0;

			main_volt_raw = mv_sm_adc_read(ADC_CH_MAIN_VOLT, SAMPLE_TIMES);
			main_volt = (main_volt_raw*ADC_REF)/ADC_FULL_RANGE * MAIN_VOLT_FAC;
			core_volt_raw = mv_sm_adc_read(ADC_CH_CORE_VOLT, SAMPLE_TIMES);
			core_volt = (core_volt_raw*ADC_REF)/ADC_FULL_RANGE;

			/* due to rdkdmp hw, we cannot get real main_volt, return a fake one */
			probe_msg[0] = 0x80;
			probe_msg[1] = mv_sm_temp_get_calibrate_data();
			mv_sm_send_msg(MV_SM_ID_POWER, &probe_msg, sizeof(probe_msg));
			PRT_DEBUG("main: %d mV, core: %d mV, temp: %d\n",
				   main_volt, core_volt, probe_msg[1]);
			break;
#endif


		case 0xee:
			{
				
				int channel =*(((INT32 *)data) + 1);
				int sample =*(((INT32 *)data) + 2);

				int val = mv_sm_adc_read(channel, sample);
				mv_sm_send_msg(MV_SM_ID_POWER, &val, sizeof(val));
				
				PRT_DEBUG("adc val: %d\n", val);

				break;
			}

		default:
			break;
		}
		break;
	case MV_SM_STATE_ACTIVE_2_LOWPOWERSTANDBY:
	case MV_SM_STATE_ACTIVE_2_NORMALSTANDBY:
		if((iMsgType == MV_SM_ACTIVE_2_LOWPOWERSTANDBY_RESP)
			|| (iMsgType == MV_SM_ACTIVE_2_NORMALSTANDBY_RESP)) {
			set_linux_startaddr(0xFFFFFFFF);
			PRT_INFO("receive WARMDOWN resp, set warmup param: 0xFFFFFFFF\n");
			is_get_rsp = TRUE;
		}
		break;
	case MV_SM_STATE_ACTIVE_2_SUSPEND:
		if(iMsgType == MV_SM_ACTIVE_2_SUSPEND_RESP) {
			set_linux_startaddr(linux_resume_addr);
			PRT_INFO("receive WARMDOWN resp, set warmup param: 0x%x\n", linux_resume_addr);
			is_get_rsp = TRUE;
		}
		break;
	case MV_SM_STATE_STANDBY_2_ACTIVE:  // we only need to know SOC has warmed up
		{
			//FIXME: response format
			if((iMsgType == MV_SM_LOWPOWERSTANDBY_2_ACTIVE_RESP)
				|| (iMsgType == MV_SM_NORMALSTANDBY_2_ACTIVE_RESP)
				|| (iMsgType == MV_SM_SUSPEND_2_ACTIVE_RESP)
				|| (iMsgType == MV_SM_COLD_2_ACTIVE_RESP)) {
#ifndef UART_WORKAROUND
				PRT_INFO("SM:Enter warmup state\n");
#endif
				is_get_rsp = TRUE;
			}
		}
		break;
	default:
		break;
	}

	return S_OK;
}

#define WIFIBT_GAP_MIN	64
#define WIFIBT_GAP_MAX	256

INT32 wifibt_gpio_data = 0;
INT32 wifibt_falling_time = 0;
INT32 wifibt_gap = 0;
INT32 wifibt_timer_id = 0;

void mv_sm_wifibt_timer_isr(void)
{
	wifibt_gap = mv_sm_timer_gettimems() - wifibt_falling_time;

	if (mv_sm_gpio_read(SM_GPIO_PORT_WIFIBT) == 1 || wifibt_gap >= WIFIBT_GAP_MAX) {
		mv_sm_timer_stop(wifibt_timer_id);
		mv_sm_gpio_set_int(SM_GPIO_PORT_WIFIBT, EDGE_SENSITIVE, FALLING_EDGE);
		if (wifibt_gap >= WIFIBT_GAP_MIN && wifibt_gap < WIFIBT_GAP_MAX)
			is_wakeup = TRUE;
		PRT_INFO("@@@WIFIBTI$$$ gap: %d\n", wifibt_gap);
	}
}

void mv_sm_wifibt_isr(void)
{
	wifibt_falling_time = mv_sm_timer_gettimems();
	wifibt_timer_id = mv_sm_timer_request();
	if (wifibt_timer_id < 0)
		PRT_ERROR("wifibt timer request failed\n");
	mv_sm_gpio_set_input(SM_GPIO_PORT_WIFIBT);
	mv_sm_timer_start(wifibt_timer_id, 1000, mv_sm_wifibt_timer_isr, 0);
}

#include "bootloader.h"

#include "pinmux_setting.h"

#include "bootloader_customize.h"
#include "gpio_defs.h"
#include "debug.h"
#include "global.h"
#include "apb_perf_base.h"
#include "sys_mgr.h"
#include "apb_timer.h"

#define	SOC_SM_CEC_REG_BASE		(MEMMAP_SM_REG_BASE + 0x61000)
#define	SD3_SLOT1_BASE_OFFSET		0x0
#define	SD3_SLOT2_BASE_OFFSET		0x800
//EMMC
#define	SD3_SLOT3_BASE_OFFSET		0x1000
#define	SET32Cec_FEPHY_CTRL_ext_pwrdn_a(r32,v)	_BFSET_(r32, 4, 0,v)

#define	RA_Cec_FEPHY_CTRL			0x0404
#define	MEMMAP_SATA_REG_BASE			0xF7E90000
#define	SATA_PORT0_BASE				(MEMMAP_SATA_REG_BASE+0x100)
#define	SATA_PORT_VSR_ADDR			(0x78)	/* port Vendor Specific Register Address */
#define	SATA_PORT_VSR_DATA			(0x7c)	/* port Vendor Specific Register Data */

extern void pin_init();
extern char board_kernel_param[256];

static void dump_pinmux_cfg(void)
{
		dbg_printf(PRN_RES,"Pinmux configuration:\n");

#ifdef CONFIG_SM
		dbg_printf(PRN_RES,"\nGSM 0x%08x 0x%08x;\n", (SM_SYS_CTRL_REG_BASE + RA_smSysCtl_SM_GSM_SEL),
				readl((SM_SYS_CTRL_REG_BASE + RA_smSysCtl_SM_GSM_SEL)));
#endif /* CONFIG_SM */
		dbg_printf(PRN_RES,"GSOC 0x%08x 0x%08x;\n", (MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_pinMux),
				readl(MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_pinMux));
		dbg_printf(PRN_RES,"GSOC1 0x%08x 0x%08x;\n", (MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_pinMux1),
				readl(MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_pinMux1));
        dbg_printf(PRN_RES,"GSOC2 0x%08x 0x%08x;\n", (MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_pinMux2),
				readl(MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_pinMux2));
        dbg_printf(PRN_RES,"GSOC3 0x%08x 0x%08x;\n", (MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_pinMux3),
				readl(MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_pinMux3));
        dbg_printf(PRN_RES,"GSOC4 0x%08x 0x%08x;\n", (MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_pinMux4),
				readl(MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_pinMux4));
        dbg_printf(PRN_RES,"GSOC5 0x%08x 0x%08x;\n", (MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_pinMux5),
				readl(MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_pinMux5));
		dbg_printf(PRN_RES,"\n");
}

unsigned int get_board_version(void)
{
	unsigned int boot_strap = readl(CHIP_CTRL_BOOT_STRAP_REG);
	unsigned chip_version;
	switch(boot_strap & 0x7)
	{
		case 0x4:
			chip_version = 100;
			break;
		case 0x5:
			chip_version = 110;
			break;
		case 0x6:
			chip_version = 120;
			break;
		case 0x7:
			chip_version = 121;
			break;
		case 0x0:
			chip_version = 122;
			break;
		case 0x1:
			chip_version = 123;
			break;
		case 0x2:
		case 0x3:
			chip_version = 124;
			break;
		default:
			chip_version = 0;
	}
	return chip_version;
}

// 1: on; 0: off
static inline void set_power_sdio_slot0(int value)
{
    SM_GPIO_PortWrite(SM_GPIO_PORT_SDIO_SLOT0_POWER, value);
}

// 1: on; 0: off
static inline void set_power_sdio_slot1(int value)
{
    GPIO_PortWrite(GPIO_PORT_SDIO_SLOT1_POWER, value);
}

// 1: up; 0: down
static inline void set_power_usb0(int value)
{
	dbg_printf(PRN_DBG,"power %s usb0.\n", (value ? "up" : "down"));
	SM_GPIO_PortSetInOut(SM_GPIO_PORT_USB0_POWER, 0);
	SM_GPIO_PortWrite(SM_GPIO_PORT_USB0_POWER, value);
}

// 1: up; 0: down
static inline void set_power_usb1(int value)
{
	dbg_printf(PRN_DBG,"power %s usb1.\n", (value ? "up" : "down"));
	GPIO_PortSetInOut(GPIO_PORT_USB1_POWER, 0);
	GPIO_PortWrite(GPIO_PORT_USB1_POWER, value);
}

/*
 * GPIO_PORT_HDMI_RX_HPD is connected to HdmiRx HPD, keep it low until application ready
 */
static inline void hpd_gpio_set_low(void)
{
	SM_GPIO_PortWrite(GPIO_PORT_HDMI_RX_HPD, 0);
}

void powerdown_smartcard(int id)
{
	unsigned int read;
	unsigned int offset;

	if (id == 0)
		offset = RA_Gbl_usim0Clk;
	else
		offset = RA_Gbl_usim1Clk;

	// clock disable
	dbg_printf(PRN_DBG,"power down smart card %d.\n", id);
	REG_READ32((MEMMAP_CHIP_CTRL_REG_BASE + offset), &read);
	read &= ~1;
	REG_WRITE32((MEMMAP_CHIP_CTRL_REG_BASE + offset), read);
}

void powerdown_eth(int id)
{
	unsigned int temp;

	if (id == 0)
	{
		// clock disable
		// --RGMII clock disable
		//REG_READ32(MEMMAP_CHIP_CTRL_REG_BASE+RA_Gbl_gethRgmiiClk, &temp);
		//SET32clkD8_ctrl_ClkEn(temp, 0);
		//REG_WRITE32(MEMMAP_CHIP_CTRL_REG_BASE+RA_Gbl_gethRgmiiClk, temp);

		// GE MAC assert reset
		REG_READ32(MEMMAP_CHIP_CTRL_REG_BASE+RA_Gbl_stickyResetN, &temp);
		SET32Gbl_stickyResetN_gethRgmiiMemRstn(temp, 0);
		SET32Gbl_stickyResetN_gethRgmiiRstn(temp, 0);
		REG_WRITE32(MEMMAP_CHIP_CTRL_REG_BASE+RA_Gbl_stickyResetN, temp);
	} else if (id == 1) {
		// power down PHY
		// --FEPHY power down mode
		REG_READ32(SOC_SM_CEC_REG_BASE+RA_Cec_FEPHY_CTRL, &temp);
		SET32Cec_FEPHY_CTRL_ext_pwrdn_a(temp, 1);
		REG_WRITE32(SOC_SM_CEC_REG_BASE+RA_Cec_FEPHY_CTRL, temp);

		// --FEPHY assert reset
		REG_READ32(SM_SYS_CTRL_REG_BASE+RA_smSysCtl_SM_RST_CTRL, &temp);
		//SET32smSysCtl_SM_RST_CTRL_WOL_RST_GO(temp, 0);
		SET32smSysCtl_SM_RST_CTRL_FEPHY_RST_GO(temp, 0);
		REG_WRITE32(SM_SYS_CTRL_REG_BASE+RA_smSysCtl_SM_RST_CTRL, temp);
	}
	dbg_printf(PRN_DBG,"power down eth%d.\n", id);
}

void powerdown_sata()
{
	T32Gbl_clkEnable reg_clk;
	T32PERIF_FUNC_SEL func_sel;
	unsigned int temp;

	//select SATA function
	REG_READ32((MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_PERIF + RA_PERIF_FUNC_SEL), &(func_sel.u32));
	func_sel.uFUNC_SEL_pcie_sata_sel = 0;
	REG_WRITE32((MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_PERIF + RA_PERIF_FUNC_SEL), (func_sel.u32));

	// setup PHYMODE2 register addr
	REG_WRITE32(SATA_PORT0_BASE + SATA_PORT_VSR_ADDR, 5);

	// turn off PHY TX and RX
	REG_READ32(SATA_PORT0_BASE + SATA_PORT_VSR_DATA, &temp);
	temp &= ~0xF;
	REG_WRITE32(SATA_PORT0_BASE + SATA_PORT_VSR_DATA, temp);

	// clock disable
	REG_READ32((MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_clkEnable), &reg_clk.u32);
	reg_clk.uclkEnable_sataCoreClkEn = 0;
	REG_WRITE32((MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_clkEnable), reg_clk.u32);
	dbg_printf(PRN_DBG,"power down sata.\n");
}

void powerdown_pcie()
{
	T32Gbl_clkEnable reg_clk;
	unsigned int read;
	T32PERIF_FUNC_SEL func_sel;

	//reset PCIE RC
	REG_READ32((MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_stickyResetN), &read); //0xF7EA010C
	read &= ~(0x1<<LSb32Gbl_stickyResetN_pcieRstn);  // bit2
	REG_WRITE32((MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_stickyResetN), read);
	delay_us(1);

	//select pcie function
	REG_READ32((MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_PERIF + RA_PERIF_FUNC_SEL), &(func_sel.u32));//F7EA049C
	func_sel.uFUNC_SEL_pcie_sata_sel = 1; //select pcie   //bit0
	REG_WRITE32((MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_PERIF + RA_PERIF_FUNC_SEL), (func_sel.u32));

	// clock disable
	REG_READ32((MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_clkEnable), &reg_clk.u32); //0xF7EA00E8
	reg_clk.uclkEnable_pcieClkEn = 0;         // bit22
	REG_WRITE32((MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_clkEnable), reg_clk.u32);
	delay_us(1);

	REG_READ32((MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_pcieTestClk), &read);  //0xF7EA01A8
	read &= ~1;
	REG_WRITE32((MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_pcieTestClk), read);

	// power down PHY
	REG_WRITE32(0xF7E900A0, 0);
	REG_WRITE32(0xF7E900A4, 0xC40040);

	//reset PCIE RC
	REG_READ32((MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_stickyResetN), &read); //0xF7EA010C
	read |= (0x1<<LSb32Gbl_stickyResetN_pcieRstn);  // bit2
	REG_WRITE32((MEMMAP_CHIP_CTRL_REG_BASE + RA_Gbl_stickyResetN), read);
	delay_us(1);
	dbg_printf(PRN_DBG,"power down pcie.\n");
}

void powerdown_sd(int id)
{
	T32Gbl_clkEnable reg_clk;
	T32Gbl_sdioXinClkCtrl xin_clk;
	T32Gbl_sdio1XinClkCtrl xin1_clk;
	UNSG32 mm4_cntl2;

	if(id == 2)
	{
		// clock disable
		REG_READ32(MEMMAP_CHIP_CTRL_REG_BASE+RA_Gbl_clkEnable, &reg_clk.u32);
		reg_clk.uclkEnable_nfcEccClkEn = 0;
		REG_WRITE32(MEMMAP_CHIP_CTRL_REG_BASE+RA_Gbl_clkEnable, reg_clk.u32);
	} else {
		// clock disable
		//REG_READ32(MEMMAP_CHIP_CTRL_REG_BASE+RA_Gbl_clkEnable, &reg_clk.u32);
		//reg_clk.uclkEnable_sdioCoreClkEn = 0;
		//REG_WRITE32(MEMMAP_CHIP_CTRL_REG_BASE+RA_Gbl_clkEnable, reg_clk.u32);
	}

	// power down PHY
	if (id == 2)
	{
		REG_READ32(MEMMAP_SDIO3_REG_BASE+SD3_SLOT3_BASE_OFFSET + 0x2C, &mm4_cntl2);
		mm4_cntl2 = mm4_cntl2&0xFFFFFFFA;
		REG_WRITE32(MEMMAP_SDIO3_REG_BASE+SD3_SLOT3_BASE_OFFSET + 0x2C, mm4_cntl2);
	}
	else if (id) {
		REG_READ32(MEMMAP_CHIP_CTRL_REG_BASE+RA_Gbl_sdio1XinClkCtrl, &xin1_clk.u32);
		xin1_clk.usdio1XinClkCtrl_ClkEN = 0;
		REG_WRITE32(MEMMAP_CHIP_CTRL_REG_BASE+RA_Gbl_sdio1XinClkCtrl, xin1_clk.u32);
		REG_READ32(MEMMAP_SDIO3_REG_BASE+SD3_SLOT2_BASE_OFFSET + 0x2C, &mm4_cntl2);
		mm4_cntl2 = mm4_cntl2&0xFFFFFFFA;
		REG_WRITE32(MEMMAP_SDIO3_REG_BASE+SD3_SLOT2_BASE_OFFSET + 0x2C, mm4_cntl2);
	} else {
		REG_READ32(MEMMAP_CHIP_CTRL_REG_BASE+RA_Gbl_sdioXinClkCtrl, &xin_clk.u32);
		xin_clk.usdioXinClkCtrl_ClkEN = 0;
		REG_WRITE32(MEMMAP_CHIP_CTRL_REG_BASE+RA_Gbl_sdioXinClkCtrl, xin_clk.u32);
		REG_READ32(MEMMAP_SDIO3_REG_BASE+SD3_SLOT1_BASE_OFFSET + 0x2C, &mm4_cntl2);
		mm4_cntl2 = mm4_cntl2&0xFFFFFFFA;
		REG_WRITE32(MEMMAP_SDIO3_REG_BASE+SD3_SLOT1_BASE_OFFSET + 0x2C, mm4_cntl2);
	}
	dbg_printf(PRN_DBG,"power down sd%d.\n", id);
}


void powerdown_unused_module()
{
	powerdown_smartcard(0);
	powerdown_smartcard(1);
	//powerdown_sd(0);
	//powerdown_sd(1);
	//powerdown_pcie();
	//powerdown_sata();
	//disable G-eth
	powerdown_eth(0);
}



unsigned int get_stages_bg2q_platform_initialize(void)
{
    return (BOOTLOADER_INITCHIP_STAGE |
            BOOTLOADER_INITCHIP_POSTSTAGE |
            BOOTLOADER_LOADKERNEL_PRESTAGE |
            BOOTLOADER_LOADKERNEL_POSTSTAGE);
}

void platform_setup_kernel_param()
{
    //setup board_kernel_param
}

unsigned int bg2q_platform_initialize(unsigned int boot_stage)
{
    switch(boot_stage) {
        case BOOTLOADER_INITCHIP_STAGE:
            {
                pin_init();
               {
                    extern void mdelay(unsigned int ms);
                    GPIO_PortSetInOut(78, 0);     // soc_gpio[78] for splash_on
                    GPIO_PortWrite(78, 0);
                    GPIO_PortSetInOut(77, 0);     // soc_gpio[77] for BE-enable
                    GPIO_PortWrite(77, 1);
                    GPIO_PortSetInOut(81, 0);     // soc_gpio[81] for HDMI-MUX-RESETn
                    GPIO_PortWrite(81, 1);        // toggle gpio81 to do HDMI input reset
                    mdelay(100);
                    GPIO_PortWrite(81, 0);
                    mdelay(100);
                    GPIO_PortWrite(81, 1);
                    GPIO_PortSetInOut(6, 0);      //soc_gpio[6] for AUDIO-RESETn
                    GPIO_PortWrite(6, 0);
                    GPIO_PortSetInOut(7, 0);      //soc_gpio[7] for SYSTEM-RESET
                    GPIO_PortWrite(7, 1);

                    GPIO_PortSetInOut(14, 0);    // soc_gpio[14] for HDMI-ARC-SEL0
                    GPIO_PortWrite(14, 0);
                    GPIO_PortSetInOut(15, 0);    // soc_gpio[15] for HDMI-ARC-SEL1
                    GPIO_PortWrite(15, 0);
                    GPIO_PortSetInOut(16, 0);    // soc_gpio[16] for HDMI-ARC-SEL2
                    GPIO_PortWrite(16, 0);
					
		    SM_GPIO_PortSetInOut (18, 1);  // reset WiFi pin 54  
		    mdelay(100);
                    SM_GPIO_PortWrite(18, 0);
		    mdelay(100);
                    SM_GPIO_PortWrite(18, 1);


                    GPIO_PortSetInOut (41, 1 );  //add for CIMAX
                    SM_GPIO_PortSetInOut (43, 0);
                    SM_GPIO_PortWrite(43, 1);    //pull AUDIO RESET
                    SM_GPIO_PortSetInOut(44, 0);
                    SM_GPIO_PortWrite(44, 0);
                    dbg_printf(PRN_RES,"\nI2C switch set as 0\n");
                    mdelay(100);
                    SM_GPIO_PortWrite(44, 1);
                    dbg_printf(PRN_RES,"\nI2C switch set as 1\n");

                    dbg_printf(PRN_RES,"\n%s %s [%s %s]\n", BOARD_TYPE, CHIP_VERSION, __DATE__, __TIME__);
#if defined(DEBUG)
                    if ((uiBoot != MV_SoC_STATE_WARMUP_1) || (uiWarmDown_2_Linux_Addr == 0xFFFFFFFF)) {
                        dump_pinmux_cfg();
                    }
	       }
#endif
            }
            break;
        case BOOTLOADER_INITCHIP_POSTSTAGE:
            {
                set_power_sdio_slot1(1);
                hpd_gpio_set_low();

                if ((uiBoot != MV_SoC_STATE_WARMDOWN_1) &&
                    (uiBoot != MV_SoC_STATE_WARMDOWN_2) &&
                    !((uiBoot == MV_SoC_STATE_WARMUP_1) && (uiWarmDown_2_Linux_Addr != 0xFFFFFFFF)))
                    set_power_sdio_slot0(0);

                //power down usb then up
                set_power_usb0(0);
                set_power_usb1(0);
		powerdown_unused_module();
            }
            break;
        case BOOTLOADER_LOADKERNEL_PRESTAGE:
            {
                //FIXME: here to setup board specific kernel params
                platform_setup_kernel_param();
            }
            break;
        case BOOTLOADER_LOADKERNEL_POSTSTAGE:
            {
                if ((uiBoot != MV_SoC_STATE_WARMDOWN_1) &&
                    (uiBoot != MV_SoC_STATE_WARMDOWN_2) &&
                    !((uiBoot == MV_SoC_STATE_WARMUP_1) && (uiWarmDown_2_Linux_Addr != 0xFFFFFFFF)))
                    set_power_sdio_slot0(1);

                set_power_usb0(1);
                set_power_usb1(1);

                // why in BOOTLOADER_LOADKERNEL_POSTSTAGE?  in this stage, we can access Bootmode
                extern int Bootmode;
                dbg_printf(PRN_ERR,"\n=====bootmode:%d\n", Bootmode);
                if (BOOTMODE_NORMAL == Bootmode) {
                    SM_GPIO_PortSetInOut (38, 1);  // reset panel backlight, min reset time: 350ms
                    SM_GPIO_PortWrite(38, 0);      // turn it on when android boot animation starts, to fix pink flash issue
                }
            }
            break;
        default:
            break;
    }

    return BOOTLOADER_NULL_STAGE;
}


static bootloader_task_t platform_tasks[] = {
    {"bg2q_platform_initialize", 0, bg2q_platform_initialize, get_stages_bg2q_platform_initialize},
    //add more platform tasks here
};

/*****************************************************************************/
/* Don't change below functions */
inline bootloader_task_t * get_platform_tasks(int *num) {
	*num = sizeof(platform_tasks) / sizeof(bootloader_task_t);
	return platform_tasks;
}


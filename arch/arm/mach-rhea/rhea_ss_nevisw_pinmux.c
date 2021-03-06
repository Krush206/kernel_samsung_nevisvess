/************************************************************************************************/
/*                                                                                              */
/*  Copyright 2011  Broadcom Corporation                                                        */
/*                                                                                              */
/*     Unless you and Broadcom execute a separate written software license agreement governing  */
/*     use of this software, this software is licensed to you under the terms of the GNU        */
/*     General Public License version 2 (the GPL), available at                                 */
/*                                                                                              */
/*          http://www.broadcom.com/licenses/GPLv2.php                                          */
/*                                                                                              */
/*     with the following added to such license:                                                */
/*                                                                                              */
/*     As a special exception, the copyright holders of this software give you permission to    */
/*     link this software with independent modules, and to copy and distribute the resulting    */
/*     executable under terms of your choice, provided that you also meet, for each linked      */
/*     independent module, the terms and conditions of the license of that module.              */
/*     An independent module is a module which is not derived from this software.  The special  */
/*     exception does not apply to any modifications of the software.                           */
/*                                                                                              */
/*     Notwithstanding the above, under no circumstances may you combine this software in any   */
/*     way with any other Broadcom software provided under a license other than the GPL,        */
/*     without Broadcom's express prior written consent.                                        */
/*                                                                                              */
/************************************************************************************************/
#include <linux/kernel.h>
#include <linux/init.h>
#include "mach/pinmux.h"
#include <mach/rdb/brcm_rdb_padctrlreg.h>

static struct __init pin_config board_pin_config[] = {

//@HW
	PIN_CFG(GPIO18, UB2TX, 0, OFF, ON, 0, 0, 8MA),	// Console TX
	PIN_CFG(SSPDI,	RXD, 0, OFF, ON, 0, 0, 8MA),	// Console RX
	//PIN_CFG(ICUSBDP, GPIO121, 0, ON, OFF, 0, 0, 8MA), // UART_SEL

	/* PMU BSC */
	PIN_BSC_CFG(PMBSCCLK, PMBSCCLK, 0x20),	// PMU_SCL
	PIN_BSC_CFG(PMBSCDAT, PMBSCDAT, 0x20),	// PMU_SDA
	PIN_CFG(PMUINT, GPIO29, 0, OFF, ON, 0, 0, 8MA),//PMU_INT

	/*
	 * Note:- For eMMC, Enable Slew-rate, Increase pin drive strength to 10mA.
	 * 	This is to fix the random eMMC timeout errors due to data crc error
	 * 	seen on few rhea edn11 hardware, where eMMC is on a daughter-card.
	 *
	 * 	We may need to revisit these settings for other platforms where the
	 * 	pin drive requirements can change.
	 *
	 */
	/* eMMC */
	PIN_CFG(MMC0CK, MMC0CK, 0, ON, OFF, 1, 0, 10MA),
	PIN_CFG(MMC0CMD, MMC0CMD, 0, OFF, ON, 1, 0, 10MA),
	PIN_CFG(MMC0RST, MMC0RST, 0, ON, OFF, 1, 0, 10MA),
	PIN_CFG(MMC0DAT7, MMC0DAT7, 0, OFF, ON, 1, 0, 10MA),
	PIN_CFG(MMC0DAT6, MMC0DAT6, 0, OFF, ON, 1, 0, 10MA),
	PIN_CFG(MMC0DAT5, MMC0DAT5, 0, OFF, ON, 1, 0, 10MA),
	PIN_CFG(MMC0DAT4, MMC0DAT4, 0, OFF, ON, 1, 0, 10MA),
	PIN_CFG(MMC0DAT3, MMC0DAT3, 0, OFF, ON, 1, 0, 10MA),
	PIN_CFG(MMC0DAT2, MMC0DAT2, 0, OFF, ON, 1, 0, 10MA),
	PIN_CFG(MMC0DAT1, MMC0DAT1, 0, OFF, ON, 1, 0, 10MA),
	PIN_CFG(MMC0DAT0, MMC0DAT0, 0, OFF, ON, 1, 0, 10MA),

    /* Micro SD - SDIO0 4 bit interface */
	PIN_CFG(SDCK, SDCK, 0, ON, OFF, 0, 0, 8MA),
	PIN_CFG(SDCMD, SDCMD, 0, ON, OFF, 0, 0, 8MA),
	PIN_CFG(SDDAT3, SDDAT3, 0, ON, OFF, 0, 0, 8MA),
	PIN_CFG(SDDAT2, SDDAT2, 0, ON, OFF, 0, 0, 8MA),
	PIN_CFG(SDDAT1, SDDAT1, 0, ON, OFF, 0, 0, 8MA),
	PIN_CFG(SDDAT0, SDDAT0, 0, ON, OFF, 0, 0, 8MA),
	PIN_CFG(DMIC0CLK, GPIO123, 0, OFF, ON, 0, 0, 16MA), //SD_DECTECT

	/*	Pinmux for keypad */
	PIN_CFG(GPIO05, KEY_R5, 0, ON, OFF, 0, 1, 8MA),
	PIN_CFG(GPIO06, KEY_R6, 0, ON, OFF, 0, 1, 8MA),
	PIN_CFG(GPIO12, KEY_C4, 0, OFF, ON, 0, 0, 8MA),
	PIN_CFG(GPIO13, KEY_C5, 0, OFF, ON, 0, 0, 8MA),

	/* STM trace - PTI */
//	PIN_CFG(TRACECLK, PTI_CLK, 0, OFF, ON, 0, 0, 8MA),	// N.C
//	PIN_CFG(TRACEDT07, RXD, 0, OFF, ON, 0, 0, 8MA),		// N.C
	PIN_CFG(TRACEDT00, GPIO109, 0, ON, OFF, 0, 0, 8MA),	// N.C
//	PIN_CFG(TRACEDT01, PTI_DAT1, 0, OFF, ON, 0, 0, 8MA),	// N.C
//	PIN_CFG(TRACEDT02, PTI_DAT2, 0, OFF, ON, 0, 0, 8MA),	// N.C
//	PIN_CFG(TRACEDT03, PTI_DAT3, 0, OFF, ON, 0, 0, 8MA),	// N.C


	/* Camera */
	/* BSC1 - CAM */
	//PIN_BSC_CFG(BSC1CLK, BSC1CLK, 0x20),			// CAM_SCL
	//PIN_BSC_CFG(BSC1DAT, BSC1DAT, 0x20),			// CAM_SDA
	PIN_CFG(BSC1CLK, GPIO51, 0, OFF, ON, 0, 0, 8MA), 		// CAM_SCL
	PIN_CFG(BSC1DAT, GPIO52, 0, OFF, ON, 0, 0, 8MA), 		// CAM_SDA

	PIN_CFG(DCLK1, DCLK1, 0, ON, OFF, 0, 0, 8MA), 		// CAM_MCLK
	//PIN_CFG(DSI0TE, GPIO37, 0, ON, OFF, 0, 0, 8MA), 	// CAM_CORE_EN
	PIN_CFG(STAT2, GPIO35, 0, ON, OFF, 0, 0, 8MA), 	// STAT2
	PIN_CFG(DSI0TE, GPIO37, 0, ON, OFF, 0, 0, 8MA), 	// NC
	PIN_CFG(DCLKREQ1, GPIO111, 0, ON, OFF, 0, 0, 8MA),	// 3M_CAM_STNBY
	PIN_CFG(GPIO33, GPIO33, 0, ON, OFF, 0, 0, 8MA), 	// 3M_CAM_RESET


	/* Bluetooth related GPIOS */

	PIN_CFG(GPIO07, GPIO7, 0, ON, OFF, 0, 0, 8MA),		// BT_WAKE
	PIN_CFG(GPS_CALREQ, GPIO99, 0, ON, OFF, 0, 0, 8MA),	// BT_RESETN
	PIN_CFG(GPIO34, GPIO34, 0, OFF, OFF, 0, 0, 8MA),	// BT_HOST_WAKE

	PIN_CFG(GPS_HOSTREQ, GPIO100, 0, ON, OFF, 0, 0, 8MA), // BT_REG_ON
//	PIN_CFG(GPIO10, GPIO10, 0, OFF, ON, 0, 0, 8MA),       // BT_SEC

	PIN_CFG(DCLKREQ4, SSP1DI, 0, ON, OFF, 0, 0, 8MA),	// BT_I2S_DO
	/* SSP4 - I2S */
	PIN_CFG(GPIO94, SSP1SYN, 0, OFF, OFF, 0, 0, 8MA),	// BT_I2S_WS
	PIN_CFG(GPIO93, SSP1CK, 0, OFF, OFF, 0, 0, 8MA),		// BT_I2S_CLK
	PIN_CFG(SPI0RXD,GPIO92, 0, ON, OFF, 0, 0, 8MA),		// BT_RST_N

	PIN_CFG(MMC1DAT7, SSP2CK, 0, OFF, OFF, 0, 0, 16MA),	// BT_PCM_CLK
	PIN_CFG(MMC1DAT6, SSP2DO, 0, OFF, OFF, 0, 0, 16MA),	// BT_PCM_IN
	PIN_CFG(MMC1DAT5, SSP2DI, 0, ON, OFF, 0, 0, 16MA),	// BT_PCM_OUT
	PIN_CFG(MMC1DAT4, SSP2SYN, 0, OFF, OFF, 0, 0, 16MA),	// BT_PCM_SYNC


//	PIN_CFG(GPIO19, UB2RX, 0, OFF, ON, 0, 0, 8MA),		// UB2_BT_UART_RX
//	PIN_CFG(GPIO20, UB2RTSN, 0, OFF, ON, 0, 0, 8MA),	// UB2_BT_UART_RTS_N
//	PIN_CFG(GPIO21, UB2CTSN, 0, OFF, ON, 0, 0, 8MA),	// UB2_BT_UART_CTS_N


	// for GPS
	PIN_CFG(GPIO28, GPIO28, 0, ON, OFF, 0, 0, 8MA),		// GPS_EN
	PIN_CFG(DMIC0DQ, GPIO124, 0, ON, OFF, 0, 0, 8MA),	// GPS_HOST_REQ
	/* GPS - BSC2 */
	PIN_BSC_CFG(GPIO16, BSC2CLK, 0x20),			// GPS_SCL
	PIN_BSC_CFG(GPIO17, BSC2DAT, 0x20),			// GPS_SDA
	PIN_CFG(GPS_PABLANK, GPIO97, 0, ON, OFF, 0, 0, 8MA),	// GPS_CAL_REQ
	//PIN_CFG(GPS_TMARK, GPIO98, 0, ON, OFF, 0, 0, 8MA),	// GPS_SYNC


	/* WLAN */
	PIN_CFG(MMC1DAT0, MMC1DAT0, 0, OFF, ON, 0, 0, 8MA),	// WLAN_SDIO_DAT0
	PIN_CFG(MMC1DAT1, MMC1DAT1, 0, OFF, ON, 0, 0, 8MA),	// WLAN_SDIO_DAT1
	PIN_CFG(MMC1DAT2, MMC1DAT2, 0, OFF, ON, 0, 0, 8MA),	// WLAN_SDIO_DAT2
	PIN_CFG(MMC1DAT3, MMC1DAT3, 0, OFF, ON, 0, 0, 8MA),	// WLAN_SDIO_DAT3
	PIN_CFG(MMC1CK, MMC1CK, 0, OFF, OFF, 0, 0, 8MA),		// WLAN_SDIO_CLK
	PIN_CFG(MMC1CMD, MMC1CMD, 0, OFF, ON, 0, 0, 8MA),	// WLAN_SDIO_CMD
	PIN_CFG(MMC1RST, GPIO70, 0, ON, OFF, 0, 0, 8MA),	// WLAN_REG_ON
	PIN_CFG(GPIO14, GPIO14, 0, ON, OFF, 0, 0, 8MA),		// WLAN_HOST_WAKE
	//PIN_CFG(CAMCS1, ANA_SYS_REQ2, 0, ON, OFF, 0, 0, 8MA),	// WLAN_CLK_REQ
	PIN_CFG(GPIO04, ANA_SYS_REQ3, 0, ON, OFF, 0, 0, 8MA),	// WLAN_CLK_REQ

	/* NFC */
/*
	PIN_CFG(SIMDET, GPIO56, 0, OFF, ON, 0, 0, 8MA),		// NFC_SDA
	PIN_CFG(GPS_CALREQ, GPIO99, 0, OFF, ON, 0, 0, 8MA),	// NFC_WAKE
*/
	/* Headset */
	PIN_CFG(STAT1, GPIO31, 0, OFF, ON, 0, 0, 4MA),	        // EAR_3.5_DETECT

	/* Sensor(Acceleromter,Magnetic,Proximity) */
	PIN_CFG(GPIO15, GPIO15, 0, OFF, ON, 0, 0, 8MA),	/* SENSOR_SDA*/
	PIN_CFG(GPIO32, GPIO32, 0, OFF, OFF, 0, 0, 8MA),	/* SENSOR_SCL*/
	//PIN_CFG(ICUSBDM, GPIO122, 0, OFF, OFF, 0, 0, 8MA),	/* PROXI_INT */

	/* SIMCARD */
	PIN_CFG(SIMRST, SIMRST, 0, OFF, OFF, 0, 0, 8MA),	// SIM_RST
	PIN_CFG(SIMDAT, SIMDAT, 0, OFF, ON, 0, 0, 8MA),	// SIM_IO
	PIN_CFG(SIMCLK, SIMCLK, 0, OFF, OFF, 0, 0, 8MA),	// SIM_CLK
	PIN_CFG(SSPSYN, SIM2RST, 0, OFF, OFF, 0, 0, 8MA),
	PIN_CFG(SSPDO, SIM2DAT, 0, OFF, ON, 0, 0, 8MA),
	PIN_CFG(SSPCK, SIM2CLK, 0, OFF, OFF, 0, 0, 8MA),

	/* TOUCH */
	PIN_CFG(SIMDET, GPIO56, 0, ON, OFF, 0, 0, 8MA),    // TOUCH_EN
	PIN_CFG(GPS_TMARK, GPIO98, 0, ON, OFF, 0, 0, 8MA), // TOUCH_EN1
	PIN_CFG(SPI0TXD, GPIO91, 0, OFF, OFF, 0, 0, 8MA), 	// TSP_INT
	PIN_CFG(SPI0FSS, GPIO89, 0, OFF, OFF, 0, 0, 8MA), 	// TSP_SDA
	PIN_CFG(SPI0CLK, GPIO90, 0, OFF, OFF, 0, 0, 8MA), 	// TSP_SCL


//@HW LCD part configuration
	PIN_CFG(LCDCS0, LCDCS2, 0, ON, OFF,  0, 0, 8MA), // LCD_CS
	PIN_CFG(LCDSCL, LCDCD,  0, OFF, ON,  0, 0, 8MA), // LCD_RS
	PIN_CFG(LCDSDA, LCDD0,	0, ON, OFF,  0, 0, 8MA), // LCD_D0

	PIN_CFG(GPIO19, LCDWE,  0, OFF, ON,  0, 0, 8MA), // LCD_WR
	PIN_CFG(GPIO20, LCDRE,  0, OFF, ON,  0, 0, 8MA), // LCD_RD

	PIN_CFG(GPIO21, LCDD7,  0, ON, OFF,  0, 0, 8MA), // LCD_D7
	PIN_CFG(GPIO22, LCDD6,  0, ON, OFF,  0, 0, 8MA), // LCD_D6
	PIN_CFG(GPIO23, LCDD5,  0, ON, OFF,  0, 0, 8MA), // LCD_D5
	PIN_CFG(GPIO24, LCDD4,  0, ON, OFF,  0, 0, 8MA), // LCD_D4
	PIN_CFG(GPIO25, LCDD3,  0, ON, OFF,  0, 0, 8MA), // LCD_D3
	PIN_CFG(GPIO26, LCDD2,  0, ON, OFF,  0, 0, 8MA), // LCD_D2
	PIN_CFG(GPIO27, LCDD1,  0, ON, OFF,  0, 0, 8MA), // LCD_D1
	PIN_CFG(GPIO00, LCDD15, 0, ON, OFF,  0, 0, 8MA),
	PIN_CFG(GPIO01, LCDD14, 0, ON, OFF,  0, 0, 8MA),
	PIN_CFG(GPIO02, LCDD13, 0, ON, OFF,  0, 0, 8MA),
	PIN_CFG(GPIO03, LCDD12, 0, ON, OFF,  0, 0, 8MA),
	PIN_CFG(GPIO08, LCDD11, 0, ON, OFF,  0, 0, 8MA),
	PIN_CFG(GPIO09, LCDD10, 0, ON, OFF,  0, 0, 8MA),
	PIN_CFG(GPIO10, LCDD9,	0, ON, OFF,  0, 0, 8MA),
	PIN_CFG(GPIO11, LCDD8,	0, ON, OFF,  0, 0, 8MA),

	PIN_CFG(CAMCS0, LCDD16, 0, ON, OFF,  0, 0, 8MA), // LCD_D16
	PIN_CFG(CAMCS1, LCDD17, 0, ON, OFF,  0, 0, 8MA), // LCD_D17


	PIN_CFG(DCLK4,  GPIO95, 0, ON, OFF, 0, 0, 8MA), // LCD_BL_EN
	PIN_CFG(LCDRES, GPIO41, 0, OFF, ON, 0, 0, 8MA), // LCD_RST
	PIN_CFG(LCDTE,  LCDTE,  0, ON, OFF,  0, 0, 8MA), // LCD_FLM

	PIN_CFG(ICUSBDP, GPIO121, 0, ON, OFF, 0, 0, 8MA), // LCD_DET

	//The below pin should be controlled by CP
     	PIN_CFG(MDMGPIO00,  GPEN00, 0, OFF, OFF, 0, 0, 8MA), // 
	PIN_CFG(MDMGPIO01,  GPEN01, 0, OFF, OFF, 0, 0, 8MA), // 
	PIN_CFG(MDMGPIO02,  GPEN02, 0, OFF, OFF, 0, 0, 8MA), // 
	PIN_CFG(MDMGPIO03,  GPEN14, 0, OFF, OFF, 0, 0, 8MA), // 
	PIN_CFG(MDMGPIO04,  GPEN04, 0, OFF, OFF, 0, 0, 8MA), // 
	PIN_CFG(MDMGPIO05,  GPEN05, 0, OFF, OFF, 0, 0, 8MA), // 
	PIN_CFG(MDMGPIO06,  GPEN06, 0, OFF, OFF, 0, 0, 8MA), // 
        PIN_CFG(MDMGPIO08,  GPEN08, 0, OFF, OFF, 0, 0, 8MA), //

	PIN_CFG(UBTX,UBTX,0,OFF,ON,0,0,8MA),//GPIO46  BT_UART_RXD
	PIN_CFG(UBRTSN,UBRTSN,0,OFF,ON,0,0,8MA),//GPIO47  BT_UART_CTS_N

};


/* board level init */
int __init pinmux_board_init(void)
{
	int i;
	for (i=0; i<ARRAY_SIZE(board_pin_config); i++)
		pinmux_set_pin_config(&board_pin_config[i]);

	return 0;
}

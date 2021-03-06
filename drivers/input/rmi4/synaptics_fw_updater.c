/*
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Copyright (c) 2011 Synaptics, Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>

#include "synaptics_fw_updater.h"
#if defined (CONFIG_RMI4_FW_CORSICA) || defined (CONFIG_RMI4_FW_CORSICASS) || defined (CONFIG_RMI4_FW_NEVISW)|| defined (CONFIG_RMI4_FW_NEVISVESS)
#include "synaptics_fw_SY001002.h"
#else
#include "synaptics_fw_F003.h"
#endif

#define _DS4_3_2_
#define CFG_F54_TXCOUNT			50
#define CFG_F54_RXCOUNT			50

/* Variables for F34 functionality */
static unsigned short SynaF34DataBase;
static unsigned short SynaF34QueryBase;
static unsigned short SynaF01DataBase;
static unsigned short SynaF01CommandBase;
static unsigned short SynaF34Reflash_BlockNum;
static unsigned short SynaF34Reflash_BlockData;
static unsigned short SynaF34ReflashQuery_BootID;
static unsigned short SynaF34ReflashQuery_FlashPropertyQuery;
static unsigned short SynaF34ReflashQuery_FirmwareBlockSize;
static unsigned short SynaF34ReflashQuery_FirmwareBlockCount;
static unsigned short SynaF34ReflashQuery_ConfigBlockSize;
static unsigned short SynaF34ReflashQuery_ConfigBlockCount;
static unsigned short SynaFirmwareBlockSize;
static unsigned short SynaFirmwareBlockCount;
static unsigned long SynaImageSize;
static unsigned short SynaConfigBlockSize;
static unsigned short SynaConfigBlockCount;
static unsigned long SynaConfigImageSize;
static unsigned short SynaBootloadID;
static unsigned short SynaF34_FlashControl;
static u8 *SynafirmwareImgData;
static u8 *SynaconfigImgData;
static u8 *SynalockImgData;
static unsigned int SynafirmwareImgVersion;

static u8 *FirmwareImage;
static u8 *ConfigImage;
static const u8 *SynaFirmwareData;

/* Variables for F54 functionality */
static unsigned short F54_Query_Base;
static unsigned short F54_Command_Base;
static unsigned short F54_Control_Base;
static unsigned short F54_Data_Base;
static unsigned short F54_Data_LowIndex;
static unsigned short F54_Data_HighIndex;
static unsigned short F54_Data_Buffer;
#ifdef _DS4_3_0_
static unsigned short F54_PhysicalTx_Addr;
static unsigned short F54_PhysicalRx_Addr;
#else
#ifdef _DS4_3_2_
static unsigned short F55_PhysicalRx_Addr = 0x01;
static unsigned short F55_PhysicalTx_Addr = 0x01 + 1;
#endif
#endif
static unsigned short F54_CBCSettings;
static unsigned short F01_Control_Base;
static unsigned short F01_Command_Base;
static unsigned short F01_Data_Base;
static unsigned short F01_Query_Base;
static unsigned short F11_Query_Base;
static unsigned short F11_MaxNumberOfTx_Addr;
static unsigned short F11_MaxNumberOfRx_Addr;

// For F1A
unsigned short F1A_Query_Base;
unsigned short F1A_Command_Base;
unsigned short F1A_Control_Base;
unsigned short F1A_Data_Base;
unsigned short F1A_Button_Mapping;

unsigned char TxChannelUsed[CFG_F54_TXCOUNT];
unsigned char RxChannelUsed[CFG_F54_RXCOUNT];
static unsigned char numberOfTx;
static unsigned char numberOfRx;

unsigned char ButtonRXUsed[CFG_F54_TXCOUNT];
unsigned char ButtonTXUsed[CFG_F54_TXCOUNT];
unsigned char ButtonMapping[16];
unsigned char MaxButton;

static struct i2c_client *client;

static int readRMI(u8 address, u8 *buf, int size)
{
	if (i2c_master_send(client, &address, 1) < 0)
		return -1;

	if (i2c_master_recv(client, buf, size) < 0)
		return -1;

	return 1;
}

static int writeRMI(u8 address, u8 *buf, int size)
{
	int ret = 1;
	u8 *msg_buf;

	msg_buf = kzalloc(size + 1, GFP_KERNEL);
	msg_buf[0] = address;
	memcpy(msg_buf + 1, buf, size);

	if (i2c_master_send(client, msg_buf, size + 1) < 0)
		ret = -1;

	kfree(msg_buf);
	return ret;
}

/* SynaSetup scans the Page Description Table (PDT) and sets up the necessary
 * variables for the reflash process. This function is a "slim" version of the
 * PDT scan function in PDT.c, since only F34 and F01 are needed for reflash.
 */
static void SynaSetup(void)
{
	u8 address;
	u8 buffer[6];

	pr_info("tsp fw. : SynaSetup ++\n");

	for (address = 0xe9; address > 0xc0; address = address - 6) {
		readRMI(address, buffer, 6);

		switch (buffer[5]) {
		case 0x34:
			SynaF34DataBase = buffer[3];
			SynaF34QueryBase = buffer[0];
			break;

		case 0x01:
			SynaF01DataBase = buffer[3];
			SynaF01CommandBase = buffer[1];
			break;
		}
	}

	SynaF34Reflash_BlockNum = SynaF34DataBase;
	SynaF34Reflash_BlockData = SynaF34DataBase + 2;
	SynaF34ReflashQuery_BootID = SynaF34QueryBase;
	SynaF34ReflashQuery_FlashPropertyQuery = SynaF34QueryBase + 2;
	SynaF34ReflashQuery_FirmwareBlockSize = SynaF34QueryBase + 3;
	SynaF34ReflashQuery_FirmwareBlockCount = SynaF34QueryBase + 5;
	SynaF34ReflashQuery_ConfigBlockSize = SynaF34QueryBase + 3;
	SynaF34ReflashQuery_ConfigBlockCount = SynaF34QueryBase + 7;

	SynafirmwareImgData = (u8 *)((&SynaFirmwareData[0]) + 0x100);
	SynaconfigImgData = (u8 *)(SynafirmwareImgData + SynaImageSize);
	SynafirmwareImgVersion = (u32)(SynaFirmwareData[7]);

	switch (SynafirmwareImgVersion) {
	case 2:
		SynalockImgData = (u8 *)((&SynaFirmwareData[0]) + 0xD0);
		break;
	case 3:
	case 4:
		SynalockImgData = (u8 *)((&SynaFirmwareData[0]) + 0xC0);
		break;
	case 5:
		SynalockImgData = (u8 *)((&SynaFirmwareData[0]) + 0xB0);
		break;
	default:
		break;
	}
}

/* SynaInitialize sets up the reflahs process
 */
static void SynaInitialize(void)
{
	u8 uData[2];

	pr_info("tsp fw. : Initializing Reflash Process...\n");

	uData[0] = 0x00;
	writeRMI(0xff, uData, 1);

	SynaSetup();

	SynafirmwareImgData = &FirmwareImage[0];
	SynaconfigImgData = &ConfigImage[0];

	readRMI(SynaF34ReflashQuery_FirmwareBlockSize, uData, 2);

	SynaFirmwareBlockSize = uData[0] | (uData[1] << 8);

}

/* SynaReadFirmwareInfo reads the F34 query registers and retrieves the block
 * size and count of the firmware section of the image to be reflashed
 */
static void SynaReadFirmwareInfo(void)
{
	u8 uData[2];
	pr_info("tsp fw. : Read Firmware Info\n");

	readRMI(SynaF34ReflashQuery_FirmwareBlockSize, uData, 2);
	SynaFirmwareBlockSize = uData[0] | (uData[1] << 8);

	readRMI(SynaF34ReflashQuery_FirmwareBlockCount, uData, 2);
	SynaFirmwareBlockCount = uData[0] | (uData[1] << 8);
	SynaImageSize = SynaFirmwareBlockCount * SynaFirmwareBlockSize;
}

/* SynaReadConfigInfo reads the F34 query registers and retrieves the block size
 * and count of the configuration section of the image to be reflashed
 */
static void SynaReadConfigInfo(void)
{
	u8 uData[2];

	pr_info("tsp fw. : Read Config Info\n");

	readRMI(SynaF34ReflashQuery_ConfigBlockSize, uData, 2);
	SynaConfigBlockSize = uData[0] | (uData[1] << 8);

	readRMI(SynaF34ReflashQuery_ConfigBlockCount, uData, 2);
	SynaConfigBlockCount = uData[0] | (uData[1] << 8);
	SynaConfigImageSize = SynaConfigBlockCount * SynaConfigBlockSize;
}

/* SynaReadBootloadID reads the F34 query registers and retrieves the bootloader
 * ID of the firmware
 */
static void SynaReadBootloadID(void)
{
	u8 uData[2];
	pr_info("tsp fw. : SynaReadBootloadID\n");

	readRMI(SynaF34ReflashQuery_BootID, uData, 2);
	SynaBootloadID = uData[0] + uData[1] * 0x100;
}

/* SynaWriteBootloadID writes the bootloader ID to the F34 data register to
 * unlock the reflash process
 */
static void SynaWriteBootloadID(void)
{
	u8 uData[2];
	pr_info("tsp fw. : SynaWriteBootloadID\n");

	uData[0] = SynaBootloadID % 0x100;
	uData[1] = SynaBootloadID / 0x100;

	writeRMI(SynaF34Reflash_BlockData, uData, 2);
}

/* SynaEnableFlashing kicks off the reflash process
 */
static void SynaEnableFlashing(void)
{
	u8 uData;
	u8 uStatus;

	pr_info("\nEnable Reflash...");

	/* Reflash is enabled by first reading the bootloader ID from
	   the firmware and write it back */
	SynaReadBootloadID();
	SynaWriteBootloadID();

	/* Make sure Reflash is not already enabled */
	do {
		readRMI(SynaF34_FlashControl, &uData, 1);
	} while (((uData & 0x0f) != 0x00));

	readRMI(SynaF01DataBase, &uStatus, 1);

	if ((uStatus & 0x40) == 0) {
		/* Write the "Enable Flash Programming command to
		F34 Control register Wait for ATTN and then clear the ATTN. */
		uData = 0x0f;
		writeRMI(SynaF34_FlashControl, &uData, 1);
		mdelay(300);
		readRMI((SynaF01DataBase + 1), &uStatus, 1);

		/* Scan the PDT again to ensure all register offsets are
		correct */
		SynaSetup();

		/* Read the "Program Enabled" bit of the F34 Control register,
		and proceed only if the bit is set.*/
		readRMI(SynaF34_FlashControl, &uData, 1);

		while (uData != 0x80) {
			/* In practice, if uData!=0x80 happens for multiple
			counts, it indicates reflash is failed to be enabled,
			and program should quit */
			;
		}
	}
}

/* SynaWaitATTN waits for ATTN to be asserted within a certain time threshold.
 * The function also checks for the F34 "Program Enabled" bit and clear ATTN
 * accordingly.
 */
static void SynaWaitATTN(void)
{
	u8 uData;
	u8 uStatus;
	int cnt = 1;

	while (gpio_get_value(91) && cnt < 30) {
		//msleep(20);
		cnt++;
	}
	do {
		readRMI(SynaF34_FlashControl, &uData, 1);
		readRMI((SynaF01DataBase + 1), &uStatus, 1);
	} while (uData != 0x80);
}

/* SynaProgramConfiguration writes the configuration section of the image block
 * by block
 */
static void SynaProgramConfiguration(void)
{
	u8 uData[2];
	u8 *puData;
	unsigned short blockNum;

	puData = (u8 *) &SynaFirmwareData[0xb100];

	pr_info("tsp fw. : Program Configuration Section...\n");

	for (blockNum = 0; blockNum < SynaConfigBlockCount; blockNum++) {
		uData[0] = blockNum & 0xff;
		uData[1] = (blockNum & 0xff00) >> 8;

		/* Block by blcok, write the block number and data to
		the corresponding F34 data registers */
		writeRMI(SynaF34Reflash_BlockNum, uData, 2);
		writeRMI(SynaF34Reflash_BlockData, puData, SynaConfigBlockSize);
		puData += SynaConfigBlockSize;

		/* Issue the "Write Configuration Block" command */
		uData[0] = 0x06;
		writeRMI(SynaF34_FlashControl, uData, 1);
		SynaWaitATTN();
		pr_info(".");
	}
}

/* SynaFinalizeReflash finalizes the reflash process
*/
static void SynaFinalizeReflash(void)
{
	u8 uData;
	u8 uStatus;

	pr_info("tsp fw. : Finalizing Reflash..\n");

	/* Issue the "Reset" command to F01 command register to reset the chip
	 This command will also test the new firmware image and check if its is
	 valid */
	uData = 1;
	writeRMI(SynaF01CommandBase, &uData, 1);

	mdelay(300);
	readRMI(SynaF01DataBase, &uData, 1);

	/* Sanity check that the reflash process is still enabled */
	do {
		readRMI(SynaF34_FlashControl, &uStatus, 1);
	} while ((uStatus & 0x0f) != 0x00);
	readRMI((SynaF01DataBase + 1), &uStatus, 1);

	SynaSetup();

	uData = 0;

	/* Check if the "Program Enabled" bit in F01 data register is cleared
	 Reflash is completed, and the image passes testing when the bit is
	 cleared */
	do {
		readRMI(SynaF01DataBase, &uData, 1);
	} while ((uData & 0x40) != 0);

	/* Rescan PDT the update any changed register offsets */
	SynaSetup();

	pr_info("tsp fw. : Reflash Completed. Please reboot.\n");
}

/* SynaFlashFirmwareWrite writes the firmware section of the image block by
 * block
 */
static void SynaFlashFirmwareWrite(void)
{
	u8 *puFirmwareData;
	u8 uData[2];
	unsigned short blockNum;

	pr_info("tsp fw. : SynaFlashFirmwareWrite\n");

	puFirmwareData = (u8 *) &SynaFirmwareData[0x100];

	for (blockNum = 0; blockNum < SynaFirmwareBlockCount; ++blockNum) {
		/* Block by blcok, write the block number and data to
		the corresponding F34 data registers */
		uData[0] = blockNum & 0xff;
		uData[1] = (blockNum & 0xff00) >> 8;
		writeRMI(SynaF34Reflash_BlockNum, uData, 2);

		writeRMI(SynaF34Reflash_BlockData, puFirmwareData,
			SynaFirmwareBlockSize);
		puFirmwareData += SynaFirmwareBlockSize;

		/* Issue the "Write Firmware Block" command */
		uData[0] = 2;
		writeRMI(SynaF34_FlashControl, uData, 1);

		SynaWaitATTN();
	}
}

/* SynaProgramFirmware prepares the firmware writing process
*/
static void SynaProgramFirmware(void)
{
	u8 uData;

	pr_info("tsp fw. : Program Firmware Section...");

	SynaReadBootloadID();
	SynaWriteBootloadID();

	uData = 3;
	writeRMI(SynaF34_FlashControl, &uData, 1);

	SynaWaitATTN();
	SynaFlashFirmwareWrite();
}

/* eraseConfigBlock erases the config block */
static void eraseConfigBlock(void)
{
	u8 uData;

	pr_info("tsp fw. : eraseConfigBlock\n");
	/* Erase of config block is done by first entering into
	bootloader mode */
	SynaReadBootloadID();
	SynaWriteBootloadID();

	/* Command 7 to erase config block */
	uData = 7;
	writeRMI(SynaF34_FlashControl, &uData, 1);

	SynaWaitATTN();
}

void set_fw_version(char *FW_KERNEL_VERSION, char* FW_DATE)
{
	FW_KERNEL_VERSION[0] = SynaFirmware[0xb100];
	FW_KERNEL_VERSION[1] = SynaFirmware[0xb101];
	FW_KERNEL_VERSION[2] = SynaFirmware[0xb102];
	FW_KERNEL_VERSION[3] = SynaFirmware[0xb103];
	FW_KERNEL_VERSION[4] = '\0';
	//strncpy(FW_DATE, fw_date, 4);
	return;
}

bool fw_update_file(struct i2c_client *ts_client)
{
	int ret = 0;
	long fw_size = 0;
	unsigned char *fw_data;
	struct file *filp;
	loff_t pos;
	mm_segment_t oldfs;

	client = ts_client;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	filp = filp_open("/sdcard/synaptics_fw.img", O_RDONLY, 0);
	if (IS_ERR(filp)) {
		pr_err("tsp fw. : file open error:%d\n", (s32)filp);
		return false;
	}

	fw_size = filp->f_path.dentry->d_inode->i_size;
	pr_info("tsp fw. : size of the file : %ld(bytes)\n", fw_size);

	fw_data = kzalloc(fw_size, GFP_KERNEL);

	pos = 0;
	ret = vfs_read(filp, (char __user *)fw_data, fw_size, &pos);
	if (ret != fw_size) {
		pr_err("tsp fw. : failed to read file (ret = %d)\n", ret);
		kfree(fw_data);
		filp_close(filp, current->files);
		return false;
	}

	filp_close(filp, current->files);

	set_fs(oldfs);

	FirmwareImage = kzalloc(16000, GFP_KERNEL);
	if (FirmwareImage == NULL) {
		pr_err("tsp fw. : alloc fw. memory failed.\n");
		return false;
	}
	ConfigImage = kzalloc(16000, GFP_KERNEL);
	if (ConfigImage == NULL) {
		pr_err("tsp fw. : alloc fw. memory failed.\n");
		return false;
	}

	SynaFirmwareData = fw_data;
	SynaInitialize();
	SynaReadConfigInfo();
	SynaReadFirmwareInfo();
	SynaF34_FlashControl = SynaF34DataBase + SynaFirmwareBlockSize + 2;
	SynaEnableFlashing();
	SynaProgramFirmware();
	SynaProgramConfiguration();
	SynaFinalizeReflash();

	kfree(FirmwareImage);
	kfree(ConfigImage);
	kfree(fw_data);

	return true;
}

bool not_reset = 0;
bool fw_update_internal(struct i2c_client *ts_client)
{
	not_reset = 1;
	client = ts_client;
	SynaFirmwareData = SynaFirmware;

	FirmwareImage = kzalloc(16000, GFP_KERNEL);
	if (FirmwareImage == NULL) {
		pr_err("tsp fw. : alloc fw. memory failed.\n");
		return false;
	}
	ConfigImage = kzalloc(16000, GFP_KERNEL);
	if (ConfigImage == NULL) {
		pr_err("tsp fw. : alloc fw. memory failed.\n");
		return false;
	}

	SynaInitialize();
	SynaReadConfigInfo();
	SynaReadFirmwareInfo();
	SynaF34_FlashControl = SynaF34DataBase + SynaFirmwareBlockSize + 2;
	SynaEnableFlashing();
	SynaProgramFirmware();
	SynaProgramConfiguration();
	SynaFinalizeReflash();

	kfree(FirmwareImage);
	kfree(ConfigImage);

	not_reset = 0;
	return true;
}

/* SynaSetup scans the Page Description Table (PDT) and sets up the necessary
 * variables for the reflash process. This function is a "slim" version of the
 * PDT scan function in PDT.c, since only F54 and F01 are needed for test.
 */


static void F54_PDTscan(void)
{
	unsigned char address;
	unsigned char buffer[6];

	for (address = 0xe9; address > 0xd0; address = address - 6) {
		readRMI(address, &buffer[0], 6);

		if (!buffer[5])
			break;

		switch (buffer[5]) {
		case 0x01:
			F01_Query_Base = buffer[0];
			F01_Command_Base = buffer[1];
			F01_Control_Base = buffer[2];
			F01_Data_Base = buffer[3];
			break;
		case 0x11:
			F11_Query_Base = buffer[0];
			F11_MaxNumberOfTx_Addr = F11_Query_Base + 2;
			F11_MaxNumberOfRx_Addr = F11_Query_Base + 3;
			break;
		case 0x54:
			F54_Query_Base = buffer[0];
			F54_Command_Base = buffer[1];
			F54_Control_Base = buffer[2];
			F54_Data_Base = buffer[3];

			F54_Data_LowIndex = F54_Data_Base + 1;
			F54_Data_HighIndex = F54_Data_Base + 2;
			F54_Data_Buffer = F54_Data_Base + 3;
			F54_CBCSettings = F54_Control_Base + 8;
#ifdef _DS4_3_0_
			F54_PhysicalRx_Addr = F54_Control_Base + 18;
#endif
			break;

		case 0x1A:
			F1A_Query_Base = buffer[0];
			F1A_Command_Base = buffer[1];
			F1A_Control_Base = buffer[2];
			F1A_Data_Base = buffer[3];

			F1A_Button_Mapping = F1A_Control_Base + 3;
			break;
		}
	}
}

static void SetPage(unsigned char page)
{
	/* changing page */
	writeRMI(0xff, &page, 1);
}

u8 * get_Product_name(struct i2c_client *ts_client)
{
	u8 uData[10];
	
	SetPage(0x00);
	F54_PDTscan(); /* scan for page 0x00 */
	
	readRMI(F01_Query_Base+11, &uData, 10);

    printk("[TSP] get_Product_name=%s\n", uData);

	
	return uData;
	
}


static void RegSetup(void)
{
	unsigned char MaxNumberTx;
	unsigned char MaxNumberRx;
	unsigned char command;
	unsigned char temp;
	int i, k;
	numberOfRx = 0;
	numberOfTx = 0;
	MaxButton = 0;

	for (i = 0 ; i < CFG_F54_TXCOUNT; i++)
	{
		ButtonRXUsed[i] = 0;
		ButtonTXUsed[i] = 0;
	}

	SetPage(0x02);
	F54_PDTscan();  /* scan for page 0x02 */

	SetPage(0x01);
	F54_PDTscan(); /* scan for page 0x01 */

	SetPage(0x00);
	F54_PDTscan(); /* scan for page 0x00 */

#ifdef _DS4_3_0_
	/* Check Used Rx channels */
	readRMI(F11_MaxNumberOfRx_Addr, &MaxNumberRx, 1);
	SetPage(0x01);
	F54_PhysicalTx_Addr = F54_PhysicalRx_Addr + MaxNumberRx;
	readRMI(F54_PhysicalRx_Addr, &RxChannelUsed[0], MaxNumberRx);

	/* Checking Used Tx channels */
	SetPage(0x00);
	readRMI(F11_MaxNumberOfTx_Addr, &MaxNumberTx, 1);
	SetPage(0x01);
	readRMI(F54_PhysicalTx_Addr, &TxChannelUsed[0], MaxNumberTx);
#else
#ifdef _DS4_3_2_
	/* Check Used Rx channels */
	readRMI(F11_MaxNumberOfRx_Addr, &MaxNumberRx, 1);
	SetPage(0x03);
	readRMI(F55_PhysicalRx_Addr, &RxChannelUsed[0], MaxNumberRx);

	/* Checking Used Tx channels */
	SetPage(0x00);
	readRMI(F11_MaxNumberOfTx_Addr, &MaxNumberTx, 1);
	SetPage(0x03);
	readRMI(F55_PhysicalTx_Addr, &TxChannelUsed[0], MaxNumberTx);
#endif
#endif

	/* Check used number of Rx */
	for (i = 0; i < MaxNumberRx; i++) {
		if (RxChannelUsed[i] == 0xff)
			break;
		numberOfRx++;
	}

	/* Check used number of Tx */
	for (i = 0; i < MaxNumberTx; i++) {
		if (TxChannelUsed[i] == 0xff)
			break;
		numberOfTx++;
	}

	SetPage(0x02);
	readRMI(F1A_Button_Mapping, ButtonMapping, 16);

	for(k=0; k<8; k++)
	{
		if((temp = ButtonMapping[k*2+1]) == 0xFF)	break;     // Button RX
		else
		{
			for(i=0; i<MaxNumberRx; i++)
			{
				if(RxChannelUsed[i] == temp) break; 
					ButtonRXUsed[k]++;
			}
			MaxButton++;
		}
	
		if((temp = ButtonMapping[k*2]) == 0xFF)	break;     // Button TX
		else
		{
			for(i=0; i<MaxNumberTx; i++)
			{
				if(TxChannelUsed[i] == temp) break; 
					ButtonTXUsed[k]++;
			}
		}
	}

	/* Enabling only the analog image reporting interrupt, and
	 * turn off the rest
	 */
	SetPage(0x00);
	command = 0x08;
	writeRMI(F01_Control_Base+1, &command, 1);

	SetPage(0x01);
}

#define NOISEMITIGATION			0x36

bool F54_SetRawCapData(struct i2c_client *ts_client, s16 *node_data)
{
	u8 *ImageBuffer;
	int i, j, k, x, length;
	unsigned char command;

	client = ts_client;

	RegSetup(); /* PDT scan for reg map adress mapping */

	length = numberOfTx * numberOfRx * 2;

	/* Set report mode to to run the AutoScan */
	command = 0x03;
	writeRMI(F54_Data_Base, &command, 1);

	/* NoCDM4 */
	command = 0x01;
	writeRMI(NOISEMITIGATION, &command, 1);

	command = 0x06;
	writeRMI(F54_Command_Base, &command, 1);

	do {
		mdelay(1);
		readRMI(F54_Command_Base, &command, 1);
	} while (command != 0x00);

	command = 0x00;
	writeRMI(F54_Data_LowIndex, &command, 1);
	writeRMI(F54_Data_HighIndex, &command, 1);

	/* Set the GetReport bit to run the AutoScan */
	command = 0x01;
	writeRMI(F54_Command_Base, &command, 1);

	/* Wait until the command is completed */
	do {
		mdelay(1);
		readRMI(F54_Command_Base, &command, 1);
	} while (command != 0x00);

	ImageBuffer =
	kmalloc(sizeof(u8) * CFG_F54_TXCOUNT * CFG_F54_RXCOUNT * 2, GFP_KERNEL);
	if (ImageBuffer == NULL) {
		pr_err("tsp fw. : alloc fw. memory failed.\n");
		return false;
	}

	/* Read raw_cap data */
	readRMI(F54_Data_Buffer, ImageBuffer, length);

	x=0;
	k=0;
	for (i=0; i<numberOfTx;i++)
	{
		for (j=0; j<numberOfRx;j++)
		{
			if(j != (numberOfRx-1))
			{
				node_data[x] =
			(s16)(ImageBuffer[k] | (ImageBuffer[k + 1] << 8));
                        //printk(" %d", node_data[x]);
				x++;
			}
			k+=2;
		}
	}
#if 1
	/* reset TSP IC */
	SetPage(0x00);
	command = 0x01;
	writeRMI(F01_Command_Base, &command, 1);
        	printk("[TSP] %s, %d\n", __func__, __LINE__ );
	mdelay(160);
        	printk("[TSP] %s, %d\n", __func__, __LINE__ );
	readRMI(F01_Data_Base + 1, &command, 1);
#endif
	kfree(ImageBuffer);

	return true;
}

/* (important) should be defined the value(=register address) according to
 * register map 'Multi Metric Noise Mitigation Control'
 */

bool F54_SetRxToRxData(struct i2c_client *ts_client, s16 *node_data)
{
	u8 *ImageBuffer;
	int i, k, length;
	u8 command;

	client = ts_client;

	RegSetup(); /* PDT scan for reg map adress mapping */

	/* Set report mode to run Rx-to-Rx 1st data */
	length = numberOfRx * numberOfTx * 2;
	command = 0x07;
	writeRMI(F54_Data_Base, &command, 1);

	/* NoCDM4 */
	command = 0x01;
	writeRMI(NOISEMITIGATION, &command, 1);

	command = 0x04;
	writeRMI(F54_Command_Base, &command, 1);

	do {
		mdelay(1);
		readRMI(F54_Command_Base, &command, 1);
	} while (command != 0x02);

	command = 0x02;
	writeRMI(F54_Command_Base, &command, 1);

	do {
		mdelay(1);
		readRMI(F54_Command_Base, &command, 1);
	} while (command != 0x00);

	command = 0x00;
	writeRMI(F54_Data_LowIndex, &command, 1);
	writeRMI(F54_Data_HighIndex, &command, 1);

	/* Set the GetReport bit to run Tx-to-Tx */
	command = 0x01;
	writeRMI(F54_Command_Base, &command, 1);

	/* Wait until the command is completed */
	do {
		mdelay(1);
		readRMI(F54_Command_Base, &command, 1);
	} while (command != 0x00);

	ImageBuffer =
	kmalloc(sizeof(u8) * CFG_F54_TXCOUNT * CFG_F54_RXCOUNT * 2, GFP_KERNEL);
	if (ImageBuffer == NULL) {
		pr_err("tsp fw. : alloc fw. memory failed.\n");
		return false;
	}

	readRMI(F54_Data_Buffer, &ImageBuffer[0], length);

	for (i = 0, k = 0; i < numberOfTx * numberOfRx; i++, k += 2) {
		node_data[i] =
			(s16)((ImageBuffer[k] | (ImageBuffer[k + 1] << 8)));
	}

	/* Set report mode to run Rx-to-Rx 2nd data */
	length = numberOfRx * (numberOfRx - numberOfTx) * 2;
	command = 0x11;
	writeRMI(F54_Data_Base, &command, 1);

	command = 0x00;
	writeRMI(F54_Data_LowIndex, &command, 1);
	writeRMI(F54_Data_HighIndex, &command, 1);

	/* Set the GetReport bit to run Tx-to-Tx */
	command = 0x01;
	writeRMI(F54_Command_Base, &command, 1);

	/* Wait until the command is completed */
	do {
		mdelay(1);
		readRMI(F54_Command_Base, &command, 1);
	} while (command != 0x00);

	readRMI(F54_Data_Buffer, &ImageBuffer[0], length);

	for (i = 0, k = 0; i < (numberOfRx - numberOfTx) * numberOfRx;
								i++, k += 2)
		node_data[(numberOfTx * numberOfRx) + i] =
			(s16)(ImageBuffer[k] | (ImageBuffer[k + 1] << 8));

	/* Set the Force Cal */
	command = 0x02;
	writeRMI(F54_Command_Base, &command, 1);

	do {
		mdelay(1);
		readRMI(F54_Command_Base, &command, 1);
	} while (command != 0x00);

	/* enable all the interrupts */
	SetPage(0x00);
	command = 0x01;
	writeRMI(F01_Command_Base, &command, 1);
	msleep(160);
	/* Read Interrupt status register to Interrupt line goes to high */
	readRMI(F01_Data_Base+1, &command, 1);

	kfree(ImageBuffer);

	return true;
}

/**
 * F54_TxToTest() - tx to tx, tx to gnd channel short test.
 * @ts_client : i2c client for i2c comm. with TS IC.
 * @node_data : array pointer test value be stored.
 * @mode : command that select return value tx-to-tx or tx-to-gnd.
 * normal value : '0' at tx-to-tx test
 *                '1' at tx-to-gnd test
 */

#define TX_TO_TX_TEST_MODE		0x05
#define TX_TO_GND_TEST_MODE		0x10

bool F54_TxToTest(struct i2c_client *ts_client, s16 *node_data, int mode)
{
	u8 ImageBuffer[CFG_F54_TXCOUNT] = {0, };
	u8 ImageArray[CFG_F54_TXCOUNT] = {0, };
	u8 command;
	int i, k, length, shift;

	client = ts_client;

	RegSetup(); /* PDT scan for reg map adress mapping */

	length = (numberOfTx + 7) / 8;

	/* Set report mode to run Tx-to-Tx */
	command = mode;
	writeRMI(F54_Data_Base, &command, 1);

	command = 0x00;
	writeRMI(F54_Data_LowIndex, &command, 1);
	writeRMI(F54_Data_HighIndex, &command, 1);

	/* Set the GetReport bit to run Tx-to-Tx */
	command = 0x01;
	writeRMI(F54_Command_Base, &command, 1);

	/* Wait until the command is completed */
	do {
		mdelay(1);
		readRMI(F54_Command_Base, &command, 1);
	} while (command != 0x00);

	readRMI(F54_Data_Buffer, &ImageBuffer[0], length);

	if (mode == TX_TO_TX_TEST_MODE) {
		/* One bit per transmitter channel */
		for (i = 0, k = 0; i < numberOfTx; i++) {
			k = i / 8;
			shift = i % 8;
			node_data[i] = ImageBuffer[k] & (1 << shift);
		}
	} else if (mode == TX_TO_GND_TEST_MODE) {
		/* One bit per transmitter channel */
		for (i = 0, k = 0; i < length * 8; i++) {
			k = i / 8;
			shift = i % 8;
			if (ImageBuffer[k] & (1 << shift))
				ImageArray[i] = 1;
		}
		for (i = 0; i < numberOfTx; i++)
			node_data[i] = ImageArray[TxChannelUsed[i]];
	}

	/* enable all the interrupts */
	SetPage(0x00);
	command = 0x01;
	writeRMI(F01_Command_Base, &command, 1);
	msleep(160);        // 160ms
	/* Read Interrupt status register to Interrupt line goes to high */
	readRMI(F01_Data_Base+1, &command, 1);

	return true;
}

#define F51_0D_DATA 0x00
#define MaxButton 2

bool F54_ButtonDeltaImage(struct i2c_client *ts_client, s16 *node_data)
{
   u8 *ImageBuffer;
	int k, z, length;

    client = ts_client;

	length = MaxButton * 2;

    ImageBuffer =
    kmalloc(sizeof(u8) * CFG_F54_TXCOUNT * CFG_F54_RXCOUNT * 2, GFP_KERNEL);
    if (ImageBuffer == NULL) {
    	pr_err("tsp fw. : alloc fw. memory failed.\n");
    	return false;
    }

	/*  Read Button Delta Value */
	SetPage(0x04);
	readRMI(F51_0D_DATA, ImageBuffer, length);

	printk("MaxButton = %d", MaxButton);
   
	k = 0;     
		   for (z = 0;z < MaxButton ; z++)
		   {
		node_data[z] =
				(s16)(ImageBuffer[k] | (ImageBuffer[k + 1] << 8));
		   k = k + 2;
	   }
	 
	printk("\n");
	
	SetPage(0x00);

    kfree(ImageBuffer);

	return true;
}

bool F54_SetDeltaImageData(struct i2c_client *ts_client, s16 *node_data)
{
	u8 *ImageBuffer;
	int i, j, k, x, length;
	unsigned char command;

	client = ts_client;

	RegSetup(); /* PDT scan for reg map adress mapping */

	length = numberOfTx * numberOfRx * 2;

	/* Set report mode to to run the AutoScan */
	command = 0x02;
	writeRMI(F54_Data_Base, &command, 1);

	command = 0x00;
	writeRMI(F54_Data_LowIndex, &command, 1);
	writeRMI(F54_Data_HighIndex, &command, 1);

	/* Set the GetReport bit to run the AutoScan */
	command = 0x01;
	writeRMI(F54_Command_Base, &command, 1);

	/* Wait until the command is completed */
	do {
		mdelay(1);
		readRMI(F54_Command_Base, &command, 1);
	} while (command != 0x00);

	ImageBuffer =
	kmalloc(sizeof(u8) * CFG_F54_TXCOUNT * CFG_F54_RXCOUNT * 2, GFP_KERNEL);
	if (ImageBuffer == NULL) {
		pr_err("tsp fw. : alloc fw. memory failed.\n");
		return false;
	}

	/* Read raw_cap data */
	readRMI(F54_Data_Buffer, ImageBuffer, length);

	x=0;
	k=0;
	for (i=0; i<numberOfTx;i++)
	{
		for (j=0; j<numberOfRx;j++)
		{
			if(j != (numberOfRx-1))
			{
				node_data[x] =
			(s16)(ImageBuffer[k] | (ImageBuffer[k + 1] << 8));
                        //printk(" %d", node_data[x]);
				x++;
			}
			k+=2;
		}
	}
#if 1
	/* reset TSP IC */
	SetPage(0x00);
	command = 0x01;
	writeRMI(F01_Command_Base, &command, 1);
        	printk("[TSP] %s, %d\n", __func__, __LINE__ );
	mdelay(160);
        	printk("[TSP] %s, %d\n", __func__, __LINE__ );
	readRMI(F01_Data_Base + 1, &command, 1);
#endif
	kfree(ImageBuffer);

	return true;
}


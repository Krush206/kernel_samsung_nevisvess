/* drivers/input/touchscreen/tma140_lucas.c
 *
 * Copyright (C) 2007 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <mach/gpio.h>
#include <linux/device.h>
#include <linux/regulator/consumer.h>
#include <linux/irq.h>


#include <linux/firmware.h>
#include <linux/uaccess.h> 
#include <linux/rtc.h>
/* firmware - update */

#define MAX_X	480 
#define MAX_Y	360
#define TSP_INT 86
#define TSP_SDA 85
#define TSP_SCL 87

#define TOUCH_EN 22

#define MAX_KEYS	2
#define MAX_USING_FINGER_NUM 2

#define SEC_TSP_FACTORY_TEST

#define GLOBAL_IDAC_NUM	10
#define NODE_NUM	80	/* 10X8 */
#define NODE_X_NUM 10
#define NODE_Y_NUM 8

#define LOCAL_IDAC_START	6
#define GLOBAL_IDAC_START	7

#define __SEND_VIRTUAL_RELEASED__		// force release previous press event, when the next touch is pressed very quickly

#define TSP_EDS_RECOVERY

unsigned int raw_count[NODE_NUM]= {{0,},};;
unsigned int difference[NODE_NUM]= {{0,},};;
unsigned int local_idac[NODE_NUM]= {{0,},};;
unsigned int global_idac[GLOBAL_IDAC_NUM]= {{0,},};;

static const int touchkey_keycodes[] = {
			KEY_MENU,
			KEY_BACK,
			//KEY_HOME,
			//KEY_SEARCH,
};


#ifdef SEC_TSP_FACTORY_TEST
#define TSP_BUF_SIZE 1024

#define TSP_CMD_STR_LEN 32
#define TSP_CMD_RESULT_STR_LEN 512
#define TSP_CMD_PARAM_NUM 8
#endif /* SEC_TSP_FACTORY_TEST */


#define TOUCH_ON 1
#define TOUCH_OFF 0

#define TRUE    1
#define FALSE    0

#define I2C_RETRY_CNT	2

//#define __TOUCH_DEBUG__
#define USE_THREADED_IRQ	1

//#define __TOUCH_KEYLED__ 

static struct regulator *touch_regulator=NULL;
#if defined (__TOUCH_KEYLED__)
static struct regulator *touchkeyled_regulator=NULL;
#endif


#if defined (TSP_EDS_RECOVERY)
static struct workqueue_struct *check_ic_wq;
#endif

static int touchkey_status[MAX_KEYS];

#define TK_STATUS_PRESS		1
#define TK_STATUS_RELEASE		0

int tsp_irq;
int st_old;

typedef struct
{
	int8_t id;	/*!< (id>>8) + size */
	int8_t status;/////////////IC
	int8_t z;	/*!< dn>0, up=0, none=-1 */
	int16_t x;			/*!< X */
	int16_t y;			/*!< Y */
} report_finger_info_t;

#if defined (__SEND_VIRTUAL_RELEASED__)
static report_finger_info_t fingerInfo[MAX_USING_FINGER_NUM+1]={0,};
#else
static report_finger_info_t fingerInfo[MAX_USING_FINGER_NUM]={0,};
#endif

enum {
	BUILT_IN = 0,
	UMS,
	REQ_FW,
};

struct touch_data {
	uint16_t addr;
	struct i2c_client *client;
	struct input_dev *input_dev;
	int use_irq;
	struct hrtimer timer;				////////////////////////IC
#if defined (TSP_EDS_RECOVERY)
	struct work_struct  esd_recovery_func;		////////////////////////IC
#endif	
	unsigned char fw_ic_ver;

#if defined(SEC_TSP_FACTORY_TEST)
	struct list_head			cmd_list_head;
	unsigned char cmd_state;
	char			cmd[TSP_CMD_STR_LEN];
	int			cmd_param[TSP_CMD_PARAM_NUM];
	char			cmd_result[TSP_CMD_RESULT_STR_LEN];
	struct mutex			cmd_lock;
	bool			cmd_is_running;

	bool ft_flag;
#endif				/* SEC_TSP_FACTORY_TEST */

	struct early_suspend early_suspend;
};


struct touch_data *ts_global;

/* firmware - update */
static int firmware_ret_val = -1;
 

unsigned char esd_conter=0;
unsigned char now_tspfw_update = 0;

unsigned char tsp_special_update = 0;
static char IsfwUpdate[20]={0};

/* touch information*/
unsigned char touch_vendor_id = 0;
unsigned char touch_hw_ver = 0;
unsigned char touch_sw_ver = 0;


#define TSP_HW_REV03			0x03
#define TSP_HW_REV04			0x04

#define TSP_SW_VER_FOR_HW03		0x07
#define TSP_SW_VER_FOR_HW04		0x04


int tsp_irq_num = 0;
int tsp_workqueue_num = 0;
int tsp_threadedirq_num = 0;

int g_touch_info_x = 0;
int g_touch_info_y = 0;
int g_touch_info_press = 0;

int prev_pressed_num=0;

static int pre_ta_stat = 0;
int tsp_status=0;
int reset_check = 0;	// flag to check in the reset sequecne : 1, or not : 0

static uint8_t raw_min=0;
static uint8_t raw_max=0;
static uint8_t IDAC_min=0;
static uint8_t IDAC_max=0;

extern int cypress_update( int );
int tsp_i2c_read(u8 reg, unsigned char *rbuf, int buf_size);

void set_tsp_for_ta_detect(int);

struct touch_trace_data {
	uint32_t time;
	int16_t fingernum;	
	int8_t status;
	int8_t id;
	uint16_t x;
	uint16_t y;

};
#define MAX_TOUCH_TRACE_NUMBER	10000
static int touch_trace_index = 0;
static struct touch_trace_data touch_trace_info[MAX_TOUCH_TRACE_NUMBER];


struct timespec ts;
struct rtc_time tm;
    

/* sys fs */
struct class *touch_class;
EXPORT_SYMBOL(touch_class);

#if 0
struct device *firmware_dev;
EXPORT_SYMBOL(firmware_dev);
#else
struct device *sec_touchscreen;
EXPORT_SYMBOL(sec_touchscreen);
#endif


static void firm_update_callfc(void);

static ssize_t read_threshold(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t firmware_In_Binary(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t firmware_In_TSP(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t firm_update(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t firmware_update_status(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t tsp_panel_rev(struct device *dev, struct device_attribute *attr, char *buf);


static DEVICE_ATTR(tsp_firm_version_phone, S_IRUGO /*0444*/, firmware_In_Binary, NULL);
static DEVICE_ATTR(tsp_firm_version_panel, S_IRUGO /*0444*/, firmware_In_TSP, NULL);
static DEVICE_ATTR(tsp_firm_update, S_IRUGO /*0444*/, firm_update, firm_update);
static DEVICE_ATTR(tsp_firm_update_status, S_IRUGO /*0444*/, firmware_update_status, firmware_update_status);
static DEVICE_ATTR(tsp_threshold, S_IRUGO /*0444*/, read_threshold, read_threshold);
static DEVICE_ATTR(get_panel_rev, S_IRUGO /*0444*/, tsp_panel_rev, tsp_panel_rev);


/* sys fs */

#if defined(SEC_TSP_FACTORY_TEST)
#define TSP_CMD(name, func) .cmd_name = name, .cmd_func = func

struct tsp_cmd {
	struct list_head	list;
	const char	*cmd_name;
	void	(*cmd_func)(void *device_data);
};

static void fw_update(void *device_data);
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void get_threshold(void *device_data);
static void module_off_master(void *device_data);
static void module_on_master(void *device_data);
static void module_off_slave(void *device_data);
static void module_on_slave(void *device_data);
static void get_chip_vendor(void *device_data);
static void get_chip_name(void *device_data);
static void get_reference(void *device_data);
static void get_raw_count(void *device_data);
static void get_difference(void *device_data);
static void get_intensity(void *device_data);
static void get_local_idac(void *device_data);
static void get_global_idac(void *device_data);
static void get_x_num(void *device_data);
static void get_y_num(void *device_data);
static void run_reference_read(void *device_data);
static void run_raw_count_read(void *device_data);
static void run_difference_read(void *device_data);
static void run_intensity_read(void *device_data);
static void not_support_cmd(void *device_data);
static void run_raw_node_read(void *device_data);
static void run_global_idac_read(void *device_data);
static void run_local_idac_read(void *device_data);


struct tsp_cmd tsp_cmds[] = {
	{TSP_CMD("fw_update", not_support_cmd),},
	{TSP_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{TSP_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{TSP_CMD("get_threshold", get_threshold),},
	{TSP_CMD("module_off_master", not_support_cmd),},
	{TSP_CMD("module_on_master", not_support_cmd),},
	{TSP_CMD("module_off_slave", not_support_cmd),},
	{TSP_CMD("module_on_slave", not_support_cmd),},
	{TSP_CMD("get_chip_vendor", get_chip_vendor),},
	{TSP_CMD("get_chip_name", get_chip_name),},
	{TSP_CMD("run_raw_node_read", run_raw_node_read),},
	{TSP_CMD("get_x_num", get_x_num),},
	{TSP_CMD("get_y_num", get_y_num),},
	{TSP_CMD("get_reference", not_support_cmd),},
	{TSP_CMD("get_raw_count", get_raw_count),},
	{TSP_CMD("get_difference", get_difference),},
	{TSP_CMD("get_intensity", not_support_cmd),},
	{TSP_CMD("get_local_idac", get_local_idac),},	
	{TSP_CMD("get_global_idac", get_global_idac),},
	{TSP_CMD("run_reference_read", not_support_cmd),},
	{TSP_CMD("run_raw_count_read", run_raw_count_read),},
	{TSP_CMD("run_difference_read", run_difference_read),},
	{TSP_CMD("run_intensity_read", not_support_cmd),},
	{TSP_CMD("run_global_idac_read", run_global_idac_read),},
	{TSP_CMD("run_local_idac_read", run_local_idac_read),},	
	{TSP_CMD("not_support_cmd", not_support_cmd),},
};
#endif

static int tsp_testmode = 0;
static int prev_wdog_val = -1;
static int tsp_irq_operation = 0;
static unsigned int touch_present = 0;


#define FW_DOWNLOADING "Downloading"
#define FW_DOWNLOAD_COMPLETE "Complete"
#define FW_DOWNLOAD_FAIL "FAIL"
#define FWUP_NOW -1

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ts_early_suspend(struct early_suspend *h);
static void ts_late_resume(struct early_suspend *h);
#endif

extern int bcm_gpio_pull_up(unsigned int gpio, bool up);
extern int bcm_gpio_pull_up_down_enable(unsigned int gpio, bool enable);
extern int set_irq_type(unsigned int irq, unsigned int type);

extern int tsp_charger_type_status;


void touch_ctrl_regulator(int on_off)
{
	if(on_off==TOUCH_ON)
	{
		gpio_request(TOUCH_EN,"Touch_en");
		gpio_direction_output(TOUCH_EN,1);
		gpio_set_value(TOUCH_EN,1);
		gpio_free(TOUCH_EN);

#if defined (__TOUCH_KEYLED__)
        regulator_set_voltage(touchkeyled_regulator,3300000,3300000);
		regulator_enable(touchkeyled_regulator);
#endif
	}
	else
	{
		gpio_request(TOUCH_EN,"Touch_en");
		gpio_direction_output(TOUCH_EN,0);
		gpio_set_value(TOUCH_EN,0);
		gpio_free(TOUCH_EN);

#if defined (__TOUCH_KEYLED__) 
		regulator_disable(touchkeyled_regulator);
#endif
	}
}
EXPORT_SYMBOL(touch_ctrl_regulator);

int tsp_reset( void )
{
	int ret=1;

      #if defined(__TOUCH_DEBUG__)
	printk("[TSP] %s, %d\n", __func__, __LINE__ );
      #endif 
      
    if(reset_check == 0)
	{
		reset_check = 1;

	touch_ctrl_regulator(0);

	gpio_direction_output( TSP_SCL , 0 ); 
	gpio_direction_output( TSP_SDA , 0 ); 
	//gpio_direction_output( TSP_INT , 0 ); 

	msleep(500);

	gpio_direction_output( TSP_SCL , 1 ); 
	gpio_direction_output( TSP_SDA , 1 ); 
	//gpio_direction_output( TSP_INT , 1 ); 

	touch_ctrl_regulator(1);

	msleep(10);

        reset_check = 0;
    }

	return ret;
}


static void process_key_event(uint8_t tsk_msg)
{
	int i;
	int keycode= 0;
	int st_new;

        printk("[TSP] process_key_event : %d\n", tsk_msg);

	if(	tsk_msg	== 0)
	{
		input_report_key(ts_global->input_dev, st_old, 0);
		printk("[TSP] release keycode: %4d, keypress: %4d\n", st_old, 0);
	}
	else{
	//check each key status
		for(i = 0; i < MAX_KEYS; i++)
		{

		st_new = (tsk_msg>>(i)) & 0x1;
		if (st_new ==1)
		{
		keycode = touchkey_keycodes[i];
		input_report_key(ts_global->input_dev, keycode, 1);
		printk("[TSP] press keycode: %4d, keypress: %4d\n", keycode, 1);
		}

		st_old = keycode;


		}
	}
}

void tsp_log(report_finger_info_t *fingerinfo, int i)
{
#if defined(__TOUCH_DEBUG__)
	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);

    printk("[TSP][%02d:%02d:%02d.%03lu] ",  tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
#endif    
    
	touch_trace_info[touch_trace_index].time = jiffies;
	touch_trace_info[touch_trace_index].status = fingerInfo[i].status;
	touch_trace_info[touch_trace_index].id = fingerInfo[i].id;
	touch_trace_info[touch_trace_index].x = fingerInfo[i].x;
	touch_trace_info[touch_trace_index].y = fingerInfo[i].y;
	touch_trace_info[touch_trace_index].fingernum=i;

//	printk("[TSP] i[%d] id[%d] xy[%d, %d] status[%d]\n", i, fingerInfo[i].id, fingerInfo[i].x, fingerInfo[i].y, fingerInfo[i].status);

	if(fingerInfo[i].status==1)
		printk("[TSP] finger down\n");
	else
		printk("[TSP] finger up\n");	

	touch_trace_index++;

	if(touch_trace_index >= MAX_TOUCH_TRACE_NUMBER)
	{
		touch_trace_index = 0;
	}
	/* -- due to touch trace -- */
	
}

#define ABS(a,b) (a>b?(a-b):(b-a))
static irqreturn_t ts_work_func(int irq, void *dev_id)
{
	int ret=0;
	uint8_t buf[14];// 01h ~ 1Fh
	uint8_t i2c_addr = 0x01;
	int i = 0;
	int pressed_num;

    if(tsp_testmode)
		return IRQ_HANDLED;

	tsp_irq_operation = 1;

	struct touch_data *ts = dev_id;
	
	ret = tsp_i2c_read( i2c_addr, buf, sizeof(buf));

	if(0x20&buf[0] == 0x20)
	{
		printk("[TSP] buf[0]=%d 5 bit is 1 retry i2c,\n",buf[0]);
		ret = tsp_i2c_read( i2c_addr, buf, sizeof(buf));
		printk("[TSP] buf[0]=%d\n",buf[0]);
		
	}

	if (ret <= 0) {
		printk("[TSP] i2c failed : ret=%d, ln=%d\n",ret, __LINE__);
		goto work_func_out;
	}
	

	//01 ???? ???? 4?????? ???? finger ????
	pressed_num= 0x0f&buf[1];
	
	fingerInfo[0].x = (buf[2] << 8) |buf[3];
	fingerInfo[0].y = (buf[4] << 8) |buf[5];
	fingerInfo[0].z = buf[6];
	fingerInfo[0].id = buf[7] >>4;

	fingerInfo[1].x = (buf[8] << 8) |buf[9];
	fingerInfo[1].y = (buf[10] << 8) |buf[11];
	fingerInfo[1].z = buf[12];
	fingerInfo[1].id = buf[7] & 0xf;

#if defined(__TOUCH_DEBUG__)
	printk("[TSP] pressed finger num [%d]\n", pressed_num);			 		
#endif

#if defined (__SEND_VIRTUAL_RELEASED__)
	if((fingerInfo[2].id != fingerInfo[0].id) && (prev_pressed_num ==1 && pressed_num==1))
	{
				
				input_mt_slot(ts->input_dev, 0);
				input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);

		fingerInfo[2].status = 0;
//#if defined(__TOUCH_DEBUG__)		
		 		printk("[TSP] send virtual release  xy[%d, %d]\n", fingerInfo[2].x, fingerInfo[2].y);
 		tsp_log(fingerInfo, 2);
//#endif
		input_sync(ts->input_dev);
			}
#endif

	/* check touch event */
	for ( i= 0; i<MAX_USING_FINGER_NUM; i++ )
#if defined (__SEND_VIRTUAL_RELEASED__)
	{
		if(fingerInfo[i].id >=1) // press interrupt
			{
			fingerInfo[i].status = 1;

				input_mt_slot(ts->input_dev, i);
				input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
				
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, fingerInfo[i].x);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, fingerInfo[i].y);

				tsp_log(fingerInfo, i);
		}
		else if(fingerInfo[i].id ==0) // release interrupt (only first finger)
		{
			if(fingerInfo[i].status == 1) // prev status is press
			{
				fingerInfo[i].status = 0;
				
				input_mt_slot(ts->input_dev, i);
				input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
				
				tsp_log(fingerInfo, i);
			}
		}

	}
#else	
	{
		//////////////////////////////////////////////////IC
		if(fingerInfo[i].id >=1) // press interrupt
		{
			fingerInfo[i].status = 1;
		}
		else if(fingerInfo[i].id ==0) // release interrupt (only first finger)
		{
			if(fingerInfo[i].status == 1) // prev status is press
				fingerInfo[i].status = 0;
			else if(fingerInfo[i].status == 0) // release already or force release
				fingerInfo[i].status = -1;
		}		

		if(fingerInfo[i].status < 0) continue;
		//////////////////////////////////////////////////IC

		input_mt_slot(ts->input_dev, i);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, fingerInfo[i].status);
		
		if(fingerInfo[i].status)
		{
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, fingerInfo[i].x);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, fingerInfo[i].y);
		}

		printk("[TSP] i[%d] id[%d] xyz[%d, %d, %d] status[%d]\n", i, fingerInfo[i].id, fingerInfo[i].x, fingerInfo[i].y, fingerInfo[i].z, fingerInfo[i].status);	
	}
#endif
	input_sync(ts->input_dev);

#if defined (__SEND_VIRTUAL_RELEASED__)
	fingerInfo[2].x = fingerInfo[0].x;
	fingerInfo[2].y = fingerInfo[0].y;
	fingerInfo[2].z = fingerInfo[0].z;
	fingerInfo[2].id = fingerInfo[0].id;	

	if(pressed_num==1)
		prev_pressed_num=1;
	else
		prev_pressed_num=0;	
	
#endif	

	

work_func_out:
	
	tsp_irq_operation = 0;
	
	return IRQ_HANDLED;
}


int tsp_i2c_read(u8 reg, unsigned char *rbuf, int buf_size)
{
	int i, ret=-1;
	struct i2c_msg rmsg;
	uint8_t start_reg;

	for (i = 0; i < I2C_RETRY_CNT; i++)
	{
		rmsg.addr = ts_global->client->addr;
		rmsg.flags = 0;//I2C_M_WR;
		rmsg.len = 1;
		rmsg.buf = &start_reg;
		start_reg = reg;
		
		ret = i2c_transfer(ts_global->client->adapter, &rmsg, 1);

		if(ret >= 0) 
		{
			rmsg.flags = I2C_M_RD;
			rmsg.len = buf_size;
			rmsg.buf = rbuf;
			ret = i2c_transfer(ts_global->client->adapter, &rmsg, 1 );

			if (ret >= 0)
				break; // i2c success
		}

		if( i == (I2C_RETRY_CNT - 1) )
		{
			printk("[TSP] Error code : %d, %d\n", __LINE__, ret );
		}
	}

	return ret;
}



void set_tsp_for_ta_detect(int state)
{
	
	int i, ret=0;
	uint8_t buf1[2] = {0,};
	uint8_t temp;

	printk("[TSP] %s, %d\n", __func__, __LINE__ );

	if(tsp_status==0)
	{	
		if((tsp_testmode == 0) && (tsp_irq_operation == 0 && (now_tspfw_update == 0)))
	    {
			if(state)
			{
				printk("[TSP] [1] set_tsp_for_ta_detect!!! state=1\n");
		        
				for (i = 0; i < I2C_RETRY_CNT; i++)
				{
					buf1[0] = 0x01; //address
					ret = i2c_master_send(ts_global->client, buf1, 1);

					if (ret >= 0)
					{
						ret = i2c_master_recv(ts_global->client, buf1, 1);

						if (ret >= 0)
						{
							temp = buf1[0] | 0x04;//0b0000 0100

							buf1[0] = 0x01;//address
							buf1[1] = temp;//data
							ret = i2c_master_send(ts_global->client, buf1, 2);

							if (ret >= 0)
							{
								printk("[TSP] 01h = 0x%x\n", temp);
								break; // i2c success
							}
						}	
					}

					printk("[TSP] %s, %d, fail\n", __func__, __LINE__ );
				}

				pre_ta_stat = 1;
			}
			else
			{
				printk("[TSP] [2] set_tsp_for_ta_detect!!! state=0\n");
		        
				for (i = 0; i < I2C_RETRY_CNT; i++)
				{
					buf1[0] = 0x01; //address
					ret = i2c_master_send(ts_global->client, buf1, 1);

					if (ret >= 0)
					{
						ret = i2c_master_recv(ts_global->client, buf1, 1);

						if (ret >= 0)
						{
							temp = buf1[0] & 0xFB;//0b1111 1011

							buf1[0] = 0x01;//address
							buf1[1] = temp;//data
							ret = i2c_master_send(ts_global->client, buf1, 2);

							if (ret >= 0)
							{
								printk("[TSP] 01h = 0x%x\n", temp);
								break; // i2c success
							}

						}	
					}

					printk("[TSP] %s, %d, fail\n", __func__, __LINE__ );
				}
				
				pre_ta_stat = 0;
			}
		}
	}
}	
EXPORT_SYMBOL(set_tsp_for_ta_detect);

#if defined (TSP_EDS_RECOVERY)
static void check_ic_work_func(struct work_struct *esd_recovery_func)
{
	int ret=0;
	uint8_t buf_esd[1];
	uint8_t wdog_val[1];

	struct touch_data *ts = container_of(esd_recovery_func, struct touch_data, esd_recovery_func);

	buf_esd[0] = 0x1F;
	wdog_val[0] = 1;
	   
	if((tsp_testmode == 0) && (tsp_irq_operation == 0 && (now_tspfw_update == 0)))
	{
		ret = i2c_master_send(ts->client, buf_esd, 1);
		if(ret >=0)
		{
			ret = i2c_master_recv(ts->client, wdog_val, 1);
			
			if(((wdog_val[0] & 0xFC) >> 2) == (uint8_t)prev_wdog_val)
			{
				printk("[TSP] %s tsp_reset counter = %x, prev = %x\n", __func__, ((wdog_val[0] & 0xFC) >> 2), (uint8_t)prev_wdog_val);
				disable_irq(ts_global->client->irq);				
				tsp_reset();
				enable_irq(ts_global->client->irq);				
				prev_wdog_val = -1;
			}
			else
			{
				#if defined(__TOUCH_DEBUG__)
				printk("[TSP] %s, No problem of tsp driver \n", __func__);
				#endif			

				prev_wdog_val = (wdog_val[0] & 0xFC) >> 2;
			}			
			esd_conter=0;			
		}
		else//if(ret < 0)
		{
			if(esd_conter==1)
			{
				disable_irq(ts_global->client->irq);
			tsp_reset();
				enable_irq(ts_global->client->irq);				
				printk("[TSP]  %s : tsp_reset() done!\n",__func__);
				esd_conter=0;
			}
			else
			{
				esd_conter++;
#if defined(__TOUCH_DEBUG__)
				printk("[TSP]  %s : esd_conter [%d]\n",__func__, esd_conter);
#endif				
			}
		}

		if( pre_ta_stat != tsp_charger_type_status )
		{
			set_tsp_for_ta_detect(tsp_charger_type_status);
		}

	}
	else
	{
#if defined(__TOUCH_DEBUG__)
		printk("[TSP] %s cannot check ESD\n");
#endif	
	}
}

static enum hrtimer_restart watchdog_timer_func(struct hrtimer *timer)
{
	//printk("[TSP] %s, %d\n", __func__, __LINE__ );

	queue_work(check_ic_wq, &ts_global->esd_recovery_func);
	hrtimer_start(&ts_global->timer, ktime_set(3, 0), HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

#endif


int ts_check(void)
{
	int ret, i;
	uint8_t buf_tmp[3]={0,0,0};
	int retry = 3;


	ret = tsp_i2c_read( 0x1B, buf_tmp, sizeof(buf_tmp));

	// i2c read retry
	if(ret <= 0)
	{
		for(i=0; i<retry;i++)
		{
			ret=tsp_i2c_read( 0x1B, buf_tmp, sizeof(buf_tmp));

			if(ret > 0)
				break;
		}
	}

	if (ret <= 0) 
	{
		printk("[TSP][%s] %s\n", __func__,"Failed i2c");
		ret = 0;
	}
	else 
	{
		printk("[TSP][%s] %s\n", __func__,"Passed i2c");

		printk("[TSP][%s][SlaveAddress : 0x%x][VendorID : 0x%x] [HW : 0x%x] [SW : 0x%x]\n", __func__,ts_global->client->addr, buf_tmp[0], buf_tmp[1], buf_tmp[2]);

		if ( buf_tmp[0] == 0xf0 )//(ts->hw_rev == 0) && (ts->fw_ver == 2))
		{
			ret = 1;
			printk("[TSP][%s] %s\n", __func__,"Passed ts_check");
		}
		else
		{
			ret = 0;
			printk("[TSP][%s] %s\n", __func__,"Failed ts_check");
		}
		
	}

	return ret;
}


#ifdef SEC_TSP_FACTORY_TEST
static void set_cmd_result(struct touch_data *info, char *buff, int len)
{
	strncat(info->cmd_result, buff, len);
}


static int get_fw_version(struct touch_data *info)
{
	uint8_t i2c_addr = 0x1B;
	uint8_t buf_tmp[3] = {0};
	int phone_ver = 0;

	printk("[TSP] %s\n",__func__);

	tsp_i2c_read( i2c_addr, buf_tmp, sizeof(buf_tmp));

	touch_vendor_id = buf_tmp[0] & 0xF0;
	touch_hw_ver = buf_tmp[1];
	touch_sw_ver = buf_tmp[2];
	printk("[TSP] %s:%d, ver tsp=%x, HW=%x, SW=%x\n", __func__,__LINE__, touch_vendor_id, touch_hw_ver, touch_sw_ver);

	return touch_sw_ver;
	
}

static int get_hw_version(struct touch_data *info)
{
	return touch_hw_ver;
}


static ssize_t show_close_tsp_test(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return snprintf(buf, TSP_BUF_SIZE, "%u\n", 0);
}

static void set_default_result(struct touch_data *info)
{
	char delim = ':';

	memset(info->cmd_result, 0x00, ARRAY_SIZE(info->cmd_result));
	memcpy(info->cmd_result, info->cmd, strlen(info->cmd));
	strncat(info->cmd_result, &delim, 1);
}

static void not_support_cmd(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;
	char buff[16] = {0};

	set_default_result(info);
	sprintf(buff, "%s", "NA");
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 4;
	dev_info(&info->client->dev, "%s: \"%s(%d)\"\n", __func__,
				buff, strnlen(buff, sizeof(buff)));
	return;
}

static void fw_update(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	not_support_cmd(info);
}

static void run_raw_node_read(void *device_data) //item 1
{

	printk("[TSP] %s entered. line : %d, \n", __func__,__LINE__);

	struct touch_data *info = (struct touch_data *)device_data;
	
	int tma140_col_num = NODE_Y_NUM; //0 ~ 7
	int tma140_row_num = NODE_X_NUM;//0 ~ 9

	int  written_bytes = 0 ;	/* & error check */

	uint8_t buf1[2]={0,};
	uint8_t buf2[NODE_NUM]={0,};

	uint16_t ref1[NODE_NUM]={0,};
	uint16_t ref2[NODE_NUM]={0,};
	char buff[TSP_CMD_STR_LEN] = {0};
	u32 max_value = 0, min_value = 0;
	
	int i,j,k;
	int ret;

	uint8_t i2c_addr;

	uint16_t RAWDATA_MIN = 70;
	uint16_t RAWDATA_MAX = 130;
	uint16_t LIDAC_MIN = 1;
	uint16_t LIDAC_MAX = 30;

	uint8_t test_result = 1;

	tsp_testmode = 1;

	set_default_result(info);


	/////* Raw Value */////
	/////* Enter Raw Data Mode */////
	for (i = 0; i < I2C_RETRY_CNT; i++)
	{
		buf1[0] = 0x00;//address
		buf1[1] = 0x40;//value
		ret = i2c_master_send(info->client, buf1, 2);

		if (ret >= 0)
			break; // i2c success
	}
	msleep(10);
	for (i = 0; i < I2C_RETRY_CNT; i++)
	{
		buf1[0] = 0x00;//address
		buf1[1] = 0xC0;//value
		ret = i2c_master_send(info->client, buf1, 2);

		if (ret >= 0)
			break; // i2c success
	}
	msleep(50);
	
	/////* Read Raw Data */////
	i2c_addr = 0x07;
	tsp_i2c_read( i2c_addr, buf2, sizeof(buf2));

	printk("[TSP] Raw Value : ");
	for(i = 0 ; i < (tma140_col_num * tma140_row_num) ; i++)
	{
		if(i==0)
		{
			min_value=max_value=buf2[i];
			
		}
		else
		{
			max_value = max(max_value, buf2[i]);
			min_value = min(min_value, buf2[i]);
		}
		
		printk(" [%d]%3d", i, buf2[i]);
	}
	printk("\n");


	snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));

	/////* Exit Inspection Mode */////
	for (i = 0; i < I2C_RETRY_CNT; i++)
    {
		buf1[0] = 0x00;//address
		buf1[1] = 0x00;//value
		ret = i2c_master_send(info->client, buf1, 2);	//exit Inspection Mode
	
		if (ret >= 0)
			break; // i2c success
    }

	mdelay(100);

	tsp_testmode = 0;


	/////* Check Result */////
	for(i = 0 ; i < (tma140_col_num * tma140_row_num) ; i++)
	{
		if(ref1[i] < RAWDATA_MIN && ref1[i] > RAWDATA_MAX)
		{
			test_result = 0;
			break;
		}

		if(ref2[i] < LIDAC_MIN && ref2[i] > LIDAC_MAX)
		{
			test_result = 0;
			break;
		}
	}

	printk("[TSP] test_result = %d", test_result);



}


static void get_fw_ver_bin(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	char buff[16] = {0};
	int hw_rev;

	printk("[TSP] %s, %d\n", __func__, __LINE__ );


	set_default_result(info);
	hw_rev = get_hw_version(info);
	if (hw_rev == TSP_HW_REV03)
		snprintf(buff, sizeof(buff), "%#02x", TSP_SW_VER_FOR_HW03);
	else
		snprintf(buff, sizeof(buff), "%#02x", TSP_SW_VER_FOR_HW04);		

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_fw_ver_ic(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	char buff[16] = {0};
	int ver;

	uint8_t i2c_addr = 0x1B;
	uint8_t buf_tmp[3] = {0};

	printk("[TSP] %s, %d\n", __func__, __LINE__ );
	
	set_default_result(info);

	tsp_i2c_read( i2c_addr, buf_tmp, sizeof(buf_tmp));

	touch_vendor_id = buf_tmp[0] & 0xF0;
	touch_hw_ver = buf_tmp[1];
	touch_sw_ver = buf_tmp[2];
//	printk("[TSP] %s:%d, ver tsp=%x, HW=%x, SW=%x\n", __func__,__LINE__, touch_vendor_id, touch_hw_ver, touch_sw_ver);


//	ver = info->fw_ic_ver;
	ver = touch_sw_ver;
	snprintf(buff, sizeof(buff), "%#02x", ver);

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_threshold(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	printk("[TSP] %s entered. line : %d, \n", __func__,__LINE__);

	uint8_t buf1[2]={0,};
	uint8_t buf2[1]={0,};


	int threshold;

	char buff[TSP_CMD_STR_LEN] = {0};
	u32 max_value = 0, min_value = 0;
	
	int i,j,k;
	int ret;

	uint8_t i2c_addr;

	tsp_testmode = 1;


	/////* Enter System Information Mode */////
	for (i = 0; i < I2C_RETRY_CNT; i++)
	{
		buf1[0] = 0x00;//address
		buf1[1] = 0x10;//value
		ret = i2c_master_send(info->client, buf1, 2);

		if (ret >= 0)
			break; // i2c success
	}
	msleep(50);
	
	/////*  Read Threshold Value */////
	i2c_addr = 0x30;
	tsp_i2c_read( i2c_addr, buf2, sizeof(buf2));

	printk(" [TSP] %d", buf2[0]);

	snprintf(buff, sizeof(buff), "%d", buf2[0]);
	
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));

	/////* Exit System Information Mode */////
	for (i = 0; i < I2C_RETRY_CNT; i++)
    {
		buf1[0] = 0x00;//address
		buf1[1] = 0x00;//value
		ret = i2c_master_send(info->client, buf1, 2);	//exit Inspection Mode
	
		if (ret >= 0)
			break; // i2c success
    }

	msleep(100);

	tsp_testmode = 0;

}

static void run_global_idac_read(void *device_data)
{
	printk("[TSP] %s entered. line : %d, \n", __func__,__LINE__);

	struct touch_data *info = (struct touch_data *)device_data;

    set_default_result(info);
    
	int tma140_col_num = NODE_Y_NUM;
	int tma140_row_num = NODE_X_NUM;

	uint8_t buf1[2]={0,};
	uint8_t buf2[86]={0,}; // 0 ~ 6 , 7 ~ 86
	uint8_t odd_even_detect;

	int i,j,k;
	int ret;

	uint8_t i2c_addr;

	uint16_t RAWDATA_MIN = 70;
	uint16_t RAWDATA_MAX = 130;
	uint16_t LIDAC_MIN = 1;
	uint16_t LIDAC_MAX = 30;

	uint8_t test_result = 1;
	
	tsp_testmode = 1;

	/////* Global IDAC Value */////
	/////* Enter Local IDAC Data Mode */////

	for (i = 0; i < 10; i++)
	{
		for (j = 0; j < I2C_RETRY_CNT; j++)
	{
		buf1[0] = 0x00;//address
		buf1[1] = 0x60;//value
		ret = i2c_master_send(ts_global->client, buf1, 2);
	
		if (ret >= 0)
			break; // i2c success
	}
		msleep(400);

	/////* Read Global IDAC Data */////

		i2c_addr = 0x01;

	tsp_i2c_read( i2c_addr, buf2, sizeof(buf2));
		odd_even_detect=buf2[0]>>6;

    #if defined(__TOUCH_DEBUG__)
		for (j = 0; j < 86; j++)
			printk("%d ",buf2[j]);

		printk("\n");

		printk("Global IDAC odd_even_detect=%d , buf2[0]=%d\n", odd_even_detect, buf2[0]);
	#endif
		if(odd_even_detect%2)
			break;

		msleep(100);			
	}

	printk("[TSP] Global IDAC Value : ");
	for(i = GLOBAL_IDAC_START ; i < GLOBAL_IDAC_START+GLOBAL_IDAC_NUM ; i++)
	{
		j=i-GLOBAL_IDAC_START;
		global_idac[j] = buf2[i];
		printk(" %d", global_idac[j]);
	}
	printk("\n");


	/////* Exit Inspection Mode */////
	for (i = 0; i < I2C_RETRY_CNT; i++)
	{	
		buf1[0] = 0x00;//address
		buf1[1] = 0x00;//value
		ret = i2c_master_send(ts_global->client, buf1, 2);	//exit Inspection Mode
	
		if (ret >= 0)
			break; // i2c success
	}

	mdelay(100);

	tsp_testmode = 0;

    info->cmd_state = 2;

}

static void run_local_idac_read(void *device_data)
{
	printk("[TSP] %s entered. line : %d, \n", __func__,__LINE__);

	struct touch_data *info = (struct touch_data *)device_data;

    set_default_result(info);
    
	int tma140_col_num = NODE_Y_NUM;
	int tma140_row_num = NODE_X_NUM;

	uint8_t buf1[2]={0,};
//	uint8_t buf2[88]={0,};
	uint8_t buf2[86]={0,}; // 0 ~ 5 , 6 ~ 85

	int i,j,k;
	int ret;
	uint8_t odd_even_detect;

	uint8_t i2c_addr;

	uint16_t RAWDATA_MIN = 70;
	uint16_t RAWDATA_MAX = 130;
	uint16_t LIDAC_MIN = 1;
	uint16_t LIDAC_MAX = 30;

	uint8_t test_result = 1;
	
	tsp_testmode = 1;

	/////* Local IDAC Value */////
	/////* Enter Local IDAC Data Mode */////


	for (i = 0; i < 10; i++)
	{
	
		for (j = 0; j < I2C_RETRY_CNT; j++)
	{
		buf1[0] = 0x00;//address
		buf1[1] = 0x60;//value
		ret = i2c_master_send(ts_global->client, buf1, 2);
	
		if (ret >= 0)
			break; // i2c success
	}
		msleep(400);


	/////* Read Local IDAC Data */////

		i2c_addr = 0x01;

	tsp_i2c_read( i2c_addr, buf2, sizeof(buf2));
		odd_even_detect=buf2[0]>>6;

    #if defined(__TOUCH_DEBUG__)

		for (j = 0; j < 86; j++)
			printk("%d ", buf2[j]);

		printk("\n");

		printk("Local IDAC, odd_even_detect=%d , buf2[0]=%d\n ", odd_even_detect, buf2[0]);
	#endif

		if(odd_even_detect%2 == 0)
			break;

		msleep(100);
	}

	if(i==3 && odd_even_detect%2 )
		printk(" Error get Local IDAC");


	printk("[TSP] Local IDAC Value : ");
	for(i = LOCAL_IDAC_START; i < LOCAL_IDAC_START + (tma140_col_num * tma140_row_num) ; i++)
	{
		j=i-LOCAL_IDAC_START;
		local_idac[j] = buf2[i];
		printk(" %d", local_idac[j]);
	}
	printk("\n");


	/////* Exit Inspection Mode */////
	for (i = 0; i < I2C_RETRY_CNT; i++)
	{	
		buf1[0] = 0x00;//address
		buf1[1] = 0x00;//value
		ret = i2c_master_send(ts_global->client, buf1, 2);	//exit Inspection Mode
	
		if (ret >= 0)
			break; // i2c success
	}

	mdelay(100);

	tsp_testmode = 0;

	info->cmd_state = 2;

}

static void module_off_master(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	not_support_cmd(info);
}

static void module_on_master(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	not_support_cmd(info);
}

static void module_off_slave(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	not_support_cmd(info);
}

static void module_on_slave(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	not_support_cmd(info);
}

static void get_chip_vendor(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	char buff[16] = {0};

	printk("[TSP] %s, %d\n", __func__, __LINE__ );

	set_default_result(info);

	snprintf(buff, sizeof(buff), "%s", "CYPRESS");
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_chip_name(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	char buff[16] = {0};

	printk("[TSP] %s, %d\n", __func__, __LINE__ );

	set_default_result(info);

	snprintf(buff, sizeof(buff), "%s", "TMA140");
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static int check_rx_tx_num(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	char buff[TSP_CMD_STR_LEN] = {0};
	int node;

	if (info->cmd_param[0] < 0 ||
			info->cmd_param[0] >= NODE_X_NUM  ||
			info->cmd_param[1] < 0 ||
			info->cmd_param[1] >= NODE_Y_NUM) {
		snprintf(buff, sizeof(buff) , "%s", "NG");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = 3;

		dev_info(&info->client->dev, "%s: parameter error: %u,%u\n",
				__func__, info->cmd_param[0],
				info->cmd_param[1]);
		node = -1;
		return node;
             }
	//node = info->cmd_param[1] * NODE_Y_NUM + info->cmd_param[0];
	node = info->cmd_param[0] * NODE_Y_NUM + info->cmd_param[1];
	dev_info(&info->client->dev, "%s: node = %d\n", __func__,
			node);
	return node;

 }


static int global_value_check(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	char buff[TSP_CMD_STR_LEN] = {0};
	int value = -1;

	if (info->cmd_param[0] < 0 ||
			info->cmd_param[0] >= NODE_X_NUM  ||
			info->cmd_param[1] < 0 ||
			info->cmd_param[1] >= NODE_Y_NUM) {
		snprintf(buff, sizeof(buff) , "%s", "NG");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = 3;

		dev_info(&info->client->dev, "%s: parameter error: %u,%u\n",
				__func__, info->cmd_param[0],
				info->cmd_param[1]);

		return value;
    }
    
	value = info->cmd_param[0];
	dev_info(&info->client->dev, "%s: global value = %d\n", __func__,
			value);
	return value;

 }

 

static void get_reference(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	not_support_cmd(info);
}

static void get_raw_count(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	char buff[16] = {0};
	unsigned int val;
	int node;

	set_default_result(info);
	node = check_rx_tx_num(info);

	if (node < 0)
		return;
    
	val = raw_count[node];
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void get_difference(void *device_data) // item2
{
	struct touch_data *info = (struct touch_data *)device_data;

	char buff[16] = {0};
	unsigned int val;
	int node;

	set_default_result(info);
	node = check_rx_tx_num(info);

	if (node < 0)
		return;
    
	val = difference[node];
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void get_intensity(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	not_support_cmd(info);
}


static void get_local_idac(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	char buff[16] = {0};
	unsigned int val;
	int node;

	set_default_result(info);
	node = check_rx_tx_num(info);

	if (node < 0)
		return;
    
	val = local_idac[node];
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));

}


static void get_global_idac(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	char buff[16] = {0};
	unsigned int val;

	int x_channel_value=0;
	
#if 0
	int node=0;
	set_default_result(info);
	node = check_rx_tx_num(info);

	if (node < 0)
		return;
    
	val = global_idac[node];
#else
	set_default_result(info);

	x_channel_value=global_value_check(info);

#endif
	
	val = global_idac[x_channel_value];

	printk("global_idac[%d]=%d\n",x_channel_value, val );

	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));


}

static void get_x_num(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	char buff[16] = {0};
	int ver;

	printk("[TSP] %s, x channel=%d\n", __func__ , NODE_X_NUM);
	
	set_default_result(info);
    
	snprintf(buff, sizeof(buff), "%d", NODE_X_NUM);

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_y_num(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	char buff[16] = {0};
	int ver;

	printk("[TSP] %s, y channel=%d\n", __func__, NODE_Y_NUM);
	
	set_default_result(info);

	snprintf(buff, sizeof(buff), "%d", NODE_Y_NUM);

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void run_reference_read(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	not_support_cmd(info);

/*	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__); */
}

static void run_raw_count_read(void *device_data) //item 1
{
	printk("[TSP] %s entered. line : %d, \n", __func__,__LINE__);

	struct touch_data *info = (struct touch_data *)device_data;

	int tma140_col_num = NODE_Y_NUM; //0 ~ 7
	int tma140_row_num = NODE_X_NUM;//0 ~ 9

	uint8_t buf1[2]={0,};
	uint8_t buf2[NODE_NUM]={0,};

	uint16_t ref1[NODE_NUM]={0,};
	uint16_t ref2[NODE_NUM]={0,};
	char buff[TSP_CMD_STR_LEN] = {0};
	u32 max_value = 0, min_value = 0;
	
	int i,j,k;
	int ret;

	uint8_t i2c_addr;

	uint16_t RAWDATA_MIN = 70;
	uint16_t RAWDATA_MAX = 130;
	uint16_t LIDAC_MIN = 1;
	uint16_t LIDAC_MAX = 30;

	uint8_t test_result = 1;

	tsp_testmode = 1;

	set_default_result(info);


	/////* Raw Value */////
	/////* Enter Raw Data Mode */////
	for (i = 0; i < I2C_RETRY_CNT; i++)
	{
		buf1[0] = 0x00;//address
		buf1[1] = 0x40;//value
		ret = i2c_master_send(info->client, buf1, 2);

		if (ret >= 0)
			break; // i2c success
	}
	msleep(10);
	for (i = 0; i < I2C_RETRY_CNT; i++)
	{
		buf1[0] = 0x00;//address
		buf1[1] = 0xC0;//value
		ret = i2c_master_send(info->client, buf1, 2);

		if (ret >= 0)
			break; // i2c success
	}
	msleep(50);
	
	/////* Read Raw Data */////
	i2c_addr = 0x07;
	tsp_i2c_read( i2c_addr, buf2, sizeof(buf2));

	printk("[TSP] Raw Value : ");
	for(i = 0 ; i < (tma140_col_num * tma140_row_num) ; i++)
	{
		if(i==0)
		{
			min_value=max_value=buf2[i];
			
		}
		else
		{
			max_value = max(max_value, buf2[i]);
			min_value = min(min_value, buf2[i]);
		}

		raw_count[i] = buf2[i];
		printk(" [%d]%3d", i, buf2[i]);
	}
	printk("\n");


	snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));

	/////* Exit Inspection Mode */////
	for (i = 0; i < I2C_RETRY_CNT; i++)
    {
		buf1[0] = 0x00;//address
		buf1[1] = 0x00;//value
		ret = i2c_master_send(info->client, buf1, 2);	//exit Inspection Mode
	
		if (ret >= 0)
			break; // i2c success
    }

	mdelay(100);

	tsp_testmode = 0;


	/////* Check Result */////
	for(i = 0 ; i < (tma140_col_num * tma140_row_num) ; i++)
	{
		if(ref1[i] < RAWDATA_MIN && ref1[i] > RAWDATA_MAX)
		{
			test_result = 0;
			break;
		}

		if(ref2[i] < LIDAC_MIN && ref2[i] > LIDAC_MAX)
		{
			test_result = 0;
			break;
}
	}

	printk("[TSP] test_result = %d", test_result);

	info->cmd_state = 2;
    
}

static void run_difference_read(void *device_data) //item2
{
		printk("[TSP] %s entered. line : %d, \n", __func__,__LINE__);

	struct touch_data *info = (struct touch_data *)device_data;

		set_default_result(info);
		
		int tma140_col_num = NODE_Y_NUM; //0 ~ 7
		int tma140_row_num = NODE_X_NUM;//0 ~ 9
	
		uint8_t buf1[2]={0,};
		uint8_t buf2[NODE_NUM]={0,};
	
		uint16_t ref1[NODE_NUM]={0,};
		uint16_t ref2[NODE_NUM]={0,};
	
		int i,j,k;
		int ret;
	
		uint8_t i2c_addr;
		
		tsp_testmode = 1;
	
		/////* Difference Value */////
		/////* Enter Difference Data Mode */////
		for (i = 0; i < I2C_RETRY_CNT; i++)
		{
			buf1[0] = 0x00;//address
			buf1[1] = 0x50;//value
			ret = i2c_master_send(ts_global->client, buf1, 2);
		
			if (ret >= 0)
				break; // i2c success
		}
		msleep(10);
		for (i = 0; i < I2C_RETRY_CNT; i++)
		{
			buf1[0] = 0x00;//address
			buf1[1] = 0xD0;//value
			ret = i2c_master_send(ts_global->client, buf1, 2);
		
			if (ret >= 0)
				break; // i2c success
		}
		msleep(50);
		
		/////* Read Difference Data */////
		i2c_addr = 0x07;
		tsp_i2c_read( i2c_addr, buf2, sizeof(buf2));

        	for(i = 0 ; i < (tma140_col_num * tma140_row_num) ; i++)
        	{
             	difference[i] = buf2[i];
        	}
	
	
		/////* Exit Inspection Mode */////
		for (i = 0; i < I2C_RETRY_CNT; i++)
		{	
			buf1[0] = 0x00;//address
			buf1[1] = 0x00;//value
			ret = i2c_master_send(ts_global->client, buf1, 2);	//exit Inspection Mode
		
			if (ret >= 0)
				break; // i2c success
		}
		
		mdelay(100);
	
		tsp_testmode = 0;

		info->cmd_state = 2;

}

static void run_intensity_read(void *device_data)
{
	struct touch_data *info = (struct touch_data *)device_data;

	not_support_cmd(info);
}

static ssize_t store_cmd(struct device *dev, struct device_attribute
				  *devattr, const char *buf, size_t count)
{
	struct touch_data *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;

	char *cur, *start, *end;
	char buff[TSP_CMD_STR_LEN] = {0};
	int len, i;
	struct tsp_cmd *tsp_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;
	int ret;

	if (info->cmd_is_running == true) {
		dev_err(&info->client->dev, "tsp_cmd: other cmd is running.\n");
		goto err_out;
	}


	/* check lock  */
	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = true;
	mutex_unlock(&info->cmd_lock);

	info->cmd_state = 1;

	for (i = 0; i < ARRAY_SIZE(info->cmd_param); i++)
		info->cmd_param[i] = 0;

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;
	memset(info->cmd, 0x00, ARRAY_SIZE(info->cmd));
	memcpy(info->cmd, buf, len);

	cur = strchr(buf, (int)delim);
	if (cur)
		memcpy(buff, buf, cur - buf);
	else
		memcpy(buff, buf, len);

	/* find command */
	list_for_each_entry(tsp_cmd_ptr, &info->cmd_list_head, list) {
		if (!strcmp(buff, tsp_cmd_ptr->cmd_name)) {
			cmd_found = true;
			break;
		}
	}

	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(tsp_cmd_ptr, &info->cmd_list_head, list) {
			if (!strcmp("not_support_cmd", tsp_cmd_ptr->cmd_name))
				break;
		}
	}

	/* parsing parameters */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(buff, 0x00, ARRAY_SIZE(buff));
		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(buff, start, end - start);
				*(buff + strlen(buff)) = '\0';
				ret = kstrtoint(buff, 10,\
						info->cmd_param + param_cnt);
				start = cur + 1;
				memset(buff, 0x00, ARRAY_SIZE(buff));
				param_cnt++;
			}
			cur++;
		} while (cur - buf <= len);
	}

	dev_info(&client->dev, "cmd = %s\n", tsp_cmd_ptr->cmd_name);
	for (i = 0; i < param_cnt; i++)
		dev_info(&client->dev, "cmd param %d= %d\n", i,
							info->cmd_param[i]);

	tsp_cmd_ptr->cmd_func(info);


err_out:
	return count;
}

static ssize_t show_cmd_status(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct touch_data *info = dev_get_drvdata(dev);
	char buff[16] = {0};

	dev_info(&info->client->dev, "tsp cmd: status:%d\n",
			info->cmd_state);

	if (info->cmd_state == 0)
		snprintf(buff, sizeof(buff), "WAITING");

	else if (info->cmd_state == 1)
		snprintf(buff, sizeof(buff), "RUNNING");

	else if (info->cmd_state == 2)
		snprintf(buff, sizeof(buff), "OK");

	else if (info->cmd_state == 3)
		snprintf(buff, sizeof(buff), "FAIL");

	else if (info->cmd_state == 4)
		snprintf(buff, sizeof(buff), "NOT_APPLICABLE");

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", buff);
}

static ssize_t show_cmd_result(struct device *dev, struct device_attribute
				    *devattr, char *buf)
{
	struct touch_data *info = dev_get_drvdata(dev);

	dev_info(&info->client->dev, "tsp cmd: result: %s\n", info->cmd_result);

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	info->cmd_state = 0;

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", info->cmd_result);
}


static DEVICE_ATTR(close_tsp_test, S_IRUGO, show_close_tsp_test, NULL);
static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, store_cmd);
static DEVICE_ATTR(cmd_status, S_IRUGO, show_cmd_status, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, show_cmd_result, NULL);


static struct attribute *sec_touch_facotry_attributes[] = {
		&dev_attr_close_tsp_test.attr,
		&dev_attr_cmd.attr,
		&dev_attr_cmd_status.attr,
		&dev_attr_cmd_result.attr,
	NULL,
};

static struct attribute_group sec_touch_factory_attr_group = {
	.attrs = sec_touch_facotry_attributes,
};
#endif /* SEC_TSP_FACTORY_TEST */


extern struct class *sec_class;
static int ts_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	struct touch_data *ts;

	uint8_t i2c_addr = 0x1B;
  	uint8_t buf_tmp[3]={0,0,0};
	uint8_t addr[1];	
	int i;
    	int ret = 0, key = 0;

#ifdef SEC_TSP_FACTORY_TEST
	struct device *fac_dev_ts;
#endif


	printk("[TSP] %s, %d\n", __func__, __LINE__ );

//	touch_ctrl_regulator(TOUCH_ON);
//	msleep(100);	
	touch_ctrl_regulator(TOUCH_OFF);
	msleep(200);
	touch_ctrl_regulator(TOUCH_ON);
	msleep(100);

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}
	
#if defined (TSP_EDS_RECOVERY)
	INIT_WORK(&ts->esd_recovery_func, check_ic_work_func);
#endif

	ts->client = client;
	i2c_set_clientdata(client, ts);

	ts_global = ts;

	tsp_irq=client->irq;

	printk("[TSP] tsp_irq = %d\n",tsp_irq);


	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		printk(KERN_ERR "ts_probe: Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}

	ts->input_dev->name = "sec_touchscreen";


	ts->input_dev->keybit[BIT_WORD(KEY_POWER)] |= BIT_MASK(KEY_POWER);
	
	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	//set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(MT_TOOL_FINGER, ts->input_dev->keybit);	// for B type multi touch protocol
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit); /*must be added for ICS*/

	input_mt_init_slots(ts->input_dev, MAX_USING_FINGER_NUM); // for B type multi touch protocol

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, MAX_X, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, MAX_Y, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);


/*
	for(key = 0; key < MAX_KEYS ; key++)
		input_set_capability(ts->input_dev, EV_KEY, touchkey_keycodes[key]);

	// for TSK
	for(key = 0; key < MAX_KEYS ; key++)
		touchkey_status[key] = TK_STATUS_RELEASE;
*/
    
	/* ts->input_dev->name = ts->keypad_info->name; */
	ret = input_register_device(ts->input_dev);
	if (ret) {
		printk(KERN_ERR "ts_probe: Unable to register %s input device\n", ts->input_dev->name);
		goto err_input_register_device_failed;
	}

    	printk("[TSP] %s, irq=%d\n", __func__, client->irq );

    if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL, ts_work_func, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, client->name, ts);
		if (ret == 0)
			ts->use_irq = 1;
		else
			dev_err(&client->dev, "request_irq failed\n");
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = ts_early_suspend;
	ts->early_suspend.resume = ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif
	printk(KERN_INFO "ts_probe: Start touchscreen %s in %s mode\n", ts->input_dev->name, ts->use_irq ? "interrupt" : "polling");

	/* sys fs */

	sec_touchscreen = device_create(sec_class, NULL, 0, ts, "sec_touchscreen");
	if (IS_ERR(sec_touchscreen)) 
	{
		dev_err(&client->dev,"Failed to create device for the sysfs1\n");
		ret = -ENODEV;
	}

	if (device_create_file(sec_touchscreen, &dev_attr_tsp_firm_version_phone) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_tsp_firm_version_phone.attr.name);
	if (device_create_file(sec_touchscreen, &dev_attr_tsp_firm_version_panel) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_tsp_firm_version_panel.attr.name);
	if (device_create_file(sec_touchscreen, &dev_attr_tsp_firm_update) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_tsp_firm_update.attr.name);
	if (device_create_file(sec_touchscreen, &dev_attr_tsp_firm_update_status) < 0)
		pr_err("[TSP] Failed to create device file(%s)!\n", dev_attr_tsp_firm_update_status.attr.name);	
	if (device_create_file(sec_touchscreen, &dev_attr_tsp_threshold) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_tsp_threshold.attr.name);
	if (device_create_file(sec_touchscreen, &dev_attr_get_panel_rev) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_get_panel_rev.attr.name);

#ifdef SEC_TSP_FACTORY_TEST
		INIT_LIST_HEAD(&ts->cmd_list_head);
		for (i = 0; i < ARRAY_SIZE(tsp_cmds); i++)
			list_add_tail(&tsp_cmds[i].list, &ts->cmd_list_head);

		mutex_init(&ts->cmd_lock);
		ts->cmd_is_running = false;

	fac_dev_ts = device_create(sec_class, NULL, 0, ts, "tsp");
	if (IS_ERR(fac_dev_ts))
		dev_err(&client->dev, "Failed to create device for the sysfs\n");

	ret = sysfs_create_group(&fac_dev_ts->kobj,	&sec_touch_factory_attr_group);
	if (ret)
		dev_err(&client->dev, "Failed to create sysfs group\n");
#endif

		/* Check point - i2c check - start */	
		for (i = 0; i < 2; i++)
		{
			printk("[TSP] %s, %d, send\n", __func__, __LINE__ );
			addr[0] = 0x1B; //address
			ret = i2c_master_send(ts_global->client, addr, 1);
	
			if (ret >= 0)
			{
				printk("[TSP] %s, %d, receive\n", __func__, __LINE__ );
				ret = i2c_master_recv(ts_global->client, buf_tmp, 3);
				if (ret >= 0)
					break; // i2c success
			}
	
			printk("[TSP] %s, %d, fail\n", __func__, __LINE__ );
		}
	
		if(ret >= 0)
		{
		touch_vendor_id = buf_tmp[0]&0xF0;
		touch_hw_ver = buf_tmp[1];
		touch_sw_ver = buf_tmp[2];
		printk("[TSP] %s:%d, ver tsp=%x, HW=%x, SW=%x\n", __func__,__LINE__, touch_vendor_id, touch_hw_ver, touch_sw_ver);

		ts->fw_ic_ver = touch_sw_ver;
#if 0
		if(touch_hw_ver == TSP_HW_REV04)
		{		
			if(TSP_SW_VER_FOR_HW04 > touch_sw_ver)
			{
				firm_update_callfc();
				printk("[TSP] FW update done!");
				
			}
		}
		else if(touch_hw_ver == TSP_HW_REV03)
		{
			if(TSP_SW_VER_FOR_HW03 > touch_sw_ver)
	        	{
			firm_update_callfc();
			printk("[TSP] FW update done!");
			}
			
		}
#endif

			touch_present = 1;
#if defined (TSP_EDS_RECOVERY)
        	hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        	ts->timer.function = watchdog_timer_func;
			hrtimer_start(&ts->timer, ktime_set(3, 0), HRTIMER_MODE_REL);
#endif			
		}
		else//if(ret < 0) 
		{
			printk(KERN_ERR "i2c_transfer failed\n");
			printk("[TSP] %s, ln:%d, Failed to register TSP!!!\n\tcheck the i2c line!!!, ret=%d\n", __func__,__LINE__, ret);
	 		//goto err_check_functionality_failed;
		}
		/* Check point - i2c check - end */


	return 0;

err_input_register_device_failed:
	input_free_device(ts->input_dev);

err_input_dev_alloc_failed:
	kfree(ts);
err_alloc_data_failed:
err_check_functionality_failed:
	return ret;
}

static int ts_remove(struct i2c_client *client)
{
	struct touch_data *ts = i2c_get_clientdata(client);
	unregister_early_suspend(&ts->early_suspend);
	if (ts->use_irq)
		free_irq(client->irq, ts);
	input_unregister_device(ts->input_dev);
	kfree(ts);
	return 0;
}

static int ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct touch_data *ts = i2c_get_clientdata(client);

	printk("[TSP] %s+\n", __func__ );

	  tsp_status=1; 
	
	if (ts->use_irq)
	{
		disable_irq(client->irq);
	}

#if defined (TSP_EDS_RECOVERY)
	if(touch_present == 1)
		hrtimer_cancel(&ts->timer);
#endif		
	
	gpio_direction_output( TSP_INT , 0 );
	gpio_direction_output( TSP_SCL , 0 ); 
	gpio_direction_output( TSP_SDA , 0 ); 

	

	msleep(20);	
	
	touch_ctrl_regulator(TOUCH_OFF);
    printk("[TSP] %s-\n", __func__ );
        
	return 0;
}

static int ts_resume(struct i2c_client *client)
{
	int ret;
#if defined (TSP_EDS_RECOVERY)
	struct touch_data *ts = i2c_get_clientdata(client);
#endif	
   	uint8_t i2c_addr = 0x1B;
	uint8_t buf[3];


	gpio_direction_output( TSP_SCL , 1 ); 
	gpio_direction_output( TSP_SDA , 1 ); 

	gpio_direction_input(TSP_INT);


	touch_ctrl_regulator(TOUCH_ON);
    
	msleep(100);
		
	ret = tsp_i2c_read( i2c_addr, buf, sizeof(buf));

	buf[0] = buf[0]&0xF0;
	
	printk("[TSP] %s: ver tsp=%x, HW=%x, SW=%x\n", __func__, buf[0], buf[1], buf[2]);
	printk("[TSP] %s: ver tsp=%x, HW=%x, SW=%x\n", __func__, touch_vendor_id, touch_hw_ver, touch_sw_ver);
	
	enable_irq(client->irq);

	if(tsp_charger_type_status == 1)
	{
		set_tsp_for_ta_detect(tsp_charger_type_status);
	}

#if defined (TSP_EDS_RECOVERY)

	prev_wdog_val = -1;

	if(touch_present == 1)
		hrtimer_start(&ts->timer, ktime_set(3, 0), HRTIMER_MODE_REL);
#endif	

  tsp_status=0; 

	printk("[TSP] %s-\n", __func__ );
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ts_early_suspend(struct early_suspend *h)
{
	struct touch_data *ts;
	ts = container_of(h, struct touch_data, early_suspend);
	ts_suspend(ts->client, PMSG_SUSPEND);
}

static void ts_late_resume(struct early_suspend *h)
{
	struct touch_data *ts;
	ts = container_of(h, struct touch_data, early_suspend);
	ts_resume(ts->client);
}
#endif

static const struct i2c_device_id ts_id[] = {
	{ "cypress-tma140", 0 },
	{ }
};

static struct i2c_driver ts_driver = {
	.probe		= ts_probe,
	.remove		= ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= ts_suspend,
	.resume		= ts_resume,
#endif
	.id_table	= ts_id,
	.driver = {
		.name	= "cypress-tma140",
	},
};

static int __devinit tsp_driver_init(void)
{

	printk("[TSP] %s\n", __func__ );

	gpio_request(TSP_INT, "ts_irq");
	gpio_direction_input(TSP_INT);

	irq_set_irq_type(gpio_to_irq(TSP_INT), IRQ_TYPE_EDGE_FALLING);


	gpio_direction_output( TSP_SCL , 1 ); 
	gpio_direction_output( TSP_SDA , 1 ); 		
 


#if defined (TSP_EDS_RECOVERY)
	check_ic_wq = create_singlethread_workqueue("check_ic_wq");	

	if (!check_ic_wq)
		return -ENOMEM;
#endif

#if defined (__TOUCH_KEYLED__)
	touchkeyled_regulator = regulator_get(NULL,"touch_keyled");
#endif
	return i2c_add_driver(&ts_driver);
}

static void __exit tsp_driver_exit(void)
{
	if (touch_regulator) 
	{
       	 regulator_put(touch_regulator);
		 touch_regulator = NULL;
    	}
#if defined (__TOUCH_KEYLED__)
	if (touchkeyled_regulator) 
	{
       	 regulator_put(touchkeyled_regulator);
		 touchkeyled_regulator = NULL;
    	}
#endif
	i2c_del_driver(&ts_driver);

	
#if defined (TSP_EDS_RECOVERY)
	if (check_ic_wq)
		destroy_workqueue(check_ic_wq);	
#endif

}

static ssize_t read_threshold(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t buf1[2]={0,};
	uint8_t buf2[1]={0,};

	int threshold;

	char buff[TSP_CMD_STR_LEN] = {0};
	u32 max_value = 0, min_value = 0;
	
	int i,j,k;
	int ret;

	uint8_t i2c_addr;

	tsp_testmode = 1;

	/////* Enter System Information Mode */////
	for (i = 0; i < I2C_RETRY_CNT; i++)
	{
		buf1[0] = 0x00;//address
		buf1[1] = 0x10;//value
		ret = i2c_master_send(ts_global->client, buf1, 2);

		if (ret >= 0)
			break; // i2c success
	}
	msleep(50);
	
	/////*  Read Threshold Value */////
	i2c_addr = 0x30;
	tsp_i2c_read( i2c_addr, buf2, sizeof(buf2));

	printk(" [TSP] %d", buf2[0]);

	/////* Exit System Information Mode */////
	for (i = 0; i < I2C_RETRY_CNT; i++)
    {
		buf1[0] = 0x00;//address
		buf1[1] = 0x00;//value
		ret = i2c_master_send(ts_global->client, buf1, 2);	//exit Inspection Mode
	
		if (ret >= 0)
			break; // i2c success
    }

	msleep(100);

	tsp_testmode = 0;

	return sprintf(buf, "%d\n",  buf2[0]);
}


static ssize_t firmware_In_Binary(struct device *dev, struct device_attribute *attr, char *buf)
{

	int phone_ver=0;

	if( touch_hw_ver == TSP_HW_REV04 )
		phone_ver = TSP_SW_VER_FOR_HW04;
	else if(touch_hw_ver == TSP_HW_REV03 )
		phone_ver = TSP_SW_VER_FOR_HW03;

	return sprintf(buf, "%d\n", phone_ver );
}


static ssize_t firmware_In_TSP(struct device *dev, struct device_attribute *attr, char *buf)
{

	uint8_t i2c_addr = 0x1B;
	uint8_t buf_tmp[3] = {0};
	int phone_ver = 0;

	printk("[TSP] %s\n",__func__);

	tsp_i2c_read( i2c_addr, buf_tmp, sizeof(buf_tmp));

	touch_vendor_id = buf_tmp[0] & 0xF0;
	touch_hw_ver = buf_tmp[1];
	touch_sw_ver = buf_tmp[2];
	printk("[TSP] %s:%d, ver tsp=%x, HW=%x, SW=%x\n", __func__,__LINE__, touch_vendor_id, touch_hw_ver, touch_sw_ver);

	return sprintf(buf, "%d\n", touch_sw_ver );
}


static ssize_t tsp_panel_rev(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t i2c_addr = 0x1B;
	uint8_t buf_tmp[3] = {0};
	int phone_ver = 0;

	printk("[TSP] %s\n",__func__);

	tsp_i2c_read( i2c_addr, buf_tmp, sizeof(buf_tmp));

	touch_vendor_id = buf_tmp[0] & 0xF0;
	touch_hw_ver = buf_tmp[1];
	touch_sw_ver = buf_tmp[2];
	printk("[TSP] %s:%d, ver tsp=%x, HW=%x, SW=%x\n", __func__,__LINE__, touch_vendor_id, touch_hw_ver, touch_sw_ver);

	return sprintf(buf, "%d\n", touch_hw_ver );
}


#define TMA140_RET_SUCCESS 0x00
int sv_tch_firmware_update = 0;


static void firm_update_callfc(void)
		{
	uint8_t update_num;

	disable_irq(tsp_irq);
	local_irq_disable();
	
	for(update_num = 1; update_num <= 5 ; update_num++)
	{
		sv_tch_firmware_update = cypress_update(touch_hw_ver);

		if(sv_tch_firmware_update == TMA140_RET_SUCCESS)
		{
			firmware_ret_val = 1; //SUCCESS
			sprintf(IsfwUpdate,"%s\n",FW_DOWNLOAD_COMPLETE);
			printk( "[TSP] %s, %d : firmware update SUCCESS !!\n", __func__, __LINE__);
			break;
		}
		else
		{
			printk( "[TSP] %s, %d : firmware update RETRY !!\n", __func__, __LINE__);
			if(update_num == 5)
			{
				firmware_ret_val = -1; //FAIL
			sprintf(IsfwUpdate,"%s\n",FW_DOWNLOAD_FAIL);				
			printk( "[TSP] %s, %d : firmware update FAIL !!\n", __func__, __LINE__);
			}
		}
	}

	printk("[TSP] enable_irq : %d\n", __LINE__ );
	local_irq_enable();
	enable_irq(tsp_irq);

}


static ssize_t firm_update(struct device *dev, struct device_attribute *attr, char *buf)
{

	printk("[TSP] %s!\n", __func__);

	sprintf(IsfwUpdate,"%s\n",FW_DOWNLOADING);

	now_tspfw_update = 1;	
	if(touch_hw_ver == TSP_HW_REV04)
	{		
		if(TSP_SW_VER_FOR_HW04 > touch_sw_ver)
		{
                	firm_update_callfc();
			printk("[TSP] 04 panel FW update done!");
			
		}
		else
		{
			printk("[TSP] Don't need to update FW");
			firmware_ret_val = 1; //SUCCESS
			sprintf(IsfwUpdate,"%s\n",FW_DOWNLOAD_COMPLETE);
		}

		
	}
	else if(touch_hw_ver == TSP_HW_REV03)
	{
		printk("[TSP] 03 panel, don't need to update FW");
		firmware_ret_val = 1; //SUCCESS
		sprintf(IsfwUpdate,"%s\n",FW_DOWNLOAD_COMPLETE);
	}
	now_tspfw_update = 0;	

	return sprintf(buf, "%d", firmware_ret_val );
}


static ssize_t firmware_update_status(struct device *dev, struct device_attribute *attr, char *buf)
		{
	printk("[TSP] %s\n",__func__);

	return sprintf(buf, "%s\n", IsfwUpdate);
		}


module_init(tsp_driver_init);
module_exit(tsp_driver_exit);

MODULE_AUTHOR("Cypress");
MODULE_DESCRIPTION("TMA140 Touchscreen Driver");
MODULE_LICENSE("GPL");

/*
 * linux/drivers/video/backlight/s2c_bl.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*******************************************************************************
* Copyright 2010 Broadcom Corporation.  All rights reserved.
*
* 	@file	drivers/video/backlight/s2c_bl.c
*
* Unless you and Broadcom execute a separate written software license agreement
* governing use of this software, this software is licensed to you under the
* terms of the GNU General Public License version 2, available at
* http://www.gnu.org/copyleft/gpl.html (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a license
* other than the GPL, without Broadcom's express prior written consent.
*******************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/ktd259b_bl.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/broadcom/lcd.h>
#include <linux/spinlock.h>
#include <linux/broadcom/PowerManager.h>
#include <linux/rtc.h>


int current_intensity;
static int backlight_pin = 95;

static DEFINE_SPINLOCK(bl_ctrl_lock);
int real_level = 1;
EXPORT_SYMBOL(real_level);

#ifdef CONFIG_HAS_EARLYSUSPEND
/* early suspend support */
//extern int gLcdfbEarlySuspendStopDraw;
#endif
static int backlight_mode=1;

#define PANEL_AUO_HX8357D	0x5b4c91
#define PANEL_BOE_ILI9486	0x5BBCD0
extern  unsigned long panelID;

#define MAX_BRIGHTNESS_IN_BLU	33

#define DIMMING_VALUE		32

#define MAX_BRIGHTNESS_VALUE	255
#define MIN_BRIGHTNESS_VALUE	20
#define BACKLIGHT_DEBUG 1
#define BACKLIGHT_SUSPEND 0
#define BACKLIGHT_RESUME 1

#if BACKLIGHT_DEBUG
#define BLDBG(fmt, args...) printk(fmt, ## args)
#else
#define BLDBG(fmt, args...)
#endif

struct ktd259b_bl_data {
	struct platform_device *pdev;
	unsigned int ctrl_pin;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend_desc;
#endif
};

struct brt_value{
	int level;				// Platform setting values
	int tune_level;			// Chip Setting values
};

#if defined (CONFIG_MACH_RHEA_SS_LUCAS)

struct brt_value brt_table_ktd[] = {
   { MIN_BRIGHTNESS_VALUE,  32 }, // Min pulse 32
   { 32,  31 },
   { 46,  30 },
   { 60,  29 },  
   { 73,  28 }, 
   { 86,  27 }, 
   { 98,  26 }, 
   { 105,  25 }, 
   { 110,  24 }, 
   { 115,  23 }, 
   { 120,  22 }, 
   { 125,  21 }, 
   { 130,  20 }, 
   { 140,  19 },//default value  
   { 155,  18 },   
   { 165,  17 },
   { 176,  16 }, 
   { 191,  15 }, 
   { 207,  14 },
   { 214,  13 },
   { 221,  12 },
   { 228,  10 },
   { 235,  8 },
   { 242,  7 },
   { 249,  5 },
   { MAX_BRIGHTNESS_VALUE,  3 }, // Max pulse 1
};

#elif defined(CONFIG_LCD_ILI9341_SUPPORT)

struct brt_value brt_table_ktd[] = {

{ MIN_BRIGHTNESS_VALUE,  32 }, // Min pulse 32
   { 30,  31 },
   { 40,  30 },
   { 50,  30 },  
   { 60,  29 }, 
   { 63,  28 }, 
   { 65,  28 }, 
   { 70,  27 }, 
   { 80,  26 }, 
   { 90,  26 }, 
   { 105,  25 }, 
   { 120,  24 }, 
   { 135,  23 }, 
   { 150,  22 }, 
   { 165,  21 },  
   { 180,  18 }, 
   { 195,  17 },
   { 210,  15 },
   { 225,  15 },
   { 240,  14 }, 
   { MAX_BRIGHTNESS_VALUE,  13 },


};

#elif defined(CONFIG_LCD_ILI9341_C_SUPPORT)

struct brt_value brt_table_ktd[] = {


{ MIN_BRIGHTNESS_VALUE,  32 }, // Min pulse 32 
   { 32,  31 },   
   { 43,  30 },   
   { 55,  29 },     
   { 66,  28 },    
   { 77,  27 },    
   { 88,  26 },    
   { 100,  25 },    
   { 111,  24 },    
   { 122,  23 },    
   { 134,  22 },    
   { 145,  21 },//default value     
   { 153,  20 },   
   { 162,  19 },    
   { 170,  18 },    
   { 179,  17 },     
   { 187,  16 },       
   { 195,  15 },   
   { 204,  14 },   
   { 212,  13 },   
   { 221,  12 },    
   { 229,  11 },    
   { 237,  10 },    
   { 246,  9 },    
   { MAX_BRIGHTNESS_VALUE,  8 }, 

};
#elif defined(CONFIG_BACKLIGHT_KTD_CORSICA)||defined(CONFIG_BACKLIGHT_KTD_NEVISW)

struct brt_value brt_table_ktd[] = {
   { MIN_BRIGHTNESS_VALUE,  32 }, // Min pulse 32
   { 32,  31 },
   { 46,  30 }, 
   { 60,  29 }, 
   { 74,  28 }, 
   { 88,  27 }, 
   { 102,  26 }, 
   { 116,  25 }, 
   { 130,  24 },
   { 136,  23 },
   { 142,  22 },
   { 148,  21 },
   { 154,  19 },
   { 160,  17 }, //default value  
   { 166,  16 },
   { 172,  15 },
   { 178,  14 }, 
   { 184,  13 }, 
   { 190,  12 }, 
   { 196,  11 }, 
   { 203,  10 }, 
   { 210,  8}, 
   { 217,  7 }, 
   { 224,  6 },
   { 231,  5 },
   { 238,  4 },
   { 245,  3 },
   { MAX_BRIGHTNESS_VALUE,  2 }, 
};

struct brt_value brt_table_ktd_auo[] = {
   { MIN_BRIGHTNESS_VALUE,  32 }, // Min pulse 32
   { 32,  31 },
   { 46,  30 }, 
   { 60,  29 }, 
   { 74,  28 }, 
   { 88,  27 }, 
   { 102,  26 }, 
   { 116,  25 }, 
   { 130,  24 },
   { 136,  23 },
   { 142,  22 },
   { 148,  21 },
   { 154,  20 },
   { 160,  19 }, //default value  
   { 166,  18 },
   { 172,  17 },
   { 178,  16 }, 
   { 184,  15 }, 
   { 190,  14 }, 
   { 196,  13 }, 
   { 203,  12 }, 
   { 210,  11}, 
   { 217,  10 }, 
   { 224,  9 },
   { 231,  8 },
   { 238,  7},
   { 245,  6 },
   { MAX_BRIGHTNESS_VALUE,  5 }, 
};


#elif defined(CONFIG_BACKLIGHT_KTD_CORSICASS)

struct brt_value brt_table_ktd[] = {
   { MIN_BRIGHTNESS_VALUE,  32 }, // Min pulse 32
   { 32,  31 },
   { 46,  30 }, 
   { 60,  29 }, 
   { 74,  28 }, 
   { 88,  27 }, 
   { 102,  26 }, 
   { 116,  25 }, 
   { 130,  24 },
   { 136,  23 },
   { 142,  22 },
   { 148,  21 },
   { 154,  20 },
   { 160,  19 }, //default value  
   { 166,  18 },
   { 172,  18 },
   { 178,  17 }, 
   { 184,  16 }, 
   { 190,  15 }, 
   { 196,  14 }, 
   { 203,  14 }, 
   { 210,  13 }, 
   { 217,  12 }, 
   { 224,  12 },
   { 231,  11 },
   { 238,  10 },
   { 245,  10 },
   { MAX_BRIGHTNESS_VALUE,  9 }, 
};

#elif defined(CONFIG_BACKLIGHT_NEVIS)
struct brt_value brt_table_ktd[] = {
   { MIN_BRIGHTNESS_VALUE,  32 }, // Min pulse 32
   { 31,  31 },
   { 42,  30 }, 
   { 53,  29 }, 
   { 64,  28 }, 
   { 76,  27 }, 
   { 88,  26 }, 
   { 100,  25 }, 
   { 112,  24 },
   { 124, 23 },
   { 136,  22 },
   { 148,  21 },
   { 160,  20 },//default value         
   { 166,  19 }, 
   { 172,  18 },     
   { 178,  17 },
   { 184,  16 }, 
   { 190,  15 }, 
   { 196,  14 }, 
   { 202,  13 }, 
   { 208,  12 }, 
   { 214,  11 }, 
   { 220,  10 }, 
   { 226,  9 },
   { 232,  8 },
   { 238,  7 },
   { 244,  6 },
   { 250,  5 }, 
   { MAX_BRIGHTNESS_VALUE,  4 }, 
};

#else
struct brt_value brt_table_ktd[] = {
   { MIN_BRIGHTNESS_VALUE,  32 }, // Min pulse 32
   { 31,  31 },
   { 42,  30 }, 
   { 53,  29 }, 
   { 64,  28 }, 
   { 76,  27 }, 
   { 88,  26 }, 
   { 100,  25 }, 
   { 112,  24 },
   { 124, 23 },
   { 136,  22 },
   { 148,  21 },
   { 160,  20 },//default value         
   { 166,  19 }, 
   { 172,  18 },     
   { 178,  17 },
   { 184,  16 }, 
   { 190,  15 }, 
   { 196,  14 }, 
   { 202,  13 }, 
   { 208,  12 }, 
   { 214,  11 }, 
   { 220,  10 }, 
   { 226,  9 },
   { 232,  8 },
   { 238,  7 },
   { 244,  6 },
   { 250,  5 }, 
   { MAX_BRIGHTNESS_VALUE,  4 }, 
};
#endif

#define MAX_BRT_STAGE_KTD (int)(sizeof(brt_table_ktd)/sizeof(struct brt_value))


extern int lcd_pm_update(PM_CompPowerLevel compPowerLevel, PM_PowerLevel sysPowerLevel);


struct mutex ktd259b_mutex;
DEFINE_MUTEX(ktd259b_mutex);

static void lcd_backlight_control(int num)
{
    int limit;
    
    limit = num;

   spin_lock(&bl_ctrl_lock);
    for(;limit>0;limit--)
    {
       udelay(2);
       gpio_set_value(backlight_pin,0);
       udelay(2); 
       gpio_set_value(backlight_pin,1);
    }
   spin_unlock(&bl_ctrl_lock);

}

/* input: intensity in percentage 0% - 100% */
static int ktd259b_backlight_update_status(struct backlight_device *bd)
{
	int user_intensity = bd->props.brightness;
    	int tune_level = 0;
	int pulse;
      int i;

        BLDBG("[BACKLIGHT] ktd259b_backlight_update_status ==> user_intensity  : %d\n", user_intensity);
    mutex_lock(&ktd259b_mutex);
#if defined(CONFIG_BACKLIGHT_KTD_CORSICA)||defined(CONFIG_BACKLIGHT_KTD_NEVISW)	
if(backlight_mode==BACKLIGHT_RESUME){

	  if(user_intensity > 0) {
	  if(user_intensity < MIN_BRIGHTNESS_VALUE) {
		  tune_level = DIMMING_VALUE; //DIMMING
	  } else if (user_intensity == MAX_BRIGHTNESS_VALUE) {
	      if(panelID == PANEL_BOE_ILI9486){
		  tune_level = brt_table_ktd[MAX_BRT_STAGE_KTD-1].tune_level;
		  }else {
		  tune_level = brt_table_ktd_auo[MAX_BRT_STAGE_KTD-1].tune_level;
		  	}
	  } else {
		  for(i = 0; i < MAX_BRT_STAGE_KTD; i++) {
		  	if(panelID == PANEL_BOE_ILI9486){
			  if(user_intensity <= brt_table_ktd[i].level ) {
				  tune_level = brt_table_ktd[i].tune_level;
				  break;
			  }
			 } else {
			 if(user_intensity <= brt_table_ktd_auo[i].level ) {
				  tune_level = brt_table_ktd_auo[i].tune_level;
				  break;
			  }
			 }
		  }
	  }
  }

#else
      if(backlight_mode==BACKLIGHT_RESUME){

    		if(user_intensity > 0) {
			if(user_intensity < MIN_BRIGHTNESS_VALUE) {
				tune_level = DIMMING_VALUE; //DIMMING
			} else if (user_intensity == MAX_BRIGHTNESS_VALUE) {
				tune_level = brt_table_ktd[MAX_BRT_STAGE_KTD-1].tune_level;
			} else {
				for(i = 0; i < MAX_BRT_STAGE_KTD; i++) {
					if(user_intensity <= brt_table_ktd[i].level ) {
						tune_level = brt_table_ktd[i].tune_level;
						break;
					}
				}
			}
		}
#endif

        BLDBG("[BACKLIGHT] ktd259b_backlight_update_status ==> tune_level : %d\n", tune_level);

    if (real_level==tune_level)
    {
        mutex_unlock(&ktd259b_mutex);
        return 0;
	}
    else
    {
/*	    if(real_level == 0)
	    {
	mdelay(200);
    }
*/   
	    if(tune_level<=0)
	    {
                gpio_set_value(backlight_pin,0);
                mdelay(3); 

	    }
	    else
	    {
    		if( real_level<=tune_level)
    		{
    			pulse = tune_level - real_level;
    		}
		else
		{
			pulse = 32 - (real_level - tune_level);
		}

    		//pulse = MAX_BRIGHTNESS_IN_BLU -tune_level;
            if (pulse==0)
            {
                mutex_unlock(&ktd259b_mutex);
                return 0;
            }

            lcd_backlight_control(pulse); 
    }

    real_level = tune_level;
    }
}
    mutex_unlock(&ktd259b_mutex);	  
       return 0;
}

static int ktd259b_backlight_get_brightness(struct backlight_device *bl)
{
        BLDBG("[BACKLIGHT] ktd259b_backlight_get_brightness\n");
    
	return current_intensity;
}

static struct backlight_ops ktd259b_backlight_ops = {
	.update_status	= ktd259b_backlight_update_status,
	.get_brightness	= ktd259b_backlight_get_brightness,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ktd259b_backlight_earlysuspend(struct early_suspend *desc)
{
    struct timespec ts;
    struct rtc_time tm;
 
    backlight_mode=BACKLIGHT_SUSPEND; 
     mutex_lock(&ktd259b_mutex);	
       gpio_set_value(backlight_pin,0);
       mdelay(3); 	
	real_level=0;
    mutex_unlock(&ktd259b_mutex);	
	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);

        printk("[%02d:%02d:%02d.%03lu][BACKLIGHT] earlysuspend\n", tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
}

static void ktd259b_backlight_earlyresume(struct early_suspend *desc)
{
	struct ktd259b_bl_data *ktd259b = container_of(desc, struct ktd259b_bl_data,
				early_suspend_desc);
	struct backlight_device *bl = platform_get_drvdata(ktd259b->pdev);
      struct timespec ts;
      struct rtc_time tm;
    
	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
       backlight_mode=BACKLIGHT_RESUME;
       printk("[%02d:%02d:%02d.%03lu][BACKLIGHT] earlyresume\n", tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
       msleep(100); 	
       backlight_update_status(bl);
}
#else
#ifdef CONFIG_PM
static int ktd259b_backlight_suspend(struct platform_device *pdev,
					pm_message_t state)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct ktd259b_bl_data *ktd259b = dev_get_drvdata(&bl->dev);
    
        BLDBG("[BACKLIGHT] ktd259b_backlight_suspend\n");
        
	return 0;
}

static int ktd259b_backlight_resume(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);

        BLDBG("[BACKLIGHT] ktd259b_backlight_resume\n");
        
	  backlight_update_status(bl);
        
	return 0;
}
#else
#define ktd259b_backlight_suspend  NULL
#define ktd259b_backlight_resume   NULL
#endif
#endif

static int ktd259b_backlight_probe(struct platform_device *pdev)
{
	struct platform_ktd259b_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl;
	struct ktd259b_bl_data *ktd259b;
    	struct backlight_properties props;
	int ret;

        BLDBG("[BACKLIGHT] ktd259b_backlight_probe\n");

	if (!data) {
		dev_err(&pdev->dev, "failed to find platform data\n");
		return -EINVAL;
	}

	ktd259b = kzalloc(sizeof(*ktd259b), GFP_KERNEL);
	if (!ktd259b) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	ktd259b->ctrl_pin = data->ctrl_pin;
    
	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = data->max_brightness;
	props.type = BACKLIGHT_PLATFORM;

	bl = backlight_device_register(pdev->name, &pdev->dev,
			ktd259b, &ktd259b_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		goto err_bl;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ktd259b->pdev = pdev;
	ktd259b->early_suspend_desc.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	ktd259b->early_suspend_desc.suspend = ktd259b_backlight_earlysuspend;
	ktd259b->early_suspend_desc.resume = ktd259b_backlight_earlyresume;
	register_early_suspend(&ktd259b->early_suspend_desc);
#endif

	bl->props.max_brightness = data->max_brightness;
	bl->props.brightness = data->dft_brightness;

	platform_set_drvdata(pdev, bl);

      	ktd259b_backlight_update_status(bl);
    
	return 0;

err_bl:
	kfree(ktd259b);
err_alloc:
	return ret;
}

static int ktd259b_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct ktd259b_bl_data *ktd259b = dev_get_drvdata(&bl->dev);

	backlight_device_unregister(bl);


#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ktd259b->early_suspend_desc);
#endif

	kfree(ktd259b);
	return 0;
}

static int ktd259b_backlight_shutdown(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct ktd259b_bl_data *ktd259b = dev_get_drvdata(&bl->dev);
	printk("[BACKLIGHT] ktd259b_backlight_shutdown\n");
       gpio_set_value(backlight_pin,0);
       mdelay(3); 	
	return 0;
}

static struct platform_driver ktd259b_backlight_driver = {
	.driver		= {
		.name	= "panel",
		.owner	= THIS_MODULE,
	},
	.probe		= ktd259b_backlight_probe,
	.remove		= ktd259b_backlight_remove,
	.shutdown      = ktd259b_backlight_shutdown,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend        = ktd259b_backlight_suspend,
	.resume         = ktd259b_backlight_resume,
#endif

};

static int __init ktd259b_backlight_init(void)
{
	return platform_driver_register(&ktd259b_backlight_driver);
}
module_init(ktd259b_backlight_init);

static void __exit ktd259b_backlight_exit(void)
{
	platform_driver_unregister(&ktd259b_backlight_driver);
}
module_exit(ktd259b_backlight_exit);

MODULE_DESCRIPTION("ktd259b based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ktd259b-backlight");


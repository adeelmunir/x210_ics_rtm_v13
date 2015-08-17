/*
 * linux/drivers/serial/tcc_ts_ak.c
 *
 * Author: <linux@telechips.com>
 * Created:     June 10, 2008
 * Description: Touchscreen driver for ssd2531 on Telechips TCC Series
 *
 * Copyright (C) 2008-2009 Telechips 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/device.h>

#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <linux/i2c.h>
#include <plat/map-base.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/regs-irq.h>
#include <linux/gpio.h>
#include <asm/irq.h>
#include <mach/hardware.h>
#include <mach/map.h>
#include <mach/irqs.h>

#include "ts_filter_chain.h"
#include "ts_filter_group.h"
#include "ts_filter_median.h"
#include "ts_filter_mean.h"
#include "ts_filter_linear.h"
#include "ts_filter_zoom.h"
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#include <linux/suspend.h>
#endif

extern int have_cap_touchscreen;

//==============================================================
#define DEVICE_NAME	"ssd2531_ts"

#define CMD_SLEEP_NO 0x80
#define CMD_ADC_ON 0x04
#define X_MAX	800
#define Y_MAX	480
#define GOODIX_TOUCH_WEIGHT_MAX 255

#define PEN_DOWN 1
#define PEN_RELEASE 0

#define I2C_BUS         1
#define SSD_Addr        0x5c

#define   NoEvent       0                //no event
#define   SFSC          1                //single finger single click  event
#define   SFDC          2                //single  finger double click event
#define   TFSC          3                //two finger single click event
#define   TFDC          4                //two finger double click event
#define   FE            5                //finger enter event
#define   FL            6                //finger leave event
#define   FM            7                //finger move event  


#define K_BACK  (KEY_BACK&KEY_MAX)
#define K_MENU  (KEY_MENU&KEY_MAX)
#define K_HOME  (KEY_HOME&KEY_MAX)
#define K_SEARCH (KEY_SEARCH&KEY_MAX)

#ifdef CONFIG_HAS_WAKELOCK
static struct early_suspend	early_suspend;
#endif


#define ssd2531_INT 	  S5PV210_GPH0(7)
#define READ_PENIRQ() 	  (gpio_get_value(ssd2531_INT))
#define GPIO_SSD_RESET  S5PV210_GPH0(6)
//#define GPIO_SSD_POWER  S5PV210_GPH2(4)

static  int debug        = 0;
#define DP(msg...) if (debug) { printk( "SSD CT:" msg); }

int y_correction[]=
{
0, 0, 1, 3, 5, 7, 10, 12, 14, 14,		
15, 17, 19, 20, 21, 23, 24, 25, 27, 28,			
29, 30, 32, 34, 34, 36, 36, 38, 38, 40,			
40, 42, 44, 44, 45, 46, 47, 48, 49, 50,			
51, 52, 53, 54, 55, 56, 57, 58, 59, 60,			
62, 62, 64, 64, 64, 66, 66, 68, 68, 70,			
70, 72, 72, 74, 74, 75, 76, 76, 77, 78,			
79, 80, 81, 82, 83, 84, 84, 85, 86, 87,			
88, 89, 90, 92, 92, 92, 94, 94, 96, 96,			
98, 98, 98, 100, 100, 102, 102, 104, 104, 105,		    
106, 106, 107, 108, 109, 110, 111, 112, 112, 113,		
114, 115, 116, 117, 118, 119, 120, 120, 122, 122,		
124, 124, 126, 126, 126, 128, 128, 130, 130, 132,		
132, 134, 134, 134, 135, 136, 137, 138, 139, 140,		
141, 142, 142, 143, 144, 145, 146, 147, 148, 149,		
150, 152, 152, 152, 154, 154, 156, 156, 158, 158,		
160, 160, 162, 162, 162, 164, 164, 165, 166, 167,		
168, 169, 170, 171, 172, 173, 174, 175, 176, 176,		
177, 178, 179, 180, 182, 182, 184, 184, 186, 186,		
188, 188, 190, 190, 192, 192, 194, 194, 195, 196,		
196, 197, 198, 199, 200, 201, 202, 203, 204, 205,		
206, 207, 208, 209, 210, 212, 212, 214, 214, 216,		
216, 218, 218, 220, 220, 222, 222, 224, 224, 225,		
226, 227, 228, 229, 230, 231, 232, 233, 234, 235,		
236, 236, 237, 238, 239, 240, 242, 242, 244, 244,		
246, 246, 248, 248, 250, 250, 252, 252, 254, 254, 
255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 
265, 266, 267, 268, 269, 270, 272, 272, 274, 274, 
274, 276, 276, 278, 278, 280, 280, 282, 282, 284, 
284, 285, 286, 287, 288, 289, 290, 291, 292, 293, 
294, 294, 295, 296, 297, 298, 299, 300, 302, 302, 
304, 304, 306, 306, 308, 308, 308, 310, 310, 312, 
312, 314, 314, 315, 316, 317, 318, 318, 319, 320, 
321, 322, 323, 324, 325, 326, 327, 328, 328, 329, 
330, 332, 332, 334, 334, 336, 336, 336, 338, 338, 
340, 340, 342, 342, 344, 344, 344, 345, 346, 347, 
348, 349, 350, 350, 351, 352, 353, 354, 355, 356, 
357, 358, 358, 359, 360, 362, 362, 364, 364, 364, 
366, 366, 368, 368, 370, 370, 372, 372, 372, 374, 
374, 375, 376, 377, 378, 378, 379, 380, 381, 382, 
383, 384, 385, 386, 386, 387, 388, 389, 390, 392, 
392, 394, 394, 394, 396, 396, 398, 398, 400, 400, 
402, 402, 404, 404, 405, 406, 406, 407, 408, 409, 
410, 411, 412, 413, 414, 415, 416, 417, 418, 419, 
420, 422, 422, 424, 424, 426, 426, 428, 430, 430, 
432, 432, 434, 434, 435, 436, 437, 439, 440, 441, 
442, 443, 445, 446, 447, 449, 450, 452, 454, 456, 
456, 458, 460, 462, 464, 465, 467, 469, 471, 479,  
};

int x_correction[]=
{
0, 1, 3, 6, 7, 9, 12, 12, 13, 15,		
18, 19, 20, 21, 24, 25, 26, 27, 30, 30,			
31, 33, 34, 36, 37, 38, 39, 42, 42, 43,			
44, 45, 47, 48, 49, 50, 52, 53, 54, 55,			
56, 58, 58, 59, 61, 62, 64, 64, 65, 66,			
67, 68, 70, 72, 72, 74, 74, 75, 76, 77,			
78, 80, 80, 81, 82, 83, 84, 86, 86, 87,			
88, 89, 90, 92, 92, 93, 94, 95, 96, 97,			
98, 99, 100, 102, 102, 103, 104, 105, 106, 108,			
108, 109, 110, 110, 111, 112, 114, 114, 115, 116,		
117, 118, 120, 120, 122, 122, 124, 124, 125, 126,		
126, 127, 128, 130, 130, 131, 132, 133, 134, 136,		
136, 137, 138, 138, 139, 140, 142, 142, 143, 144,		
145, 146, 147, 148, 148, 149, 150, 152, 152, 153,		
154, 155, 156, 158, 158, 158, 159, 160, 161, 162,		
164, 164, 165, 166, 166, 167, 168, 170, 170, 172,		
172, 174, 174, 175, 176, 176, 177, 178, 180, 180,		
181, 182, 183, 184, 184, 186, 186, 187, 188, 189,		
190, 192, 192, 193, 194, 194, 195, 196, 197, 198,		
199, 200, 202, 202, 202, 203, 204, 205, 206, 208,		
208, 209, 210, 211, 212, 212, 214, 214, 215, 216,		
217, 218, 220, 220, 222, 222, 222, 224, 224, 225,		
226, 227, 228, 230, 230, 231, 232, 232, 233, 234,		
236, 236, 237, 238, 239, 240, 242, 242, 243, 244,		
244, 245, 246, 247, 248, 249, 250, 252, 252, 253,		
254, 255, 256, 256, 258, 258, 259, 260, 261, 262, 
264, 264, 265, 266, 267, 268, 270, 270, 270, 272, 
272, 274, 274, 275, 276, 277, 278, 280, 280, 281, 
282, 283, 284, 286, 286, 287, 288, 288, 289, 290, 
292, 292, 293, 294, 295, 296, 297, 298, 299, 300, 
302, 302, 303, 304, 305, 306, 308, 308, 309, 310, 
311, 312, 312, 314, 314, 315, 316, 317, 318, 320, 
320, 322, 322, 324, 324, 325, 326, 327, 328, 330, 
330, 331, 332, 333, 334, 336, 336, 337, 338, 339, 
340, 342, 342, 343, 344, 345, 346, 347, 348, 349, 
350, 352, 352, 353, 354, 355, 356, 358, 358, 359, 
360, 361, 362, 364, 364, 365, 366, 367, 368, 370, 
370, 372, 372, 374, 374, 375, 376, 377, 378, 380, 
380, 381, 382, 383, 384, 386, 386, 387, 388, 389, 
390, 392, 392, 393, 394, 395, 396, 397, 398, 399, 
400, 402, 402, 403, 404, 405, 406, 406, 408, 408, 
409, 410, 411, 412, 414, 414, 415, 416, 417, 418, 
420, 420, 422, 422, 424, 424, 425, 426, 427, 428, 
430, 430, 431, 432, 433, 434, 436, 436, 437, 438, 
439, 440, 442, 442, 443, 444, 445, 446, 447, 448, 
449, 450, 452, 452, 453, 454, 455, 456, 458, 458, 
459, 460, 461, 462, 464, 464, 465, 466, 467, 468, 
470, 470, 472, 472, 474, 474, 475, 476, 477, 478, 
480, 480, 481, 482, 483, 484, 486, 486, 486, 487, 
488, 489, 490, 492, 492, 493, 494, 495, 496, 497, 
498, 499, 500, 502, 502, 503, 504, 505, 506, 508, 
508, 509, 510, 510, 511, 512, 514, 514, 515, 516, 
517, 518, 520, 520, 522, 522, 524, 524, 525, 526, 
527, 528, 528, 530, 530, 531, 532, 533, 534, 536, 
536, 537, 538, 539, 540, 542, 542, 542, 543, 544, 
545, 546, 547, 548, 549, 550, 552, 552, 553, 554, 
554, 555, 556, 558, 558, 559, 560, 561, 562, 564, 
564, 565, 566, 566, 567, 568, 570, 570, 572, 572, 
574, 574, 575, 576, 576, 577, 578, 580, 580, 581, 
582, 583, 584, 586, 586, 586, 587, 588, 589, 590, 
592, 592, 593, 594, 595, 596, 596, 597, 598, 599, 
600, 602, 602, 603, 604, 604, 605, 606, 608, 608, 
609, 610, 611, 612, 614, 614, 614, 615, 616, 617, 
618, 620, 620, 622, 622, 622, 624, 624, 625, 626, 
627, 628, 630, 630, 631, 632, 632, 633, 634, 636, 
636, 637, 638, 639, 640, 640, 642, 642, 643, 644, 
645, 646, 647, 648, 649, 650, 650, 652, 652, 653, 
654, 655, 656, 658, 658, 659, 660, 660, 661, 662, 
664, 664, 665, 666, 667, 668, 670, 670, 672, 672, 
672, 674, 674, 675, 676, 677, 678, 680, 680, 681, 
682, 683, 684, 686, 686, 687, 688, 688, 689, 690, 
692, 692, 693, 694, 695, 696, 697, 698, 699, 700, 
702, 702, 703, 704, 705, 706, 708, 708, 709, 710, 
711, 712, 714, 714, 715, 716, 717, 718, 720, 720, 
722, 722, 724, 724, 725, 726, 727, 730, 730, 731, 
732, 733, 734, 736, 736, 737, 739, 740, 742, 742, 
743, 744, 745, 747, 748, 749, 750, 752, 753, 754, 
755, 756, 758, 759, 760, 761, 764, 764, 765, 767, 
768, 770, 772, 772, 774, 775, 777, 778, 780, 781, 
783, 786, 786, 787, 789, 792, 793, 795, 797, 799, 
};


static int g_ssd_detect_status;
struct ts_event {
	unsigned short pressure;
	unsigned short x;
	unsigned short y;
};

struct ssd2531_ts {
    struct input_dev *input;
    unsigned int x, y, z1, z2, pressure, valid;
    int pen_status, opened, running;
    int irq;
    struct mutex mutex;
    struct delayed_work work_q;
    int interval_ms;
	struct ts_filter_chain * chain;
};

struct ssd2531_ts *ssd2531_ts;
static struct timer_list ts_timer;
static int irq_no;
int  tea_tch_getid(void);

const static struct ts_filter_group_configuration config_ts_group = {
	.length = 12,
	.close_enough = 10,
	.threshold = 6,
	.attempts = 10,
};

const static struct ts_filter_median_configuration config_ts_median = {
	.extent = 5,
	.decimation_below = 3,
	.decimation_threshold = 2 * 3,
	.decimation_above = 2,
};

const static struct ts_filter_mean_configuration config_ts_mean = {
	.length = 2,
};

const static struct ts_filter_linear_configuration config_ts_linear = {	
	.constants = {16017, 0, 2739156, 23, 17056, 1639328, 65536},
	.coord0 = 0,
	.coord1 = 1,
};

const static struct ts_filter_zoom_configuration config_ts_zoom = {
	.constants = {0},
	.coord0 = 0,
	.coord1 = 1,
};

const static struct ts_filter_chain_configuration filter_configuration[] =
{
	{&ts_filter_mean_api,	&config_ts_mean.config},
	{&ts_filter_linear_api,	&config_ts_linear.config},
	{&ts_filter_zoom_api,	&config_ts_zoom.config},
	{NULL, NULL},
};

static const struct i2c_device_id ak_i2c_id[] = {
	{ "s3c-ssd2531", 0},
	{ }
};

#ifdef CONFIG_I2C

static struct i2c_driver ak_i2c_driver;
static struct i2c_client *ak_i2c_client;

static void IIC_SEND_CMD(unsigned char reg, unsigned char val)
{
        unsigned char cmd[2];
        cmd[0] = reg;
        cmd[1] = val;
        i2c_master_send(ak_i2c_client, cmd, 2);
}

static void IIC_SEND_CMD2(unsigned char reg, unsigned char val1,unsigned char val2)
{
        unsigned char cmd[3];
        cmd[0] = reg;
        cmd[1] = val1;
        cmd[2] = val2;
        i2c_master_send(ak_i2c_client, cmd, 3);
}

static unsigned char IIC_READ_DAT(unsigned char reg,unsigned char *buff,unsigned char length)
{

    struct i2c_adapter *adap;
    struct i2c_msg msgs[250];

    adap = i2c_get_adapter(I2C_BUS);
    if (!adap)
        return 0;
    // set offset
    msgs[0].addr  = SSD_Addr;
    msgs[0].flags = 0;
    msgs[0].len   = 1;
    msgs[0].buf   = &reg;

    // read data
    msgs[1].addr  = SSD_Addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len   = length;
    msgs[1].buf   = buff;

    // read from DDC line
    if (i2c_transfer(adap, msgs, 2) < 0)
        return 0;

    return 1;
}

int  tea_dev_init_70HF(void)
{
	int err;
	
    printk("** %s **\n",__FUNCTION__);

    err = gpio_request(GPIO_SSD_RESET, "GPH0");
	if (err)
	{
		printk(KERN_ERR "#### failed to request GPH0 for ts_ssd\n");
	}

    //lqm added.
    /* Output mode */
	s3c_gpio_cfgpin(GPIO_SSD_RESET, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_SSD_RESET, S3C_GPIO_PULL_UP);
	// end added

    gpio_direction_output(GPIO_SSD_RESET, 0);
    //mdelay(20);
    msleep(20);
    gpio_direction_output(GPIO_SSD_RESET, 1);

    //gpio_free(GPIO_SSD_RESET);//lqm masked.

    msleep(10);                                                                      
    IIC_SEND_CMD(0x23, 0x00);                 // Exit sleep mode                      
    msleep(10);                                                                      
    IIC_SEND_CMD(0x2B, 0x03);                 // Enable DSP clock                     
    IIC_SEND_CMD(0xd4, 0x08);                 // Clk speed                            
    IIC_SEND_CMD(0xd7, 0x04);                 // ADC range                            
                                                                                      
    // Touch pnel configuration                                        
    IIC_SEND_CMD(0x06, 0x0f);                  // Set drive lines                     
    IIC_SEND_CMD(0x07, 0x06);                 // Set sense lines                      
                                                                                      
    IIC_SEND_CMD(0x08, 0x09);                 // Set 1st drive line reg               
    IIC_SEND_CMD(0x09, 0x08);                 // Set 2nd drive line reg               
    IIC_SEND_CMD(0x0A, 0x07);                 // Set 3rd drive line reg               
    IIC_SEND_CMD(0x0B, 0x06);                 // Set 4th drive line reg               
    IIC_SEND_CMD(0x0C, 0x05);                 // Set 5th drive line reg               
    IIC_SEND_CMD(0x0D, 0x04);                 // Set 6th drive line reg               
    IIC_SEND_CMD(0x0E, 0x03);                 // Set 7th drive line reg               
    IIC_SEND_CMD(0x0F, 0x02);                 // Set 8th drive line reg               
    IIC_SEND_CMD(0x10, 0x01);                 // Set 9th drive line reg               
    IIC_SEND_CMD(0x11, 0x00);                 // Set 10th drive line reg              
    IIC_SEND_CMD(0x12, 0x14);                 // Set 11th drive line reg              
    IIC_SEND_CMD(0x13, 0x13);                 // Set 12th drive line reg              
    IIC_SEND_CMD(0x14, 0x12);                 // Set 13th drive line reg              
    IIC_SEND_CMD(0x15, 0x11);                 // Set 14th drive line reg              
    IIC_SEND_CMD(0x16, 0x10);                 // Set 15th drive line reg              
    IIC_SEND_CMD(0x17, 0x0f);                 // Set 16th drive line reg              
    IIC_SEND_CMD(0x18, 0x0e);                 // Set 17th drive line reg              
    IIC_SEND_CMD(0x19, 0x0d);                 // Set 18th drive line reg              
    IIC_SEND_CMD(0x1a, 0x0c);                 // Set 19th drive line reg              
    IIC_SEND_CMD(0x1b, 0x0b);                 // Set 20th drive line reg              
    IIC_SEND_CMD(0x1c, 0x0a);                 // Set 21th drive line reg              
                                                                                      
    // Touch dtection setting                                          
    IIC_SEND_CMD(0x2A, 0x03);                 // Set sub-frames                       
    IIC_SEND_CMD(0x8d, 0x01);
    IIC_SEND_CMD(0x8e, 0x02);
    IIC_SEND_CMD2(0x94, 0x00,0x00); 
    IIC_SEND_CMD(0x8d, 0x00); 
    IIC_SEND_CMD(0x25, 0x02);                 // Set scan mode (200hz)         
                                                                                      
    msleep(10);                                                                      
                                                                                      
    IIC_SEND_CMD(0xc1, 0x02);                 // charge pump 2nd booster (?)          
    IIC_SEND_CMD(0xd5, 0x0f);                 // driving voltage (15.5V)              
                                                                                      
    msleep(10);                                                                      
    msleep(10);                                                                      
    msleep(10);                                                                      
                                                                                      
    IIC_SEND_CMD(0xd9, 0x01);                 // enable sense filter                  
//    IIC_SEND_CMD(0x58, 0x03);                 //ZSL Press 
    IIC_SEND_CMD(0x59, 0x01);                 //ENABLE_MOVE_TOLERANCE,                
//    IIC_SEND_CMD(0x5b, 0x01);                 //MOVE_TOLERANCE,                       
    IIC_SEND_CMD(0x5b, 0x10);                 //MOVE_TOLERANCE,                       
    IIC_SEND_CMD(0x5a, 0x00);                 // maximum missed frames                
    IIC_SEND_CMD(0x2c, 0x02);                 // median filter parameter (2 tap)      
    IIC_SEND_CMD(0x3d, 0x01);                                                         
    IIC_SEND_CMD(0x38, 0x00);                 // delta data range                     
    IIC_SEND_CMD(0x33, 0x01);                 // minimum finger area                  
//    IIC_SEND_CMD(0x34, 0x50);                 // minimum finger level                 
//    IIC_SEND_CMD(0x34, 0x28);                 // minimum finger level                 
    IIC_SEND_CMD(0x34, 0x35);                 // minimum finger level                 
    IIC_SEND_CMD2(0x35, 0x00,0x00);            // Finger weight threshold             
    IIC_SEND_CMD(0x36, 0x1e);                 // Set Max. Finger area                 
    IIC_SEND_CMD(0x37, 0x01);                 // Set Segmentation depth               
//    IIC_SEND_CMD(0x39, 0x01);                 // Finger tracking mode                 
    IIC_SEND_CMD(0x39, 0x00);                 // Finger tracking mode                 
    IIC_SEND_CMD(0x56, 0x01);                 // Moving average                       
//    IIC_SEND_CMD(0x58, 0x03);                 // 
    IIC_SEND_CMD2(0x51, 0x00,0x00);            // single click timing (disabled)      
    IIC_SEND_CMD2(0x52, 0x00,0x00);            // double click timing (disabled)      
    IIC_SEND_CMD(0x53, 0x10);                 // CG tolerance                         
    IIC_SEND_CMD(0x54, 0x30);                 // X tracking tolerance                 
    IIC_SEND_CMD(0x55, 0x30);                 // Y tracking tolerance                 
    IIC_SEND_CMD(0x65, 0x02);                                                         
    IIC_SEND_CMD(0x66, 0x57); //0x40                 // scale X [xx.xxxxxx]                  
    IIC_SEND_CMD(0x67, 0x50); //0x40                // scale Y [xx.xxxxxx]                  
    IIC_SEND_CMD(0xa2, 0x00);                 // Reset Init Reference                 
    IIC_SEND_CMD(0xab, 0x64);                                                         
    IIC_SEND_CMD(0xac, 0x00);                                                         
    IIC_SEND_CMD(0xad, 0x03);                // touch key scan rate: 0->20hz  3->50hz           
    IIC_SEND_CMD(0xae, 0x0f);	                // enable touch key channels 0-3        
    IIC_SEND_CMD(0xaf, 0x28);                                                         
    IIC_SEND_CMD(0xba, 0x00);                // enable touch key IRQ 0-3              
    IIC_SEND_CMD(0xbc, 0x01);	                // enable touch key                     
    IIC_SEND_CMD2(0x7a, 0xff,0xbf);            // mask all events but finger leave    
    IIC_SEND_CMD(0x7b, 0xe0);                // mask IRQ: fifo overflow, large object.
    IIC_SEND_CMD(0xa2, 0x00);                // Reset Init Reference.

    return 0;
}

int  tea_tch_getid(void)
{
    unsigned char buf[10];
    int result;
    IIC_READ_DAT(0x02,buf,2);
    printk("Device ID :0x%x 0x%x \n",buf[0],buf[1]);
    if ((buf[0]==0x25)&&(buf[1]==0x31))
    {
        printk("SSD touch device Found!\n");
        result=0;
    }else
    {
        printk("Warring! SSD touch device Not Found!\n");
        result=-1;
    }
    return result;
}

int detect_ssd_touch(void)
{
    return g_ssd_detect_status;
}
EXPORT_SYMBOL_GPL(detect_ssd_touch);

static void TOUCH_READ_DAT(unsigned char reg, unsigned int val)
{
	unsigned char *buf;
	buf = (unsigned char *)val;
	i2c_master_recv(ak_i2c_client, buf, 2);
}

static int ak_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	printk(KERN_INFO "##################### %s: client = %p\n", __func__, ak_i2c_client);
	return 0;
}

static int ak_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static struct i2c_driver ak_i2c_driver = {
	.driver = {
		.name	= "s3c-ssd2531",
	},
	.probe		= ak_i2c_probe,
	.remove		= ak_i2c_remove,
	.id_table	= ak_i2c_id,
};

#endif //CONFIG_I2C

static void write_cmd_ak(unsigned char cmd)
{
	
#ifndef CONFIG_I2C
	udelay(1);
#endif
}

static short read_ak(int cmd)
{
	unsigned char data[]={0,0};
	short value;
	
	cmd |= (CMD_SLEEP_NO | CMD_ADC_ON);
    write_cmd_ak(cmd);
	
	TOUCH_READ_DAT(0, (unsigned int)(&data[0]));
		
	value = (data[0] & 0x00ff) << 4;
	value |= ((data[1] & 0x00ff) >> 4);
	value &= 0xfff;
	
#ifndef CONFIG_I2C
	udelay(1);
#endif
	return(value>>1);
}

short tea_tch_readi2c(int channel)
{
    short ret;
    unsigned char buf[2];
    int cmd;

    cmd = (int) buf[0];
    ret = read_ak(cmd);
    
	return ret;
}

int tea_tch_writei2c(unsigned char command)
{	
	if(ak_i2c_client == NULL)
	{
		printk("tea_tch_writei2c(tcc_ts_ak.c): ak_i2c_client is NULL \n");
    	return -1;
	}
	i2c_master_send(ak_i2c_client, &command, 1);
    return 0;
}

#define KEY_F_TIME   5

void tca_process_cp_key(struct input_dev *dev)
{
    static int key1_press=0,key2_press=0,key3_press=0,key4_press=0;
    static int key_f1=0,key_f2=0,key_f3=0,key_f4=0;
    unsigned char regb9_value;    

    IIC_READ_DAT(0xb9,&regb9_value,1);//status
    DP("regb9-> 0x%x \n",regb9_value);
    if(regb9_value&0x01)
    {
	key_f1++;
	if(key_f1<KEY_F_TIME)
	    return ;
	DP("-- Key Press 1--\n");
	key_f1=key_f2=key_f3=key_f4=0;
	key1_press=1;
        input_report_key(dev, K_SEARCH, 1);
	return ;
    }
    else if(!(regb9_value&0x01)&&(key1_press==1))
    {
	key1_press=0;
	key_f1=key_f2=key_f3=key_f4=0;
        input_report_key(dev, K_SEARCH, 0);
	return ;
    }

    if(regb9_value&0x02)
    {
	key_f2++;
	if(key_f2<KEY_F_TIME)
	    return ;
	DP("-- Key Press 2--\n");
	key_f1=key_f2=key_f3=key_f4=0;
	key2_press=1;
        input_report_key(dev, K_MENU, 1);
	return ;
    }
    else if(!(regb9_value&0x02)&&(key2_press==1))
    {
	key2_press=0;
	key_f1=key_f2=key_f3=key_f4=0;
        input_report_key(dev, K_MENU, 0);
	return ;
    }

    if(regb9_value&0x04)
    {
	key_f3++;
	if(key_f3<KEY_F_TIME)
	    return ;
	DP("-- Key Press 3--\n");
	key_f1=key_f2=key_f3=key_f4=0;
	key3_press=1;
        input_report_key(dev, K_HOME, 1);
	return ;
    }
    else if(!(regb9_value&0x04)&&(key3_press==1))
    {
	key3_press=0;
	key_f1=key_f2=key_f3=key_f4=0;
        input_report_key(dev, K_HOME, 0);
	return ;
    }

    if(regb9_value&0x08)
    {
	key_f4++;
	if(key_f4<KEY_F_TIME)
	    return ;
	DP("-- Key Press 4--\n");
	key_f1=key_f2=key_f3=key_f4=0;
	key4_press=1;
        input_report_key(dev, K_BACK, 1);
	return ;
    }
    else if(!(regb9_value&0x08)&&(key4_press==1))
    {
	key4_press=0;
	key_f1=key_f2=key_f3=key_f4=0;
        input_report_key(dev, K_BACK, 0);
	return ;
    }
    key_f1=key_f2=key_f3=key_f4=0;
}

int process_eve(unsigned char EventNo,struct input_dev *dev)
{
    switch(EventNo)
    {
    case FL:
     {
	    DP("Finger leave , Event= 0x%x\n",EventNo);
		DP("** release Event**\n");
		DP("Finger1 leave \n");
		input_report_abs(dev, ABS_MT_TOUCH_MAJOR, 0);
		input_report_abs(dev, ABS_MT_WIDTH_MAJOR, 0);
		input_report_abs(dev, ABS_MT_POSITION_X, 0);
		input_report_abs(dev, ABS_MT_POSITION_Y, 0);
		input_report_key(dev, BTN_TOUCH, 0);
		input_report_abs(dev, ABS_PRESSURE, 0);
		input_mt_sync(dev);
		input_sync(dev);
	    break;
     }
     default:
     {
     break;
     }
  }
}



int tca_process_ssd_event(int * Coord_X, int * Coord_Y,struct SSD2531_ts* ts_data, struct input_dev *dev )
{
    unsigned char PointNo,EventNo,Coordinate[5],reg79_value,fg1_buf[4],fg2_buf[4];
    unsigned int fg1_press,fg2_press;
    unsigned int fg1_x,fg1_y,fg2_x,fg2_y;
    static int f_free=0,Exceptional_int=0;
    PointNo=0;
//    DP("- i - \n");
    fg1_x=fg1_y=fg2_x=fg2_y=1;
    fg1_press=fg2_press=0;
#if 1
    tca_process_cp_key(dev);
    IIC_READ_DAT(0x79,&reg79_value,1);//status
    DP("reg79-> 0x%x \n",reg79_value);
    if(!(reg79_value&0x03))
    {
    #if 0
	if (f_free>0)
	{   
	    f_free=0;
	    goto f_release;
	}
	#endif
	DP("Clear int !\n");
	IIC_READ_DAT(0x80,Coordinate,4);//status
	EventNo = Coordinate[0]&0x0f;
	process_eve(EventNo,dev);
	goto No_Event;
    }
	f_free=1;
#endif         

    
    IIC_READ_DAT(0x7c,fg1_buf,4);//
    IIC_READ_DAT(0x7d,fg2_buf,4);//

    fg1_y = fg1_buf[0] |((fg1_buf[2]&0x30)<<4);
    fg1_x = fg1_buf[1] |((fg1_buf[2]&0x03)<<8);
    fg1_press=(fg1_buf[3]&0xf0)>>4;
	fg1_y=y_correction[fg1_y];
    fg1_x=x_correction[fg1_x];
    DP("fg1 x:%d y:%d press:%d \n",fg1_x,fg1_y,fg1_press);
    fg2_y = fg2_buf[0] |((fg2_buf[2]&0x30)<<4);
    fg2_x = fg2_buf[1] |((fg2_buf[2]&0x03)<<8);
    fg2_press=(fg2_buf[3]&0xf0)>>4;
	fg2_y=y_correction[fg2_y];
    fg2_x=x_correction[fg2_x];
    DP("fg2 x:%d y:%d press:%d \n",fg2_x,fg2_y,fg2_press);

#if 1

    if(((fg1_x>0)&&(fg1_x<=X_MAX))&&((fg1_y>0)&&(fg1_y<=Y_MAX))&&(fg1_press>0))
    {
	DP("- r1 -\n");
	input_report_abs(dev, ABS_MT_TOUCH_MAJOR, 255);
        input_report_abs(dev, ABS_X, fg1_x );
        input_report_abs(dev, ABS_Y, Y_MAX-fg1_y );
        input_report_abs(dev, ABS_PRESSURE, 255);
        input_report_key(dev, BTN_TOUCH, 1);
    }

    if(((fg2_x>0)&&(fg2_x<=X_MAX))&&((fg2_y>0)&&(fg2_y<=Y_MAX))&&(fg2_press>0))
    {
	DP("- r2 -\n");
	input_report_abs(dev, ABS_MT_TOUCH_MAJOR, 255);
        input_report_key(dev, BTN_2, 2);
        input_report_abs(dev, ABS_HAT0X,fg2_x);
        input_report_abs(dev, ABS_HAT0Y,Y_MAX-fg2_y);
        input_report_key(dev, BTN_TOUCH, 2);
    }

#endif
    if(((fg1_x>0)&&(fg1_x<=X_MAX))&&((fg1_y>0)&&(fg1_y<=Y_MAX))&&(fg1_press>0))
    {
	input_report_abs(dev, ABS_MT_TOUCH_MAJOR, 255);
        input_report_abs(dev, ABS_MT_POSITION_X, fg1_x);
        input_report_abs(dev, ABS_MT_POSITION_Y,Y_MAX-fg1_y);
        input_report_abs(dev, ABS_PRESSURE, 255);
        input_report_key(dev, BTN_TOUCH, 1);
        input_mt_sync(dev);
    }

#if 1
    if(((fg2_x>0)&&(fg2_x<=X_MAX))&&((fg2_y>0)&&(fg2_y<=Y_MAX))&&(fg2_press>0))
    {
        input_report_abs(dev, ABS_MT_TOUCH_MAJOR, 255);
       // input_report_abs(dev, ABS_MT_WIDTH_MAJOR, fg2_press);
        input_report_abs(dev, ABS_MT_POSITION_X, fg2_x);
        input_report_abs(dev, ABS_MT_POSITION_Y,Y_MAX- fg2_y);
        input_report_abs(dev, ABS_PRESSURE, 255);
        input_report_key(dev, BTN_TOUCH, 1);
	input_mt_sync(dev);    
    }
#endif
f_release:
    if((((fg1_x>0)&&(fg1_x<=X_MAX))&&((fg1_y>0)&&(fg1_y<=Y_MAX))&&(fg1_press<1))||(((fg2_x>0)&&(fg2_x<=X_MAX))&&((fg2_y>0)&&(fg2_y<=Y_MAX))&&(fg2_press<1)))
    {
	IIC_READ_DAT(0x80,Coordinate,4);//status
	EventNo = Coordinate[0]&0x0f;
	process_eve(EventNo,dev);
    }
    input_sync(dev);
//-------------------------------------------------------
    return 0;

No_Event:
	DP("No E\n");

    return 0;
}


static inline void ak_pen_release(struct ssd2531_ts* ts_data, struct input_dev *dev)
{
    if (ts_data->pen_status != PEN_RELEASE)
	{
		ts_filter_chain_clear(ssd2531_ts->chain);
                ts_data->pen_status = PEN_RELEASE;
		input_report_abs(dev, ABS_MT_TOUCH_MAJOR, 0);
		//input_report_abs(dev, ABS_X, ts_data->x);
		//input_report_abs(dev, ABS_Y, ts_data->y);
		//input_report_abs(dev, ABS_Z, 0);
		input_report_key(dev, BTN_TOUCH, 0);
		input_report_abs(dev, ABS_PRESSURE, 0);
        input_sync(dev);
        /* wake_up_interruptible(&ts_data->wait_q); */
    }
}

static inline void ak_pen_pressure(struct ssd2531_ts* ts_data, struct input_dev *dev)
{
	int buf[3];

	buf[1] = ts_data->x;
	buf[2] = ts_data->y;

	switch (ts_filter_chain_feed(ssd2531_ts->chain, &buf[1]))
	{
	case 0:
		/* The filter wants more points. */
		break;

	case 1:
		/* We have a point from the filters or no filtering enabled. */
		ts_data->x = buf[1];
		ts_data->y = buf[2];
                if (1) //(get_lcd_blk_status())
                {
		    ts_data->pen_status = PEN_DOWN;
		    input_report_abs(dev, ABS_MT_TOUCH_MAJOR, 255);
		    input_report_abs(dev, ABS_X, ts_data->x);
		    input_report_abs(dev, ABS_Y, ts_data->y);
		    input_report_abs(dev, ABS_Z, 0);
		    input_report_key(dev, BTN_TOUCH, 1);
		    input_report_abs(dev, ABS_PRESSURE, 1);
		    input_sync(dev);
		}

		break;

	case -1:
		/* error, ignore the event */
		ts_filter_chain_clear(ssd2531_ts->chain);
		break;

	default:
		printk("Invalid ts_filter_chain_feed return value.\n");
		break;
	};
}

static void ts_fetch_thread(struct work_struct *work)
{
    struct ssd2531_ts* ts_data = container_of(work, struct ssd2531_ts, work_q);
    struct input_dev *dev = ssd2531_ts->input;
    int valid,flag;
   
    ts_data->running = 1;

    if (READ_PENIRQ()) {		
        ak_pen_release(ts_data, dev);
        ts_data->running = 0;
      	enable_irq(irq_no);
    } else {    
		flag=tca_process_ssd_event(&(ts_data->x), &(ts_data->y),ts_data, dev);
      	enable_irq(irq_no);
    }
}


static irqreturn_t ssd2531_irq_handler(int irq, void *dev_id)
{
    struct ssd2531_ts *ts_data = (struct ssd2531_ts *)dev_id;	

	disable_irq_nosync(irq_no);	
	schedule_delayed_work(&(ssd2531_ts->work_q),msecs_to_jiffies(1));
	
	return IRQ_HANDLED;	
}

static void ts_timer_handler(unsigned long data)
{
    if(ssd2531_ts->opened){
        if (schedule_delayed_work(&(ssd2531_ts->work_q),msecs_to_jiffies(1))== 0) {
            printk("cannot schedule work !!!\n");
            ssd2531_ts->running = 0;
        }
    }else {
        //tca_tchhw_disableirq();
    }
}


//static int ssd2531ts_suspend(struct platform_device *dev, pm_message_t state)
static int ssd2531ts_suspend(struct early_suspend *h)

{
	// power disable
	/*int err;
    err = gpio_request(GPIO_SSD_POWER, "GPH2");
	if (err)
	{
		printk(KERN_ERR "#### failed to request GPH2 for ts_ssd\n");  
	}
    gpio_direction_output(GPIO_SSD_POWER, 0);

    gpio_free(GPIO_SSD_POWER);*/
    //printk("%s\n", __func__);

    return 0;
}

//static int ssd2531ts_resume(struct platform_device *dev)
static int ssd2531ts_resume(struct early_suspend *h)
{
	//printk("%s\n", __func__);
	if(g_ssd_detect_status != 0)
		tea_dev_init_70HF();
    return 0;
}

static int __devinit ssd2531ts_probe(struct platform_device *pdev)
{
    struct input_dev *input_dev;
    int err = -ENOMEM,result;
    int i;
    struct i2c_board_info info;
    struct i2c_adapter *adapter;
    struct i2c_client *client;

    printk("##########%s\n", __func__);
    memset(&info, 0, sizeof(struct i2c_board_info));
    info.addr = SSD_Addr;
    strlcpy(info.type, "s3c-ssd2531", I2C_NAME_SIZE);
     adapter = i2c_get_adapter(I2C_BUS);
    client = i2c_new_device(adapter, &info);
    i2c_put_adapter(adapter);
    if (!client)
    {
        printk("can't add i2c device at 0x%x\n", (unsigned int)info.addr);
        return -ENODEV;
    }

    ak_i2c_client = client;
   	
    result=i2c_add_driver(&ak_i2c_driver);
    if(result)
    {
        printk("fail : ssd2531ts_probe i2c_add_driver(%d) \n", result);
		goto det_fail;
    }	

#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend.suspend = ssd2531ts_suspend;
	early_suspend.resume = ssd2531ts_resume;
	early_suspend.level = 150;
	register_early_suspend(&early_suspend);
#endif

    tea_dev_init_70HF();
    result=tea_tch_getid();

    if(result != 0)
    {
	printk("SSD Touch not found!\n");
        g_ssd_detect_status=0;
        goto det_fail;
    }
        g_ssd_detect_status=1;

 	/* INPUT DEVICE */
    ssd2531_ts = kzalloc(sizeof(struct ssd2531_ts), GFP_KERNEL);

    input_dev = input_allocate_device();

    if (!ssd2531_ts || !input_dev)
    {
        goto fail1;
    }

    platform_set_drvdata(pdev, ssd2531_ts);

    ssd2531_ts->running = 0;
    ssd2531_ts->opened = 1;
    ssd2531_ts->pen_status = PEN_RELEASE;
    ssd2531_ts->interval_ms = 25;

    ssd2531_ts->input = input_dev;

    input_dev->name = "ssd-touch";
    input_dev->phys = "ssd-touch/input0";
    input_dev->id.version = 0x0001;
    input_dev->dev.parent = &pdev->dev;

    set_bit(EV_SYN, input_dev->evbit);
    set_bit(EV_KEY, input_dev->evbit);
    set_bit(EV_ABS, input_dev->evbit);
    set_bit(BTN_TOUCH, input_dev->keybit);
    set_bit(K_BACK, input_dev->keybit);
    set_bit(K_MENU, input_dev->keybit);
    set_bit(K_HOME, input_dev->keybit);
    set_bit(K_SEARCH, input_dev->keybit);
    input_set_abs_params(input_dev, ABS_X, 0, X_MAX, 0, 0);
    input_set_abs_params(input_dev, ABS_Y, 0, Y_MAX, 0, 0);
    input_set_abs_params(input_dev, ABS_PRESSURE, 0, 255, 0, 0);
    input_set_abs_params(input_dev, ABS_TOOL_WIDTH, 0, 255, 0, 0);
    input_set_abs_params(input_dev, ABS_HAT0X, 0, X_MAX, 0, 0);
    input_set_abs_params(input_dev, ABS_HAT0Y, 0, Y_MAX, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, X_MAX, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, Y_MAX, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);    

	/*
	 * create the filter chain set up for the 2 coordinates we produce
     */
	ssd2531_ts->chain = ts_filter_chain_create(pdev, filter_configuration, 2);
	if(IS_ERR(ssd2531_ts->chain))
		goto fail0;
	ts_filter_chain_clear(ssd2531_ts->chain);
    INIT_DELAYED_WORK(&(ssd2531_ts->work_q), ts_fetch_thread);    
    mutex_init(&(ssd2531_ts->mutex));

    init_timer(&ts_timer);
    ts_timer.data = (unsigned long)ssd2531_ts;
    ts_timer.function = ts_timer_handler;    
  	
  	irq_no = IRQ_EINT7;

	/* Input mode */  
    s3c_gpio_cfgpin(ssd2531_INT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(ssd2531_INT, S3C_GPIO_PULL_NONE);
	
	//CTSPINT EINT7
	err = gpio_request(ssd2531_INT, "GPH0");
	if (err)
	{
		printk(KERN_INFO "gpio request error : %d\n", err);
	}
	else
	{
		s3c_gpio_cfgpin(ssd2531_INT, (0xf << 0));
		s3c_gpio_setpull(ssd2531_INT, S3C_GPIO_PULL_NONE);
	}

	set_irq_type(irq_no, IRQ_TYPE_LEVEL_LOW);	

	if (request_irq(irq_no, ssd2531_irq_handler, IRQ_TYPE_LEVEL_LOW, DEVICE_NAME, ssd2531_ts))
    {
        printk(KERN_INFO"request ssd2531 irq failed.\n");
        err = -EBUSY;
        goto fail1;
    }
	
   	err = input_register_device(ssd2531_ts->input);
    if (err)
        goto fail2;	

    have_cap_touchscreen = 1;

    printk("ssd2531 (irq=%d) driver is successfully loaded\n", irq_no);    
	return 0;

fail2: free_irq(irq_no, ssd2531_ts);
fail1: input_free_device(input_dev);
fail0: ts_filter_chain_destroy(ssd2531_ts->chain);
	kfree(ssd2531_ts);
det_fail:
	return err;

}

static int __devexit ssd2531ts_remove(struct platform_device *pdev)
{
    ssd2531_ts->opened = 0;    
	del_timer_sync(&ts_timer);
    flush_scheduled_work();
	free_irq(irq_no, ssd2531_ts);
    input_unregister_device(ssd2531_ts->input);
    ts_filter_chain_destroy(ssd2531_ts->chain);
    kfree(ssd2531_ts);

    return 0;
}

static struct platform_driver ssd2531_driver = {
    .driver     = {
        .name   = "s3c-ssd2531-ts",
    },
    .probe      = ssd2531ts_probe,
    .remove     = ssd2531ts_remove,
    //.suspend    = ssd2531ts_suspend,
    //.resume     = ssd2531ts_resume,
};


static struct platform_device ssd2531_device = {
        .name      = "s3c-ssd2531-ts",
        .id        = -1,
};

static int __init ssd2531ts_init(void)
{
    int res;
	
    if(have_cap_touchscreen != 0)
    	return 0;

    printk("s5pv210 ssd2531 Touch Driver\n");

	/* enable cap touchscreen power */
/*
	s3c_gpio_setpull(S5PV210_GPH2(3), S3C_GPIO_PULL_UP);
	s3c_gpio_cfgpin(S5PV210_GPH2(3), S3C_GPIO_SFN(1));
	gpio_set_value(S5PV210_GPH2(3), 1);
*/
	res = platform_device_register(&ssd2531_device);
    if(res)
    {
         printk("failed to register SSD2531 ts device\n");
    }

	res =  platform_driver_register(&ssd2531_driver);
    if(res)
    {
        printk("fail : platform driver %s (%d) \n", ssd2531_driver.driver.name, res);
        return res;
    }

    return 0;
}


void __exit ssd2531ts_exit(void)
{
    platform_driver_unregister(&ssd2531_driver);
	i2c_del_driver(&ak_i2c_driver);
}

module_init(ssd2531ts_init);
module_exit(ssd2531ts_exit);

MODULE_AUTHOR("linux@telechips.com");
MODULE_DESCRIPTION("telechips ssd2531 touch driver");
MODULE_LICENSE("GPL");

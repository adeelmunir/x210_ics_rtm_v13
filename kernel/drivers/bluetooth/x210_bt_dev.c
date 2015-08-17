/*
 * File:        drivers/char/x210_bt_dev.c
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/gpio.h>
#include <linux/platform_device.h>

#include <asm/mach-types.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio-bank.h>
#include <mach/regs-gpio.h>
#include <mach/gpio-smdkv210.h>
#include <mach/x210_gpio_reg.h>

#include <linux/proc_fs.h>
#define BT_PROC_RESUME_NAME "bt_resume"
#define BT_PROC_NAME "bluetooth_x210"


#define BT_DEV_ON 1
#define BT_DEV_OFF 0

#define BT_DEV_MAJOR_NUM 234
#define BT_DEV_MINOR_NUM 1

#define IOCTL_BT_DEV_POWER _IO(BT_DEV_MAJOR_NUM, 100)
#define IOCTL_BT_DEV_SPECIFIC _IO(BT_DEV_MAJOR_NUM, 101)
#define IOCTL_BT_DEV_CTRL	_IO(BT_DEV_MAJOR_NUM, 102)

#define CSR_MODULE                  0x12
#define BRCM_MODULE                 0x34
#define RDA_MODULE                  0x56

#define BT_LDO_OFF	0x2
#define BT_LDO_ON	0x3

#define BT_DEBUG_F
//#undef BT_DEBUG_F

#ifdef BT_DEBUG_F
//#define BT_DEBUG(fmt,args...)  printk(KERN_WARNING"[ x210_bt_dev ]: " fmt, ## args)
#define BT_DEBUG(fmt,args...)
#else
#define BT_DEBUG(fmt,args...)		do{ }while(0)
#endif

#define DEV_NAME "ut_bt_dev"

#define BT_LDO_PIN S5PV210_GPA1(3) //lqm changed. TXD3 to ldo pin
#define BT_POWER_PIN S5PV210_GPC1(1)


static struct class *bt_dev_class;

typedef struct {
	int module;  // 0x12:CSR, 0x34:Broadcom, 0x56 RDA58XX
	int resume_flg;
	int TMP2;
} x210_bt_info_t;


static int s32_resume_flg = 0;
static int s32_ldo_flg = 0;
#if 0
#include <linux/i2c.h>

#define RDA5870E_REG_INDEX    16
#define RDA_RF_ADRW 0x2c
#define RDA_RF_ADRR 0x2d

#define RDA_CORE_ADRW 0x2a
#define RDA_CORE_ADRR 0x2b

struct rda5870e_device {
	struct i2c_client		*i2c_client;

	/* driver management */
	int 					users;

	/* buffer locking */
	struct mutex			mutex;

	uint8					rda_reg[RDA5870E_REG_INDEX];
	uint16					ChipID;
};

//================================================================
int rda5870e_Set_Register(struct rda5870e_device *device, int reqnr)
{
	int i;
	uint8 wData[RDA5870E_REG_INDEX];

	for (i=0; i<reqnr; i++) {
		wData[i] = device->rda_reg[i];
	}

	if (i2c_master_send(device->i2c_client, wData, reqnr) != reqnr) {
		printk(KERN_INFO "%s : i2c write error !!!\n", __func__);
		return -EIO;
	}

	return 0;
}

//================================================================
int rda5870e_Get_Register(struct rda5870e_device *device, uint8 *buffer, int count)
{
	if (i2c_master_recv(device->i2c_client, buffer, count) != count) {
		printk(KERN_INFO "%s : i2c read error !!!\n", __func__);
		return -EIO;
	}

	return 0;
}

uint16 RDA5870e_GetChipID(struct rda5870e_device *device)
{
	uint8 RDAFM_reg_data[6] = {0};
	uint16 cChipID;

	RDAFM_reg_data[0] = 0x00;
	RDAFM_reg_data[1] = 0x00;
	memcpy(device->rda_reg, RDAFM_reg_data, 2);
	rda5870e_Set_Register(device, 2);
	msleep(50);

	rda5870e_Get_Register(device, &(RDAFM_reg_data[0]), 6);
	cChipID = (RDAFM_reg_data[4]*0x100) + RDAFM_reg_data[5];

	return cChipID;
}
#endif

static void x210_bt_ldo_power_ctr(int onoff)
{
	if(onoff){
		s3c_gpio_cfgpin(BT_LDO_PIN, S3C_GPIO_SFN(1));//GPA1_3 output
		s3c_gpio_setpull(BT_LDO_PIN, S3C_GPIO_PULL_UP);
		gpio_set_value(BT_LDO_PIN, 1);
		msleep(5);
		gpio_set_value(BT_LDO_PIN, 1);
		msleep(50);
		s32_ldo_flg = BT_LDO_ON;
	} else {
		gpio_set_value(BT_LDO_PIN, 0);
		msleep(5);
		s3c_gpio_cfgpin(BT_LDO_PIN, S3C_GPIO_SFN(1));//GPA1_3 output
		s3c_gpio_setpull(BT_LDO_PIN, S3C_GPIO_PULL_UP);
		gpio_set_value(BT_LDO_PIN, 0);
		msleep(5);
		s32_ldo_flg = BT_LDO_OFF;
	}
	
}

static int x210_bt_dev_open(struct inode *inode, struct file *file)
{
	BT_DEBUG("%s();\n", __func__);
	return 0;
}

static int x210_bt_dev_release(struct inode *inode, struct file *file)
{
	BT_DEBUG("%s();\n", __func__);
	return 0;
}

int x210_bt_power33_control(int on_off)
{
	BT_DEBUG("##%s(%d)===power_33===\n", __func__, on_off);
#if 1	
	if(on_off == BT_DEV_ON) {
		//write_power_item_value(POWER_BLUETOOTH, 1);
		gpio_direction_output(BT_POWER_PIN, 1);
		msleep(10);
		gpio_direction_output(BT_POWER_PIN, 0);
		//write_power_item_value(POWER_BLUETOOTH, 0);
		msleep(500);
		gpio_direction_output(BT_POWER_PIN, 1);
		//write_power_item_value(POWER_BLUETOOTH, 1);
		msleep(100);
		
	} else if(on_off==BT_DEV_OFF) {
//		gpio_direction_output(BT_POWER_PIN, 1);
		//write_power_item_value(POWER_BLUETOOTH, 0);
//		msleep(200);
	}
#endif

	return 0;
}


static int x210_bt_get_info(x210_bt_info_t* arg)
{
	x210_bt_info_t *info_t;
	int module_t;
	
	info_t = (x210_bt_info_t *)arg;
	copy_from_user(info_t, (x210_bt_info_t *)arg, sizeof(x210_bt_info_t));
	module_t = 0;
	
	module_t = RDA_MODULE;

	info_t->module = module_t;
	info_t->resume_flg = s32_resume_flg;
	info_t->TMP2 = 0;
	BT_DEBUG(" module[%d, %d, %d]\n", module_t, s32_resume_flg);
	
	copy_to_user((x210_bt_info_t *)arg, info_t, sizeof(x210_bt_info_t));

//	s32_resume_flg = 0;
	return 0;
}


int x210_bt_dev_ioctl(struct file *file,
                 unsigned int cmd, void *arg)
{
	int *parm1;

	memset(&parm1, 0, sizeof(int));
	BT_DEBUG(" x210_bt_dev_ioctl cmd[%d] arg[%d]\n", cmd, arg);	

	switch(cmd)
	{
		case IOCTL_BT_DEV_POWER:	//power		NU
			parm1 = (int*)arg;
			BT_DEBUG(" IOCTL_BT_DEV_POWER    cmd[%d] parm1[%d]\n", cmd, *parm1);
			x210_bt_power33_control(*parm1);
			break;

		case IOCTL_BT_DEV_SPECIFIC:
			printk(" IOCTL_BT_DEV_SPECIFIC    cmd[%d]\n", cmd);
			x210_bt_get_info((x210_bt_info_t*)arg);
			break;

		case IOCTL_BT_DEV_CTRL://ldo
    			parm1 = (int*)arg;
			//BT_DEBUG(" IOCTL_BT_DEV_CTRL    cmd[%d] parm1[%d]\n", cmd, *parm1);
			printk("*********x210********* IOCTL_BT_DEV_CTRL    cmd[%d] parm1[%d]\n", cmd, *parm1);
			x210_bt_ldo_power_ctr(*parm1);
			break;

		default :
			printk(KERN_WARNING" default  x210_bt_dev_ioctl cmd[%d]\n", cmd);
			break;
	}

	return 0;
}

struct file_operations x210_bt_dev_ops = {
    .owner      = THIS_MODULE,
    .unlocked_ioctl      = x210_bt_dev_ioctl,
    .open       = x210_bt_dev_open,
    .release    = x210_bt_dev_release,
};


static struct platform_device x210_bt_device = {
	.name		= "bt_sleep",
};

static struct platform_device *devices[] __initdata = {
	&x210_bt_device,
};


static int x210_sleep_probe(struct platform_device *pdev)
{
	printk("%s(); name =%d\n", __func__, pdev->name);
	
	return 0;
}

static int x210_sleep_remove(struct platform_device *pdev)
{
	printk("%s();\n", __func__);
	return 0;
}

static int x210_suspend(struct platform_device *dev, pm_message_t state)
{
	printk(KERN_ALERT, "%s();\n", __func__);
	//gpio_direction_output(BT_LDO_PIN, 1);
	s3c_gpio_cfgpin(BT_LDO_PIN, S3C_GPIO_SFN(1));//GPA1_3 output
	s3c_gpio_setpull(BT_LDO_PIN, S3C_GPIO_PULL_UP);
	gpio_set_value(BT_LDO_PIN, 1);
	return 0;
}

static int x210_resume(struct platform_device *dev)
{
	printk(KERN_ALERT, "%s();\n", __func__);
	s3c_gpio_cfgpin(BT_LDO_PIN, S3C_GPIO_SFN(1));//GPA1_3 output
	s3c_gpio_setpull(BT_LDO_PIN, S3C_GPIO_PULL_UP);
	gpio_set_value(BT_LDO_PIN, 1);
	s32_resume_flg = s32_ldo_flg;
	return 0;
}

static struct platform_driver bt_sleep_driver = {
	.probe = x210_sleep_probe,
	.remove = x210_sleep_remove,
	.driver = {
		.name = "bt_sleep",
		.owner = THIS_MODULE,
	},
	.suspend = x210_suspend,
	.resume = x210_resume,
};

static int bt_switch_writeproc(struct file *file,const char *buffer,
                           unsigned long count, void *data)
{
    int value; 
    value = 0; 
    sscanf(buffer, "%d", &value);

    s32_resume_flg = value;
    
    return count;
}

static int bt_switch_readproc(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
    int len;
    len = sprintf(page, "%d\n", (s32_resume_flg==BT_LDO_ON) ? 1 : 0);

    if (off + count >= len)
        *eof = 1;	
    
    if (len < off)
        return 0;
    
    *start = page + off;

    s32_resume_flg = 0;
	
    return ((count < len - off) ? count : len - off);
}

static int __init bt_init_module(void)
{
	int ret;
	extern struct proc_dir_entry proc_root;
	struct proc_dir_entry *root_entry;
	struct proc_dir_entry * s_proc = NULL; 

	root_entry = proc_mkdir(BT_PROC_NAME, &proc_root);
	s_proc = create_proc_entry(BT_PROC_RESUME_NAME, 0666, root_entry);
	if (s_proc != NULL){
	    s_proc->write_proc = bt_switch_writeproc;
	    s_proc->read_proc = bt_switch_readproc;
	}

	ret = register_chrdev(BT_DEV_MAJOR_NUM, DEV_NAME, &x210_bt_dev_ops);

	bt_dev_class = class_create(THIS_MODULE, DEV_NAME);
	device_create(bt_dev_class, NULL, MKDEV(BT_DEV_MAJOR_NUM, BT_DEV_MINOR_NUM), NULL, DEV_NAME);

	s3c_gpio_cfgpin(BT_LDO_PIN, S3C_GPIO_SFN(1));//GPA1_3 output
	s3c_gpio_setpull(BT_LDO_PIN, S3C_GPIO_PULL_UP);
	//gpio_set_value(BT_LDO_PIN, 0);
	gpio_set_value(BT_LDO_PIN, 1);//lqm test
	gpio_request(BT_POWER_PIN, "GPC1");
	gpio_direction_output(BT_POWER_PIN, 1);
	
	if(ret < 0){
		printk("[%d]fail to register the character device\n", ret);
		return ret;
	}

	platform_add_devices(devices, ARRAY_SIZE(devices));

	ret = platform_driver_register(&bt_sleep_driver);
	if(ret < 0) {
		return ret;
	}
	
	return ret;
}

static void __exit bt_cleanup_module(void)
{
    printk("x210_bt_dev cleanup_module\n");
    unregister_chrdev(BT_DEV_MAJOR_NUM, DEV_NAME);
    platform_driver_unregister(&bt_sleep_driver);
}

module_init(bt_init_module);
module_exit(bt_cleanup_module);

MODULE_AUTHOR("liuqiming");
MODULE_DESCRIPTION("x210 bluetooth driver");
MODULE_LICENSE("GPL");



#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <asm/uaccess.h>


#include <asm/io.h> 
#include <linux/timer.h> 
#include <linux/sched.h> 
#include <linux/timer.h> 
#include <mach/hardware.h> 
#include <asm/io.h> 
#include <plat/gpio-cfg.h> 
#include <mach/gpio.h>
#include <mach/gpio-smdkc110.h>
#include <mach/regs-gpio.h>
#include <mach/map.h>
#include <linux/gpio.h> 
#include <linux/cdev.h> 
#include <linux/delay.h>
#include <linux/gpio.h> 
#include <asm/delay.h>
#include <plat/gpio-cfg.h> 
#include <mach/gpio.h>
#include <mach/gpio-smdkc110.h>
#include <mach/regs-gpio.h>

int initialize_Module(void);
void Cleanup_Module(void);

int Device_Open(struct inode * , struct file *);
int Device_Release(struct inode * , struct file *);
ssize_t Device_Write(struct file * , const char *, size_t, loff_t *);
static ssize_t Device_Read(struct file * , char *, size_t, loff_t *);
static int Device_ioctl(struct inode * , struct file *, unsigned int, unsigned long);
static int init_device(void);

#define SUCCESS 0
#define Kernel_info "KERNEL INFO"
#define Kernel_Alert "KERN ALERT"
#define DEVICE_NAME "jni-test"
#define BUF_LEN 80

#define GPIO_LED_Strip1	S5PV210_GPH0(5)		//GPIO Side  Buttons Led D22
#define GPIO_BUTTON	S5PV210_GPH0(3)		// GPIO side Down Button


//static int major;
static int major = 100;					// for static value, assigned "0" for dynamic number
static int device_open  = 0;

static char msg[BUF_LEN];
static char *msg_Ptr;

static struct class *sample_dev_class; 	//required  #include <linux/device.h>

static const struct file_operations fops = {
	.owner    = THIS_MODULE,
	.open     = Device_Open,
	.read     = Device_Read,
	.write    = Device_Write,
	.unlocked_ioctl    = Device_ioctl,
	.release  = Device_Release,
};


int Device_Open(struct inode *inode , struct file *file)
{
	static int counter = 0;
	if(device_open)
		return -EBUSY;
		
	device_open++;
	sprintf(msg , "---module open %d times\n" , counter++);
	msg_Ptr = msg;
	try_module_get(THIS_MODULE);
	
	return SUCCESS;
}

int Device_Release(struct inode *inode , struct file *file)
{
	device_open--;
	module_put(THIS_MODULE);
	return 0;
}

static ssize_t Device_Read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
	int bytes_read = 0;
	static int counter = 1;
	printk(Kernel_Alert "-----Read function called inside thread 2 \t%d times----\n", counter);
	counter++;
/*	for(j = 0; j < 5; j++)	
	{
		gpio_set_value(GPIO_LED_Strip1, 0);
		mdelay(400);
		gpio_set_value(GPIO_LED_Strip1, 1);
		mdelay(400);
	} for blinking led */

	// while (gpio_get_value(GPIO_BUTTON) )
	
/*	if (GPIO_BUTTON == 1)
		gpio_set_value(GPIO_LED_Strip1, 0);
	if  (GPIO_BUTTON == 0)
		gpio_set_value(GPIO_LED_Strip1, 1);
	else 
	
		for(j = 0; j < 5; j++)
		{
			gpio_set_value(GPIO_LED_Strip1, 0);
			mdelay(500);
			gpio_set_value(GPIO_LED_Strip1, 1);
			mdelay(500);
		}*/
		
	if(*msg_Ptr == 0)
		return 0;
		
	while(length && *msg_Ptr){
		
		put_user(*(msg_Ptr) , buffer++);
		length--;
		bytes_read++;
	}
	return bytes_read++;
}

ssize_t Device_Write(struct file *filp, const char *buffer, size_t length, loff_t *offset)
{
	static int i;
	int ret;
	
	ret = gpio_request(S5PV210_GPH0(5), "GPH05");	
		printk(Kernel_Alert "GPIO request failed for GPH0 %d\n", ret);
		
	ret = gpio_request(S5PV210_GPB(4), "GPB");

		printk(Kernel_Alert "GPIO request failed for GPB4 %d\n", ret);
		
	ret = gpio_request(S5PV210_GPB(5), "GPB");

		printk(Kernel_Alert "GPIO request failed for GPB5 %d\n", ret);
		
	ret = gpio_request(S5PV210_GPB(6), "GPB");

		printk(Kernel_Alert "GPIO request failed for GPB6 %d\n", ret);
	
	ret = gpio_request(S5PV210_GPB(7), "GPB");
	
		printk(Kernel_Alert "GPIO request failed for GPB7 %d\n", ret);
	
	gpio_set_value(GPIO_LED_Strip1, 0);
	mdelay(600);
	gpio_set_value(GPIO_LED_Strip1, 1);
	mdelay(600);

	return 0;
}

static int Device_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg) 
{
	printk(Kernel_Alert "---:P ioctl Called ----\n");
	return 0;
}


/////////////////////////Module Initialization//////////////////////////
int initialize_Module(void)
{
	int ret = -ENODEV;
	
///////////Initialize Led D22 GPIO as Output////////////////
	s3c_gpio_cfgpin(GPIO_LED_Strip1, S3C_GPIO_SFN(1));	       
	s3c_gpio_setpull(GPIO_LED_Strip1, S3C_GPIO_PULL_UP);
	gpio_set_value(GPIO_LED_Strip1, 1);

///////////Initialize Back button GPIO as Input////////////////
	s3c_gpio_cfgpin(GPIO_BUTTON, S3C_GPIO_SFN(0));
	s3c_gpio_setpull(GPIO_BUTTON, S3C_GPIO_PULL_UP);
	//gpio_set_value(GPIO_BUTTON, 1);


	ret = register_chrdev(major , DEVICE_NAME , &fops);
	if(ret < 0)
	{
		printk(Kernel_Alert "---Registering char device failed with %d\n" , major);
		return major;
	}		
	sample_dev_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(sample_dev_class)) 
	{
		unregister_chrdev(major, DEVICE_NAME); 
		return PTR_ERR(sample_dev_class); 
	}
	device_create(sample_dev_class , NULL, MKDEV(major, 0), NULL, DEVICE_NAME); 	//Create a node under /dev/class/[DEVICE_NAME]
	
	printk(Kernel_info "---Registering Sample Driver Successful with %d\n" , major);
	printk(Kernel_info "---'mknod /dev/%s c %d 0'.\n" , DEVICE_NAME , major);
	return SUCCESS;
}

static int __init init_device(void) 
{ 
	int ret = -ENODEV; //Call the function 
	printk(Kernel_info "------------for checking purpose----------------Sample_Driver_init---\n");
	ret = initialize_Module(); 

	if(ret)
	{
		printk(Kernel_info "Sample_Driver_init fail!\n");
		return ret;
	}

	
	return 0;
}

///////////////////////////Module Cleaning//////////////////////////////
static void __exit cleanup_device(void)
{
	device_destroy(sample_dev_class, MKDEV(major, 0)); 
	class_destroy(sample_dev_class); 
	unregister_chrdev(major, DEVICE_NAME); 
	printk(Kernel_info "---Unregistering Sample Driver Successful\n");
}

MODULE_AUTHOR("haseeb zahid");
MODULE_LICENSE("GPL"); 
MODULE_DESCRIPTION("traning Sample"); 
MODULE_ALIAS_CHARDEV(major, 0); 

module_init(init_device);
module_exit(cleanup_device);

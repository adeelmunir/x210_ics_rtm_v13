/*
 * driver/input/misc/gsensor.c
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <mach/gpio.h>
#include <mach/gpio-smdkv210.h>
#include <plat/gpio-cfg.h>
#include <linux/irq.h>


#define GSENSOR_I2C_BUS			(0)
#define GSENSOR_I2C_ADDRESS		(0x0f)

struct gsensor {
	struct workqueue_struct * wq;
	struct work_struct work;
	struct timer_list timer;
	struct i2c_client * client;
	struct input_dev * input_dev;

	unsigned int rate;
};

static int gsensor_i2c_write_reg(struct i2c_client * client, uint8_t reg, uint8_t val)
{
	uint8_t cmd[2];

    cmd[0] = reg;
    cmd[1] = val;
    i2c_master_send(client, cmd, 2);

    return 1;
}

static int gsensor_i2c_read_reg(struct i2c_client * client, uint8_t reg, uint8_t * val)
{
    i2c_master_send(client, &reg, 1);
    i2c_master_recv(client, val, 1);

    return  1;
}

static void gsensor_power_on(void)
{
	/*
	 * enable gsensor power
	 */
	s3c_gpio_setpull(GPIO_GENSOR_POWER_EN, S3C_GPIO_PULL_UP);
	s3c_gpio_cfgpin(GPIO_GENSOR_POWER_EN, S3C_GPIO_SFN(1));
	gpio_set_value(GPIO_GENSOR_POWER_EN, 1);

	mdelay(20);
}

static int gsensor_initial(struct i2c_client * client)
{
	uint8_t val;
	int cnt = 20;

	/* sleep */
	gsensor_i2c_read_reg(client, 0x1b, &val);
	val = val & 0x7f;
	gsensor_i2c_write_reg(client, 0x1b, val);

	/* reset */
	gsensor_i2c_read_reg(client, 0x1d, &val);
	val = val | 0x80;
	gsensor_i2c_write_reg(client, 0x1d, val);

	/* wait for software reset */
	do{
		mdelay(10);
		gsensor_i2c_read_reg(client, 0x1d, &val);
	} while( ((val & 0x80) == 0x80) && (--cnt > 0) );

	/* initial regs */
	gsensor_i2c_write_reg(client, 0x1b, 0x00);
	gsensor_i2c_write_reg(client, 0x1e, 0x38);
	gsensor_i2c_write_reg(client, 0x21, 0x04);
	gsensor_i2c_write_reg(client, 0x1b, 0x60);
	gsensor_i2c_write_reg(client, 0x1b, 0xe0);

	/* read chip id */
	gsensor_i2c_read_reg(client, 0x0f, &val);
	printk("gsensor's ID: 0x%02x\n", val);

	return 0;
}

static void gsensor_work_func(struct work_struct * work)
{
	struct gsensor * g = container_of(work, struct gsensor, work);
	uint8_t x_l, x_h, y_l, y_h, z_l, z_h;
	int x_sign,y_sign,z_sign;
	int x, y, z;

	gsensor_i2c_read_reg(g->client, 0x06, &x_l);
	gsensor_i2c_read_reg(g->client, 0x07, &x_h);
	gsensor_i2c_read_reg(g->client, 0x08, &y_l);
	gsensor_i2c_read_reg(g->client, 0x09, &y_h);
	gsensor_i2c_read_reg(g->client, 0x0a, &z_l);
	gsensor_i2c_read_reg(g->client, 0x0b, &z_h);

	x_l = x_l >> 4;
	x = (x_h << 4) + x_l;
	x_sign = x >> 11;
	if (x_sign == 1)
	{
		x = ((~(x) + 0x01) & 0x0FFF);
		x = -(x);
	}

	y_l = y_l >> 4;
	y = (y_h << 4) + y_l;
	y_sign = y >> 11;
	if (y_sign == 1)
	{
		y = ((~(y) + 0x01) & 0x0FFF);
		y = -(y);
	}

	z_l = z_l >> 4;
	z = (z_h << 4) + z_l;
	z_sign = z >> 11;
	if (z_sign == 1)
	{
		z = ((~(z) + 0x01) & 0x0FFF);
		z = -(z);
	}

	input_event(g->input_dev, EV_ABS, ABS_X, -y);
	input_event(g->input_dev, EV_ABS, ABS_Y, -x);
	input_event(g->input_dev, EV_ABS, ABS_Z, -z);
	input_sync(g->input_dev);

	//printk("gsensor: %d, %d, %d\n", x, y, z);

	mod_timer(&(g->timer), jiffies + msecs_to_jiffies(g->rate));
}

static void timer_handler(unsigned long data)
{
	struct gsensor * g = (struct gsensor *)data;
	queue_work(g->wq, &g->work);
}

static ssize_t gsensor_rate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gsensor * g = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", g->rate);
}

static ssize_t gsensor_rate_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct gsensor * g = dev_get_drvdata(dev);
	unsigned long rate;
	int err;

	err = strict_strtoul(buf, 10, &rate);
	if(err)
		return err;

	g->rate = rate;
	return count;
}

static DEVICE_ATTR(rate, 0666, gsensor_rate_show, gsensor_rate_store);

static struct attribute * gensor_attributes[] = {
	&dev_attr_rate.attr,
	NULL,
};

static struct attribute_group gensor_attr_group = {
	.name	= NULL,
	.attrs	= gensor_attributes,
};

static int gsensor_iic_probe(struct i2c_client * client, const struct i2c_device_id * id)
{
	struct gsensor * g;
	int range;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_WORD_DATA))
		return -ENODEV;

	g = kzalloc(sizeof(*g), GFP_KERNEL);
	if(!g)
		return -ENOMEM;

	g->wq = create_workqueue("gsensor_wq");
	if(!g->wq)
	{
		kfree(g);
		return -ENOMEM;
	}

	INIT_WORK(&g->work, gsensor_work_func);
	g->client = client;
	i2c_set_clientdata(client, g);

	g->input_dev = input_allocate_device();
	if (g->input_dev == NULL)
	{
		destroy_workqueue(g->wq);
		kfree(g);
		return -ENOMEM;
	}

	g->input_dev->name = "gsensor";
	g->input_dev->phys = "input/gsensor";
	g->input_dev->id.bustype = BUS_I2C;
	g->input_dev->id.vendor = 0xDEAD;
	g->input_dev->id.product = 0xBEEF;
	g->input_dev->id.version = 10001;

	set_bit(EV_ABS, g->input_dev->evbit);
	set_bit(ABS_X, g->input_dev->absbit);
	set_bit(ABS_Y, g->input_dev->absbit);
	set_bit(ABS_Z, g->input_dev->absbit);

	/* 12-bits */
	range = 0xfff;
	input_set_abs_params(g->input_dev, ABS_X, -range, range, 3, 3);
	input_set_abs_params(g->input_dev, ABS_Y, -range, range, 3, 3);
	input_set_abs_params(g->input_dev, ABS_Z, -range, range, 3, 3);

	set_bit(EV_KEY, g->input_dev->evbit);
	set_bit(BTN_TOUCH, g->input_dev->keybit);

    ret = input_register_device(g->input_dev);
	if(ret)
	{
		destroy_workqueue(g->wq);
		kfree(g);
		return ret;
	}

	if(gsensor_initial(client) != 0)
	{
		destroy_workqueue(g->wq);
		kfree(g);
		return -1;
	}

	sysfs_create_group(&(client->dev.kobj), &gensor_attr_group);

	g->rate = 1000;
	init_timer(&(g->timer));
	g->timer.data = (unsigned long) g;
	g->timer.function = timer_handler;
	g->timer.expires = jiffies + msecs_to_jiffies(g->rate);
	add_timer(&(g->timer));

	printk("Found Gsensor hardware\n");
	return 0;
}

static int gsensor_iic_remove(struct i2c_client * client)
{
	struct gsensor * g = i2c_get_clientdata(client);

	sysfs_remove_group(&(client->dev.kobj), &gensor_attr_group);
	del_timer_sync(&(g->timer));
	i2c_set_clientdata(client, NULL);
	input_unregister_device(g->input_dev);
	if(g->input_dev)
		kfree(g->input_dev);
	kfree(g);

	return 0;
}

static int gsensor_iic_suspend(struct i2c_client * client, pm_message_t message)
{
	return 0;
}

static int gsensor_iic_resume(struct i2c_client * client)
{
	gsensor_power_on();
	gsensor_initial(client);

	return 0;
}

static const struct i2c_device_id gsensor_iic_id[] = {
	{ "gsensor-iic", 0},
	{ }
};

static struct i2c_driver gsensor_iic_driver = {
	.driver		= {
		.name	= "gsensor-iic",
	},

	.probe		= gsensor_iic_probe,
	.remove		= gsensor_iic_remove,
	.suspend	= gsensor_iic_suspend,
	.resume		= gsensor_iic_resume,
	.id_table	= gsensor_iic_id,
};

static int gsensor_probe(struct platform_device * pdev)
{
	struct i2c_adapter * adapter;
	struct i2c_client * client;
	struct i2c_board_info info;
	int ret;

	adapter = i2c_get_adapter(GSENSOR_I2C_BUS);
	if(adapter == NULL)
		return -ENODEV;

	memset(&info, 0, sizeof(struct i2c_board_info));
	info.addr = GSENSOR_I2C_ADDRESS;
	strlcpy(info.type, "gsensor-iic", I2C_NAME_SIZE);

	client = i2c_new_device(adapter, &info);
    if(!client)
    {
        printk("Unable to add I2C device for 0x%x\n", info.addr);
        return -ENODEV;
    }
    i2c_put_adapter(adapter);

	ret = i2c_add_driver(&gsensor_iic_driver);
	if(ret)
		return ret;

	return 0;
}

static int gsensor_remove(struct platform_device * pdev)
{
	i2c_del_driver(&gsensor_iic_driver);

	return 0;
}

static int gsensor_suspend(struct platform_device * dev, pm_message_t state)
{
	return 0;
}

static int gsensor_resume(struct platform_device * dev)
{
	return 0;
}

static struct platform_device gsensor_device = {
	.name	= "gsensor",
	.id		= -1,
};

static struct platform_driver gsensor_driver = {
	.driver		= {
		.name	= "gsensor",
	},
	.probe		= gsensor_probe,
	.remove		= gsensor_remove,
	.suspend	= gsensor_suspend,
	.resume		= gsensor_resume,
};

static int __init gsensor_init(void)
{
    int res;
	
	printk("Initial GSensor Driver\n");
	gsensor_power_on();

	res = platform_device_register(&gsensor_device);
	if(res)
	{
		printk("failed to register platform device\n");
	}

	res = platform_driver_register(&gsensor_driver);
	if(res != 0)
	{
		printk("fail to register platform driver\n");
		return res;
	}

    return 0;
}

void __exit gsensor_exit(void)
{
	platform_driver_unregister(&gsensor_driver);
}

module_init(gsensor_init);
module_exit(gsensor_exit);

MODULE_AUTHOR("www.9tripod.com");
MODULE_DESCRIPTION("x210 gsensor Driver");
MODULE_LICENSE("GPL");

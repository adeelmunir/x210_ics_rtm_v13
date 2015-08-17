//##########################################################################
//#                                                                        #
//#        Copyright (C) 2011 by Beijing IRTOUCHSYSTEMS Co., Ltd           #
//#                                                                        #
//#                         IRTOUCH USB Touchscreen                        #    
//#                                                                        #
//#                           Create by Smart Zhu                          #
//#                                                                        #
//##########################################################################
 

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/usb/input.h>
#include <linux/hid.h>
#include <linux/input.h>
#include <linux/cdev.h>
#include <asm/uaccess.h> 

/*
 * Version Information
 */
#define DRIVER_VERSION "v1.0"
#define DRIVER_AUTHOR "Smart Zhu <smart.ju888@gmail.com>"
#define DRIVER_DESC "IRTOUCH USB touchscreen driver"
#define DRIVER_LICENSE "GPL"

#define boolean(x,y) (x>y?1:0)

#define SET_COMMAND 0x01
#define GET_COMMAND 0x02

#define SET_COMMAND_MODE 0x71
#define SET_WORK_MODE	0x93
#define	GET_CALIB_X	0x2D
#define	SET_CALIB_X	0x2E
#define	GET_CALIB_Y	0x2B
#define	SET_CALIB_Y	0x2C
#define	GET_DEVICE_CONFIG	0x30
#define	SET_DEVICE_CONFIG	0x31

#define COMMAND_RETRY_COUNT  5

#define MAJOR_DEVICE_NUMBER 47
#define MINOR_DEVICE_NUMBER 192
#define TOUCH_DEVICE_NAME "irtouch"

#define CTLCODE_COORDINATE 0xc0
#define CTLCODE_SET_CALIB_PARA_X 0xc1
#define CTLCODE_SET_CALIB_PARA_Y 0xc2
#define CTLCODE_CALIB_START 0xc3

#define START_CALIB_FLAG 0Xf0
#define UPDATE_FLAG 0Xf1

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE(DRIVER_LICENSE);


#pragma pack(1)
struct irtouch_point
{
	unsigned char	identifier;
	short		x;
	short		y;
	short		z;
	short		width;
	short		height;
	unsigned char	status;
	unsigned char	reserver;
};

struct irtouch_pkt
{
	unsigned char	startBit;
	unsigned char	commandType;
	unsigned char	command;
	unsigned char	pkgLength;
	unsigned char	deviceId;
	struct irtouch_point	points[3];
	unsigned char	actualCounter;
	unsigned char	CRC;	
};

struct points
{
	int	x;
	int	y;
};

struct calib_param{
	long	A00;
	long	A01;
	long	A10;
	unsigned char reserve[46];
};

struct	device_config{
	unsigned char deviceID;
	unsigned char monitorID;
	unsigned char reserved[8];
	unsigned char calibrateStatus;
	unsigned char reserved1[47];
};

struct tmpPoint
{
	int x;
	int y;
	unsigned char status;
};

struct AlgContext
{
	int	actualCounter;
	struct irtouch_point inPoint[4];
};

#pragma pack()

struct irtouchusb {
	char name[128];
	char phys[64];
	struct usb_device *usbdev;
	struct input_dev *dev;
	struct urb *irq;

	struct irtouch_pkt * irpkt;
	dma_addr_t data_dma;
};

struct device_context{
	bool startCalib;
	bool isCalibX;
	bool isCalibY;
	bool calibrated;
	struct points points;
	struct AlgContext algCtx;
	struct calib_param calibX;
	struct calib_param calibY;
	struct device_config devConfig;
};

dev_t devno = 0;
struct cdev cdev;
struct device_context *devContext;
static struct class *irser_class;
static struct usb_driver irtouch_driver;
static struct file_operations irtouch_fops;

static struct usb_class_driver irtouch_class_driver = {
	.name = "irtouch",
	.fops = &irtouch_fops,
	.minor_base = MINOR_DEVICE_NUMBER,
};

static void irtouch_translatePoint(struct irtouch_point pt, int *x, int *y)
{
	*x = ((devContext->calibX.A01 * pt.y) / 10000) + ((devContext->calibX.A10 * pt.x) / 10000) + devContext->calibX.A00;
	*y = ((devContext->calibY.A01 * pt.y) / 10000) + ((devContext->calibY.A10 * pt.x) / 10000) + devContext->calibY.A00;
}

static int irtouch_build_packet(
		unsigned char 	command_type, 
		unsigned char 	command, 
		int 		device_id, 
		unsigned char * data, 
		int 		length, 
		unsigned char * out_data
		)
{
	unsigned char 	tmp = 0x55;
	unsigned char 	package[64] = {0};
	int		out_length = 0;
	int i;

	package[0] = 0xaa;
	package[1] = command_type;
	package[2] = command;
	package[3] = (unsigned char)length + 1;
	package[4] = (unsigned char)device_id;

	memcpy(&package[5],data,length);

	for(i = 0; i < 5 + length; i++)
	{
		tmp = package[i] + tmp;
	}

	package[5 + length] = (unsigned char)tmp;

	out_length = 6 + length;

	memcpy(out_data, package, out_length);

	return out_length;
}

unsigned char irtouch_send_command(struct usb_device * udev,
			unsigned char 	command_type,
			unsigned char * in_data,
			unsigned char * out_data,
			int 		length
		)
{
	unsigned char	buf[64];
	int		count = 0;
	int		ret = 0;

	memset(buf,0,sizeof(buf));

	do{
		ret = usb_control_msg(udev, 
					usb_sndctrlpipe(udev, 0), 
					0, 
					USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
					0, 
					0, 
					(char *)in_data,
					length, 
					1000);
		msleep(10);

		ret = usb_control_msg(udev, 
					usb_rcvctrlpipe(udev, 0), 
					0, 
					0xc0, 
					0, 
					0, 
					(char *)buf, 
					64, 
					1000);
		msleep(10);

		count++;

	}while((buf[1] != 0x10) && (count < COMMAND_RETRY_COUNT));

	if(count >= COMMAND_RETRY_COUNT)
	{
		return false;
	}

	memcpy(out_data,buf,sizeof(buf));

	return true;
	
}

static void irtouch_get_device_param(struct usb_device * udev)
{
	unsigned char	inData[58];
	unsigned char	outData[64];
	unsigned char	getData[64];
	int status = -1;
	int length = 0;

	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));
	memset(getData,0,sizeof(getData));

	//set touchscreen to command mode
	inData[0] = 0x01;

	length = irtouch_build_packet(SET_COMMAND,SET_COMMAND_MODE,0,inData,1,outData);
	status = irtouch_send_command(udev,SET_COMMAND,outData,getData,length);
	if(!status)
	{
		err("IRTOUCH:    %s  Set command mode failed.",__func__);
	}
	
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));
	memset(getData,0,sizeof(getData));

	inData[0] = 0xF4;

	length = irtouch_build_packet(SET_COMMAND,SET_WORK_MODE,0,inData,1,outData);
	status = irtouch_send_command(udev,SET_COMMAND,outData,getData,length);
	if(!status)
	{
		err("IRTOUCH:    %s  Set device to work status failed.",__func__);
	}

	//get device configuration
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));
	memset(getData,0,sizeof(getData));

	length = irtouch_build_packet(GET_COMMAND,GET_DEVICE_CONFIG,1,inData,0,outData);
	status = irtouch_send_command(udev,GET_COMMAND,outData,getData,length);
	if(!status)
	{
		err("IRTOUCH:    %s  Get device configuration failed.",__func__);
	}
	memcpy(&devContext->devConfig,&getData[5],sizeof(struct device_config));

	//get calib x parameter
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));
	memset(getData,0,sizeof(getData));

	length = irtouch_build_packet(GET_COMMAND,GET_CALIB_X,0,inData,0,outData);
	status = irtouch_send_command(udev,GET_COMMAND,outData,getData,length);
	if(!status)
	{
		err("IRTOUCH:    %s  Get calib X param failed.",__func__);
	}
	memcpy(&devContext->calibX,&getData[5],sizeof(struct calib_param));

	//get calib y parameter
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));
	memset(getData,0,sizeof(getData));

	length = irtouch_build_packet(GET_COMMAND,GET_CALIB_Y,0,inData,0,outData);
	status = irtouch_send_command(udev,GET_COMMAND,outData,getData,length);
	if(!status)
	{
		err("IRTOUCH:    %s  Get calib Y param failed.",__func__);
	}
	memcpy(&devContext->calibY,&getData[5],sizeof(struct calib_param));

	//set touchscreen to work mode
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));
	memset(getData,0,sizeof(getData));

	length = irtouch_build_packet(SET_COMMAND,SET_COMMAND_MODE,0,inData,1,outData);
	status = irtouch_send_command(udev,SET_COMMAND,outData,getData,length);
	if(!status)
	{
		err("IRTOUCH:    %s  Set to work mode failed.",__func__);
	}
}

static void irtouch_set_calib(struct usb_device * udev)
{
	unsigned char	inData[58];
	unsigned char	outData[64];
	unsigned char	getData[64];
	int status = -1;
	int length = 0;

	//set touchscreen to command mode
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));
	memset(getData,0,sizeof(getData));

	inData[0] = 0x01;

	length = irtouch_build_packet(SET_COMMAND,SET_COMMAND_MODE,0,inData,1,outData);
	status = irtouch_send_command(udev,SET_COMMAND,outData,getData,length);
	if(!status)
	{
		err("IRTOUCH:    %s  Set to command mode failed.",__func__);
	}

	//Set device configuration
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));

	devContext->devConfig.calibrateStatus = 1;
	memcpy(&inData,&devContext->devConfig,58);
	
	length = irtouch_build_packet(SET_COMMAND,SET_DEVICE_CONFIG,1,inData,58,outData);
	status = irtouch_send_command(udev,SET_COMMAND,outData,getData,length);
	if(!status)
	{
		err("IRTOUCH:    %s  Set device configuration failed.",__func__);
	}

	//Set calib X param
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));

	memcpy(&inData,&devContext->calibX,58);
	
	length = irtouch_build_packet(SET_COMMAND,SET_CALIB_X,0,inData,58,outData);
	status = irtouch_send_command(udev,SET_COMMAND,outData,getData,length);
	if(!status)
	{
		err("IRTOUCH:    %s  Set calib X param failed.",__func__);
	}

	//Set calib Y param
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));

	memcpy(&inData,&devContext->calibY,58);
	
	length = irtouch_build_packet(SET_COMMAND,SET_CALIB_Y,0,inData,58,outData);
	status = irtouch_send_command(udev,SET_COMMAND,outData,getData,length);
	if(!status)
	{
		err("IRTOUCH:    %s  Set calib Y param failed.",__func__);
	}

	//set touchscreen to work mode
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));

	length = irtouch_build_packet(SET_COMMAND,SET_COMMAND_MODE,0,inData,1,outData);	
	status = irtouch_send_command(udev,SET_COMMAND,outData,getData,length);
	if(!status)
	{
		err("IRTOUCH:    %s  Set to work mode failed.",__func__);
	}
}

static int irtouch_open(struct inode * inode, struct file * filp)
{
	struct irtouchusb * irtouch;
	struct usb_interface * intf;
	
	dbg("IRTOUCH:  init_open");

	intf = usb_find_interface(&irtouch_driver, MINOR_DEVICE_NUMBER);
	if(!intf)
	{
		err("IRTOUCH:    %s usb_find_interface ERROR!",__func__);
		return -1;
	}

	irtouch = usb_get_intfdata(intf);
	if(!irtouch)
	{
		err("IRTOUCH:    %s touch is NULL!",__func__);
		return -1;
	}

	filp->private_data = irtouch;

	return 0;
}

static int irtouch_release(struct inode * inode, struct file * filp)
{
	dbg("IRTOUCH:  irtouch_release");
	return 0;
}

static long irtouch_ioctl(struct file * filp, unsigned int ctl_code, unsigned long ctl_param)
{
	unsigned char status;
	int ret = -1;
	struct irtouchusb * irtouch;

	dbg("IRTOUCH:    irtouch_ioctl");

	if(NULL == filp->private_data)
	{
		err("IRTOUCH:    %s private_data is NULL!",__func__);
		return ret;
	}

	irtouch = filp->private_data;

	switch(ctl_code)
	{
		case CTLCODE_CALIB_START:				
			ret = copy_from_user(&status, (unsigned char *)ctl_param, sizeof(unsigned char));
			if(ret == 0)
			{
				if(status == 0x01)
				{
					devContext->startCalib = true;
				}
				else
				{
					devContext->startCalib = false;
				}
			}	
			
			break;

		case CTLCODE_COORDINATE:
			ret = copy_to_user((struct points *)ctl_param, &devContext->points, sizeof(struct points));
			if(ret != 0)
			{
				err("IRTOUCH:    %s <CTLCODE_COORDINATE>copy_to_user failed!",__func__);
			}
			break;
	
		case CTLCODE_SET_CALIB_PARA_X:
			devContext->isCalibX = true;
			ret = copy_from_user(&devContext->calibX, (struct calib_param *)ctl_param, sizeof(struct calib_param));
			if(ret != 0)
			{
				err("IRTOUCH:    %s <CTLCODE_SET_CALIB_PARA_X>copy_to_user failed!",__func__);
			}
			break;

		case CTLCODE_SET_CALIB_PARA_Y:
			devContext->isCalibY = true;
			ret = copy_from_user(&devContext->calibY, (struct calib_param *)ctl_param, sizeof(struct calib_param));
			if(ret != 0)
			{
				err("IRTOUCH:    %s <CTLCODE_SET_CALIB_PARA_Y>copy_to_user failed!",__func__);
			}
			break;

		default:
			break;
	}

	if(devContext->isCalibX && devContext->isCalibY)
	{
		devContext->calibrated = true;
		devContext->isCalibX = false;
		devContext->isCalibY = false;
		irtouch_set_calib(irtouch->usbdev);
	}

	return 0;
}

static bool processInputData(struct irtouchusb *ir,struct AlgContext *pAlgCtx)
{
	int i = 0;

	if(((ir->irpkt->actualCounter & 0x1F) == 4) && ((ir->irpkt->actualCounter >> 5) == 0))
	{
		for(i=0;i<3;i++)
		{
			pAlgCtx->inPoint[i] = ir->irpkt->points[i];
		}
		
		return false;		
	}
	else if(((ir->irpkt->actualCounter & 0x1F) == 0) && (ir->irpkt->actualCounter >> 5 == 1))
	{
		pAlgCtx->inPoint[3] = ir->irpkt->points[0];
		pAlgCtx->actualCounter = 4;
	}
	else
	{
		pAlgCtx->actualCounter = ir->irpkt->actualCounter & 0x1F;

		for(i=0;i<(ir->irpkt->actualCounter & 0x1F);i++)
		{
			pAlgCtx->inPoint[i] = ir->irpkt->points[i];
		}
	}
	
	return true;
}

static void irtouch_irq(struct urb *urb)
{
	struct irtouchusb *ir = urb->context;
	struct input_dev *dev = ir->dev;
	bool ret = false;
	int status;
	int i;
	int x = 0, y = 0;

	switch (urb->status) {
		case 0:            /* success */
			break;
		case -ECONNRESET:  /* unlink */
		case -ENOENT:
		case -ESHUTDOWN:
			return;    /* -EPIPE:  should clear the halt */
		default:	   /* error */
			goto resubmit;
	}
	
	ret = processInputData(ir,&devContext->algCtx);

	if(ret)
	{		
		for(i=0;i<devContext->algCtx.actualCounter;i++)
		{
			
			if(devContext->algCtx.inPoint[i].status == 0x07)
			{
				x = devContext->algCtx.inPoint[i].x;
				y = devContext->algCtx.inPoint[i].y;
				
				if(devContext->startCalib)
				{
					devContext->points.x = devContext->algCtx.inPoint[0].x;
					devContext->points.y = devContext->algCtx.inPoint[0].y;
				}
				
				if(devContext->devConfig.calibrateStatus && !devContext->startCalib)
				{
					irtouch_translatePoint(devContext->algCtx.inPoint[i],&x,&y);
				}
					
				input_report_abs(dev,ABS_MT_TRACKING_ID,devContext->algCtx.inPoint[i].identifier);
				input_report_abs(dev,ABS_MT_POSITION_X, x);
				input_report_abs(dev,ABS_MT_POSITION_Y, y);
				input_report_abs(dev,ABS_MT_TOUCH_MAJOR, max(x,y));
				input_report_abs(dev,ABS_MT_TOUCH_MINOR, min(x,y));
			}
			else
			{
				input_report_abs(dev,ABS_MT_TOUCH_MAJOR, 0);
			}
			input_mt_sync(dev);
		}
	
		input_sync(dev);
	}

resubmit:
	status = usb_submit_urb (urb, GFP_ATOMIC);
	if (status)
		err ("can't resubmit intr, %s-%s/input0, status %d",
				ir->usbdev->bus->bus_name,
				ir->usbdev->devpath, status);
}

static struct file_operations irtouch_fops = {
	.owner = THIS_MODULE,
	.open = irtouch_open,
	.unlocked_ioctl = irtouch_ioctl,
	.release = irtouch_release,
};

static int irtouch_open_device(struct input_dev *dev)
{
	struct irtouchusb *ir = input_get_drvdata(dev);

	ir->irq->dev = ir->usbdev;
	if (usb_submit_urb(ir->irq, GFP_KERNEL))
		return -EIO;

	return 0;
}

static void irtouch_close_device(struct input_dev *dev)
{
	struct irtouchusb *ir = input_get_drvdata(dev);

	usb_kill_urb(ir->irq);
}

static bool irtouch_mkdev(void)
{
	int retval;

	printk("******************* test11\r\n");

	//create device node
	devno = MKDEV(MAJOR_DEVICE_NUMBER,MINOR_DEVICE_NUMBER);

	retval = register_chrdev_region(devno,1,TOUCH_DEVICE_NAME);
	if(retval < 0)
	{
		err("IRTOUCH:    %s register chrdev error.",__func__);
		return false;
	}

	printk("******************* test22\r\n");

	cdev_init(&cdev,&irtouch_fops);
	cdev.owner = THIS_MODULE;
	cdev.ops = &irtouch_fops;
	retval = cdev_add(&cdev,devno,1);

	if(retval)
	{
		err("IRTOUCH:    %s  Adding char_reg_setup_cdev error=%d",__func__,retval);
		return false;
	}

	printk("******************* test33\r\n");

	irser_class = class_create(THIS_MODULE, TOUCH_DEVICE_NAME);
	if(IS_ERR(irser_class))
	{
		err("IRTOUCH:    %s class create failed.",__func__);
		return false;
	}

	printk("******************* test44\r\n");
	
	device_create(irser_class,NULL,MKDEV(MAJOR_DEVICE_NUMBER,MINOR_DEVICE_NUMBER),NULL,TOUCH_DEVICE_NAME);

	return true;
}

static int irtouch_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	struct usb_host_interface *interface;
	struct usb_endpoint_descriptor *endpoint;
	struct irtouchusb *ir;
	struct input_dev *input_dev;
	int pipe;
	unsigned int rsize = 0;
	int error = -ENOMEM;
	char *rdesc;

	interface = intf->cur_altsetting;

	if (interface->desc.bNumEndpoints != 1)
		return -ENODEV;

	endpoint = &interface->endpoint[0].desc;
	if (!(endpoint->bEndpointAddress & USB_DIR_IN))
		return -ENODEV;
	if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) != USB_ENDPOINT_XFER_INT)
		return -ENODEV;

	pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);

	ir = kzalloc(sizeof(struct irtouchusb), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!ir || !input_dev)
		goto fail1;

	if (!(devContext = kmalloc(sizeof(struct device_context), GFP_KERNEL)))  
		return -ENOMEM; 

	memset(devContext,0,sizeof(struct device_context));

	ir->irpkt = usb_alloc_coherent(dev, 64, GFP_ATOMIC, &ir->data_dma);
	if (!ir->irpkt)
		goto fail1;

	ir->irq = usb_alloc_urb(0, GFP_KERNEL);
	if (!ir->irq)
		goto fail2;

	if (!(rdesc = kmalloc(rsize, GFP_KERNEL)))
		goto fail3;

	ir->usbdev = dev;
	ir->dev = input_dev;

	if (dev->manufacturer)
		strlcpy(ir->name, dev->manufacturer, sizeof(ir->name));

	if (dev->product) {
		if (dev->manufacturer)
			strlcat(ir->name, " ", sizeof(ir->name));
		strlcat(ir->name, dev->product, sizeof(ir->name));
	}

	if (!strlen(ir->name))
		snprintf(ir->name, sizeof(ir->name),
				"IRTOUCH dual-touch touchscreen %04x:%04x",
				le16_to_cpu(dev->descriptor.idVendor),
				le16_to_cpu(dev->descriptor.idProduct));

	usb_make_path(dev, ir->phys, sizeof(ir->phys));
	strlcat(ir->phys, "/input0", sizeof(ir->phys));

	input_dev->name = ir->name;
	input_dev->phys = ir->phys;
	usb_to_input_id(dev, &input_dev->id);
	input_dev->dev.parent = &intf->dev;

	input_dev->evbit[0] = BIT(EV_KEY) | BIT(EV_ABS);
	set_bit(BTN_TOUCH, input_dev->keybit);
	input_dev->absbit[0] = BIT(ABS_X) | BIT(ABS_Y);
	set_bit(ABS_PRESSURE, input_dev->absbit);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, 32767, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, 32767, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 32767, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR, 0, 32767, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 10, 0, 0);

	input_set_drvdata(input_dev, ir);

	input_dev->open = irtouch_open_device;
	input_dev->close = irtouch_close_device;

	usb_fill_int_urb(ir->irq, dev, pipe, ir->irpkt, 64,
			irtouch_irq, ir, endpoint->bInterval);
	ir->irq->transfer_dma = ir->data_dma;
	ir->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	error = input_register_device(ir->dev);

	usb_set_intfdata(intf, ir);

	error = usb_register_dev(intf, &irtouch_class_driver);
	if(error)
	{
		err("IRTOUCH:     %s Not able to get a minor for this device.",__func__);
		usb_set_intfdata(intf,NULL);
		goto fail1;
	}

	irtouch_get_device_param(dev);

	irtouch_mkdev();

	return 0;

fail4:
	kfree(rdesc);
fail3:
	usb_free_urb(ir->irq);
fail2:
	usb_free_coherent(dev, 64, ir->irpkt, ir->data_dma);
	
fail1:
	input_free_device(input_dev);
	kfree(ir);
	kfree(devContext);
	return -ENOMEM;
}

static void irtouch_disconnect(struct usb_interface *intf)
{
	struct irtouchusb *ir = usb_get_intfdata (intf);

	dev_t devno = MKDEV(MAJOR_DEVICE_NUMBER,MINOR_DEVICE_NUMBER);

	cdev_del(&cdev);
	unregister_chrdev_region(devno,1);

	device_destroy(irser_class,devno);
	class_destroy(irser_class);

	usb_deregister_dev(intf, &irtouch_class_driver);

	usb_set_intfdata(intf, NULL);
	if(ir) 
	{
		usb_kill_urb(ir->irq);
		input_unregister_device(ir->dev);
		usb_free_urb(ir->irq);
		usb_free_coherent(interface_to_usbdev(intf), 64, ir->irpkt, ir->data_dma);
		kfree(ir);
	}

	if(devContext)
	{
		kfree(devContext);
	}
}

static struct usb_device_id irtouch_id_table [] = {
	{ USB_DEVICE(0x6615, 0x0080) }, 
	{ }    /* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, irtouch_id_table);

static struct usb_driver irtouch_driver = {
	.name        = "IRTOUCH",
	.probe        = irtouch_probe,
	.disconnect    = irtouch_disconnect,
	.id_table    = irtouch_id_table,
};

static int __init irtouch_init(void)
{
	int res;

	int retval = usb_register(&irtouch_driver);
	if (retval == 0)
		printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_VERSION ":" DRIVER_DESC);

	return retval;
}

static void __exit irtouch_exit(void)
{
	usb_deregister(&irtouch_driver);
}

module_init(irtouch_init);
module_exit(irtouch_exit);



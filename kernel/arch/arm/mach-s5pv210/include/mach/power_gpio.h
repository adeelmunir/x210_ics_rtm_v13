#ifndef POWER_GPIO_INCLUDED
#define POWER_GPIO_INCLUDED
/*
 *  misc power and gpio proc file for urbetter
 *  Copyright (c) 2009 Urbetter Technologies, Inc.
 */

enum power_item
{
	POWER_MAIN = 0,			//power lock pin
	POWER_WIFI,				//USB wifi Vdd enable, don't touch it if SDIO.
	POWER_WIFI_PD,			//wifi power down pin
	POWER_ETH,
	POWER_LCD,				//panel/t-con power
	POWER_BACKLIGHT,		//backlight led power
	POWER_AUDIO,			//codec
	POWER_SPEAKER,			//amplifier
	POWER_GSENSOR,
	POWER_NAND,
	POWER_CAMERA,
	POWER_USB,
	POWER_SDIO,				//SDIO device (wifi) Vdd enable
	POWER_3G,
	POWER_GPS,
	POWER_BLUETOOTH,
	STATE_AC,				//AC adapter
	STATE_CHARGE,			//battery charge
	LED_POWER,
	LED_CHARGE,
	USB_OTG_SWITCH,
	POWER_TP,
	POWER_BT_PH,
	POWER_GPS_RST,

	POWER_ITEM_MAX,
	POWER_ITEM_INVALID,
};

//read ON/OFF status
int read_power_item_value(int index);

//set ON/OFF status immediately
int write_power_item_value(int index, int value);

//set ON/OFF status with debounce in milliseconds.
int post_power_item_value(int index, int value, int ms_debounce);

#endif //POWER_GPIO_INCLUDED


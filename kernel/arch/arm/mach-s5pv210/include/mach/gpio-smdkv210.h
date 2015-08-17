#ifndef __GPIO_SMDKV210_H_
#define __GPIO_SMDKV210_H_

/*
 * Gsensor
 */
#define GPIO_GENSOR_POWER_EN			S5PV210_GPD0(3)

#define S5PV210_GPD_0_0_TOUT_0  (0x2 << 0)
#define S5PV210_GPD_0_1_TOUT_1  (0x2 << 4)
#define S5PV210_GPD_0_2_TOUT_2  (0x2 << 8)
#define S5PV210_GPD_0_3_TOUT_3  (0x2 << 12)

#define S5PV210_GPH0_0_EXT_INT0_0         (0xf<<0)
#define S5PV210_GPH0_1_EXT_INT1_1         (0xf<<4)
#define S5PV210_GPH0_2_EXT_INT2_2         (0xf<<8)
#define S5PV210_GPH0_3_EXT_INT3_3         (0xf<<12)
#define S5PV210_GPH0_4_EXT_INT4_4         (0xf<<16)
#define S5PV210_GPH0_5_EXT_INT5_5         (0xf<<20)
#define S5PV210_GPH0_6_EXT_INT6_6         (0xf<<24)
#define S5PV210_GPH0_7_EXT_INT7_7         (0xf<<28)

#define S5PV210_GPH0_0_INTPUT         (0<<0)
#define S5PV210_GPH0_1_INTPUT         (0<<4)
#define S5PV210_GPH0_2_INTPUT         (0<<8)
#define S5PV210_GPH0_3_INTPUT         (0<<12)
#define S5PV210_GPH0_4_INTPUT         (0<<16)
#define S5PV210_GPH0_5_INTPUT         (0<<20)
#define S5PV210_GPH0_6_INTPUT         (0<<24)
#define S5PV210_GPH0_7_INTPUT         (0<<28)

#if 0/* for default smdk con gpio, del */
#define GPIO_PS_VOUT			S5PV210_GPH0(2)
#define GPIO_PS_VOUT_AF			0xFF

#define GPIO_BUCK_1_EN_A		S5PV210_GPH0(3)
#define GPIO_BUCK_1_EN_B		S5PV210_GPH0(4)

#define GPIO_BUCK_2_EN			S5PV210_GPH0(5)
#define GPIO_DET_35			S5PV210_GPH0(6)
#define GPIO_DET_35_AF			0xFF

#define GPIO_nPOWER			S5PV210_GPH2(6)

#define GPIO_EAR_SEND_END		S5PV210_GPH3(6)
#define GPIO_EAR_SEND_END_AF		0xFF

#define GPIO_HWREV_MODE0		S5PV210_GPJ0(2)
#define GPIO_HWREV_MODE1		S5PV210_GPJ0(3)
#define GPIO_HWREV_MODE2		S5PV210_GPJ0(4)
#define GPIO_HWREV_MODE3		S5PV210_GPJ0(7)

#define GPIO_PS_ON			S5PV210_GPJ1(4)

#define GPIO_MICBIAS_EN			S5PV210_GPJ4(2)

#define GPIO_UART_SEL			S5PV210_MP05(7)
#endif

/*add by urbetter*/
#define GPIO_LEVEL_LOW			0
#define GPIO_LEVEL_HIGH			1
#define GPIO_LEVEL_NONE			2
#define GPIO_INPUT			0
#define GPIO_OUTPUT			1

/* LCD GPIO */
#define GPIO_LCD_BACKLIGHT_PWM		S5PV210_GPD0(0)	/*S5PV210_GPD0(3)*/
#define GPIO_LCD_BACKLIGHT_EN	 	S5PV210_GPH2(4)
#define GPIO_LCD_POWER_5V		S5PV210_GPH1(6)	
#define GPIO_LCD_POWER_33V		S5PV210_GPH1(7) // low en	


/* touch screen used pin */
/*
#define GPIO_TS_EINT		S5PV210_GPH1(0)
#define GPIO_TS_POWER		S5PV210_GPH2(3)
#define GPIO_TS_WAKE		S5PV210_GPH2(1)
#define GPIO_TS_COBY_WAKE	S5PV210_GPH3(0)
*/
////////  modified by Leslie Xiao //////
#define GPIO_TS_EINT		S5PV210_GPH0(5)
#define GPIO_TS_POWER		S5PV210_GPH2(3)
#define GPIO_TS_WAKE		S5PV210_GPH1(7)
#define GPIO_TS_COBY_WAKE	S5PV210_GPH3(0)
///////////////////////////////////
#define GPIO_TS_RESET		GPIO_TS_WAKE
#define GPIO_TS_COBY_RESET		GPIO_TS_COBY_WAKE

/* audio jack */
#define GPIO_SPK_EN  S5PV210_GPH0(6)

/* vibrator enable */
#define GPIO_VIBTONE_EN	S5PV210_GPJ2(3)//S5PV210_GPH0(6)

#endif
/* end of __GPIO_SMDKV210_H_ */


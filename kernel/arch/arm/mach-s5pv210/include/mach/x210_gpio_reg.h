//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
//------------------------------------------------------------------------------
//
//  Header:  gpio_reg.h
//
//  Defines the Input/Output Ports (GPIO) control registers and associated
//  types and constants.
//


#ifndef __X210__GPIO_H
#define __X210__GPIO_H

//#if __cplusplus
//extern "C" {
//#endif

#include "x210_GpioEntry_reg.h"

#define GPIO_TOTALNUM                   23  // Total Number of GPA0~GPJ4
#define GPIO_GROUPNUM                   6
#define MP_TOTALNUM                     25  // Total Number of MP0_1~MP0_7, MP1_0~MP1_8, MP2_0~MP2_8
#define MP_GROUPNUM                     6
#define ETC_TOTALNUM                    4
#define ETC_GROUPNUM                    2
#define GPIO_EINT_NUM                   22  // Total Number of GPIO Ext Interrupt

typedef struct                             // 0xE0200000
{
    _GPIO_PORT_REG      GPA0;           // 0x0
    _GPIO_PORT_REG      GPA1;
    _GPIO_PORT_REG      GPB;
    _GPIO_PORT_REG      GPC0;
    _GPIO_PORT_REG      GPC1;
    _GPIO_PORT_REG      GPD0;
    _GPIO_PORT_REG      GPD1;
    _GPIO_PORT_REG      GPE0;
    _GPIO_PORT_REG      GPE1;
    _GPIO_PORT_REG      GPF0;
    _GPIO_PORT_REG      GPF1;
    _GPIO_PORT_REG      GPF2;
    _GPIO_PORT_REG      GPF3;
    _GPIO_PORT_REG      GPG0;
    _GPIO_PORT_REG      GPG1;
    _GPIO_PORT_REG      GPG2;
    _GPIO_PORT_REG      GPG3;
    _LP_PORT_REG        GPI;              // 0x220
    _GPIO_PORT_REG      GPJ0;
    _GPIO_PORT_REG      GPJ1;
    _GPIO_PORT_REG      GPJ2;
    _GPIO_PORT_REG      GPJ3;
    _GPIO_PORT_REG      GPJ4;           // 0x2C0 ~ 0x2DC

    _MP_PORT_REG        MP0_1;       // 0x2E0
    _MP_PORT_REG        MP0_2;
    _MP_PORT_REG        MP0_3;
    _MP_PORT_REG        MP0_4;
    _MP_PORT_REG        MP0_5;
    _MP_PORT_REG        MP0_6;
    _MP_PORT_REG        MP0_7;

    _MP_PORT_REG        MP1_0;
    _MP_PORT_REG        MP1_1;
    _MP_PORT_REG        MP1_2;
    _MP_PORT_REG        MP1_3;
    _MP_PORT_REG        MP1_4;
    _MP_PORT_REG        MP1_5;
    _MP_PORT_REG        MP1_6;
    _MP_PORT_REG        MP1_7;
    _MP_PORT_REG        MP1_8;

    _MP_PORT_REG        MP2_0;
    _MP_PORT_REG        MP2_1;
    _MP_PORT_REG        MP2_2;
    _MP_PORT_REG        MP2_3;
    _MP_PORT_REG        MP2_4;
    _MP_PORT_REG        MP2_5;
    _MP_PORT_REG        MP2_6;
    _MP_PORT_REG        MP2_7;
    _MP_PORT_REG        MP2_8;    // 0x5E0 ~ 0x5FC

    _ETC_PORT_REG       ETC0;      // 0x600 ~ 0x61C
    _ETC_PORT_REG       ETC1;
    _ETC_PORT_REG       ETC2;
    _ETC_PORT_REG       ETC4;      // 0x660 ~ 0x67C

    UINT32              ETC_PAD[32]; //0x680 ~ 0x6FC

    _GPIOINTCON_REG     GPIOINTCON;  // 0x700 ~ 0x754
    UINT32              GPIOCON_PAD[42]; //0x758 ~ 0x7FC

    _GPIOINTFLT_REG     GPIOINTFLT;    // 0x800 ~ 0x8AC
    UINT32              GPIOFLT_PAD[20]; // 0x8B0 ~ 0x8FC

    _GPIOINTMSK_REG     GPIOINTMSK; //0x900 ~ 0x954
    UINT32              GPIOMSK_PAD[42]; // 0x958 ~ 0x9FC

    _GPIOINTPND_REG     GPIOINTPND; // 0xA00 ~ 0xA54
    UINT32              GPIOPND_PAD[42]; // 0xA58~0xAFC

    UINT32              GPIOINT_GRPPRI;
    UINT32              GPIOINT_PRIORITY;
    UINT32              GPIOINT_SERVICE;
    UINT32              GPIOINT_SERVICE_PEND;
    UINT32              GPIOINT_GRPFIXPRI;

     _GPIOINTFIXPRI_REG GPIOINTFIXPRI;  // 0xB14 ~ 0xB68
    UINT32              GPIOPRI_PAD[37]; //0xB6C ~ 0xBFC

    _ALIVE_PORT_REG     GPH0;               // 0xC00 ~ 0xC1C
    _ALIVE_PORT_REG     GPH1;
    _ALIVE_PORT_REG     GPH2;
    _ALIVE_PORT_REG     GPH3;               // 0xC60 ~ 0xC7C

    UINT32              GPH_PAD[96];  // 0xC80 ~ 0xDFC

    _EINTCON_REG       EINTCON;  // 0xE00 ~ 0xE0C
    UINT32             EINTCON_PAD[28]; // 0xE10 ~ 0xE7C

    _EINTFLT_REG       EINTFLT;    // 0xE80~ 0xE9C
    UINT32             EINTFLT_PAD[24]; // 0xEA0 ~ 0xEFC

    _EINTMSK_REG       EINTMSK; //0xF00 ~ 0xF0C
    UINT32             EINTMSK_PAD[12]; // 0xF10 ~ 0xF3C

    _EINTPND_REG       EINTPND; // 0xF40 ~ 0xF4C
    UINT32             EINTPND_PAD[12]; // 0xF50~0xF7C

    UINT32             PDNEN;          // 0xF80
} UT210_GPIO_REG, *PUT210_GPIO_REG;


//------------------------------------------------------------------------------
//
//  Type: VIC_REG
//
//  Interrupt control registers. This register bank is located by the constant
//  BASE_REG_XX_VICX in the configuration file base_reg_cfg.h.
//

typedef struct 
{
    UINT32 VICIRQSTATUS;        // 0x00
    UINT32 VICFIQSTATUS;        // 0x04
    UINT32 VICRAWINTR;            // 0x08
    UINT32 VICINTSELECT;        // 0x0c

    UINT32 VICINTENABLE;        // 0x10
    UINT32 VICINTENCLEAR;        // 0x14
    UINT32 VICSOFTINT;            // 0x18
    UINT32 VICSOFTINTCLEAR;    // 0x1c

    UINT32 VICPROTECTION;        // 0x20
    UINT32 VICSWPRIORITYMASK;    // 0x24
    UINT32 VICPRIORITYDAISY;    // 0x28
    UINT32 PAD0;                // 0x2c

    UINT32 PAD1[52];                // 0x30~0xff

    UINT32 VICVECTADDR0;        // 0x100
    UINT32 VICVECTADDR1;        // 0x104
    UINT32 VICVECTADDR2;        // 0x108
    UINT32 VICVECTADDR3;        // 0x10c

    UINT32 VICVECTADDR4;        // 0x110
    UINT32 VICVECTADDR5;        // 0x114
    UINT32 VICVECTADDR6;        // 0x118
    UINT32 VICVECTADDR7;        // 0x11c

    UINT32 VICVECTADDR8;        // 0x120
    UINT32 VICVECTADDR9;        // 0x124
    UINT32 VICVECTADDR10;        // 0x128
    UINT32 VICVECTADDR11;        // 0x12c

    UINT32 VICVECTADDR12;        // 0x130
    UINT32 VICVECTADDR13;        // 0x134
    UINT32 VICVECTADDR14;        // 0x138
    UINT32 VICVECTADDR15;        // 0x13c

    UINT32 VICVECTADDR16;        // 0x140
    UINT32 VICVECTADDR17;        // 0x144
    UINT32 VICVECTADDR18;        // 0x148
    UINT32 VICVECTADDR19;        // 0x14c

    UINT32 VICVECTADDR20;        // 0x150
    UINT32 VICVECTADDR21;        // 0x154
    UINT32 VICVECTADDR22;        // 0x158
    UINT32 VICVECTADDR23;        // 0x15c

    UINT32 VICVECTADDR24;        // 0x160
    UINT32 VICVECTADDR25;        // 0x164
    UINT32 VICVECTADDR26;        // 0x168
    UINT32 VICVECTADDR27;        // 0x16c

    UINT32 VICVECTADDR28;        // 0x170
    UINT32 VICVECTADDR29;        // 0x174
    UINT32 VICVECTADDR30;        // 0x178
    UINT32 VICVECTADDR31;        // 0x17c

    UINT32 PAD2[32];                // 0x180~0x1ff

    UINT32 VICVECTPRIORITY0;    // 0x200
    UINT32 VICVECTPRIORITY1;    // 0x204
    UINT32 VICVECTPRIORITY2;    // 0x208
    UINT32 VICVECTPRIORITY3;    // 0x20c

    UINT32 VICVECTPRIORITY4;    // 0x210
    UINT32 VICVECTPRIORITY5;    // 0x214
    UINT32 VICVECTPRIORITY6;    // 0x218
    UINT32 VICVECTPRIORITY7;    // 0x21c

    UINT32 VICVECTPRIORITY8;    // 0x220
    UINT32 VICVECTPRIORITY9;    // 0x224
    UINT32 VICVECTPRIORITY10;    // 0x228
    UINT32 VICVECTPRIORITY11;    // 0x22c

    UINT32 VICVECTPRIORITY12;    // 0x230
    UINT32 VICVECTPRIORITY13;    // 0x234
    UINT32 VICVECTPRIORITY14;    // 0x238
    UINT32 VICVECTPRIORITY15;    // 0x23c

    UINT32 VICVECTPRIORITY16;    // 0x240
    UINT32 VICVECTPRIORITY17;    // 0x244
    UINT32 VICVECTPRIORITY18;    // 0x248
    UINT32 VICVECTPRIORITY19;    // 0x24c

    UINT32 VICVECTPRIORITY20;    // 0x250
    UINT32 VICVECTPRIORITY21;    // 0x254
    UINT32 VICVECTPRIORITY22;    // 0x258
    UINT32 VICVECTPRIORITY23;    // 0x25c

    UINT32 VICVECTPRIORITY24;    // 0x260
    UINT32 VICVECTPRIORITY25;    // 0x264
    UINT32 VICVECTPRIORITY26;    // 0x268
    UINT32 VICVECTPRIORITY27;    // 0x26c

    UINT32 VICVECTPRIORITY28;    // 0x270
    UINT32 VICVECTPRIORITY29;    // 0x274
    UINT32 VICVECTPRIORITY30;    // 0x278
    UINT32 VICVECTPRIORITY31;    // 0x27c

    UINT32 PAD3[800];            // 0x280~0xeff

    UINT32 VICADDRESS;            // 0xf00
    UINT32 PAD4[3];                // 0xf04~0xf0f

    UINT32 PAD5[52];                // 0xf10~0xfdf

    UINT32 VICPERIPHID0;        // 0xfe0
    UINT32 VICPERIPHID1;        // 0xfe4
    UINT32 VICPERIPHID2;        // 0xfe8
    UINT32 VICPERIPHID3;        // 0xfec

    UINT32 VICPCELLID0;            // 0xff0
    UINT32 VICPCELLID1;            // 0xff4
    UINT32 VICPCELLID2;            // 0xff8
    UINT32 VICPCELLID3;            // 0xffc
} VIC_REG, *PVIC_REG;
//#if __cplusplus
//}
//#endif

#endif

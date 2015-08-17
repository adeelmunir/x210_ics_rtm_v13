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
//  Header:  gpioentry_reg.h
//
//  Defines the Input/Output Ports (GPIO) control registers entris and associated
//  types and constants.
//

/*
// For Multi-functional input/output port pins
// GPA ~ GPJ Groups except GPI(Low Power), GPH (Alive)
*/
typedef u32 UINT32;

typedef struct
{
    UINT32 GP_CON;                    // + 0x0
    UINT32 GP_DAT;                    // + 0x4
    UINT32 GP_PUD;                    // + 0x8
    UINT32 GP_DRV_SR;              // + 0xC
    UINT32 GP_CONPDN;             // + 0x10
    UINT32 GP_PUDPDN;             // + 0x14
    UINT32 GP_PAD[2];                // + 0x18, 0x1C
} _GPIO_PORT_REG, *_PGPIO_PORT_REG;

/*
// For Low Power port pins : GPI Group(I2S, PCM)
*/
typedef struct
{
    UINT32 GP_CON;                    // + 0x0
    UINT32 GP_RVD0;                   // + 0x4
    UINT32 GP_PUD;                    // + 0x8
    UINT32 GP_DRV_SR;              // + 0xC
    UINT32 GP_RVD1;                   // + 0x10
    UINT32 GP_PUDPDN;             // + 0x14
    UINT32 GP_RVD2[2];              // + 0x18, 0x1C
} _LP_PORT_REG, *_PLP_PORT_REG;

/*
// For memory port pins : MP0 ~ MP2 Groups
*/
typedef struct
{
    UINT32 MP_CON;                    // + 0x0
    UINT32 MP_DAT;                    // + 0x4
    UINT32 MP_PUD;                    // + 0x8
    UINT32 MP_DRV_SR;              // + 0xC
    UINT32 MP_CONPDN;             // + 0x10
    UINT32 MP_PUDPDN;             // + 0x14
    UINT32 MP_PAD[2];                // + 0x18, 0x1C
} _MP_PORT_REG, *_PMP_PORT_REG;

/*
// ETC Groups : ETC0, ETC1, ETC4
*/
typedef struct
{
    UINT32 ETC_PAD0[2];                // + 0x0, 0x4
    UINT32 ETC_PUD;                       // + 0x8
    UINT32 ETC_DRV_SR;                 // + 0xc
    UINT32 ETC_PAD1[4];                //+ 0x10, 0x14, 0x18, 0x1C
} _ETC_PORT_REG, *_PETC_PORT_REG;

/*
// External Interrupt control registers
*/
typedef struct
{
    UINT32 GPA0_INT_CON;        //  0xE0200700
    UINT32 GPA1_INT_CON;
    UINT32 GPB_INT_CON;
    UINT32 GPC0_INT_CON;
    UINT32 GPC1_INT_CON;
    UINT32 GPD0_INT_CON;
    UINT32 GPD1_INT_CON;
    UINT32 GPE0_INT_CON;
    UINT32 GPE1_INT_CON;
    UINT32 GPF0_INT_CON;
    UINT32 GPF1_INT_CON;
    UINT32 GPF2_INT_CON;
    UINT32 GPF3_INT_CON;
    UINT32 GPG0_INT_CON;
    UINT32 GPG1_INT_CON;
    UINT32 GPG2_INT_CON;
    UINT32 GPG3_INT_CON;
    UINT32 GPJ0_INT_CON;
    UINT32 GPJ1_INT_CON;
    UINT32 GPJ2_INT_CON;
    UINT32 GPJ3_INT_CON;
    UINT32 GPJ4_INT_CON;
} _GPIOINTCON_REG, *_PGPIOINTCON_REG;

/*
// External Interrupt filter contro registers
*/
typedef struct
{
    UINT32 _FLT_CON0;
    UINT32 _FLT_CON1;
} __GPIOINTFLT_ENTRY;

typedef struct
{
    __GPIOINTFLT_ENTRY GPA0_INT_FLT_CON;           // 0xE0200800
    __GPIOINTFLT_ENTRY GPA1_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPB_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPC0_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPC1_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPD0_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPD1_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPE0_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPE1_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPF0_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPF1_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPF2_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPF3_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPG0_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPG1_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPG2_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPG3_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPJ0_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPJ1_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPJ2_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPJ3_INT_FLT_CON;
    __GPIOINTFLT_ENTRY GPJ4_INT_FLT_CON;
} _GPIOINTFLT_REG, *_PGPIOINTFLT_REG;

/*
// External Interrupt mask registers
*/
typedef struct
{
    UINT32 GPA0_INT_MASK;        // 0xE0200900
    UINT32 GPA1_INT_MASK;
    UINT32 GPB_INT_MASK;
    UINT32 GPC0_INT_MASK;
    UINT32 GPC1_INT_MASK;
    UINT32 GPD0_INT_MASK;
    UINT32 GPD1_INT_MASK;
    UINT32 GPE0_INT_MASK;
    UINT32 GPE1_INT_MASK;
    UINT32 GPF0_INT_MASK;
    UINT32 GPF1_INT_MASK;
    UINT32 GPF2_INT_MASK;
    UINT32 GPF3_INT_MASK;
    UINT32 GPG0_INT_MASK;
    UINT32 GPG1_INT_MASK;
    UINT32 GPG2_INT_MASK;
    UINT32 GPG3_INT_MASK;
    UINT32 GPJ0_INT_MASK;
    UINT32 GPJ1_INT_MASK;
    UINT32 GPJ2_INT_MASK;
    UINT32 GPJ3_INT_MASK;
    UINT32 GPJ4_INT_MASK;
} _GPIOINTMSK_REG, *_PGPIOINTMSK_REG;

/*
// External Interrupt pending registers
*/
typedef struct
{
    UINT32 GPA0_INT_PEND;        // 0xE0200A00
    UINT32 GPA1_INT_PEND;
    UINT32 GPB_INT_PEND;
    UINT32 GPC0_INT_PEND;
    UINT32 GPC1_INT_PEND;
    UINT32 GPD0_INT_PEND;
    UINT32 GPD1_INT_PEND;
    UINT32 GPE0_INT_PEND;
    UINT32 GPE1_INT_PEND;
    UINT32 GPF0_INT_PEND;
    UINT32 GPF1_INT_PEND;
    UINT32 GPF2_INT_PEND;
    UINT32 GPF3_INT_PEND;
    UINT32 GPG0_INT_PEND;
    UINT32 GPG1_INT_PEND;
    UINT32 GPG2_INT_PEND;
    UINT32 GPG3_INT_PEND;
    UINT32 GPJ0_INT_PEND;
    UINT32 GPJ1_INT_PEND;
    UINT32 GPJ2_INT_PEND;
    UINT32 GPJ3_INT_PEND;
    UINT32 GPJ4_INT_PEND;
} _GPIOINTPND_REG, *_PGPIOINTPND_REG;

/*
// GPIO Interrupt priority registers
*/
typedef struct
{
    UINT32 GPA0_INT_FIXPRI;        // 0xE0200B14
    UINT32 GPA1_INT_FIXPRI;
    UINT32 GPB_INT_FIXPRI;
    UINT32 GPC0_INT_FIXPRI;
    UINT32 GPC1_INT_FIXPRI;
    UINT32 GPD0_INT_FIXPRI;
    UINT32 GPD1_INT_FIXPRI;
    UINT32 GPE0_INT_FIXPRI;
    UINT32 GPE1_INT_FIXPRI;
    UINT32 GPF0_INT_FIXPRI;
    UINT32 GPF1_INT_FIXPRI;
    UINT32 GPF2_INT_FIXPRI;
    UINT32 GPF3_INT_FIXPRI;
    UINT32 GPG0_INT_FIXPRI;
    UINT32 GPG1_INT_FIXPRI;
    UINT32 GPG2_INT_FIXPRI;
    UINT32 GPG3_INT_FIXPRI;
    UINT32 GPJ0_INT_FIXPRI;
    UINT32 GPJ1_INT_FIXPRI;
    UINT32 GPJ2_INT_FIXPRI;
    UINT32 GPJ3_INT_FIXPRI;
    UINT32 GPJ4_INT_FIXPRI;
    
} _GPIOINTFIXPRI_REG, *_PGPIOINTFIXPRI_REG;


#define GPIO_CON_MASK	0xf			
#define GPH_CON_MASK	GPIO_CON_MASK			
#define GPH_CON_INPUT	0x0
#define GPH_CON_OUTPUT	0x1
#define GPH_CON_EX_INT	0xf

#define GPH_PUD_MASK	0x03
#define GPH_PUD_DOWN_PULL	0x01
#define GPH_PUD_UP_PULL	0x02
/*
// For Multi-functional input/output port pins : Alive GPH
*/
typedef struct
{
    UINT32 GP_CON;                    // + 0x0
    UINT32 GP_DAT;                    // + 0x4
    UINT32 GP_PUD;                    // + 0x8
    UINT32 GP_DRV_SR;              // + 0xC
    UINT32 GP_PAD[4];                // + 0x10, 0x14, 0x18, 0x1C
} _ALIVE_PORT_REG, *_PALIVE_PORT_REG;

/*
// Alive External Interrupt control registers
*/
typedef struct
{
    UINT32 EXT_INT0_CON;        //  0xE0200E00
    UINT32 EXT_INT1_CON;
    UINT32 EXT_INT2_CON;
    UINT32 EXT_INT3_CON;
} _EINTCON_REG, *_PEINTCON_REG;

/*
// Alive External Interrupt filter contro registers
*/
typedef struct
{
    __GPIOINTFLT_ENTRY EXT_FLT0_CON;           // 0xE0200E80
    __GPIOINTFLT_ENTRY EXT_FLT1_CON;
    __GPIOINTFLT_ENTRY EXT_FLT2_CON;
    __GPIOINTFLT_ENTRY EXT_FLT3_CON;
} _EINTFLT_REG, *_PEINTFLT_REG;

/*
// Alive External Interrupt mask registers
*/
typedef struct
{
    UINT32 EXT_INT0_MASK;        // 0xE0200F00
    UINT32 EXT_INT1_MASK;
    UINT32 EXT_INT2_MASK;
    UINT32 EXT_INT3_MASK;
} _EINTMSK_REG, *_PEINTMSK_REG;

/*
// Alive External Interrupt pending registers
*/
typedef struct
{
    UINT32 EXT_INT0_PEND;        // 0xE0200F40
    UINT32 EXT_INT1_PEND;
    UINT32 EXT_INT2_PEND;
    UINT32 EXT_INT3_PEND;
} _EINTPND_REG, *_PEINTPND_REG;




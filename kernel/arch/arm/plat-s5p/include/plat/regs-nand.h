/* arch/arm/plat-s5p/include/plat/regs-nand.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Register definition file for Samsung NAND driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARM_REGS_S5P_NAND
#define __ASM_ARM_REGS_S5P_NAND


#define S5P_NFREG(x) (x)

/* for s3c_nand.c */
#define S3C_NFCONF		S5P_NFREG(0x00)
#define S3C_NFCONT		S5P_NFREG(0x04)
#define S3C_NFCMMD		S5P_NFREG(0x08)
#define S3C_NFADDR		S5P_NFREG(0x0c)
#define S3C_NFDATA8		S5P_NFREG(0x10)
#define S3C_NFDATA		S5P_NFREG(0x10)
#define S3C_NFMECCDATA0		S5P_NFREG(0x14)
#define S3C_NFMECCDATA1		S5P_NFREG(0x18)
#define S3C_NFSECCDATA		S5P_NFREG(0x1c)
#define S3C_NFSBLK		S5P_NFREG(0x20)
#define S3C_NFEBLK		S5P_NFREG(0x24)
#define S3C_NFSTAT		S5P_NFREG(0x28)
#define S3C_NFMECCERR0		S5P_NFREG(0x2c)
#define S3C_NFMECCERR1		S5P_NFREG(0x30)
#define S3C_NFMECC0		S5P_NFREG(0x34)
#define S3C_NFMECC1		S5P_NFREG(0x38)
#define S3C_NFSECC		S5P_NFREG(0x3c)
#define S3C_NFMLCBITPT		S5P_NFREG(0x40)
#define S3C_NF8ECCERR0		S5P_NFREG(0x44)
#define S3C_NF8ECCERR1		S5P_NFREG(0x48)
#define S3C_NF8ECCERR2		S5P_NFREG(0x4C)
#define S3C_NFM8ECC0		S5P_NFREG(0x50)
#define S3C_NFM8ECC1		S5P_NFREG(0x54)
#define S3C_NFM8ECC2		S5P_NFREG(0x58)
#define S3C_NFM8ECC3		S5P_NFREG(0x5C)
#define S3C_NFMLC8BITPT0	S5P_NFREG(0x60)
#define S3C_NFMLC8BITPT1	S5P_NFREG(0x64)

#if 1//add by urbetter
/* new registers in S5P6440 for 8/12/16 ECC */
#define S3C_NFECCCONF		S5P_NFREG(0x20000) /* ECC configuration */
#define S3C_NFECCCONT		S5P_NFREG(0x20020) /* ECC control */
#define S3C_NFECCSTAT		S5P_NFREG(0x20030) /* ECC status */
#define S3C_NFECCSECSTAT	S5P_NFREG(0x20040) /* ECC sector status */
#define S3C_NFECCPRGECC0		S5P_NFREG(0x20090) /* ECC parity code0 for page program */
#define S3C_NFECCPRGECC1		S5P_NFREG(0x20094) /* ECC parity code1 for page program */
#define S3C_NFECCPRGECC2		S5P_NFREG(0x20098) /* ECC parity code2 for page program */
#define S3C_NFECCPRGECC3		S5P_NFREG(0x2009C) /* ECC parity code3 for page program */
#define S3C_NFECCPRGECC4		S5P_NFREG(0x200A0) /* ECC parity code4 for page program */
#define S3C_NFECCPRGECC5		S5P_NFREG(0x200A4) /* ECC parity code5 for page program */
#define S3C_NFECCPRGECC6		S5P_NFREG(0x200A8) /* ECC parity code6 for page program */
#define S3C_NFECCERL0		S5P_NFREG(0x200C0) /* ECC error byte location0 */
#define S3C_NFECCERL1		S5P_NFREG(0x200C4) /* ECC error byte location1 */
#define S3C_NFECCERL2		S5P_NFREG(0x200C8) /* ECC error byte location2 */
#define S3C_NFECCERL3		S5P_NFREG(0x200CC) /* ECC error byte location3 */
#define S3C_NFECCERL4		S5P_NFREG(0x200D0) /* ECC error byte location4 */
#define S3C_NFECCERL5		S5P_NFREG(0x200D4) /* ECC error byte location5 */
#define S3C_NFECCERL6		S5P_NFREG(0x200D8) /* ECC error byte location6 */
#define S3C_NFECCERL7		S5P_NFREG(0x200DC) /* ECC error byte location7 */
#define S3C_NFECCERP0		S5P_NFREG(0x200F0) /* ECC error bit pattern0 */
#define S3C_NFECCERP1		S5P_NFREG(0x200F4) /* ECC error bit pattern1 */
#define S3C_NFECCERP2		S5P_NFREG(0x200F8) /* ECC error bit pattern2 */
#define S3C_NFECCERP3		S5P_NFREG(0x200FC) /* ECC error bit pattern3 */
#define S3C_NFECCCONECC0		S5P_NFREG(0x20110) /* ECC parity conversion code0 */
#define S3C_NFECCCONECC1		S5P_NFREG(0x20114) /* ECC parity conversion code1 */
#define S3C_NFECCCONECC2		S5P_NFREG(0x20118) /* ECC parity conversion code2 */
#define S3C_NFECCCONECC3		S5P_NFREG(0x2011C) /* ECC parity conversion code3 */
#define S3C_NFECCCONECC4		S5P_NFREG(0x20120) /* ECC parity conversion code4 */
#define S3C_NFECCCONECC5		S5P_NFREG(0x20124) /* ECC parity conversion code5 */
#define S3C_NFECCCONECC6		S5P_NFREG(0x20128) /* ECC parity conversion code6 */

#define NFCONF_ECC_MLC			(1<<24)

#define NFCONF_ECC_1BIT			(0<<23)
#define NFCONF_ECC_4BIT			(2<<23)
#define NFCONF_ECC_8BIT			(1<<23)

#define NFCONT_ECC_ENC			(1<<18)
#define NFCONT_WP			(1<<16)
#define NFCONT_MECCLOCK			(1<<7)
#define NFCONT_SECCLOCK			(1<<6)
#define NFCONT_INITMECC			(1<<5)
#define NFCONT_INITSECC			(1<<4)
#define NFCONT_INITECC			(NFCONT_INITMECC | NFCONT_INITSECC)
#define NFCONT_CS			(1<<1)
#define NFSTAT_ECCENCDONE		(1<<25)
#define NFSTAT_ECCDECDONE		(1<<24)
#define NFSTAT_RnB			(1<<0)
#define NFESTAT0_ECCBUSY		(1<<31)

#endif
#define S3C_NFCONF_NANDBOOT	(1<<31)
#define S3C_NFCONF_ECCCLKCON	(1<<30)
#define S3C_NFCONF_ECC_MLC	(1<<24)
#define S3C_NFCONF_ECC_1BIT	(0<<23)
#define S3C_NFCONF_ECC_4BIT	(2<<23)
#define S3C_NFCONF_ECC_8BIT	(1<<23)
#define S3C_NFCONF_TACLS(x)	((x)<<12)
#define S3C_NFCONF_TWRPH0(x)	((x)<<8)
#define S3C_NFCONF_TWRPH1(x)	((x)<<4)
#define S3C_NFCONF_ADVFLASH	(1<<3)
#define S3C_NFCONF_PAGESIZE	(1<<2)
#define S3C_NFCONF_ADDRCYCLE	(1<<1)
#define S3C_NFCONF_BUSWIDTH	(1<<0)

#define S3C_NFCONT_ECC_ENC	(1<<18)
#define S3C_NFCONT_LOCKTGHT	(1<<17)
#define S3C_NFCONT_LOCKSOFT	(1<<16)
#define S3C_NFCONT_MECCLOCK	(1<<7)
#define S3C_NFCONT_SECCLOCK	(1<<6)
#define S3C_NFCONT_INITMECC	(1<<5)
#define S3C_NFCONT_INITSECC	(1<<4)
#define S3C_NFCONT_nFCE1	(1<<2)
#define S3C_NFCONT_nFCE0	(1<<1)
#define S3C_NFCONT_INITECC	(S3C_NFCONT_INITSECC | S3C_NFCONT_INITMECC)

#define S3C_NFSTAT_ECCENCDONE	(1<<7)
#define S3C_NFSTAT_ECCDECDONE	(1<<6)
#define S3C_NFSTAT_ILEGL_ACC	(1<<5)
#define S3C_NFSTAT_RnB_CHANGE	(1<<4)
#define S3C_NFSTAT_nFCE1	(1<<3)
#define S3C_NFSTAT_nFCE0	(1<<2)
#define S3C_NFSTAT_Res1		(1<<1)
#define S3C_NFSTAT_READY	(1<<0)
#define S3C_NFSTAT_CLEAR	((1<<7) |(1<<6) |(1<<5) |(1<<4))
#define S3C_NFSTAT_BUSY		(1<<0)

#define S3C_NFECCERR0_ECCBUSY	(1<<31)

#endif /* __ASM_ARM_REGS_S5P_NAND */


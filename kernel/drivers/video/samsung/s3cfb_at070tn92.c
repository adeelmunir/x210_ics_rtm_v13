/* linux/drivers/video/samsung/s3cfb_at070tn92.c
 *
 * 9tripod x210 7" WVGA Display Panel Support
 *
 * liuqiming, Copyright (c) 2009 9tripod technology
 * 	http://www.9tripod.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "s3cfb.h"

static struct s3cfb_lcd at070tn92 = {
	.width = 800,
	.height = 480,
	.bpp = 32,
	.freq = 60,

	.timing = {
		.h_fp	= 210,
		.h_bp	= 38,
		.h_sw	= 10,
		.v_fp	= 22,
		.v_fpe	= 1,
		.v_bp	= 18,
		.v_bpe	= 1,
		.v_sw	= 7,
	},
	.polarity = {
		.rise_vclk = 0,
		.inv_hsync = 1,
		.inv_vsync = 1,
		.inv_vden = 0,
	},
};

/* name should be fixed as 's3cfb_set_lcd_info' */
void s3cfb_set_lcd_info(struct s3cfb_global *ctrl)
{
	at070tn92.init_ldi = NULL;
	ctrl->lcd = &at070tn92;
}


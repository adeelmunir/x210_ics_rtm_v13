/*
 *  smdkv2xx_wm8978.c
 *
 *  Copyright (c) 
 *  Author: 
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/io.h>
#include <mach/map.h>
#include <mach/regs-clock.h>

#include "../codecs/wm8978.h"
//#include "s3c-dma.h"
//#include "s3c64xx-i2s-v4.h"
#include "i2s.h"
#include "s3c-i2s-v2.h"
#include <mach/map.h>
#define S3C_VA_AUDSS	S3C_ADDR(0x01600000)	/* Audio SubSystem */
#include <mach/regs-audss.h>


#include <sound/soc.h>
#include <linux/clk.h>
#include <sound/pcm_params.h>
#include <asm/mach-types.h>
#include <mach/regs-clock.h>
#include "../codecs/wm8978.h"
#include "i2s.h"
#include "s3c-i2s-v2.h"

#define S5PV210_I2S_NUM   1

#undef DEBUG_WM8978
//#define DEBUG_WM8978	//song removed the debugmsg,2012-08-01
#ifdef DEBUG_WM8978
#define WM8978_DEBUG(format, arg...) printk(/*KERN_DEBUG*/"[audio_wm wm8978]: " format, ## arg)
#else
#define WM8978_DEBUG(format, arg...) do{ } while(0)
#endif

#if 0
#define wait_stable(utime_out)					\
	do {							\
		if (!utime_out)					\
			utime_out = 1000;			\
		utime_out = loops_per_jiffy / HZ * utime_out;	\
		while (--utime_out) { 				\
			cpu_relax();				\
		}						\
	} while (0);
#else

#define wait_stable(utime_out)					\
	do {							\
		while (--utime_out) { 				\
			cpu_relax();				\
		}						\
	} while (0);
#endif

extern struct snd_soc_platform s3c_dma_wrapper;
static int set_epll_rate(unsigned long rate);
#define UT_GSM_PATCH

/* define the scenarios */
#ifdef UT_GSM_PATCH
#define SMT_AUDIO_OFF			0
#define SMT_GSM_PATH		1
#define SMT_MUSIC_PATH		2
#define SMT_DEFAULT_PATH	SMT_MUSIC_PATH
#else
#define SMT_AUDIO_OFF			0
#define SMT_GSM_CALL_AUDIO_HANDSET	1
#define SMT_GSM_CALL_AUDIO_HEADSET	2
#define SMT_GSM_CALL_AUDIO_BLUETOOTH	3
#define SMT_STEREO_TO_SPEAKERS		4
#define SMT_STEREO_TO_HEADPHONES	5
#define SMT_CAPTURE_HANDSET		6
#define SMT_CAPTURE_HEADSET		7
#define SMT_CAPTURE_BLUETOOTH		8
#define SMT_ENABLE_SPEAKERS_AND_HANDSET	9
#define SMT_DEFAULT_PATH	SMT_GSM_CALL_AUDIO_HANDSET

#endif
int speak_control = 0;//baiduman 
static int smdkv210_scenario = 0;
//extern void open_speaker(void);


static int set_scenario_endpoints(struct snd_soc_codec *codec, int scenario)
{
	u32 reg;
    
	printk(KERN_INFO"%s(); %d\n",__FUNCTION__, smdkv210_scenario);

#ifdef UT_GSM_PATCH
        switch (smdkv210_scenario) {
		case SMT_AUDIO_OFF:
			speak_control = 0;
			snd_soc_write(codec, WM8978_LEFT_MIXER_CONTROL, 0);
			snd_soc_write(codec, WM8978_RIGHT_MIXER_CONTROL, 0);
			break;
			
		case SMT_GSM_PATH:		
			//open_speaker();
					 
			snd_soc_write(codec, WM8978_LEFT_MIXER_CONTROL, ((7<<6)|(1<<5)|(1<<0)));
			snd_soc_write(codec, WM8978_RIGHT_MIXER_CONTROL, ((7<<6)|(1<<5)|(1<<0)));
			speak_control = 1;
               #if 1// out gain for GSM 
                    reg = snd_soc_read(codec, WM8978_LOUT1_HP_CONTROL);
                    snd_soc_write(codec, WM8978_LOUT1_HP_CONTROL, (reg | 0X003f));//R52
                    reg = snd_soc_read(codec, WM8978_ROUT1_HP_CONTROL);
                    snd_soc_write(codec, WM8978_ROUT1_HP_CONTROL, (reg | 0X003f));//R53
               #endif
			break;
		default:
		case SMT_MUSIC_PATH:
			speak_control = 1;
			//open_speaker();
					 
			snd_soc_write(codec, WM8978_LEFT_MIXER_CONTROL, (1<<0));
			snd_soc_write(codec, WM8978_RIGHT_MIXER_CONTROL, (1<<0));
		break;
        }
#else
        switch (smdkv210_scenario) {
        case SMT_AUDIO_OFF:
                snd_soc_dapm_disable_pin(codec, "Audio Out1");
                snd_soc_dapm_disable_pin(codec, "Audio Out2");
                snd_soc_dapm_disable_pin(codec, "GSM Line Out");
                snd_soc_dapm_disable_pin(codec, "GSM Line In");
                snd_soc_dapm_disable_pin(codec, "Headset Mic");
                snd_soc_dapm_disable_pin(codec, "Call Mic");
                break;
        case SMT_ENABLE_SPEAKERS_AND_HANDSET:
                snd_soc_dapm_enable_pin(codec, "Audio Out1");
                snd_soc_dapm_enable_pin(codec, "Audio Out2");
				
                snd_soc_dapm_enable_pin(codec, "GSM Line Out");
                snd_soc_dapm_enable_pin(codec, "GSM Line In");
                snd_soc_dapm_disable_pin(codec, "Headset Mic");
                snd_soc_dapm_enable_pin(codec, "Call Mic");
                break;
        case SMT_GSM_CALL_AUDIO_HANDSET:
                snd_soc_dapm_enable_pin(codec, "Audio Out1");
                snd_soc_dapm_enable_pin(codec, "GSM Line Out");
                snd_soc_dapm_enable_pin(codec, "GSM Line In");
                snd_soc_dapm_disable_pin(codec, "Headset Mic");
                snd_soc_dapm_enable_pin(codec, "Call Mic");
                break;
        case SMT_GSM_CALL_AUDIO_HEADSET:
                snd_soc_dapm_enable_pin(codec, "Audio Out1");
                snd_soc_dapm_enable_pin(codec, "GSM Line Out");
                snd_soc_dapm_enable_pin(codec, "GSM Line In");
                snd_soc_dapm_enable_pin(codec, "Headset Mic");
                snd_soc_dapm_disable_pin(codec, "Call Mic");
                break;
        case SMT_GSM_CALL_AUDIO_BLUETOOTH:
		snd_soc_dapm_disable_pin(codec, "Audio Out2");
                snd_soc_dapm_disable_pin(codec, "Audio Out1");
                snd_soc_dapm_enable_pin(codec, "GSM Line Out");
                snd_soc_dapm_enable_pin(codec, "GSM Line In");
                snd_soc_dapm_disable_pin(codec, "Headset Mic");
                snd_soc_dapm_disable_pin(codec, "Call Mic");
                break;
        case SMT_STEREO_TO_SPEAKERS:
                snd_soc_dapm_enable_pin(codec, "Audio Out2");
                snd_soc_dapm_disable_pin(codec, "Audio Out1");
                snd_soc_dapm_disable_pin(codec, "GSM Line Out");
                snd_soc_dapm_disable_pin(codec, "GSM Line In");
                snd_soc_dapm_disable_pin(codec, "Headset Mic");
                snd_soc_dapm_disable_pin(codec, "Call Mic");
                break;
        case SMT_STEREO_TO_HEADPHONES:
                snd_soc_dapm_disable_pin(codec, "Audio Out2");
                snd_soc_dapm_enable_pin(codec, "Audio Out1");
                snd_soc_dapm_disable_pin(codec, "GSM Line Out");
                snd_soc_dapm_disable_pin(codec, "GSM Line In");
                snd_soc_dapm_disable_pin(codec, "Headset Mic");
                snd_soc_dapm_disable_pin(codec, "Call Mic");
                break;
        case SMT_CAPTURE_HANDSET:
                snd_soc_dapm_disable_pin(codec, "Audio Out1");
                snd_soc_dapm_disable_pin(codec, "GSM Line Out");
                snd_soc_dapm_disable_pin(codec, "GSM Line In");
                snd_soc_dapm_disable_pin(codec, "Headset Mic");
                snd_soc_dapm_enable_pin(codec, "Call Mic");

                break;
        case SMT_CAPTURE_HEADSET:
                snd_soc_dapm_disable_pin(codec, "Audio Out1");
                snd_soc_dapm_disable_pin(codec, "GSM Line Out");
                snd_soc_dapm_disable_pin(codec, "GSM Line In");
                snd_soc_dapm_enable_pin(codec, "Headset Mic");
                snd_soc_dapm_disable_pin(codec, "Call Mic");
                break;
        case SMT_CAPTURE_BLUETOOTH:
                snd_soc_dapm_disable_pin(codec, "Audio Out2");
                snd_soc_dapm_disable_pin(codec, "Audio Out1");
                snd_soc_dapm_disable_pin(codec, "GSM Line Out");
                snd_soc_dapm_disable_pin(codec, "GSM Line In");
                snd_soc_dapm_disable_pin(codec, "Headset Mic");
                snd_soc_dapm_disable_pin(codec, "Call Mic");
                break;
        default:
                snd_soc_dapm_disable_pin(codec, "Audio Out2");
                snd_soc_dapm_disable_pin(codec, "Audio Out1");
                snd_soc_dapm_disable_pin(codec, "GSM Line Out");
                snd_soc_dapm_disable_pin(codec, "GSM Line In");
                snd_soc_dapm_disable_pin(codec, "Headset Mic");
                snd_soc_dapm_disable_pin(codec, "Call Mic");
        }

        snd_soc_dapm_sync(codec);
#endif

        return 0;
}
static int smdkv210_get_scenario(struct snd_kcontrol *kcontrol,
        struct snd_ctl_elem_value *ucontrol)
{
        ucontrol->value.integer.value[0] = smdkv210_scenario;
        return 0;
}
static int smdkv210_set_scenario(struct snd_kcontrol *kcontrol,
        struct snd_ctl_elem_value *ucontrol)
{
        struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	 
        if (smdkv210_scenario == ucontrol->value.integer.value[0]) {
		return 0;
        }
        smdkv210_scenario = ucontrol->value.integer.value[0];
        set_scenario_endpoints(codec, smdkv210_scenario);
        return 1;
}

static int s5pv210_hifi_hw_params(struct snd_pcm_substream *substream,	struct snd_pcm_hw_params *params)
{
//	struct snd_soc_pcm_runtime *rtd = substream->private_data;
//	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
//	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;

	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int pll_out;
	unsigned int epll_out_rate;
	int ret;
	int rfs, bfs, psr, rclk;

	WM8978_DEBUG("Entered %s , %d\n",__FUNCTION__, params_format(params));

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case 16000:
	case 22050:
	case 24000:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
	case 12000:
		if (bfs == 48)
			rfs = 768;
		else
			rfs = 512;
		break;
	default:
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 4096000:
	case 5644800:
	case 6144000:
	case 8467200:
	case 9216000:
		psr = 8;
		break;
	case 8192000:
	case 11289600:
	case 12288000:
	case 16934400:
	case 18432000:
		psr = 4;
		break;
	case 22579200:
	case 24576000:
	case 33868800:
	case 36864000:
		psr = 2;
		break;
	case 67737600:
	case 73728000:
		psr = 1;
		break;
	default:
		printk("Not yet supported!\n");
		return -EINVAL;
	}

	epll_out_rate = rclk * psr;
	printk("epll_out=%d, rclk=%d, psr=%d\n", epll_out_rate, rclk, psr);

	/* Set EPLL clock rate */
	ret = set_epll_rate(epll_out_rate);
	if (ret < 0) {
		printk(KERN_ERR "%s: set epll rate failed\n", __func__);
		return ret;
	}

	/* Set the Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		printk(KERN_ERR "%s: set codec dai failed\n", __func__);
		return ret;
	}
	/* Set the AP DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		printk(KERN_ERR "%s: set cpu dai failed\n", __func__);
		return ret;
	}

//	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C64XX_CLKSRC_CDCLK,
//					0, SND_SOC_CLOCK_OUT);

       ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1,                                        
                                                params_rate(params), SND_SOC_CLOCK_OUT);
	if (ret < 0) {
		printk(KERN_ERR "%s: set S3C64XX_CLKSRC_CDCLK for cpu dai failed\n", __func__);
		return ret;
	}
	
	/* We use SCLK_AUDIO for basic ops in SoC-Master mode */
	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C64XX_CLKSRC_MUX,
					0, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		printk(KERN_ERR "%s: set S3C64XX_CLKSRC_MUX for cpu dai failed\n", __func__);
		return ret;
	}
/*

	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_RCLK, rfs);
	if (ret < 0) {
		printk(KERN_ERR "%s:set clk divider RCLK for cpu dai failed\n", __func__);
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_BCLK, bfs);
	if (ret < 0) {
		printk(KERN_ERR "%s:set clk divider BCLK for cpu dai failed\n", __func__);
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_PRESCALER,psr-1);
	if (ret < 0) {
		printk(KERN_ERR "%s:set clk prescaler for cpu dai failed\n", __func__);
		return ret;
	}

*/
	return 0;
}


static int s5pv210_hifi_hw_params1(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int pll_out;
	int bfs, rfs, psr, ret;
	struct clk    *clk_epll;
       WM8978_DEBUG("======== enter %s,format=%d,rate=%d =======\n",
	   	                              __FUNCTION__,params_format(params),params_rate(params));
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U8:
	case SNDRV_PCM_FORMAT_S8:
		bfs = 16;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}
	WM8978_DEBUG("%s , %d\n",__FUNCTION__, __LINE__);

	/* The Fvco for WM8580 PLLs must fall within [90,100]MHz.
	 * This criterion can't be met if we request PLL output
	 * as {8000x256, 64000x256, 11025x256}Hz.
	 * As a wayout, we rather change rfs to a minimum value that
	 * results in (params_rate(params) * rfs), and itself, acceptable
	 * to both - the CODEC and the CPU.
	 */
	switch (params_rate(params)) {
	case 16000:
	case 22050:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
		rfs = 256;
	//	psr = 4; //todo...
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
		rfs = 512;
		break;
	default:
		return -EINVAL;
	}

	WM8978_DEBUG("%s , %d\n",__FUNCTION__, __LINE__);

      pll_out = params_rate(params) * rfs;
	  WM8978_DEBUG("%s , %d\n",__FUNCTION__, __LINE__);

	switch (pll_out) {
        case 4096000:
        case 5644800:
        case 6144000:
        case 8467200:
        case 9216000:
                psr = 8;
                break;
        case 8192000:
        case 11289600:
        case 12288000:
        case 16934400:
        case 18432000:
                psr = 4;
                break;
        case 22579200:
        case 24576000:
        case 33868800:
        case 36864000:
                psr = 2;
                break;
        case 67737600:
        case 73728000:
                psr = 1;
                break;
        default:
                printk("Not yet supported!\n");
                return -EINVAL;
        }
      
	WM8978_DEBUG("%s , %d\n",__FUNCTION__, __LINE__);

	clk_epll = clk_get(NULL, "fout_epll");
        if (IS_ERR(clk_epll)) {
                printk(KERN_ERR
                        "failed to get fout_epll\n");
                return -EBUSY;
        }
		WM8978_DEBUG("%s , %d\n",__FUNCTION__, __LINE__);

//	pll_out = params_rate(params) * rfs;
//	pll_out *= 4;

		WM8978_DEBUG("%s , %d\n",__FUNCTION__, __LINE__);
      pll_out *= psr;
	clk_set_rate(clk_epll, pll_out);
	WM8978_DEBUG("%s , %d\n",__FUNCTION__, __LINE__);

//	psr -= 1;

	
//	printk("PSR=%d RFS=%d BFS=%d\n",psr, rfs, bfs);

	/* Set the Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* Set the AP DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;
	

//	 ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1,
//                                        params_rate(params), SND_SOC_CLOCK_OUT);
//        if (ret < 0)
//                return ret;
#if 0 //todo ... cpu_dat clkdiv functionalities need to be checked.
	/* Set the AP Prescalar/Pll */
        ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_PRESCALER, psr);

        if (ret < 0) {
                printk(KERN_ERR
                        "smdkv210:AP prescalar setting error!\n");
                return ret;
        }
	/* Set the AP RFS */
        ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_RCLK, rfs);
        if (ret < 0) {
                printk(KERN_ERR
                        "smdkv210 : AP RFS setting error!\n");
                return ret;
        }

        /* Set the AP BFS */
        ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_BCLK, bfs);

        if (ret < 0) {
                printk(KERN_ERR
                        "smdkv210 : AP BCLK setting error!\n");
                return ret;
        }
#endif
/* Set the AP BFS */
        ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_BCLK, bfs);

        if (ret < 0) {
                printk(KERN_ERR
                        "smdkv210 : AP bfs setting error!\n");
                return ret;
        }

	 ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1,
                                        params_rate(params), SND_SOC_CLOCK_OUT);
        if (ret < 0){
                printk(KERN_ERR
                        "smdkv210 : AP set_sysclk  setting error!\n");
                return ret;
	}
	 
	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
                                        params_rate(params), OPCLK_BUSCLK);
        if (ret < 0){
                printk(KERN_ERR
                        "smdkv210 : AP OP_CLK setting error!\n");
                return ret;
	}
#if 0 
	/* Set WM8580 to drive MCLK from its PLLA */
	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_MCLK,
					WM8580_CLKSRC_PLLA);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_pll(codec_dai, WM8580_PLLA, 0,
					SMDK_WM8580_FREQ, pll_out);
	if (ret < 0)
		return ret;

#endif
//	pll_out /= 4;
      pll_out /= psr;
	ret = snd_soc_dai_set_sysclk(codec_dai, WM8978_PLL,
				     pll_out, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;
	clk_put(clk_epll);
	return 0;
}


/* machine dapm widgets */
static const struct snd_soc_dapm_widget s5pv210_dapm_widgets[] = {
	SND_SOC_DAPM_LINE("Audio Out1", NULL),
	SND_SOC_DAPM_LINE("Audio Out2", NULL),
	SND_SOC_DAPM_LINE("GSM Line Out", NULL),
	SND_SOC_DAPM_LINE("GSM Line In", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Call Mic", NULL),
};
static const char *smt_scenarios[] = {
#ifdef UT_GSM_PATCH
	"Off",
	"GSM_Mode",
	"Music_Mode",
#else	
	"Off",
	"GSM Handset",
	"GSM Headset",
	"GSM Bluetooth",
	"Speakers",
	"Headphones",
	"Capture Handset",
	"Capture Headset",
	"Capture Bluetooth"
#endif	
};

static const struct soc_enum smdk_scenario_enum[] = {
        SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(smt_scenarios), smt_scenarios),
};

static const struct snd_kcontrol_new wm8978_s5pv210_controls[] = {
	SOC_ENUM_EXT("SMT Mode", smdk_scenario_enum[0], smdkv210_get_scenario, smdkv210_set_scenario),
};

/* example machine audio_mapnections */
static const struct snd_soc_dapm_route audio_map[] = {

	/* Connections to the ... */
	{"Audio Out1", NULL, "LOUT1"},
	{"Audio Out1", NULL, "ROUT1"},

	{"Audio Out2", NULL, "LOUT2"},
	{"Audio Out2", NULL, "ROUT2"},
	/* Connections to the GSM Module */
	{"GSM Line Out", NULL, "OUT4"},
	{"GSM Line Out", NULL, "OUT4"},
	{"MICP", NULL, "GSM Line In"},
	{"MICN", NULL, "GSM Line In"},

	/* Connections to Headset */
	{"L2", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Headset Mic"},

	/* Call Mic */
	{"MICP", NULL, "Mic Bias"},
	{"MICN", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Call Mic"},
};

static int s5pv210_wm8978_init(struct snd_soc_codec *codec)
{
	int ret;

	WM8978_DEBUG("Entered %s\n",__FUNCTION__);

#ifndef UT_GSM_PATCH
	snd_soc_dapm_new_controls(codec, s5pv210_dapm_widgets,
					  ARRAY_SIZE(s5pv210_dapm_widgets));
#endif

	/* add iDMA controls */
	//ret = snd_soc_add_controls(codec, &wm8978_s5pv210_controls[0], ARRAY_SIZE(wm8978_s5pv210_controls));
	//if (ret < 0)
	//	return ret;

	/* set endpoints to default mode */
    //    set_scenario_endpoints(codec, SMT_AUDIO_OFF);	
//
//	smdkv210_scenario = SMT_DEFAULT_PATH;
//	set_scenario_endpoints(codec, SMT_DEFAULT_PATH);

#ifndef UT_GSM_PATCH
	/* set up s5pv210 specific audio paths */
	snd_soc_dapm_add_routes(codec, audio_map, ARRAY_SIZE(audio_map));

	snd_soc_dapm_sync(codec);
#endif

#if 0
	/* Set the Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(&wm8978_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* Set the AP DAI configuration */
	ret = snd_soc_dai_set_fmt(&wm8978_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* Set WM8580 to drive MCLK from its MCLK-pin */
	ret = snd_soc_dai_set_clkdiv(&wm8978_dai, 1, 1);
	if (ret < 0)
		return ret;

	/* Explicitly set WM8580-DAC to source from MCLK */
	ret = snd_soc_dai_set_clkdiv(&wm8978_dai, 2, 1);
	if (ret < 0)
		return ret;

#endif
	return 0;
}

static struct snd_soc_ops s5pv210_hifi_ops = {
	.hw_params = s5pv210_hifi_hw_params1,
};

enum {
	PRI_MID=0,
	PRI_HDMI,
	SEC_PLAYBACK,
};
static struct snd_soc_dai_link smdkv2xx_dai[] = {

      [PRI_MID] = { /* Primary Playback i/f */		
      .name = "WM8978 PAIF TX",		
      .stream_name = "Playback",	
  //    .stream_name = "Capture",	
      //.cpu_dai_name = "samsung-i2s.1",
      .cpu_dai_name = "samsung-i2s.0",	//x210	
      .codec_dai_name = "wm8978_codec",	
      .platform_name = "samsung-audio",		
      //.codec_name = "wm8978.1-001a",	
      .codec_name = "wm8978.2-001a",//x210	
      .init = s5pv210_wm8978_init,
      .ops = &s5pv210_hifi_ops,	
      },
   [PRI_HDMI] = {
		  .name = "WM8978 PAIF RX",		
  		  .stream_name = "Capture",		
 // 	.stream_name = "Playback",
		  .cpu_dai_name = "samsung-i2s.0",		
		  .codec_dai_name = "wm8978_codec",
		  .platform_name = "samsung-audio",		
		  //.codec_name = "wm8978.1-001a",
		  .codec_name = "wm8978.2-001a",		
		  .init = s5pv210_wm8978_init,		
		  .ops = &s5pv210_hifi_ops,	
  	},  
};

static struct snd_soc_card smdkv2xx = {
	.name = "smdk",	
	.dai_link = smdkv2xx_dai,
	.num_links = ARRAY_SIZE(smdkv2xx_dai),
};

//static struct snd_soc_device smdkv2xx_snd_devdata = {
//	.card = &smdkv2xx,
//	.codec_dev = &soc_codec_dev_wm8978,
//};

static struct platform_device *smdkv2xx_snd_device;

static int set_epll_rate(unsigned long rate)
{
	struct clk *fout_epll;
	unsigned int wait_utime = 500;

	WM8978_DEBUG("Entered %s , rate=%ld\n", __func__, rate);
	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
		return -ENOENT;
	}

	if (rate == clk_get_rate(fout_epll)) {
		//printk("---set_epll_rate, rate=%ld\n", clk_get_rate(fout_epll));
		goto out;
	}

	clk_disable(fout_epll);
	wait_stable(wait_utime);

	clk_set_rate(fout_epll, rate);
	
	//wait_stable(wait_utime);

	clk_enable(fout_epll);

out:
	clk_put(fout_epll);

	return 0;
}

static void smdkv210_audio_clk_init(void)
{
	u32 val;
	u32 reg;

	/* We use I2SCLK for rate generation, so set EPLLout as
	 * the parent of I2SCLK.
	 */
	WM8978_DEBUG("Entered %s\n", __func__);
	 
	val = readl(S5P_CLKSRC_AUDSS);
	val &= ~(0x3<<2);
	val |= (1<<0);
	writel(val, S5P_CLKSRC_AUDSS);

	val = readl(S5P_CLKGATE_AUDSS);
	val |= (0x7f<<0);
	writel(val, S5P_CLKGATE_AUDSS);

#ifdef CONFIG_S5P_LPAUDIO
	/* yman.seo CLKOUT is prior to CLK_OUT of SYSCON. XXTI & XUSBXTI work in sleep mode */
	reg = __raw_readl(S5P_OTHERS);
	reg &= ~(0x0003 << 8);
	reg |= 0x0003 << 8; /* XUSBXTI */
	__raw_writel(reg, S5P_OTHERS);
#else
	/* yman.seo Set XCLK_OUT as 24MHz (XUSBXTI -> 24MHz) */
	reg = __raw_readl(S5P_CLK_OUT);
	reg &= ~S5P_CLKOUT_CLKSEL_MASK;
	reg |= S5P_CLKOUT_CLKSEL_XUSBXTI;
	reg &= ~S5P_CLKOUT_DIV_MASK;
	reg |= 0x0001 << S5P_CLKOUT_DIV_SHIFT;	/* DIVVAL = 1, Ratio = 2 = DIVVAL + 1 */
	__raw_writel(reg, S5P_CLK_OUT);

	reg = __raw_readl(S5P_OTHERS);
	reg &= ~(0x0003 << 8);
	reg |= 0x0000 << 8; /* Clock from SYSCON */
	__raw_writel(reg, S5P_OTHERS);
#endif


}

extern int i2s_select_flag;

static int __init smdkv2xx_audio_init(void)
{
	int ret;
	WM8978_DEBUG(" smdkv210_audio_init start\n");	 
	//i2s_select_flag=1;
	i2s_select_flag=0;//lqm.x210
      smdkv210_audio_clk_init();
	smdkv2xx_snd_device = platform_device_alloc("soc-audio", -1);
	if (!smdkv2xx_snd_device)
		return -ENOMEM;
	

	platform_set_drvdata(smdkv2xx_snd_device, &smdkv2xx);
	//smdkv2xx_snd_devdata.dev = &smdkv2xx_snd_device->dev;

	ret = platform_device_add(smdkv2xx_snd_device);
	if (ret)
		platform_device_put(smdkv2xx_snd_device);

	
	return ret;
}

static void __exit smdkv2xx_audio_exit(void)
{
	platform_device_unregister(smdkv2xx_snd_device);
}

module_init(smdkv2xx_audio_init);
module_exit(smdkv2xx_audio_exit);

MODULE_AUTHOR("liuqiming");
MODULE_DESCRIPTION("ALSA SoC SMDK64XX WM8978");
MODULE_LICENSE("GPL");

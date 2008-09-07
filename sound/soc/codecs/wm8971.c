/*
 * wm8971.c  --  WM8971 ALSA SoC Audio driver
 *
 * Copyright 2005 Lab126, Inc.
 *
 * Author: Kenneth Kiraly <kiraly@lab126.com>
 *
 * Based on wm8753.c by Liam Girdwood
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include "wm8971.h"

#define AUDIO_NAME "wm8971"
#define WM8971_VERSION "0.20"

#undef	WM8971_DEBUG

#ifdef WM8971_DEBUG
#define dbg(format, arg...) \
	printk(KERN_DEBUG AUDIO_NAME ": " format "\n" , ## arg)
#else
#define dbg(format, arg...) do {} while (0)
#endif
#define err(format, arg...) \
	printk(KERN_ERR AUDIO_NAME ": " format "\n" , ## arg)
#define info(format, arg...) \
	printk(KERN_INFO AUDIO_NAME ": " format "\n" , ## arg)
#define warn(format, arg...) \
	printk(KERN_WARNING AUDIO_NAME ": " format "\n" , ## arg)

#define	WM8971_REG_COUNT		43

static struct workqueue_struct *wm8971_workq = NULL;

/* codec private data */
struct wm8971_priv {
	unsigned int sysclk;
};

/*
 * wm8971 register cache
 * We can't read the WM8971 register space when we
 * are using 2 wire for device control, so we cache them instead.
 */
static const u16 wm8971_reg[] = {
	0x0097, 0x0097, 0x0079, 0x0079,  /*  0 */
	0x0000, 0x0008, 0x0000, 0x000a,  /*  4 */
	0x0000, 0x0000, 0x00ff, 0x00ff,  /*  8 */
	0x000f, 0x000f, 0x0000, 0x0000,  /* 12 */
	0x0000, 0x007b, 0x0000, 0x0032,  /* 16 */
	0x0000, 0x00c3, 0x00c3, 0x00c0,  /* 20 */
	0x0000, 0x0000, 0x0000, 0x0000,  /* 24 */
	0x0000, 0x0000, 0x0000, 0x0000,  /* 28 */
	0x0000, 0x0000, 0x0050, 0x0050,  /* 32 */
	0x0050, 0x0050, 0x0050, 0x0050,  /* 36 */
	0x0079, 0x0079, 0x0079,          /* 40 */
};

static inline unsigned int wm8971_read_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg)
{
	u16 *cache = codec->reg_cache;
	if (reg < WM8971_REG_COUNT)
		return cache[reg];

	return -1;
}

static inline void wm8971_write_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg, unsigned int value)
{
	u16 *cache = codec->reg_cache;
	if (reg < WM8971_REG_COUNT)
		cache[reg] = value;
}

static int wm8971_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	u8 data[2];

	/* data is
	 *   D15..D9 WM8753 register offset
	 *   D8...D0 register data
	 */
	data[0] = (reg << 1) | ((value >> 8) & 0x0001);
	data[1] = value & 0x00ff;

	wm8971_write_reg_cache (codec, reg, value);
	if (codec->mach_write(codec->control_data, (long)data, 2) == 2)
		return 0;
	else
		return -EIO;
}

#define wm8971_reset(c)	wm8971_write(c, WM8971_RESET, 0)

/* WM8971 Controls */
static const char *wm8971_bass[] = { "Linear Control", "Adaptive Boost" };
static const char *wm8971_bass_filter[] = { "130Hz @ 48kHz",
	"200Hz @ 48kHz" };
static const char *wm8971_treble[] = { "8kHz", "4kHz" };
static const char *wm8971_alc_func[] = { "Off", "Right", "Left", "Stereo" };
static const char *wm8971_ng_type[] = { "Constant PGA Gain",
	"Mute ADC Output" };
static const char *wm8971_deemp[] = { "None", "32kHz", "44.1kHz", "48kHz" };
static const char *wm8971_mono_mux[] = {"Stereo", "Mono (Left)",
	"Mono (Right)", "Digital Mono"};
static const char *wm8971_dac_phase[] = { "Non Inverted", "Inverted" };
static const char *wm8971_lline_mux[] = {"Line", "NC", "NC", "PGA",
	"Differential"};
static const char *wm8971_rline_mux[] = {"Line", "Mic", "NC", "PGA",
	"Differential"};
static const char *wm8971_lpga_sel[] = {"Line", "NC", "NC", "Differential"};
static const char *wm8971_rpga_sel[] = {"Line", "Mic", "NC", "Differential"};
static const char *wm8971_adcpol[] = {"Normal", "L Invert", "R Invert",
	"L + R Invert"};

static const struct soc_enum wm8971_enum[] = {
	SOC_ENUM_SINGLE(WM8971_BASS, 7, 2, wm8971_bass),			/* 0 */
	SOC_ENUM_SINGLE(WM8971_BASS, 6, 2, wm8971_bass_filter),
	SOC_ENUM_SINGLE(WM8971_TREBLE, 6, 2, wm8971_treble),
	SOC_ENUM_SINGLE(WM8971_ALC1, 7, 4, wm8971_alc_func),
	SOC_ENUM_SINGLE(WM8971_NGATE, 1, 2, wm8971_ng_type),        /* 4 */
	SOC_ENUM_SINGLE(WM8971_ADCDAC, 1, 4, wm8971_deemp),
	SOC_ENUM_SINGLE(WM8971_ADCTL1, 4, 4, wm8971_mono_mux),
	SOC_ENUM_SINGLE(WM8971_ADCTL1, 1, 2, wm8971_dac_phase),
	SOC_ENUM_SINGLE(WM8971_LOUTM1, 0, 5, wm8971_lline_mux),     /* 8 */
	SOC_ENUM_SINGLE(WM8971_ROUTM1, 0, 5, wm8971_rline_mux),
	SOC_ENUM_SINGLE(WM8971_LADCIN, 6, 4, wm8971_lpga_sel),
	SOC_ENUM_SINGLE(WM8971_RADCIN, 6, 4, wm8971_rpga_sel),
	SOC_ENUM_SINGLE(WM8971_ADCDAC, 5, 4, wm8971_adcpol),        /* 12 */
	SOC_ENUM_SINGLE(WM8971_ADCIN, 6, 4, wm8971_mono_mux),
};

static const struct snd_kcontrol_new wm8971_snd_controls[] = {
	SOC_DOUBLE_R("Capture Volume", WM8971_LINVOL, WM8971_RINVOL, 0, 63, 0),
	SOC_DOUBLE_R("Capture ZC Switch", WM8971_LINVOL, WM8971_RINVOL, 6, 1, 0),
	SOC_DOUBLE_R("Capture Switch", WM8971_LINVOL, WM8971_RINVOL, 7, 1, 1),

	SOC_DOUBLE_R("Headphone Playback ZC Switch", WM8971_LOUT1V,
		WM8971_ROUT1V, 7, 1, 0),
	SOC_DOUBLE_R("Speaker Playback ZC Switch", WM8971_LOUT2V,
		WM8971_ROUT2V, 7, 1, 0),
	SOC_SINGLE("Mono Playback ZC Switch", WM8971_MOUTV, 7, 1, 0),

	SOC_DOUBLE_R("PCM Volume", WM8971_LDAC, WM8971_RDAC, 0, 255, 0),

	SOC_DOUBLE_R("Bypass Left Playback Volume", WM8971_LOUTM1,
		WM8971_LOUTM2, 4, 7, 1),
	SOC_DOUBLE_R("Bypass Right Playback Volume", WM8971_ROUTM1,
		WM8971_ROUTM2, 4, 7, 1),
	SOC_DOUBLE_R("Bypass Mono Playback Volume", WM8971_MOUTM1,
		WM8971_MOUTM2, 4, 7, 1),

	SOC_DOUBLE_R("Headphone Playback Volume", WM8971_LOUT1V,
		WM8971_ROUT1V, 0, 127, 0),
	SOC_DOUBLE_R("Speaker Playback Volume", WM8971_LOUT2V,
		WM8971_ROUT2V, 0, 127, 0),

	SOC_ENUM("Bass Boost", wm8971_enum[0]),
	SOC_ENUM("Bass Filter", wm8971_enum[1]),
	SOC_SINGLE("Bass Volume", WM8971_BASS, 0, 7, 1),

	SOC_SINGLE("Treble Volume", WM8971_TREBLE, 0, 7, 0),
	SOC_ENUM("Treble Cut-off", wm8971_enum[2]),

	SOC_SINGLE("Capture Filter Switch", WM8971_ADCDAC, 0, 1, 1),

	SOC_SINGLE("ALC Target Volume", WM8971_ALC1, 0, 7, 0),
	SOC_SINGLE("ALC Max Volume", WM8971_ALC1, 4, 7, 0),

	SOC_SINGLE("ALC Capture Target Volume", WM8971_ALC1, 0, 7, 0),
	SOC_SINGLE("ALC Capture Max Volume", WM8971_ALC1, 4, 7, 0),
	SOC_ENUM("ALC Capture Function", wm8971_enum[3]),
	SOC_SINGLE("ALC Capture ZC Switch", WM8971_ALC2, 7, 1, 0),
	SOC_SINGLE("ALC Capture Hold Time", WM8971_ALC2, 0, 15, 0),
	SOC_SINGLE("ALC Capture Decay Time", WM8971_ALC3, 4, 15, 0),
	SOC_SINGLE("ALC Capture Attack Time", WM8971_ALC3, 0, 15, 0),
	SOC_SINGLE("ALC Capture NG Threshold", WM8971_NGATE, 3, 31, 0),
	SOC_ENUM("ALC Capture NG Type", wm8971_enum[4]),
	SOC_SINGLE("ALC Capture NG Switch", WM8971_NGATE, 0, 1, 0),

	SOC_SINGLE("Capture 6dB Attenuate", WM8971_ADCDAC, 8, 1, 0),
	SOC_SINGLE("Playback 6dB Attenuate", WM8971_ADCDAC, 7, 1, 0),

	SOC_ENUM("Playback De-emphasis", wm8971_enum[5]),
	SOC_ENUM("Playback Function", wm8971_enum[6]),
	SOC_ENUM("Playback Phase", wm8971_enum[7]),

	SOC_DOUBLE_R("Mic Boost", WM8971_LADCIN, WM8971_RADCIN, 4, 3, 0),
};

/* add non-DAPM controls */
static int wm8971_add_controls(struct snd_soc_codec *codec, 
	struct snd_card *card)
{
	int err, i;

	for (i = 0; i < ARRAY_SIZE(wm8971_snd_controls); i++) {
		err = snd_ctl_add(card,
				snd_soc_cnew(&wm8971_snd_controls[i],
					codec, NULL));
		if (err < 0)
			return err;
	}
	return 0;
}

/*
 * DAPM Controls
 */

/* Left Mixer */
static const struct snd_kcontrol_new wm8971_left_mixer_controls[] = {
SOC_DAPM_SINGLE("Playback Switch", WM8971_LOUTM1, 8, 1, 0),
SOC_DAPM_SINGLE("Left Bypass Switch", WM8971_LOUTM1, 7, 1, 0),
SOC_DAPM_SINGLE("Right Playback Switch", WM8971_LOUTM2, 8, 1, 0),
SOC_DAPM_SINGLE("Right Bypass Switch", WM8971_LOUTM2, 7, 1, 0),
};

/* Right Mixer */
static const struct snd_kcontrol_new wm8971_right_mixer_controls[] = {
SOC_DAPM_SINGLE("Left Playback Switch", WM8971_ROUTM1, 8, 1, 0),
SOC_DAPM_SINGLE("Left Bypass Switch", WM8971_ROUTM1, 7, 1, 0),
SOC_DAPM_SINGLE("Playback Switch", WM8971_ROUTM2, 8, 1, 0),
SOC_DAPM_SINGLE("Right Bypass Switch", WM8971_ROUTM2, 7, 1, 0),
};

/* Mono Mixer */
static const struct snd_kcontrol_new wm8971_mono_mixer_controls[] = {
SOC_DAPM_SINGLE("Left Playback Switch", WM8971_MOUTM1, 8, 1, 0),
SOC_DAPM_SINGLE("Left Bypass Switch", WM8971_MOUTM1, 7, 1, 0),
SOC_DAPM_SINGLE("Right Playback Switch", WM8971_MOUTM2, 8, 1, 0),
SOC_DAPM_SINGLE("Right Bypass Switch", WM8971_MOUTM2, 7, 1, 0),
};

/* Left Line Mux */
static const struct snd_kcontrol_new wm8971_left_line_controls =
SOC_DAPM_ENUM("Route", wm8971_enum[8]);

/* Right Line Mux */
static const struct snd_kcontrol_new wm8971_right_line_controls =
SOC_DAPM_ENUM("Route", wm8971_enum[9]);

/* Left PGA Mux */
static const struct snd_kcontrol_new wm8971_left_pga_controls =
SOC_DAPM_ENUM("Route", wm8971_enum[10]);

/* Right PGA Mux */
static const struct snd_kcontrol_new wm8971_right_pga_controls =
SOC_DAPM_ENUM("Route", wm8971_enum[11]);

/* Mono ADC Mux */
static const struct snd_kcontrol_new wm8971_monomux_controls =
SOC_DAPM_ENUM("Route", wm8971_enum[13]);

static const struct snd_soc_dapm_widget wm8971_dapm_widgets[] = {
	SND_SOC_DAPM_MIXER("Left Mixer", SND_SOC_NOPM, 0, 0,
		&wm8971_left_mixer_controls[0],
		ARRAY_SIZE(wm8971_left_mixer_controls)),
	SND_SOC_DAPM_MIXER("Right Mixer", SND_SOC_NOPM, 0, 0,
		&wm8971_right_mixer_controls[0],
		ARRAY_SIZE(wm8971_right_mixer_controls)),
	SND_SOC_DAPM_MIXER("Mono Mixer", WM8971_PWR2, 2, 0,
		&wm8971_mono_mixer_controls[0],
		ARRAY_SIZE(wm8971_mono_mixer_controls)),

	SND_SOC_DAPM_PGA("Right Out 2", WM8971_PWR2, 3, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Left Out 2", WM8971_PWR2, 4, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right Out 1", WM8971_PWR2, 5, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Left Out 1", WM8971_PWR2, 6, 0, NULL, 0),
	SND_SOC_DAPM_DAC("Right DAC", "Right Playback", WM8971_PWR2, 7, 0),
	SND_SOC_DAPM_DAC("Left DAC", "Left Playback", WM8971_PWR2, 8, 0),
	SND_SOC_DAPM_PGA("Mono Out 1", WM8971_PWR2, 2, 0, NULL, 0),

	SND_SOC_DAPM_MICBIAS("Mic Bias", WM8971_PWR1, 1, 0),
	SND_SOC_DAPM_ADC("Right ADC", "Right Capture", WM8971_PWR1, 2, 0),
	SND_SOC_DAPM_ADC("Left ADC", "Left Capture", WM8971_PWR1, 3, 0),

	SND_SOC_DAPM_MUX("Left PGA Mux", WM8971_PWR1, 5, 0,
		&wm8971_left_pga_controls),
	SND_SOC_DAPM_MUX("Right PGA Mux", WM8971_PWR1, 4, 0,
		&wm8971_right_pga_controls),
	SND_SOC_DAPM_MUX("Left Line Mux", SND_SOC_NOPM, 0, 0,
		&wm8971_left_line_controls),
	SND_SOC_DAPM_MUX("Right Line Mux", SND_SOC_NOPM, 0, 0,
		&wm8971_right_line_controls),

	SND_SOC_DAPM_MUX("Left ADC Mux", SND_SOC_NOPM, 0, 0,
		&wm8971_monomux_controls),
	SND_SOC_DAPM_MUX("Right ADC Mux", SND_SOC_NOPM, 0, 0,
		&wm8971_monomux_controls),

	SND_SOC_DAPM_OUTPUT("LOUT1"),
	SND_SOC_DAPM_OUTPUT("ROUT1"),
	SND_SOC_DAPM_OUTPUT("LOUT2"),
	SND_SOC_DAPM_OUTPUT("ROUT2"),
	SND_SOC_DAPM_OUTPUT("MONO"),

	SND_SOC_DAPM_INPUT("LINPUT1"),
	SND_SOC_DAPM_INPUT("RINPUT1"),
	SND_SOC_DAPM_INPUT("MIC"),
};

static const char *audio_map[][3] = {
	/* left mixer */
	{"Left Mixer", "Playback Switch", "Left DAC"},
	{"Left Mixer", "Left Bypass Switch", "Left Line Mux"},
	{"Left Mixer", "Right Playback Switch", "Right DAC"},
	{"Left Mixer", "Right Bypass Switch", "Right Line Mux"},

	/* right mixer */
	{"Right Mixer", "Left Playback Switch", "Left DAC"},
	{"Right Mixer", "Left Bypass Switch", "Left Line Mux"},
	{"Right Mixer", "Playback Switch", "Right DAC"},
	{"Right Mixer", "Right Bypass Switch", "Right Line Mux"},

	/* left out 1 */
	{"Left Out 1", NULL, "Left Mixer"},
	{"LOUT1", NULL, "Left Out 1"},

	/* left out 2 */
	{"Left Out 2", NULL, "Left Mixer"},
	{"LOUT2", NULL, "Left Out 2"},

	/* right out 1 */
	{"Right Out 1", NULL, "Right Mixer"},
	{"ROUT1", NULL, "Right Out 1"},

	/* right out 2 */
	{"Right Out 2", NULL, "Right Mixer"},
	{"ROUT2", NULL, "Right Out 2"},

	/* mono mixer */
	{"Mono Mixer", "Left Playback Switch", "Left DAC"},
	{"Mono Mixer", "Left Bypass Switch", "Left Line Mux"},
	{"Mono Mixer", "Right Playback Switch", "Right DAC"},
	{"Mono Mixer", "Right Bypass Switch", "Right Line Mux"},

	/* mono out */
	{"Mono Out", NULL, "Mono Mixer"},
	{"MONO1", NULL, "Mono Out"},

	/* Left Line Mux */
	{"Left Line Mux", "Line", "LINPUT1"},
	{"Left Line Mux", "PGA", "Left PGA Mux"},
	{"Left Line Mux", "Differential", "Differential Mux"},

	/* Right Line Mux */
	{"Right Line Mux", "Line", "RINPUT1"},
	{"Right Line Mux", "Mic", "MIC"},
	{"Right Line Mux", "PGA", "Right PGA Mux"},
	{"Right Line Mux", "Differential", "Differential Mux"},

	/* Left PGA Mux */
	{"Left PGA Mux", "Line", "LINPUT1"},
	{"Left PGA Mux", "Differential", "Differential Mux"},

	/* Right PGA Mux */
	{"Right PGA Mux", "Line", "RINPUT1"},
	{"Right PGA Mux", "Differential", "Differential Mux"},

	/* Differential Mux */
	{"Differential Mux", "Line", "LINPUT1"},
	{"Differential Mux", "Line", "RINPUT1"},

	/* Left ADC Mux */
	{"Left ADC Mux", "Stereo", "Left PGA Mux"},
	{"Left ADC Mux", "Mono (Left)", "Left PGA Mux"},
	{"Left ADC Mux", "Digital Mono", "Left PGA Mux"},

	/* Right ADC Mux */
	{"Right ADC Mux", "Stereo", "Right PGA Mux"},
	{"Right ADC Mux", "Mono (Right)", "Right PGA Mux"},
	{"Right ADC Mux", "Digital Mono", "Right PGA Mux"},

	/* ADC */
	{"Left ADC", NULL, "Left ADC Mux"},
	{"Right ADC", NULL, "Right ADC Mux"},

	/* terminator */
	{NULL, NULL, NULL},
};

static int wm8971_add_widgets(struct snd_soc_codec *codec, 
	struct snd_soc_card *soc_card)
{
	int i;

	for(i = 0; i < ARRAY_SIZE(wm8971_dapm_widgets); i++) {
		snd_soc_dapm_new_control(soc_card, codec, 
			&wm8971_dapm_widgets[i]);
	}

	/* set up audio path audio_mapnects */
	for(i = 0; audio_map[i][0] != NULL; i++) {
		snd_soc_dapm_connect_input(soc_card, audio_map[i][0],
			audio_map[i][1], audio_map[i][2]);
	}

	snd_soc_dapm_new_widgets(soc_card);
	return 0;
}

struct _coeff_div {
	u32 mclk;
	u32 rate;
	u16 fs;
	u8 sr:5;
	u8 usb:1;
};

/* codec hifi mclk clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
	/* 8k */
	{12288000, 8000, 1536, 0x6, 0x0},
	{11289600, 8000, 1408, 0x16, 0x0},
	{18432000, 8000, 2304, 0x7, 0x0},
	{16934400, 8000, 2112, 0x17, 0x0},
	{12000000, 8000, 1500, 0x6, 0x1},

	/* 11.025k */
	{11289600, 11025, 1024, 0x18, 0x0},
	{16934400, 11025, 1536, 0x19, 0x0},
	{12000000, 11025, 1088, 0x19, 0x1},

	/* 16k */
	{12288000, 16000, 768, 0xa, 0x0},
	{18432000, 16000, 1152, 0xb, 0x0},
	{12000000, 16000, 750, 0xa, 0x1},

	/* 22.05k */
	{11289600, 22050, 512, 0x1a, 0x0},
	{16934400, 22050, 768, 0x1b, 0x0},
	{12000000, 22050, 544, 0x1b, 0x1},

	/* 32k */
	{12288000, 32000, 384, 0xc, 0x0},
	{18432000, 32000, 576, 0xd, 0x0},
	{12000000, 32000, 375, 0xa, 0x1},

	/* 44.1k */
	{11289600, 44100, 256, 0x10, 0x0},
	{16934400, 44100, 384, 0x11, 0x0},
	{12000000, 44100, 272, 0x11, 0x1},

	/* 48k */
	{12288000, 48000, 256, 0x0, 0x0},
	{18432000, 48000, 384, 0x1, 0x0},
	{12000000, 48000, 250, 0x0, 0x1},

	/* 88.2k */
	{11289600, 88200, 128, 0x1e, 0x0},
	{16934400, 88200, 192, 0x1f, 0x0},
	{12000000, 88200, 136, 0x1f, 0x1},

	/* 96k */
	{12288000, 96000, 128, 0xe, 0x0},
	{18432000, 96000, 192, 0xf, 0x0},
	{12000000, 96000, 125, 0xe, 0x1},
};

static int get_coeff(int mclk, int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(coeff_div); i++) {
		if (coeff_div[i].rate == rate && coeff_div[i].mclk == mclk)
			return i;
	}
	return -EINVAL;
}

static int wm8971_set_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct wm8971_priv *wm8971 = codec->private_data;

	switch (freq) {
	case 11289600:
	case 12000000:
	case 12288000:
	case 16934400:
	case 18432000:
		wm8971->sysclk = freq;
		return 0;
	}
	return -EINVAL;
}

static int wm8971_set_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 iface = 0;

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface = 0x0040;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface |= 0x0002;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= 0x0001;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= 0x0003;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface |= 0x0013;
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= 0x0090;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= 0x0080;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		iface |= 0x0010;
		break;
	default:
		return -EINVAL;
	}

	wm8971_write(codec, WM8971_IFACE, iface);
	return 0;
}

static int wm8971_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_link *pcm_link = substream->private_data;
	struct snd_soc_codec *codec = pcm_link->codec;
	struct wm8971_priv *wm8971 = codec->private_data;
	u16 iface = wm8971_read_reg_cache(codec, WM8971_IFACE) & 0x1f3;
	u16 srate = wm8971_read_reg_cache(codec, WM8971_SRATE) & 0x1c0;
	int coeff = get_coeff(wm8971->sysclk, params_rate(params));

	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		iface |= 0x0004;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		iface |= 0x0008;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		iface |= 0x000c;
		break;
	}

	/* set iface & srate */
	wm8971_write(codec, WM8971_IFACE, iface);
	if (coeff >= 0)
		wm8971_write(codec, WM8971_SRATE, srate |
			(coeff_div[coeff].sr << 1) | coeff_div[coeff].usb);

	return 0;
}

static int wm8971_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 mute_reg = wm8971_read_reg_cache(codec, WM8971_ADCDAC) & 0xfff7;

	if (mute)
		wm8971_write(codec, WM8971_ADCDAC, mute_reg | 0x8);
	else
		wm8971_write(codec, WM8971_ADCDAC, mute_reg);
	return 0;
}

static int wm8971_dapm_event(struct snd_soc_codec *codec, int event)
{
	u16 pwr_reg = wm8971_read_reg_cache(codec, WM8971_PWR1) & 0xfe3e;

	switch (event) {
	case SNDRV_CTL_POWER_D0: /* full On */
		/* set vmid to 50k and unmute dac */
		wm8971_write(codec, WM8971_PWR1, pwr_reg | 0x00c1);
		break;
	case SNDRV_CTL_POWER_D1: /* partial On */
	case SNDRV_CTL_POWER_D2: /* partial On */
		/* set vmid to 5k for quick power up */
		wm8971_write(codec, WM8971_PWR1, pwr_reg | 0x01c0);
		break;
	case SNDRV_CTL_POWER_D3hot: /* Off, with power */
		/* mute dac and set vmid to 500k, enable VREF */
		wm8971_write(codec, WM8971_PWR1, pwr_reg | 0x0140);
		break;
	case SNDRV_CTL_POWER_D3cold: /* Off, without power */
		wm8971_write(codec, WM8971_PWR1, 0x0001);
		break;
	}
	codec->dapm_state = event;
	return 0;
}

static void wm8971_work(struct work_struct *work)
{
	struct snd_soc_codec *codec =
		container_of(work, struct snd_soc_codec, delayed_work.work);
	wm8971_dapm_event(codec, codec->dapm_state);
}

static int wm8971_suspend(struct device *dev, pm_message_t state)
{
	struct snd_soc_codec *codec = to_snd_soc_codec(dev);
	
	wm8971_dapm_event(codec, SNDRV_CTL_POWER_D3cold);
	return 0;
}

static int wm8971_resume(struct device *dev)
{
	struct snd_soc_codec *codec = to_snd_soc_codec(dev);
	int i;
	u8 data[2];
	u16 *cache = codec->reg_cache;

	/* Sync reg_cache with the hardware */
	for (i = 0; i < ARRAY_SIZE(wm8971_reg); i++) {
		if (i + 1 == WM8971_RESET)
			continue;
		data[0] = (i << 1) | ((cache[i] >> 8) & 0x0001);
		data[1] = cache[i] & 0x00ff;
		codec->mach_write(codec->control_data, (long)data, 2);
	}

	wm8971_dapm_event(codec, SNDRV_CTL_POWER_D3hot);

	/* charge wm8971 caps */
	if (codec->suspend_dapm_state == SNDRV_CTL_POWER_D0) {
		wm8971_dapm_event(codec, SNDRV_CTL_POWER_D2);
		codec->dapm_state = SNDRV_CTL_POWER_D0;
		queue_delayed_work(wm8971_workq, &codec->delayed_work,
			msecs_to_jiffies(1000));
	}

	return 0;
}

/*
 * initialise the WM8971 codec
 */
static int wm8971_codec_io_probe(struct snd_soc_codec *codec,
	struct snd_soc_card *soc_card)
{
	int reg;

	wm8971_reset(codec);

	/* charge output caps */
	wm8971_dapm_event(codec, SNDRV_CTL_POWER_D2);
	codec->dapm_state = SNDRV_CTL_POWER_D3hot;
	queue_delayed_work(wm8971_workq, &codec->delayed_work,
		msecs_to_jiffies(1000));

	/* set the update bits */
	reg = wm8971_read_reg_cache(codec, WM8971_LDAC);
	wm8971_write(codec, WM8971_LDAC, reg | 0x0100);
	reg = wm8971_read_reg_cache(codec, WM8971_RDAC);
	wm8971_write(codec, WM8971_RDAC, reg | 0x0100);

	reg = wm8971_read_reg_cache(codec, WM8971_LOUT1V);
	wm8971_write(codec, WM8971_LOUT1V, reg | 0x0100);
	reg = wm8971_read_reg_cache(codec, WM8971_ROUT1V);
	wm8971_write(codec, WM8971_ROUT1V, reg | 0x0100);

	reg = wm8971_read_reg_cache(codec, WM8971_LOUT2V);
	wm8971_write(codec, WM8971_LOUT2V, reg | 0x0100);
	reg = wm8971_read_reg_cache(codec, WM8971_ROUT2V);
	wm8971_write(codec, WM8971_ROUT2V, reg | 0x0100);

	reg = wm8971_read_reg_cache(codec, WM8971_LINVOL);
	wm8971_write(codec, WM8971_LINVOL, reg | 0x0100);
	reg = wm8971_read_reg_cache(codec, WM8971_RINVOL);
	wm8971_write(codec, WM8971_RINVOL, reg | 0x0100);
	
	wm8971_add_controls(codec, soc_card->card);
	wm8971_add_widgets(codec, soc_card);

	return 0;
}

static struct snd_soc_codec_ops wm8971_codec_ops = {
	.dapm_event	= wm8971_dapm_event,
	.read		= wm8971_read_reg_cache,
	.write		= wm8971_write,
	.io_probe	= wm8971_codec_io_probe,
};

static int wm8971_codec_probe(struct device *dev)
{
	struct snd_soc_codec *codec = to_snd_soc_codec(dev);

	info("WM8971 Audio Codec %s", WM8971_VERSION);

	codec->reg_cache = kmemdup(wm8971_reg, sizeof(wm8971_reg), GFP_KERNEL);
	if (codec->reg_cache == NULL)
		return -ENOMEM;
	codec->reg_cache_size = sizeof(wm8971_reg);
	
	INIT_DELAYED_WORK(&codec->delayed_work, wm8971_work);
	codec->owner = THIS_MODULE;
	codec->ops = &wm8971_codec_ops;
	return 0;
}

static int wm8971_codec_remove(struct device *dev)
{
	struct snd_soc_codec *codec = to_snd_soc_codec(dev);
	
	if (codec->control_data)
		wm8971_dapm_event(codec, SNDRV_CTL_POWER_D3cold);
	kfree(codec->reg_cache);
	return 0;
}

#define WM8971_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 | \
		SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000)

#define WM8971_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
	SNDRV_PCM_FMTBIT_S24_LE)


static const struct snd_soc_pcm_stream wm8971_dai_playback = {
	.stream_name	= "Playback",
	.channels_min	= 1,
	.channels_max	= 2,
	.rates		= WM8971_RATES,
	.formats	= WM8971_FORMATS,
};

static const struct snd_soc_pcm_stream wm8971_dai_capture = {
	.stream_name	= "Capture",
	.channels_min	= 1,
	.channels_max	= 2,
	.rates		= WM8971_RATES,
	.formats	= WM8971_FORMATS,
};

/* dai ops, called by soc_card drivers */
static const struct snd_soc_dai_ops wm8971_dai_ops = {
	.digital_mute	= wm8971_digital_mute,
	.set_sysclk	= wm8971_set_sysclk,
	.set_fmt	= wm8971_set_fmt,
};

/* audio ops, called by alsa */
static const struct snd_soc_ops wm8971_dai_audio_ops = {
	.hw_params	= wm8971_hw_params,
};

static int wm8971_dai_probe(struct device *dev)
{
	struct snd_soc_dai *dai = to_snd_soc_dai(dev);
	struct wm8971_priv *wm8971;
	
	wm8971 = kzalloc(sizeof(struct wm8971_priv), GFP_KERNEL);
	if (wm8971 == NULL)
		return -ENOMEM;
	
	dai->private_data = wm8971;
	dai->ops = &wm8971_dai_ops;
	dai->audio_ops = &wm8971_dai_audio_ops;
	dai->capture = &wm8971_dai_capture;
	dai->playback = &wm8971_dai_playback;
	return 0;
}

static int wm8971_dai_remove(struct device *dev)
{
	struct snd_soc_dai *dai = to_snd_soc_dai(dev);
	kfree(dai->private_data);
	return 0;
}

const char wm8971_codec[SND_SOC_CODEC_NAME_SIZE] = "wm8971-codec";
EXPORT_SYMBOL_GPL(wm8971_codec);

static struct snd_soc_device_driver wm8971_codec_driver = {
	.type	= SND_SOC_BUS_TYPE_CODEC,
	.driver	= {
		.name 		= wm8971_codec,
		.owner		= THIS_MODULE,
		.bus 		= &asoc_bus_type,
		.probe		= wm8971_codec_probe,
		.remove		= __devexit_p(wm8971_codec_remove),
		.suspend	= wm8971_suspend,
		.resume		= wm8971_resume,
	},
};

const char wm8971_hifi_dai[SND_SOC_CODEC_NAME_SIZE] = "wm8971-hifi-dai";
EXPORT_SYMBOL_GPL(wm8971_hifi_dai);

static struct snd_soc_device_driver wm8971_hifi_dai_driver = {
	.type	= SND_SOC_BUS_TYPE_DAI,
	.driver	= {
		.name 		= wm8971_hifi_dai,
		.owner		= THIS_MODULE,
		.bus 		= &asoc_bus_type,
		.probe		= wm8971_dai_probe,
		.remove		= __devexit_p(wm8971_dai_remove),
	},
};

static __init int wm8971_init(void)
{
	int ret = 0;
	
	ret = driver_register(&wm8971_codec_driver.driver);
	if (ret < 0)
		return ret;
	ret = driver_register(&wm8971_hifi_dai_driver.driver);
	if (ret < 0) {
		driver_unregister(&wm8971_codec_driver.driver);
		return ret;
	}
	return ret;
}

static __exit void wm8971_exit(void)
{
	driver_unregister(&wm8971_hifi_dai_driver.driver);
	driver_unregister(&wm8971_codec_driver.driver);
}

module_init(wm8971_init);
module_exit(wm8971_exit);


MODULE_DESCRIPTION("ASoC WM8971 driver");
MODULE_AUTHOR("Lab126");
MODULE_LICENSE("GPL");

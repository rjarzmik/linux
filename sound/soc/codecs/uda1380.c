/*
 * uda1380.c - Philips UDA1380 ALSA SoC audio driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2007 Philipp Zabel <philipp.zabel@gmail.com>
 * Improved support for DAPM and audio routing/mixing capabilities,
 * added TLV support.
 *
 * Modified by Richard Purdie <richard@openedhand.com> to fit into SoC
 * codec model.
 *
 * Copyright (c) 2005 Giorgio Padrin <giorgio@mandarinlogiq.org>
 * Copyright 2005 Openedhand Ltd.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/initval.h>
#include <sound/info.h>
#include <sound/pcm.h>
#include <sound/soc-codec.h>
#include <sound/soc-dai.h>
#include <sound/soc-dapm.h>
#include <sound/soc-card.h>
#include <sound/tlv.h>

#include "uda1380.h"

#define UDA1380_VERSION "0.6"
#define AUDIO_NAME "uda1380"
/*
 * Debug
 */

#define UDA1380_DEBUG 0

#ifdef UDA1380_DEBUG
#define dbg(format, arg...) \
	printk(KERN_DEBUG AUDIO_NAME ": " format "\n" , ## arg)
#else
#define dbg(format, arg...) do {} while (0)
#endif

struct uda1380_data {
	struct snd_soc_dai *dai;
};

/*
 * uda1380 register cache
 */
static const u16 uda1380_reg[UDA1380_CACHEREGNUM] = {
	0x0502, 0x0000, 0x0000, 0x3f3f,
	0x0202, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0xff00, 0x0000, 0x4800,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x8000, 0x0002, 0x0000,
};

/*
 * read uda1380 register cache
 */
static inline unsigned int uda1380_read_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg)
{
	u16 *cache = codec->reg_cache;
	if (reg == UDA1380_RESET)
		return 0;
	if (reg >= UDA1380_CACHEREGNUM)
		return -1;
	return cache[reg];
}

/*
 * write uda1380 register cache
 */
static inline void uda1380_write_reg_cache(struct snd_soc_codec *codec,
	u16 reg, unsigned int value)
{
	u16 *cache = codec->reg_cache;
	if (reg >= UDA1380_CACHEREGNUM)
		return;
	cache[reg] = value;
}

/*
 * write to the UDA1380 register space
 */
static int uda1380_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	u8 data[3];

	/* data is
	 *   data[0] is register offset
	 *   data[1] is MS byte
	 *   data[2] is LS byte
	 */
	data[0] = reg;
	data[1] = (value & 0xff00) >> 8;
	data[2] = value & 0x00ff;

	uda1380_write_reg_cache (codec, reg, value);

	/* the interpolator & decimator regs must only be written when the
	 * codec DAI is active.
	 */
	if (!codec->active && (reg >= UDA1380_MVOL))
		return 0;
	dbg("uda hw write %x val %x", reg, value);
	if (codec->soc_phys_write(codec->control_data, (long)data, 3) == 3) {
#if UDA1380_DEBUG
		unsigned int val;
		i2c_master_send(codec->control_data, data, 1);
		i2c_master_recv(codec->control_data, data, 2);
		val = (data[0]<<8) | data[1];
		if (val != value) {
			dbg("READ BACK VAL %x", (data[0]<<8) | data[1]);
			return -EIO;
		}
#endif
		return 0;
	} else
		return -EIO;
}

#define uda1380_reset(c)	uda1380_write(c, UDA1380_RESET, 0)

/* declarations of ALSA reg_elem_REAL controls */
static const char *uda1380_deemp[] = {"None", "32kHz", "44.1kHz", "48kHz",
				      "96kHz"};
static const char *uda1380_input_sel[] = { "Line", "Mic + Line R", "Line L", "Mic" };
static const char *uda1380_output_sel[] = {"DAC", "Analog Mixer"};
static const char *uda1380_spf_mode[] = {"Flat", "Minimum1", "Minimum2",
					 "Maximum"};
static const char *uda1380_capture_sel[] = {"ADC", "Digital Mixer"};
static const char *uda1380_sel_ns[] = { "3rd-order", "5th-order" };
static const char *uda1380_mix_control[] = { "off", "PCM only", "before sound processing", "after sound processing" };
static const char *uda1380_sdet_setting[] = { "3200", "4800", "9600", "19200" };
static const char *uda1380_os_setting[] = { "single-speed", "double-speed (no mixing)", "quad-speed (no mixing)" };

static const struct soc_enum uda1380_deemp_enum[] = {
	SOC_ENUM_SINGLE(UDA1380_DEEMP, 8, 5, uda1380_deemp),
	SOC_ENUM_SINGLE(UDA1380_DEEMP, 0, 5, uda1380_deemp),
};
static const struct soc_enum uda1380_input_sel_enum =
	SOC_ENUM_SINGLE(UDA1380_ADC, 2, 4, uda1380_input_sel);		/* SEL_MIC, SEL_LNA */
static const struct soc_enum uda1380_output_sel_enum =
	SOC_ENUM_SINGLE(UDA1380_PM, 7, 2, uda1380_output_sel);		/* R02_EN_AVC */
static const struct soc_enum uda1380_spf_enum =
	SOC_ENUM_SINGLE(UDA1380_MODE, 14, 4, uda1380_spf_mode);		/* M */
static const struct soc_enum uda1380_capture_sel_enum =
	SOC_ENUM_SINGLE(UDA1380_IFACE, 6, 2, uda1380_capture_sel);	/* SEL_SOURCE */
static const struct soc_enum uda1380_sel_ns_enum =
	SOC_ENUM_SINGLE(UDA1380_MIXER, 14, 2, uda1380_sel_ns);		/* SEL_NS */
static const struct soc_enum uda1380_mix_enum =
	SOC_ENUM_SINGLE(UDA1380_MIXER, 12, 4, uda1380_mix_control);	/* MIX, MIX_POS */
static const struct soc_enum uda1380_sdet_enum =
	SOC_ENUM_SINGLE(UDA1380_MIXER, 4, 4, uda1380_sdet_setting);	/* SD_VALUE */
static const struct soc_enum uda1380_os_enum =
	SOC_ENUM_SINGLE(UDA1380_MIXER, 0, 3, uda1380_os_setting);	/* OS */

/**
 * snd_soc_info_volsw - single mixer info callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to provide information about a single mixer control.
 *
 * Returns 0 for success.
 */
int snd_soc_info_volsw_s8(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	int max = (kcontrol->private_value >> 16) & 0xff;
	int min = (kcontrol->private_value >> 24) & 0xff;

	/* 00000000 (0) ...(-0.25 dB)... 11111000 (-78 dB), 11111100 -INF */

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
	uinfo->value.integer.min = min;
	uinfo->value.integer.max = max;
	return 0;
}

/**
 * snd_soc_get_volsw_sgn - single mixer get callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to get the value of a single mixer control.
 *
 * Returns 0 for success.
 */
int snd_soc_get_volsw_s8(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int reg = kcontrol->private_value & 0xff;
//	int max = (kcontrol->private_value >> 16) & 0xff;
//	int min = (kcontrol->private_value >> 24) & 0xff;

	ucontrol->value.integer.value[0] =
		(signed char)(snd_soc_read(codec, reg) & 0xff);
	ucontrol->value.integer.value[1] =
		(signed char)((snd_soc_read(codec, reg) >> 8) & 0xff);

	return 0;
}

/**
 * snd_soc_put_volsw_sgn - single mixer put callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to set the value of a single mixer control.
 *
 * Returns 0 for success.
 */
int snd_soc_put_volsw_s8(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int reg = kcontrol->private_value & 0xff;
//	int max = (kcontrol->private_value >> 16) & 0xff;
//	int min = (kcontrol->private_value >> 24) & 0xff;
	int err;
	unsigned short val, val2, val_mask = 0; //lg

	val = (signed char)ucontrol->value.integer.value[0] & 0xff;
	val2 = (signed char)ucontrol->value.integer.value[1] & 0xff;
	val |= val2<<8;
	err = snd_soc_update_bits(codec, reg, val_mask, val);
	return err;
}

DECLARE_TLV_DB_SCALE(amix_tlv, -4950, 150, 1);	/* from -48 dB in 1.5 dB steps (mute instead of -49.5 dB) */

static const unsigned int mvol_tlv[] = {
	TLV_DB_RANGE_HEAD(3),
	0,15, TLV_DB_SCALE_ITEM(-8200, 100, 1),	/* from -78 dB in 1 dB steps (3 dB steps, really) */
	16,43, TLV_DB_SCALE_ITEM(-6600, 50, 0),	/* from -66 dB in 0.5 dB steps (2 dB steps, really) */
	44,252, TLV_DB_SCALE_ITEM(-5200, 25, 0),	/* from -52 dB in 0.25 dB steps */
};

static const unsigned int vc_tlv[] = {
	TLV_DB_RANGE_HEAD(4),
	0,7, TLV_DB_SCALE_ITEM(-7800, 150, 1),	/* from -72 dB in 1.5 dB steps (6 dB steps really) */
	8,15, TLV_DB_SCALE_ITEM(-6600, 75, 0),	/* from -66 dB in 0.75 dB steps (3 dB steps really) */
	16,43, TLV_DB_SCALE_ITEM(-6000, 50, 0),	/* from -60 dB in 0.5 dB steps (2 dB steps really) */ 
	44,228, TLV_DB_SCALE_ITEM(-4600, 25, 0),/* from -46 dB in 0.25 dB steps */
};

DECLARE_TLV_DB_SCALE(tr_tlv, 0, 200, 0);	/* from 0 dB to 6 dB in 2 dB steps, if SPF mode != flat */
DECLARE_TLV_DB_SCALE(bb_tlv, 0, 200, 0);	/* from 0 dB to 24 dB in 2 dB steps, if SPF mode == maximum */
						/* (SPF mode == flat cuts off at 18 dB max */

static const unsigned int dec_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	-128,48, TLV_DB_SCALE_ITEM(-6350, 50, 1),
	/* 0011000 (48 dB) ...(0.5 dB)... 10000001 (-63 dB) 10000000 -INF */
};

DECLARE_TLV_DB_SCALE(pga_tlv, 0, 300, 0);	/* from 0 dB to 24 dB in 3 dB steps */
DECLARE_TLV_DB_SCALE(vga_tlv, 0, 200, 0);	/* from 0 dB to 30 dB in 2 dB steps */


#define SOC_DOUBLE_S8_TLV(xname, reg, min, max, tlv_array) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname),\
	.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ|SNDRV_CTL_ELEM_ACCESS_READWRITE,\
	.tlv.p = (tlv_array), \
	.info = snd_soc_info_volsw_s8, .get = snd_soc_get_volsw_s8, \
	.put = snd_soc_put_volsw_s8, \
	.private_value = (reg) | (((signed char)max) << 16) | (((signed char)min) << 24) }

static const struct snd_kcontrol_new uda1380_snd_controls[] = {
	SOC_DOUBLE_TLV("Analog Mixer Volume", UDA1380_AMIX, 0, 8, 44, 1, amix_tlv),	/* AVCR, AVCL */
	SOC_DOUBLE_TLV("Master Playback Volume", UDA1380_MVOL, 0, 8, 252, 1, mvol_tlv),	/* MVCL, MVCR */
	SOC_SINGLE_TLV("ADC Playback Volume", UDA1380_MIXVOL, 8, 228, 1, vc_tlv),	/* VC2 */
	SOC_SINGLE_TLV("PCM Playback Volume", UDA1380_MIXVOL, 0, 228, 1, vc_tlv),	/* VC1 */
	SOC_ENUM("Sound Processing Filter", uda1380_spf_enum),				/* M */
	SOC_DOUBLE_TLV("Tone Control - Treble", UDA1380_MODE, 4, 12, 3, 0, tr_tlv), 	/* TRL, TRR */
	SOC_DOUBLE_TLV("Tone Control - Bass", UDA1380_MODE, 0, 8, 15, 0, bb_tlv),	/* BBL, BBR */
/**/	SOC_SINGLE("Master Playback Switch", UDA1380_DEEMP, 14, 1, 1),		/* MTM */
	SOC_SINGLE("ADC Playback Switch", UDA1380_DEEMP, 11, 1, 1),		/* MT2 from decimation filter */
	SOC_ENUM("ADC Playback De-emphasis", uda1380_deemp_enum[0]),		/* DE2 */
	SOC_SINGLE("PCM Playback Switch", UDA1380_DEEMP, 3, 1, 1),		/* MT1, from digital data input */
	SOC_ENUM("PCM Playback De-emphasis", uda1380_deemp_enum[1]),		/* DE1 */
	SOC_SINGLE("DAC Polarity inverting Switch", UDA1380_MIXER, 15, 1, 0),	/* DA_POL_INV */
	SOC_ENUM("Noise Shaper", uda1380_sel_ns_enum),				/* SEL_NS */
	SOC_ENUM("Digital Mixer Signal Control", uda1380_mix_enum),		/* MIX_POS, MIX */
	SOC_SINGLE("Silence Switch", UDA1380_MIXER, 7, 1, 0),			/* SILENCE, force DAC output to silence */
	SOC_SINGLE("Silence Detector Switch", UDA1380_MIXER, 6, 1, 0),		/* SDET_ON */
	SOC_ENUM("Silence Detector Setting", uda1380_sdet_enum),		/* SD_VALUE */
	SOC_ENUM("Oversampling Input", uda1380_os_enum),			/* OS */
	SOC_DOUBLE_S8_TLV("ADC Capture Volume", UDA1380_DEC, -128, 48, dec_tlv),	/* ML_DEC, MR_DEC */
/**/	SOC_SINGLE("ADC Capture Switch", UDA1380_PGA, 15, 1, 1),		/* MT_ADC */
	SOC_DOUBLE_TLV("Line Capture Volume", UDA1380_PGA, 0, 8, 8, 0, pga_tlv), /* PGA_GAINCTRLL, PGA_GAINCTRLR */
	SOC_SINGLE("ADC Polarity inverting Switch", UDA1380_ADC, 12, 1, 0),	/* ADCPOL_INV */
	SOC_SINGLE_TLV("Mic Capture Volume", UDA1380_ADC, 8, 15, 0, vga_tlv),	/* VGA_CTRL */
	SOC_SINGLE("DC Filter Bypass Switch", UDA1380_ADC, 1, 1, 0),		/* SKIP_DCFIL (before decimator) */
	SOC_SINGLE("DC Filter Enable Switch", UDA1380_ADC, 0, 1, 0),		/* EN_DCFIL (at output of decimator) */
	SOC_SINGLE("AGC Timing", UDA1380_AGC, 8, 7, 0),			/* TODO: enum, see table 62 */
	SOC_SINGLE("AGC Target level", UDA1380_AGC, 2, 3, 1),			/* AGC_LEVEL */
	/* -5.5, -8, -11.5, -14 dBFS */
	SOC_SINGLE("AGC Switch", UDA1380_AGC, 0, 1, 0),
};

/**
 * uda1380_snd_soc_cnew
 *
 * temporary copy of snd_soc_cnew that doesn't overwrite .access
 */
struct snd_kcontrol *uda1380_snd_soc_cnew(const struct snd_kcontrol_new *_template,
	void *data, char *long_name)
{
	struct snd_kcontrol_new template;

	memcpy(&template, _template, sizeof(template));
	if (long_name)
		template.name = long_name;
	template.index = 0;

	return snd_ctl_new1(&template, data);
}

/* add non dapm controls */
static int uda1380_add_controls(struct snd_soc_codec *codec, 
	struct snd_card *card)
{
	int err, i;

	for (i = 0; i < ARRAY_SIZE(uda1380_snd_controls); i++) {
		err = snd_ctl_add(card,
			uda1380_snd_soc_cnew(&uda1380_snd_controls[i],
				codec, NULL));
		if (err < 0)
			return err;
	}

	return 0;
}

/* Input mux */
static const struct snd_kcontrol_new uda1380_input_mux_control =
	SOC_DAPM_ENUM("Route", uda1380_input_sel_enum);

/* Output mux */
static const struct snd_kcontrol_new uda1380_output_mux_control =
	SOC_DAPM_ENUM("Route", uda1380_output_sel_enum);

/* Capture mux */
static const struct snd_kcontrol_new uda1380_capture_mux_control =
	SOC_DAPM_ENUM("Route", uda1380_capture_sel_enum);


static const struct snd_soc_dapm_widget uda1380_dapm_widgets[] = {
	SND_SOC_DAPM_MUX("Input Mux", SND_SOC_NOPM, 0, 0,
		&uda1380_input_mux_control),
	SND_SOC_DAPM_MUX("Output Mux", SND_SOC_NOPM, 0, 0,
		&uda1380_output_mux_control),
	SND_SOC_DAPM_MUX("Capture Mux", SND_SOC_NOPM, 0, 0,
		&uda1380_capture_mux_control),
	SND_SOC_DAPM_PGA("Left PGA", UDA1380_PM, 3, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right PGA", UDA1380_PM, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Mic LNA", UDA1380_PM, 4, 0, NULL, 0),
	SND_SOC_DAPM_ADC("Left ADC", "Left Capture", UDA1380_PM, 2, 0),
	SND_SOC_DAPM_ADC("Right ADC", "Right Capture", UDA1380_PM, 0, 0),
	SND_SOC_DAPM_INPUT("VINM"),
	SND_SOC_DAPM_INPUT("VINL"),
	SND_SOC_DAPM_INPUT("VINR"),
	SND_SOC_DAPM_MIXER("Analog Mixer", UDA1380_PM, 6, 0, NULL, 0),
	SND_SOC_DAPM_OUTPUT("VOUTLHP"),
	SND_SOC_DAPM_OUTPUT("VOUTRHP"),
	SND_SOC_DAPM_OUTPUT("VOUTL"),
	SND_SOC_DAPM_OUTPUT("VOUTR"),
	SND_SOC_DAPM_DAC("DAC", "Playback", UDA1380_PM, 10, 0),
	SND_SOC_DAPM_PGA("HeadPhone Driver", UDA1380_PM, 13, 0, NULL, 0),
};

static const char *audio_map[][3] = {

	/* output mux */
	{"HeadPhone Driver", NULL, "Output Mux"},
	{"VOUTR", NULL, "Output Mux"},
	{"VOUTL", NULL, "Output Mux"},

	{"Analog Mixer", NULL, "VINR"},
	{"Analog Mixer", NULL, "VINL"},
	{"Analog Mixer", NULL, "DAC"},

	{"Output Mux", "DAC", "DAC"},
	{"Output Mux", "Analog Mixer", "Analog Mixer"},

	/* {"DAC", "Digital Mixer", "I2S" } */

	/* headphone driver */
	{"VOUTLHP", NULL, "HeadPhone Driver"},
	{"VOUTRHP", NULL, "HeadPhone Driver"},

	/* input mux */
	{"Left ADC", NULL, "Input Mux"},
	{"Input Mux", "Mic", "Mic LNA"},
	{"Input Mux", "Mic + Line R", "Mic LNA"},
	{"Input Mux", "Line L", "Left PGA"},
	{"Input Mux", "Line", "Left PGA"},

	/* right input */
	{"Right ADC", "Mic + Line R", "Right PGA"},
	{"Right ADC", "Line", "Right PGA"},

	/* inputs */
	{"Mic LNA", NULL, "VINM"},
	{"Left PGA", NULL, "VINL"},
	{"Right PGA", NULL, "VINR"},

	/* terminator */
	{NULL, NULL, NULL},
};

static int uda1380_add_widgets(struct snd_soc_codec *codec, 
	struct snd_soc_card *soc_card)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(uda1380_dapm_widgets); i++)
		snd_soc_dapm_new_control(soc_card, codec, 
			&uda1380_dapm_widgets[i]);

	/* set up audio path interconnects */
	for (i = 0; audio_map[i][0] != NULL; i++)
		snd_soc_dapm_add_route(soc_card, audio_map[i][0],
			audio_map[i][1], audio_map[i][2]);

	snd_soc_dapm_init(soc_card);
	return 0;
}

static int uda1380_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	int iface;
	/* set up DAI based upon fmt */

	iface = uda1380_read_reg_cache (codec, UDA1380_IFACE);
	iface &= ~(R01_SFORI_MASK | R01_SIM | R01_SFORO_MASK);

	/* FIXME: how to select I2S for DATAO and MSB for DATAI correctly? */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface |= R01_SFORI_I2S | R01_SFORO_I2S;
		break;
	case SND_SOC_DAIFMT_LSB:
		iface |= R01_SFORI_LSB16 | R01_SFORO_I2S;
		break;
	case SND_SOC_DAIFMT_MSB:
		iface |= R01_SFORI_MSB | R01_SFORO_I2S;
	}

	if ((fmt & SND_SOC_DAIFMT_MASTER_MASK) == SND_SOC_DAIFMT_CBM_CFM)
		iface |= R01_SIM;

	uda1380_write(codec, UDA1380_IFACE, iface);
	return 0;
}

/*
 * Flush reg cache
 * We can only write the interpolator and decimator registers
 * when the DAI is being clocked by the CPU DAI. It's up to the
 * soc_card and cpu DAI driver to do this before we are called.
 */
static int uda1380_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int reg, reg_start, reg_end, clk;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		reg_start = UDA1380_MVOL;
		reg_end = UDA1380_MIXER;
	} else {
		reg_start = UDA1380_DEC;
		reg_end = UDA1380_AGC;
		/* FIXME - wince values */
//		uda1380_write(codec, UDA1380_PM, uda1380_read_reg_cache (codec, UDA1380_PM) | R02_PON_LNA | R02_PON_DAC);
		uda1380_write(codec, UDA1380_PM, 0x851f);
//		uda1380_write_reg_cache(codec, UDA1380_DEC, 0x0000);
//		uda1380_write_reg_cache(codec, UDA1380_PGA, uda1380_read_reg_cache (codec, UDA1380_PM) & ~R21_MT_ADC); /* unmute */
//		uda1380_write_reg_cache(codec, UDA1380_ADC, 0x0001/*0x090d*/);
			// ADC_POL_INV=0 VGA_CTRL=9(1001:18dB) gain SEL_LNA=1 SEL_MIC=1 EN_DCFIL=1
	}

	/* FIXME disable DAC_CLK */
	clk = uda1380_read_reg_cache (codec, 00);
	uda1380_write(codec, UDA1380_CLK, clk & ~R00_DAC_CLK);

	for (reg = reg_start; reg <= reg_end; reg++ ) {
		dbg("flush reg %x val %x:",reg, uda1380_read_reg_cache (codec, reg));
		uda1380_write(codec, reg, uda1380_read_reg_cache (codec, reg));
	}

	/* FIXME enable DAC_CLK */
	uda1380_write(codec, UDA1380_CLK, clk | R00_DAC_CLK);

	return 0;
}

static int uda1380_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 clk = uda1380_read_reg_cache(codec, UDA1380_CLK);

	/* set WSPLL power and divider if running from this clock */
	if (clk & R00_DAC_CLK) {
		int rate = params_rate(params);
		u16 pm = uda1380_read_reg_cache(codec, UDA1380_PM);
		clk &= ~0x3; /* clear SEL_LOOP_DIV */
		switch (rate) {
		case 6250 ... 12500:
			clk |= 0x0;
			break;
		case 12501 ... 25000:
			clk |= 0x1;
			break;
		case 25001 ... 50000:
			clk |= 0x2;
			break;
		case 50001 ... 100000:
			clk |= 0x3;
			break;
		}
		uda1380_write(codec, UDA1380_PM, R02_PON_PLL | pm);
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		clk |= R00_EN_DAC | R00_EN_INT;
	else
		clk |= R00_EN_ADC | R00_EN_DEC;

	uda1380_write(codec, UDA1380_CLK, clk);
	return 0;
}

static void uda1380_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 clk = uda1380_read_reg_cache(codec, UDA1380_CLK);

	/* shut down WSPLL power if running from this clock */
	if (clk & R00_DAC_CLK) {
		u16 pm = uda1380_read_reg_cache(codec, UDA1380_PM);
		uda1380_write(codec, UDA1380_PM, ~R02_PON_PLL & pm);
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		clk &= ~(R00_EN_DAC | R00_EN_INT);
	else
		clk &= ~(R00_EN_ADC | R00_EN_DEC);

	uda1380_write(codec, UDA1380_CLK, clk);
}

static int uda1380_mute(struct snd_soc_dai *codec_dai, int mute)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 mute_reg = uda1380_read_reg_cache(codec, UDA1380_DEEMP) & ~R13_MTM;

	/* FIXME: mute(codec,0) is called when the magician clock is already
	 * set to WSPLL, but for some unknown reason writing to interpolator
	 * registers works only when clocked by SYSCLK */
	u16 clk = uda1380_read_reg_cache(codec, UDA1380_CLK);
	uda1380_write(codec, UDA1380_CLK, ~R00_DAC_CLK & clk);
	if (mute)
		uda1380_write(codec, UDA1380_DEEMP, mute_reg | R13_MTM);
	else
		uda1380_write(codec, UDA1380_DEEMP, mute_reg);
	uda1380_write(codec, UDA1380_CLK, clk);
	return 0;
}

int uda1380_set_dac_clk(struct snd_soc_codec *codec, int dac_clk)
{
	/* set clock input */
	switch (dac_clk) {
	case UDA1380_DAC_CLK_SYSCLK:
		uda1380_write(codec, UDA1380_CLK, 0);
		break;
	case UDA1380_DAC_CLK_WSPLL:
		uda1380_write(codec, UDA1380_CLK, R00_DAC_CLK);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(uda1380_set_dac_clk);

static int uda1380_set_bias_level(struct snd_soc_codec *codec, 
	enum snd_soc_dapm_bias_level level)
{
	int pm = uda1380_read_reg_cache(codec, UDA1380_PM);

	switch (level) {
	case SND_SOC_BIAS_ON: /* full On */
	case SND_SOC_BIAS_PREPARE: /* partial On */
		/* enable internal bias */
		uda1380_write(codec, UDA1380_PM, R02_PON_BIAS | pm);
		break;
	case SND_SOC_BIAS_STANDBY: /* Off, with power */
		/* everything off except internal bias */
		uda1380_write(codec, UDA1380_PM, R02_PON_BIAS);
		break;
	case SND_SOC_BIAS_OFF: /* Off, without power */
		/* everything off, inactive */
		uda1380_write(codec, UDA1380_PM, 0x0);
		break;
	}
	codec->bias_level = level;
	return 0;
}

static int uda1380_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_codec *codec = platform_get_drvdata(pdev);

	uda1380_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int uda1380_resume(struct platform_device *pdev)
{
	struct snd_soc_codec *codec = platform_get_drvdata(pdev);

	int i;
	u8 data[2];
	u16 *cache = codec->reg_cache;

	/* Sync reg_cache with the hardware */
	for (i = 0; i < ARRAY_SIZE(uda1380_reg); i++) {
		data[0] = (i << 1) | ((cache[i] >> 8) & 0x0001);
		data[1] = cache[i] & 0x00ff;
		codec->soc_phys_write(codec->control_data, (long)data, 2);
	}
	uda1380_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	uda1380_set_bias_level(codec, codec->suspend_bias_level);
	return 0;
}

static int uda1380_codec_init(struct snd_soc_codec *codec,
	struct snd_soc_card *soc_card)
{
	uda1380_reset(codec);

	/* power on device */
	uda1380_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	uda1380_add_controls(codec, soc_card->card);
	uda1380_add_widgets(codec, soc_card);
	return 0;
}

static void uda1380_codec_exit(struct snd_soc_codec *codec,
	struct snd_soc_card *soc_card)
{
	uda1380_set_bias_level(codec, SND_SOC_BIAS_OFF);
}

#define UDA1380_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
		       SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
		       SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)

#define UDA1380_FORMATS SNDRV_PCM_FMTBIT_S16_LE

static struct snd_soc_dai_caps uda1380_playback = {
	.stream_name	= "Playback",
	.channels_min	= 1,
	.channels_max	= 2,
	.rates		= UDA1380_RATES,
	.formats	= UDA1380_FORMATS,
};

static struct snd_soc_dai_caps uda1380_capture = {
	.stream_name	= "Capture",
	.channels_min	= 1,
	.channels_max	= 2,
	.rates		= UDA1380_RATES,
	.formats	= UDA1380_FORMATS,
};

/* dai ops, called by soc_card drivers */
static struct snd_soc_dai_ops uda1380_dai_ops = {
	.digital_mute	= uda1380_mute,
	.set_fmt	= uda1380_set_dai_fmt,
	/* audio ops, called by alsa */
	.hw_params	= uda1380_hw_params,
	.prepare	= uda1380_prepare,
	.shutdown	= uda1380_shutdown,
};

/* for modprobe */
const char uda1380_codec_id[] = "uda1380-codec";
EXPORT_SYMBOL_GPL(uda1380_codec_id);

const char uda1380_codec_dai_id[] = "uda1380-codec-dai";
EXPORT_SYMBOL_GPL(uda1380_codec_dai_id);

static int uda1380_dai_probe(struct uda1380_data *uda1380, struct device *dev)
{
	struct snd_soc_dai *dai;
	int ret;

	dai = snd_soc_dai_allocate();
	if (dai == NULL)
		return -ENOMEM;

	dai->name = uda1380_codec_dai_id;
	dai->ops = &uda1380_dai_ops;
	dai->playback = &uda1380_playback;
	dai->capture = &uda1380_capture;
	dai->dev = dev;
	ret = snd_soc_register_codec_dai(dai);
	if (ret < 0) {
		snd_soc_dai_free(dai);
		return ret;
	}
	uda1380->dai = dai;
	return 0;
}

static int uda1380_codec_probe(struct platform_device *pdev)
{
	struct snd_soc_codec *codec;
	struct uda1380_data *uda1380;
	int ret;

	printk(KERN_INFO "uda1380 Audio Codec %s", UDA1380_VERSION);

	codec = snd_soc_codec_allocate();
	if (codec == NULL)
		return -ENOMEM;

	uda1380 = kzalloc(sizeof(struct uda1380_data), GFP_KERNEL);
	if (uda1380 == NULL) {
		ret = -ENOMEM;
		goto uda1380_err;
	}

	codec->dev = &pdev->dev;
	codec->name = uda1380_codec_id;
	codec->set_bias_level = uda1380_set_bias_level;
	codec->codec_read = uda1380_read_reg_cache;
	codec->codec_write = uda1380_write;
	codec->init = uda1380_codec_init;
	codec->exit = uda1380_codec_exit;
	codec->reg_cache_size = UDA1380_CACHEREGNUM;
	codec->reg_cache_step = 1;
	codec->private_data = uda1380;
	platform_set_drvdata(pdev, codec);
		
	ret = snd_soc_register_codec(codec);
	if (ret < 0)
		goto codec_err;
	ret = uda1380_dai_probe(uda1380, &pdev->dev);
	if (ret < 0)
		goto dai_err;
	return ret;

dai_err:
	snd_soc_register_codec(codec);
codec_err:
	kfree(uda1380);
uda1380_err:
	snd_soc_codec_free(codec);
	return ret;
}

static int uda1380_codec_remove(struct platform_device *pdev)
{
	struct snd_soc_codec *codec = platform_get_drvdata(pdev);
	struct uda1380_data *uda1380 = codec->private_data;
	
	snd_soc_unregister_codec_dai(uda1380->dai);
	snd_soc_dai_free(uda1380->dai);
	kfree(uda1380);
	snd_soc_unregister_codec(codec);
	snd_soc_codec_free(codec);
	return 0;
}

static struct platform_driver uda1380_codec_driver = {
	.driver = {
		.name		= uda1380_codec_id,
		.owner		= THIS_MODULE,
	},
	.probe		= uda1380_codec_probe,
	.remove		= __devexit_p(uda1380_codec_remove),
	.suspend	= uda1380_suspend,
	.resume		= uda1380_resume,
};

static __init int uda1380_init(void)
{
	return platform_driver_register(&uda1380_codec_driver);
}

static __exit void uda1380_exit(void)
{
	platform_driver_unregister(&uda1380_codec_driver);
}

module_init(uda1380_init);
module_exit(uda1380_exit)

MODULE_AUTHOR("Giorgio Padrin");
MODULE_DESCRIPTION("Audio support for codec Philips UDA1380");
MODULE_LICENSE("GPL");

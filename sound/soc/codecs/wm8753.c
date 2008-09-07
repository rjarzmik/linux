/*
 * wm8753.c  --  WM8753 ALSA Soc Audio driver
 *
 * Copyright 2003 Wolfson Microelectronics PLC.
 * Author: Liam Girdwood
 *         liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 * Notes:
 *  The WM8753 is a low power, high quality stereo codec with integrated PCM
 *  codec designed for portable digital telephony applications.
 *
 * Dual DAI:-
 *
 * This driver support 2 DAI PCM's. This makes the default PCM available for
 * HiFi audio (e.g. MP3, ogg) playback/capture and the other PCM available for
 * voice.
 *
 * Please note that the voice PCM can be connected directly to a Bluetooth
 * codec or GSM modem and thus cannot be read or written to, although it is
 * available to be configured with snd_hw_params(), etc and kcontrols in the
 * normal alsa manner.
 *
 * Fast DAI switching:-
 *
 * The driver can now fast switch between the DAI configurations via a
 * an alsa kcontrol. This allows the PCM to remain open.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <asm/div64.h>

#include "wm8753.h"

#define AUDIO_NAME "wm8753"
#define WM8753_VERSION "0.16"

/*
 * Debug
 */

#define WM8753_DEBUG 0

#ifdef WM8753_DEBUG
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

static int caps_charge = 2000;
module_param(caps_charge, int, 0);
MODULE_PARM_DESC(caps_charge, "WM8753 cap charge time (msecs)");

/* dai private data */
struct wm8753_dai_priv {
	unsigned int clk;
};

/* codec private data */
struct wm8753_codec_priv {
	unsigned int codec_dai_mode;
	struct snd_soc_dai *hifi_dai;
	struct snd_soc_dai *voice_dai;
};


/*
 * wm8753 register cache
 * We can't read the WM8753 register space when we
 * are using 2 wire for device control, so we cache them instead.
 */
static const u16 wm8753_reg[] = {
	0x0008, 0x0000, 0x000a, 0x000a,
	0x0033, 0x0000, 0x0007, 0x00ff,
	0x00ff, 0x000f, 0x000f, 0x007b,
	0x0000, 0x0032, 0x0000, 0x00c3,
	0x00c3, 0x00c0, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0055,
	0x0005, 0x0050, 0x0055, 0x0050,
	0x0055, 0x0050, 0x0055, 0x0079,
	0x0079, 0x0079, 0x0079, 0x0079,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0097, 0x0097, 0x0000, 0x0004,
	0x0000, 0x0083, 0x0024, 0x01ba,
	0x0000, 0x0083, 0x0024, 0x01ba,
	0x0000, 0x0000
};

/*
 * read wm8753 register cache
 */
static inline unsigned int wm8753_read_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg)
{
	u16 *cache = codec->reg_cache;
	if (reg < 1 || reg > (ARRAY_SIZE(wm8753_reg) + 1))
		return -1;
	return cache[reg - 1];
}

/*
 * write wm8753 register cache
 */
static inline void wm8753_write_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg, unsigned int value)
{
	u16 *cache = codec->reg_cache;
	if (reg < 1 || reg > 0x3f)
		return;
	cache[reg - 1] = value;
}

/*
 * write to the WM8753 register space
 */
static int wm8753_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	u8 data[2];

	/* data is
	 *   D15..D9 WM8753 register offset
	 *   D8...D0 register data
	 */
	data[0] = (reg << 1) | ((value >> 8) & 0x0001);
	data[1] = value & 0x00ff;

	wm8753_write_reg_cache (codec, reg, value);
	if (codec->soc_phys_write(codec->control_data, (long)data, 2) == 2)
		return 0;
	else
		return -EIO;
}

#define wm8753_reset(c) wm8753_write(c, WM8753_RESET, 0)

/*
 * WM8753 Controls
 */
static const char *wm8753_base[] = {"Linear Control", "Adaptive Boost"};
static const char *wm8753_base_filter[] =
	{"130Hz @ 48kHz", "200Hz @ 48kHz", "100Hz @ 16kHz", "400Hz @ 48kHz",
	"100Hz @ 8kHz", "200Hz @ 8kHz"};
static const char *wm8753_treble[] = {"8kHz", "4kHz"};
static const char *wm8753_alc_func[] = {"Off", "Right", "Left", "Stereo"};
static const char *wm8753_ng_type[] = {"Constant PGA Gain", "Mute ADC Output"};
static const char *wm8753_3d_func[] = {"Capture", "Playback"};
static const char *wm8753_3d_uc[] = {"2.2kHz", "1.5kHz"};
static const char *wm8753_3d_lc[] = {"200Hz", "500Hz"};
static const char *wm8753_deemp[] = {"None", "32kHz", "44.1kHz", "48kHz"};
static const char *wm8753_mono_mix[] = {"Stereo", "Left", "Right", "Mono"};
static const char *wm8753_dac_phase[] = {"Non Inverted", "Inverted"};
static const char *wm8753_line_mix[] = {"Line 1 + 2", "Line 1 - 2",
	"Line 1", "Line 2"};
static const char *wm8753_mono_mux[] = {"Line Mix", "Rx Mix"};
static const char *wm8753_right_mux[] = {"Line 2", "Rx Mix"};
static const char *wm8753_left_mux[] = {"Line 1", "Rx Mix"};
static const char *wm8753_rxmsel[] = {"RXP - RXN", "RXP + RXN", "RXP", "RXN"};
static const char *wm8753_sidetone_mux[] = {"Left PGA", "Mic 1", "Mic 2",
	"Right PGA"};
static const char *wm8753_mono2_src[] = {"Inverted Mono 1", "Left", "Right",
	"Left + Right"};
static const char *wm8753_out3[] = {"VREF", "ROUT2", "Left + Right"};
static const char *wm8753_out4[] = {"VREF", "Capture ST", "LOUT2"};
static const char *wm8753_radcsel[] = {"PGA", "Line or RXP-RXN", "Sidetone"};
static const char *wm8753_ladcsel[] = {"PGA", "Line or RXP-RXN", "Line"};
static const char *wm8753_mono_adc[] = {"Stereo", "Analogue Mix Left",
	"Analogue Mix Right", "Digital Mono Mix"};
static const char *wm8753_adc_hp[] = {"3.4Hz @ 48kHz", "82Hz @ 16k",
	"82Hz @ 8kHz", "170Hz @ 8kHz"};
static const char *wm8753_adc_filter[] = {"HiFi", "Voice"};
static const char *wm8753_mic_sel[] = {"Mic 1", "Mic 2", "Mic 3"};
static const char *wm8753_dai_mode[] = {"DAI 0", "DAI 1", "DAI 2", "DAI 3"};
static const char *wm8753_dat_sel[] = {"Stereo", "Left ADC", "Right ADC",
	"Channel Swap"};

static const struct soc_enum wm8753_enum[] = {
SOC_ENUM_SINGLE(WM8753_BASS, 7, 2, wm8753_base),
SOC_ENUM_SINGLE(WM8753_BASS, 4, 6, wm8753_base_filter),
SOC_ENUM_SINGLE(WM8753_TREBLE, 6, 2, wm8753_treble),
SOC_ENUM_SINGLE(WM8753_ALC1, 7, 4, wm8753_alc_func),
SOC_ENUM_SINGLE(WM8753_NGATE, 1, 2, wm8753_ng_type),
SOC_ENUM_SINGLE(WM8753_3D, 7, 2, wm8753_3d_func),
SOC_ENUM_SINGLE(WM8753_3D, 6, 2, wm8753_3d_uc),
SOC_ENUM_SINGLE(WM8753_3D, 5, 2, wm8753_3d_lc),
SOC_ENUM_SINGLE(WM8753_DAC, 1, 4, wm8753_deemp),
SOC_ENUM_SINGLE(WM8753_DAC, 4, 4, wm8753_mono_mix),
SOC_ENUM_SINGLE(WM8753_DAC, 6, 2, wm8753_dac_phase),
SOC_ENUM_SINGLE(WM8753_INCTL1, 3, 4, wm8753_line_mix),
SOC_ENUM_SINGLE(WM8753_INCTL1, 2, 2, wm8753_mono_mux),
SOC_ENUM_SINGLE(WM8753_INCTL1, 1, 2, wm8753_right_mux),
SOC_ENUM_SINGLE(WM8753_INCTL1, 0, 2, wm8753_left_mux),
SOC_ENUM_SINGLE(WM8753_INCTL2, 6, 4, wm8753_rxmsel),
SOC_ENUM_SINGLE(WM8753_INCTL2, 4, 4, wm8753_sidetone_mux),
SOC_ENUM_SINGLE(WM8753_OUTCTL, 7, 4, wm8753_mono2_src),
SOC_ENUM_SINGLE(WM8753_OUTCTL, 0, 3, wm8753_out3),
SOC_ENUM_SINGLE(WM8753_ADCTL2, 7, 3, wm8753_out4),
SOC_ENUM_SINGLE(WM8753_ADCIN, 2, 3, wm8753_radcsel),
SOC_ENUM_SINGLE(WM8753_ADCIN, 0, 3, wm8753_ladcsel),
SOC_ENUM_SINGLE(WM8753_ADCIN, 4, 4, wm8753_mono_adc),
SOC_ENUM_SINGLE(WM8753_ADC, 2, 4, wm8753_adc_hp),
SOC_ENUM_SINGLE(WM8753_ADC, 4, 2, wm8753_adc_filter),
SOC_ENUM_SINGLE(WM8753_MICBIAS, 6, 3, wm8753_mic_sel),
SOC_ENUM_SINGLE(WM8753_IOCTL, 2, 4, wm8753_dai_mode),
SOC_ENUM_SINGLE(WM8753_ADC, 7, 4, wm8753_dat_sel),
};


static int wm8753_get_dai(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec =  snd_kcontrol_chip(kcontrol);
	int mode = wm8753_read_reg_cache(codec, WM8753_IOCTL);

	ucontrol->value.integer.value[0] = (mode & 0xc) >> 2;
	return 0;
}

static int wm8753_set_dai(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec =  snd_kcontrol_chip(kcontrol);
	struct wm8753_codec_priv *wm8753 = codec->private_data;
	int mode = wm8753_read_reg_cache(codec, WM8753_IOCTL);

	if (((mode & 0xc) >> 2) == ucontrol->value.integer.value[0])
		return 0;

	mode &= 0xfff3;
	mode |= (ucontrol->value.integer.value[0] << 2);

	wm8753_write(codec, WM8753_IOCTL, mode);
	wm8753->codec_dai_mode = ucontrol->value.integer.value[0];
	return 1;
}

static const DECLARE_TLV_DB_LINEAR(rec_mix_tlv, -1500, 600);

static const struct snd_kcontrol_new wm8753_snd_controls[] = {
SOC_DOUBLE_R("PCM Volume", WM8753_LDAC, WM8753_RDAC, 0, 255, 0),

SOC_DOUBLE_R("ADC Capture Volume", WM8753_LADC, WM8753_RADC, 0, 255, 0),

SOC_DOUBLE_R("Headphone Playback Volume", WM8753_LOUT1V, WM8753_ROUT1V, 0, 127, 0),
SOC_DOUBLE_R("Speaker Playback Volume", WM8753_LOUT2V, WM8753_ROUT2V, 0, 127, 0),

SOC_SINGLE("Mono Playback Volume", WM8753_MOUTV, 0, 127, 0),

SOC_DOUBLE_R("Bypass Playback Volume", WM8753_LOUTM1, WM8753_ROUTM1, 4, 7, 1),
SOC_DOUBLE_R("Sidetone Playback Volume", WM8753_LOUTM2, WM8753_ROUTM2, 4, 7, 1),
SOC_DOUBLE_R("Voice Playback Volume", WM8753_LOUTM2, WM8753_ROUTM2, 0, 7, 1),

SOC_DOUBLE_R("Headphone Playback ZC Switch", WM8753_LOUT1V, WM8753_ROUT1V, 7, 1, 0),
SOC_DOUBLE_R("Speaker Playback ZC Switch", WM8753_LOUT2V, WM8753_ROUT2V, 7, 1, 0),

SOC_SINGLE("Mono Bypass Playback Volume", WM8753_MOUTM1, 4, 7, 1),
SOC_SINGLE("Mono Sidetone Playback Volume", WM8753_MOUTM2, 4, 7, 1),
SOC_SINGLE("Mono Voice Playback Volume", WM8753_MOUTM2, 4, 7, 1),
SOC_SINGLE("Mono Playback ZC Switch", WM8753_MOUTV, 7, 1, 0),

SOC_ENUM("Bass Boost", wm8753_enum[0]),
SOC_ENUM("Bass Filter", wm8753_enum[1]),
SOC_SINGLE("Bass Volume", WM8753_BASS, 0, 15, 1),

SOC_SINGLE("Treble Volume", WM8753_TREBLE, 0, 15, 1),
SOC_ENUM("Treble Cut-off", wm8753_enum[2]),

SOC_DOUBLE_TLV("Sidetone Capture Volume", WM8753_RECMIX1, 0, 4, 7, 1, rec_mix_tlv),
SOC_SINGLE_TLV("Voice Sidetone Capture Volume", WM8753_RECMIX2, 0, 7, 1, rec_mix_tlv),

SOC_DOUBLE_R("Capture Volume", WM8753_LINVOL, WM8753_RINVOL, 0, 63, 0),
SOC_DOUBLE_R("Capture ZC Switch", WM8753_LINVOL, WM8753_RINVOL, 6, 1, 0),
SOC_DOUBLE_R("Capture Switch", WM8753_LINVOL, WM8753_RINVOL, 7, 1, 1),

SOC_ENUM("Capture Filter Select", wm8753_enum[23]),
SOC_ENUM("Capture Filter Cut-off", wm8753_enum[24]),
SOC_SINGLE("Capture Filter Switch", WM8753_ADC, 0, 1, 1),

SOC_SINGLE("ALC Capture Target Volume", WM8753_ALC1, 0, 7, 0),
SOC_SINGLE("ALC Capture Max Volume", WM8753_ALC1, 4, 7, 0),
SOC_ENUM("ALC Capture Function", wm8753_enum[3]),
SOC_SINGLE("ALC Capture ZC Switch", WM8753_ALC2, 8, 1, 0),
SOC_SINGLE("ALC Capture Hold Time", WM8753_ALC2, 0, 15, 1),
SOC_SINGLE("ALC Capture Decay Time", WM8753_ALC3, 4, 15, 1),
SOC_SINGLE("ALC Capture Attack Time", WM8753_ALC3, 0, 15, 0),
SOC_SINGLE("ALC Capture NG Threshold", WM8753_NGATE, 3, 31, 0),
SOC_ENUM("ALC Capture NG Type", wm8753_enum[4]),
SOC_SINGLE("ALC Capture NG Switch", WM8753_NGATE, 0, 1, 0),

SOC_ENUM("3D Function", wm8753_enum[5]),
SOC_ENUM("3D Upper Cut-off", wm8753_enum[6]),
SOC_ENUM("3D Lower Cut-off", wm8753_enum[7]),
SOC_SINGLE("3D Volume", WM8753_3D, 1, 15, 0),
SOC_SINGLE("3D Switch", WM8753_3D, 0, 1, 0),

SOC_SINGLE("Capture 6dB Attenuate", WM8753_ADCTL1, 2, 1, 0),
SOC_SINGLE("Playback 6dB Attenuate", WM8753_ADCTL1, 1, 1, 0),

SOC_ENUM("De-emphasis", wm8753_enum[8]),
SOC_ENUM("Playback Mono Mix", wm8753_enum[9]),
SOC_ENUM("Playback Phase", wm8753_enum[10]),

SOC_SINGLE("Mic2 Capture Volume", WM8753_INCTL1, 7, 3, 0),
SOC_SINGLE("Mic1 Capture Volume", WM8753_INCTL1, 5, 3, 0),

SOC_ENUM_EXT("DAI Mode", wm8753_enum[26], wm8753_get_dai, wm8753_set_dai),

SOC_ENUM("ADC Data Select", wm8753_enum[27]),
};

/*
 * _DAPM_ Controls
 */

/* Left Mixer */
static const struct snd_kcontrol_new wm8753_left_mixer_controls[] = {
SOC_DAPM_SINGLE("Voice Playback Switch", WM8753_LOUTM2, 8, 1, 0),
SOC_DAPM_SINGLE("Sidetone Playback Switch", WM8753_LOUTM2, 7, 1, 0),
SOC_DAPM_SINGLE("Left Playback Switch", WM8753_LOUTM1, 8, 1, 0),
SOC_DAPM_SINGLE("Bypass Playback Switch", WM8753_LOUTM1, 7, 1, 0),
};

/* Right mixer */
static const struct snd_kcontrol_new wm8753_right_mixer_controls[] = {
SOC_DAPM_SINGLE("Voice Playback Switch", WM8753_ROUTM2, 8, 1, 0),
SOC_DAPM_SINGLE("Sidetone Playback Switch", WM8753_ROUTM2, 7, 1, 0),
SOC_DAPM_SINGLE("Right Playback Switch", WM8753_ROUTM1, 8, 1, 0),
SOC_DAPM_SINGLE("Bypass Playback Switch", WM8753_ROUTM1, 7, 1, 0),
};

/* Mono mixer */
static const struct snd_kcontrol_new wm8753_mono_mixer_controls[] = {
SOC_DAPM_SINGLE("Left Playback Switch", WM8753_MOUTM1, 8, 1, 0),
SOC_DAPM_SINGLE("Right Playback Switch", WM8753_MOUTM2, 8, 1, 0),
SOC_DAPM_SINGLE("Voice Playback Switch", WM8753_MOUTM2, 3, 1, 0),
SOC_DAPM_SINGLE("Sidetone Playback Switch", WM8753_MOUTM2, 7, 1, 0),
SOC_DAPM_SINGLE("Bypass Playback Switch", WM8753_MOUTM1, 7, 1, 0),
};

/* Mono 2 Mux */
static const struct snd_kcontrol_new wm8753_mono2_controls =
SOC_DAPM_ENUM("Route", wm8753_enum[17]);

/* Out 3 Mux */
static const struct snd_kcontrol_new wm8753_out3_controls =
SOC_DAPM_ENUM("Route", wm8753_enum[18]);

/* Out 4 Mux */
static const struct snd_kcontrol_new wm8753_out4_controls =
SOC_DAPM_ENUM("Route", wm8753_enum[19]);

/* ADC Mono Mix */
static const struct snd_kcontrol_new wm8753_adc_mono_controls =
SOC_DAPM_ENUM("Route", wm8753_enum[22]);

/* Record mixer */
static const struct snd_kcontrol_new wm8753_record_mixer_controls[] = {
SOC_DAPM_SINGLE("Voice Capture Switch", WM8753_RECMIX2, 3, 1, 0),
SOC_DAPM_SINGLE("Left Capture Switch", WM8753_RECMIX1, 3, 1, 0),
SOC_DAPM_SINGLE("Right Capture Switch", WM8753_RECMIX1, 7, 1, 0),
};

/* Left ADC mux */
static const struct snd_kcontrol_new wm8753_adc_left_controls =
SOC_DAPM_ENUM("Route", wm8753_enum[21]);

/* Right ADC mux */
static const struct snd_kcontrol_new wm8753_adc_right_controls =
SOC_DAPM_ENUM("Route", wm8753_enum[20]);

/* MIC mux */
static const struct snd_kcontrol_new wm8753_mic_mux_controls =
SOC_DAPM_ENUM("Route", wm8753_enum[16]);

/* ALC mixer */
static const struct snd_kcontrol_new wm8753_alc_mixer_controls[] = {
SOC_DAPM_SINGLE("Line Capture Switch", WM8753_INCTL2, 3, 1, 0),
SOC_DAPM_SINGLE("Mic2 Capture Switch", WM8753_INCTL2, 2, 1, 0),
SOC_DAPM_SINGLE("Mic1 Capture Switch", WM8753_INCTL2, 1, 1, 0),
SOC_DAPM_SINGLE("Rx Capture Switch", WM8753_INCTL2, 0, 1, 0),
};

/* Left Line mux */
static const struct snd_kcontrol_new wm8753_line_left_controls =
SOC_DAPM_ENUM("Route", wm8753_enum[14]);

/* Right Line mux */
static const struct snd_kcontrol_new wm8753_line_right_controls =
SOC_DAPM_ENUM("Route", wm8753_enum[13]);

/* Mono Line mux */
static const struct snd_kcontrol_new wm8753_line_mono_controls =
SOC_DAPM_ENUM("Route", wm8753_enum[12]);

/* Line mux and mixer */
static const struct snd_kcontrol_new wm8753_line_mux_mix_controls =
SOC_DAPM_ENUM("Route", wm8753_enum[11]);

/* Rx mux and mixer */
static const struct snd_kcontrol_new wm8753_rx_mux_mix_controls =
SOC_DAPM_ENUM("Route", wm8753_enum[15]);

/* Mic Selector Mux */
static const struct snd_kcontrol_new wm8753_mic_sel_mux_controls =
SOC_DAPM_ENUM("Route", wm8753_enum[25]);

static const struct snd_soc_dapm_widget wm8753_dapm_widgets[] = {
SND_SOC_DAPM_MICBIAS("Mic Bias", WM8753_PWR1, 5, 0),
SND_SOC_DAPM_MIXER("Left Mixer", WM8753_PWR4, 0, 0,
	&wm8753_left_mixer_controls[0], ARRAY_SIZE(wm8753_left_mixer_controls)),
SND_SOC_DAPM_PGA("Left Out 1", WM8753_PWR3, 8, 0, NULL, 0),
SND_SOC_DAPM_PGA("Left Out 2", WM8753_PWR3, 6, 0, NULL, 0),
SND_SOC_DAPM_DAC("Left DAC", "Left HiFi Playback", WM8753_PWR1, 3, 0),
SND_SOC_DAPM_OUTPUT("LOUT1"),
SND_SOC_DAPM_OUTPUT("LOUT2"),
SND_SOC_DAPM_MIXER("Right Mixer", WM8753_PWR4, 1, 0,
	&wm8753_right_mixer_controls[0], ARRAY_SIZE(wm8753_right_mixer_controls)),
SND_SOC_DAPM_PGA("Right Out 1", WM8753_PWR3, 7, 0, NULL, 0),
SND_SOC_DAPM_PGA("Right Out 2", WM8753_PWR3, 5, 0, NULL, 0),
SND_SOC_DAPM_DAC("Right DAC", "Right HiFi Playback", WM8753_PWR1, 2, 0),
SND_SOC_DAPM_OUTPUT("ROUT1"),
SND_SOC_DAPM_OUTPUT("ROUT2"),
SND_SOC_DAPM_MIXER("Mono Mixer", WM8753_PWR4, 2, 0,
	&wm8753_mono_mixer_controls[0], ARRAY_SIZE(wm8753_mono_mixer_controls)),
SND_SOC_DAPM_PGA("Mono Out 1", WM8753_PWR3, 2, 0, NULL, 0),
SND_SOC_DAPM_PGA("Mono Out 2", WM8753_PWR3, 1, 0, NULL, 0),
SND_SOC_DAPM_DAC("Voice DAC", "Voice Playback", WM8753_PWR1, 4, 0),
SND_SOC_DAPM_OUTPUT("MONO1"),
SND_SOC_DAPM_MUX("Mono 2 Mux", SND_SOC_NOPM, 0, 0, &wm8753_mono2_controls),
SND_SOC_DAPM_OUTPUT("MONO2"),
SND_SOC_DAPM_MIXER("Out3 Left + Right", -1, 0, 0, NULL, 0),
SND_SOC_DAPM_MUX("Out3 Mux", SND_SOC_NOPM, 0, 0, &wm8753_out3_controls),
SND_SOC_DAPM_PGA("Out 3", WM8753_PWR3, 4, 0, NULL, 0),
SND_SOC_DAPM_OUTPUT("OUT3"),
SND_SOC_DAPM_MUX("Out4 Mux", SND_SOC_NOPM, 0, 0, &wm8753_out4_controls),
SND_SOC_DAPM_PGA("Out 4", WM8753_PWR3, 3, 0, NULL, 0),
SND_SOC_DAPM_OUTPUT("OUT4"),
SND_SOC_DAPM_MIXER("Playback Mixer", WM8753_PWR4, 3, 0,
	&wm8753_record_mixer_controls[0],
	ARRAY_SIZE(wm8753_record_mixer_controls)),
SND_SOC_DAPM_ADC("Left ADC", "Left Capture", WM8753_PWR2, 3, 0),
SND_SOC_DAPM_ADC("Right ADC", "Right Capture", WM8753_PWR2, 2, 0),
SND_SOC_DAPM_MUX("Capture Left Mixer", SND_SOC_NOPM, 0, 0,
	&wm8753_adc_mono_controls),
SND_SOC_DAPM_MUX("Capture Right Mixer", SND_SOC_NOPM, 0, 0,
	&wm8753_adc_mono_controls),
SND_SOC_DAPM_MUX("Capture Left Mux", SND_SOC_NOPM, 0, 0,
	&wm8753_adc_left_controls),
SND_SOC_DAPM_MUX("Capture Right Mux", SND_SOC_NOPM, 0, 0,
	&wm8753_adc_right_controls),
SND_SOC_DAPM_MUX("Mic Sidetone Mux", SND_SOC_NOPM, 0, 0,
	&wm8753_mic_mux_controls),
SND_SOC_DAPM_PGA("Left Capture Volume", WM8753_PWR2, 5, 0, NULL, 0),
SND_SOC_DAPM_PGA("Right Capture Volume", WM8753_PWR2, 4, 0, NULL, 0),
SND_SOC_DAPM_MIXER("ALC Mixer", WM8753_PWR2, 6, 0,
	&wm8753_alc_mixer_controls[0], ARRAY_SIZE(wm8753_alc_mixer_controls)),
SND_SOC_DAPM_MUX("Line Left Mux", SND_SOC_NOPM, 0, 0,
	&wm8753_line_left_controls),
SND_SOC_DAPM_MUX("Line Right Mux", SND_SOC_NOPM, 0, 0,
	&wm8753_line_right_controls),
SND_SOC_DAPM_MUX("Line Mono Mux", SND_SOC_NOPM, 0, 0,
	&wm8753_line_mono_controls),
SND_SOC_DAPM_MUX("Line Mixer", WM8753_PWR2, 0, 0,
	&wm8753_line_mux_mix_controls),
SND_SOC_DAPM_MUX("Rx Mixer", WM8753_PWR2, 1, 0,
	&wm8753_rx_mux_mix_controls),
SND_SOC_DAPM_PGA("Mic 1 Volume", WM8753_PWR2, 8, 0, NULL, 0),
SND_SOC_DAPM_PGA("Mic 2 Volume", WM8753_PWR2, 7, 0, NULL, 0),
SND_SOC_DAPM_MUX("Mic Selection Mux", SND_SOC_NOPM, 0, 0,
	&wm8753_mic_sel_mux_controls),
SND_SOC_DAPM_INPUT("LINE1"),
SND_SOC_DAPM_INPUT("LINE2"),
SND_SOC_DAPM_INPUT("RXP"),
SND_SOC_DAPM_INPUT("RXN"),
SND_SOC_DAPM_INPUT("ACIN"),
SND_SOC_DAPM_OUTPUT("ACOP"),
SND_SOC_DAPM_INPUT("MIC1N"),
SND_SOC_DAPM_INPUT("MIC1"),
SND_SOC_DAPM_INPUT("MIC2N"),
SND_SOC_DAPM_INPUT("MIC2"),
SND_SOC_DAPM_VMID("VREF"),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* left mixer */
	{"Left Mixer", "Left Playback Switch", "Left DAC"},
	{"Left Mixer", "Voice Playback Switch", "Voice DAC"},
	{"Left Mixer", "Sidetone Playback Switch", "Mic Sidetone Mux"},
	{"Left Mixer", "Bypass Playback Switch", "Line Left Mux"},

	/* right mixer */
	{"Right Mixer", "Right Playback Switch", "Right DAC"},
	{"Right Mixer", "Voice Playback Switch", "Voice DAC"},
	{"Right Mixer", "Sidetone Playback Switch", "Mic Sidetone Mux"},
	{"Right Mixer", "Bypass Playback Switch", "Line Right Mux"},

	/* mono mixer */
	{"Mono Mixer", "Voice Playback Switch", "Voice DAC"},
	{"Mono Mixer", "Left Playback Switch", "Left DAC"},
	{"Mono Mixer", "Right Playback Switch", "Right DAC"},
	{"Mono Mixer", "Sidetone Playback Switch", "Mic Sidetone Mux"},
	{"Mono Mixer", "Bypass Playback Switch", "Line Mono Mux"},

	/* left out */
	{"Left Out 1", NULL, "Left Mixer"},
	{"Left Out 2", NULL, "Left Mixer"},
	{"LOUT1", NULL, "Left Out 1"},
	{"LOUT2", NULL, "Left Out 2"},

	/* right out */
	{"Right Out 1", NULL, "Right Mixer"},
	{"Right Out 2", NULL, "Right Mixer"},
	{"ROUT1", NULL, "Right Out 1"},
	{"ROUT2", NULL, "Right Out 2"},

	/* mono 1 out */
	{"Mono Out 1", NULL, "Mono Mixer"},
	{"MONO1", NULL, "Mono Out 1"},

	/* mono 2 out */
	{"Mono 2 Mux", "Left + Right", "Out3 Left + Right"},
	{"Mono 2 Mux", "Inverted Mono 1", "MONO1"},
	{"Mono 2 Mux", "Left", "Left Mixer"},
	{"Mono 2 Mux", "Right", "Right Mixer"},
	{"Mono Out 2", NULL, "Mono 2 Mux"},
	{"MONO2", NULL, "Mono Out 2"},

	/* out 3 */
	{"Out3 Left + Right", NULL, "Left Mixer"},
	{"Out3 Left + Right", NULL, "Right Mixer"},
	{"Out3 Mux", "VREF", "VREF"},
	{"Out3 Mux", "Left + Right", "Out3 Left + Right"},
	{"Out3 Mux", "ROUT2", "ROUT2"},
	{"Out 3", NULL, "Out3 Mux"},
	{"OUT3", NULL, "Out 3"},

	/* out 4 */
	{"Out4 Mux", "VREF", "VREF"},
	{"Out4 Mux", "Capture ST", "Capture ST Mixer"},
	{"Out4 Mux", "LOUT2", "LOUT2"},
	{"Out 4", NULL, "Out4 Mux"},
	{"OUT4", NULL, "Out 4"},

	/* record mixer  */
	{"Playback Mixer", "Left Capture Switch", "Left Mixer"},
	{"Playback Mixer", "Voice Capture Switch", "Mono Mixer"},
	{"Playback Mixer", "Right Capture Switch", "Right Mixer"},

	/* Mic/SideTone Mux */
	{"Mic Sidetone Mux", "Left PGA", "Left Capture Volume"},
	{"Mic Sidetone Mux", "Right PGA", "Right Capture Volume"},
	{"Mic Sidetone Mux", "Mic 1", "Mic 1 Volume"},
	{"Mic Sidetone Mux", "Mic 2", "Mic 2 Volume"},

	/* Capture Left Mux */
	{"Capture Left Mux", "PGA", "Left Capture Volume"},
	{"Capture Left Mux", "Line or RXP-RXN", "Line Left Mux"},
	{"Capture Left Mux", "Line", "LINE1"},

	/* Capture Right Mux */
	{"Capture Right Mux", "PGA", "Right Capture Volume"},
	{"Capture Right Mux", "Line or RXP-RXN", "Line Right Mux"},
	{"Capture Right Mux", "Sidetone", "Capture ST Mixer"},

	/* Mono Capture mixer-mux */
	{"Capture Right Mixer", "Stereo", "Capture Right Mux"},
	{"Capture Left Mixer", "Analogue Mix Left", "Capture Left Mux"},
	{"Capture Left Mixer", "Analogue Mix Left", "Capture Right Mux"},
	{"Capture Right Mixer", "Analogue Mix Right", "Capture Left Mux"},
	{"Capture Right Mixer", "Analogue Mix Right", "Capture Right Mux"},
	{"Capture Left Mixer", "Digital Mono Mix", "Capture Left Mux"},
	{"Capture Left Mixer", "Digital Mono Mix", "Capture Right Mux"},
	{"Capture Right Mixer", "Digital Mono Mix", "Capture Left Mux"},
	{"Capture Right Mixer", "Digital Mono Mix", "Capture Right Mux"},

	/* ADC */
	{"Left ADC", NULL, "Capture Left Mixer"},
	{"Right ADC", NULL, "Capture Right Mixer"},

	/* Left Capture Volume */
	{"Left Capture Volume", NULL, "ACIN"},

	/* Right Capture Volume */
	{"Right Capture Volume", NULL, "Mic 2 Volume"},

	/* ALC Mixer */
	{"ALC Mixer", "Line Capture Switch", "Line Mixer"},
	{"ALC Mixer", "Mic2 Capture Switch", "Mic 2 Volume"},
	{"ALC Mixer", "Mic1 Capture Switch", "Mic 1 Volume"},
	{"ALC Mixer", "Rx Capture Switch", "Rx Mixer"},

	/* Line Left Mux */
	{"Line Left Mux", "Line 1", "LINE1"},
	{"Line Left Mux", "Rx Mix", "Rx Mixer"},

	/* Line Right Mux */
	{"Line Right Mux", "Line 2", "LINE2"},
	{"Line Right Mux", "Rx Mix", "Rx Mixer"},

	/* Line Mono Mux */
	{"Line Mono Mux", "Line Mix", "Line Mixer"},
	{"Line Mono Mux", "Rx Mix", "Rx Mixer"},

	/* Line Mixer/Mux */
	{"Line Mixer", "Line 1 + 2", "LINE1"},
	{"Line Mixer", "Line 1 - 2", "LINE1"},
	{"Line Mixer", "Line 1 + 2", "LINE2"},
	{"Line Mixer", "Line 1 - 2", "LINE2"},
	{"Line Mixer", "Line 1", "LINE1"},
	{"Line Mixer", "Line 2", "LINE2"},

	/* Rx Mixer/Mux */
	{"Rx Mixer", "RXP - RXN", "RXP"},
	{"Rx Mixer", "RXP + RXN", "RXP"},
	{"Rx Mixer", "RXP - RXN", "RXN"},
	{"Rx Mixer", "RXP + RXN", "RXN"},
	{"Rx Mixer", "RXP", "RXP"},
	{"Rx Mixer", "RXN", "RXN"},

	/* Mic 1 Volume */
	{"Mic 1 Volume", NULL, "MIC1N"},
	{"Mic 1 Volume", NULL, "Mic Selection Mux"},

	/* Mic 2 Volume */
	{"Mic 2 Volume", NULL, "MIC2N"},
	{"Mic 2 Volume", NULL, "MIC2"},

	/* Mic Selector Mux */
	{"Mic Selection Mux", "Mic 1", "MIC1"},
	{"Mic Selection Mux", "Mic 2", "MIC2N"},
	{"Mic Selection Mux", "Mic 3", "MIC2"},

	/* ACOP */
	{"ACOP", NULL, "ALC Mixer"},
};

static int wm8753_add_widgets(struct snd_soc_codec *codec,
	struct snd_soc_card *soc_card)
{
	int ret;

	ret = snd_soc_dapm_new_controls(soc_card, codec,
					wm8753_dapm_widgets,
					ARRAY_SIZE(wm8753_dapm_widgets));
	if (ret < 0)
		return ret;

	/* set up audio path audio_map */
	ret = snd_soc_dapm_add_routes(soc_card, audio_map,
				     ARRAY_SIZE(audio_map));
	if (ret < 0)
		return ret;

	return snd_soc_dapm_init(soc_card);
}

/* PLL divisors */
struct _pll_div {
	u32 div2:1;
	u32 n:4;
	u32 k:24;
};

/* The size in bits of the pll divide multiplied by 10
 * to allow rounding later */
#define FIXED_PLL_SIZE ((1 << 22) * 10)

static void pll_factors(struct _pll_div *pll_div, unsigned int target,
	unsigned int source)
{
	u64 Kpart;
	unsigned int K, Ndiv, Nmod;

	Ndiv = target / source;
	if (Ndiv < 6) {
		source >>= 1;
		pll_div->div2 = 1;
		Ndiv = target / source;
	} else
		pll_div->div2 = 0;

	if ((Ndiv < 6) || (Ndiv > 12))
		printk(KERN_WARNING
			"wm8753: unsupported N = %d\n", Ndiv);

	pll_div->n = Ndiv;
	Nmod = target % source;
	Kpart = FIXED_PLL_SIZE * (long long)Nmod;

	do_div(Kpart, source);

	K = Kpart & 0xFFFFFFFF;

	/* Check if we need to round */
	if ((K % 10) >= 5)
		K += 5;

	/* Move down to proper range now rounding is done */
	K /= 10;

	pll_div->k = K;
}

static int wm8753_set_dai_pll(struct snd_soc_dai *dai,
		int pll_id, unsigned int freq_in, unsigned int freq_out)
{
	u16 reg, enable;
	int offset;
	struct snd_soc_codec *codec = dai->codec;

	if (pll_id < WM8753_PLL1 || pll_id > WM8753_PLL2)
		return -ENODEV;

	if (pll_id == WM8753_PLL1) {
		offset = 0;
		enable = 0x10;
		reg = wm8753_read_reg_cache(codec, WM8753_CLOCK) & 0xffef;
	} else {
		offset = 4;
		enable = 0x8;
		reg = wm8753_read_reg_cache(codec, WM8753_CLOCK) & 0xfff7;
	}

	if (!freq_in || !freq_out) {
		/* disable PLL  */
		wm8753_write(codec, WM8753_PLL1CTL1 + offset, 0x0026);
		wm8753_write(codec, WM8753_CLOCK, reg);
		return 0;
	} else {
		u16 value = 0;
		struct _pll_div pll_div;

		pll_factors(&pll_div, freq_out * 8, freq_in);

		/* set up N and K PLL divisor ratios */
		/* bits 8:5 = PLL_N, bits 3:0 = PLL_K[21:18] */
		value = (pll_div.n << 5) + ((pll_div.k & 0x3c0000) >> 18);
		wm8753_write(codec, WM8753_PLL1CTL2 + offset, value);

		/* bits 8:0 = PLL_K[17:9] */
		value = (pll_div.k & 0x03fe00) >> 9;
		wm8753_write(codec, WM8753_PLL1CTL3 + offset, value);

		/* bits 8:0 = PLL_K[8:0] */
		value = pll_div.k & 0x0001ff;
		wm8753_write(codec, WM8753_PLL1CTL4 + offset, value);

		/* set PLL as input and enable */
		wm8753_write(codec, WM8753_PLL1CTL1 + offset, 0x0027 |
			(pll_div.div2 << 3));
		wm8753_write(codec, WM8753_CLOCK, reg | enable);
	}
	return 0;
}

struct _coeff_div {
	u32 mclk;
	u32 rate;
	u8 sr:5;
	u8 usb:1;
};

/* codec hifi mclk (after PLL) clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
	/* 8k */
	{12288000, 8000, 0x6, 0x0},
	{11289600, 8000, 0x16, 0x0},
	{18432000, 8000, 0x7, 0x0},
	{16934400, 8000, 0x17, 0x0},
	{12000000, 8000, 0x6, 0x1},

	/* 11.025k */
	{11289600, 11025, 0x18, 0x0},
	{16934400, 11025, 0x19, 0x0},
	{12000000, 11025, 0x19, 0x1},

	/* 16k */
	{12288000, 16000, 0xa, 0x0},
	{18432000, 16000, 0xb, 0x0},
	{12000000, 16000, 0xa, 0x1},

	/* 22.05k */
	{11289600, 22050, 0x1a, 0x0},
	{16934400, 22050, 0x1b, 0x0},
	{12000000, 22050, 0x1b, 0x1},

	/* 32k */
	{12288000, 32000, 0xc, 0x0},
	{18432000, 32000, 0xd, 0x0},
	{12000000, 32000, 0xa, 0x1},

	/* 44.1k */
	{11289600, 44100, 0x10, 0x0},
	{16934400, 44100, 0x11, 0x0},
	{12000000, 44100, 0x11, 0x1},

	/* 48k */
	{12288000, 48000, 0x0, 0x0},
	{18432000, 48000, 0x1, 0x0},
	{12000000, 48000, 0x0, 0x1},

	/* 88.2k */
	{11289600, 88200, 0x1e, 0x0},
	{16934400, 88200, 0x1f, 0x0},
	{12000000, 88200, 0x1f, 0x1},

	/* 96k */
	{12288000, 96000, 0xe, 0x0},
	{18432000, 96000, 0xf, 0x0},
	{12000000, 96000, 0xe, 0x1},
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

/*
 * Clock after PLL and dividers
 */
static int wm8753_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	struct wm8753_dai_priv *wm8753 = dai->private_data;

	switch (freq) {
	case 11289600:
	case 12000000:
	case 12288000:
	case 16934400:
	case 18432000:
		wm8753->clk = freq;
		return 0;
	}
	return -EINVAL;
}

/*
 * Set's ADC and Voice DAC format.
 */
static int wm8753_vdac_adc_set_dai_fmt(struct snd_soc_dai *dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 voice = wm8753_read_reg_cache(codec, WM8753_PCM) & 0x01ec;

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		voice |= 0x0002;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		voice |= 0x0001;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		voice |= 0x0003;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		voice |= 0x0013;
		break;
	default:
		return -EINVAL;
	}

	wm8753_write(codec, WM8753_PCM, voice);
	return 0;
}

/*
 * Set PCM DAI bit size and sample rate.
 */
static int wm8753_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *rdai)
{
	struct snd_soc_pcm_runtime *pcm_link = substream->private_data;
	struct snd_soc_codec *codec = pcm_link->codec;
	struct wm8753_dai_priv *wm8753 = rdai->private_data;
	u16 voice = wm8753_read_reg_cache(codec, WM8753_PCM) & 0x01f3;
	u16 srate = wm8753_read_reg_cache(codec, WM8753_SRATE1) & 0x017f;

	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		voice |= 0x0004;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		voice |= 0x0008;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		voice |= 0x000c;
		break;
	}

	/* sample rate */
	if (params_rate(params) * 384 == wm8753->clk)
		srate |= 0x80;
	wm8753_write(codec, WM8753_SRATE1, srate);

	wm8753_write(codec, WM8753_PCM, voice);
	return 0;
}

/*
 * Set's PCM dai fmt and BCLK.
 */
static int wm8753_pcm_set_dai_fmt(struct snd_soc_dai *dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 voice, ioctl;

	voice = wm8753_read_reg_cache(codec, WM8753_PCM) & 0x011f;
	ioctl = wm8753_read_reg_cache(codec, WM8753_IOCTL) & 0x015d;

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		ioctl |= 0x2;
	case SND_SOC_DAIFMT_CBM_CFS:
		voice |= 0x0040;
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
	case SND_SOC_DAIFMT_DSP_B:
		/* frame inversion not valid for DSP modes */
		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			break;
		case SND_SOC_DAIFMT_IB_NF:
			voice |= 0x0080;
			break;
		default:
			return -EINVAL;
		}
		break;
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_LEFT_J:
		voice &= ~0x0010;
		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			break;
		case SND_SOC_DAIFMT_IB_IF:
			voice |= 0x0090;
			break;
		case SND_SOC_DAIFMT_IB_NF:
			voice |= 0x0080;
			break;
		case SND_SOC_DAIFMT_NB_IF:
			voice |= 0x0010;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	wm8753_write(codec, WM8753_PCM, voice);
	wm8753_write(codec, WM8753_IOCTL, ioctl);
	return 0;
}

static int wm8753_set_dai_clkdiv(struct snd_soc_dai *dai,
		int div_id, int div)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 reg;

	switch (div_id) {
	case WM8753_PCMDIV:
		reg = wm8753_read_reg_cache(codec, WM8753_CLOCK) & 0x003f;
		wm8753_write(codec, WM8753_CLOCK, reg | div);
		break;
	case WM8753_BCLKDIV:
		reg = wm8753_read_reg_cache(codec, WM8753_SRATE2) & 0x01c7;
		wm8753_write(codec, WM8753_SRATE2, reg | div);
		break;
	case WM8753_VXCLKDIV:
		reg = wm8753_read_reg_cache(codec, WM8753_SRATE2) & 0x003f;
		wm8753_write(codec, WM8753_SRATE2, reg | div);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/*
 * Set's HiFi DAC format.
 */
static int wm8753_hdac_set_dai_fmt(struct snd_soc_dai *dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 hifi = wm8753_read_reg_cache(codec, WM8753_HIFI) & 0x01e0;

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		hifi |= 0x0002;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		hifi |= 0x0001;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		hifi |= 0x0003;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		hifi |= 0x0013;
		break;
	default:
		return -EINVAL;
	}

	wm8753_write(codec, WM8753_HIFI, hifi);
	return 0;
}

/*
 * Set's I2S DAI format.
 */
static int wm8753_i2s_set_dai_fmt(struct snd_soc_dai *dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 ioctl, hifi;

	hifi = wm8753_read_reg_cache(codec, WM8753_HIFI) & 0x011f;
	ioctl = wm8753_read_reg_cache(codec, WM8753_IOCTL) & 0x00ae;

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		ioctl |= 0x1;
	case SND_SOC_DAIFMT_CBM_CFS:
		hifi |= 0x0040;
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
	case SND_SOC_DAIFMT_DSP_B:
		/* frame inversion not valid for DSP modes */
		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			break;
		case SND_SOC_DAIFMT_IB_NF:
			hifi |= 0x0080;
			break;
		default:
			return -EINVAL;
		}
		break;
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_LEFT_J:
		hifi &= ~0x0010;
		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			break;
		case SND_SOC_DAIFMT_IB_IF:
			hifi |= 0x0090;
			break;
		case SND_SOC_DAIFMT_IB_NF:
			hifi |= 0x0080;
			break;
		case SND_SOC_DAIFMT_NB_IF:
			hifi |= 0x0010;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	wm8753_write(codec, WM8753_HIFI, hifi);
	wm8753_write(codec, WM8753_IOCTL, ioctl);
	return 0;
}

/*
 * Set PCM DAI bit size and sample rate.
 */
static int wm8753_i2s_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *rdai)
{
	struct snd_soc_pcm_runtime *pcm_link = substream->private_data;
	struct snd_soc_codec *codec = pcm_link->codec;
	struct wm8753_dai_priv *wm8753 = rdai->private_data;
	u16 srate = wm8753_read_reg_cache(codec, WM8753_SRATE1) & 0x01c0;
	u16 hifi = wm8753_read_reg_cache(codec, WM8753_HIFI) & 0x01f3;
	int coeff;

	/* is digital filter coefficient valid ? */
	coeff = get_coeff(wm8753->clk, params_rate(params));
	if (coeff < 0) {
		printk(KERN_ERR "wm8753 invalid MCLK or rate\n");
		return coeff;
	}
	wm8753_write(codec, WM8753_SRATE1, srate | (coeff_div[coeff].sr << 1) |
		coeff_div[coeff].usb);

	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		hifi |= 0x0004;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		hifi |= 0x0008;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		hifi |= 0x000c;
		break;
	}

	wm8753_write(codec, WM8753_HIFI, hifi);
	return 0;
}

static int wm8753_mode1v_set_dai_fmt(struct snd_soc_dai *dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 clock;

	/* set clk source as pcmclk */
	clock = wm8753_read_reg_cache(codec, WM8753_CLOCK) & 0xfffb;
	wm8753_write(codec, WM8753_CLOCK, clock);

	if (wm8753_vdac_adc_set_dai_fmt(dai, fmt) < 0)
		return -EINVAL;
	return wm8753_pcm_set_dai_fmt(dai, fmt);
}

static int wm8753_mode1h_set_dai_fmt(struct snd_soc_dai *dai,
		unsigned int fmt)
{
	if (wm8753_hdac_set_dai_fmt(dai, fmt) < 0)
		return -EINVAL;
	return wm8753_i2s_set_dai_fmt(dai, fmt);
}

static int wm8753_mode2_set_dai_fmt(struct snd_soc_dai *dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 clock;

	/* set clk source as pcmclk */
	clock = wm8753_read_reg_cache(codec, WM8753_CLOCK) & 0xfffb;
	wm8753_write(codec, WM8753_CLOCK, clock);

	if (wm8753_vdac_adc_set_dai_fmt(dai, fmt) < 0)
		return -EINVAL;
	return wm8753_i2s_set_dai_fmt(dai, fmt);
}

static int wm8753_mode3_4_set_dai_fmt(struct snd_soc_dai *dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 clock;

	/* set clk source as mclk */
	clock = wm8753_read_reg_cache(codec, WM8753_CLOCK) & 0xfffb;
	wm8753_write(codec, WM8753_CLOCK, clock | 0x4);

	if (wm8753_hdac_set_dai_fmt(dai, fmt) < 0)
		return -EINVAL;
	if (wm8753_vdac_adc_set_dai_fmt(dai, fmt) < 0)
		return -EINVAL;
	return wm8753_i2s_set_dai_fmt(dai, fmt);
}

static int wm8753_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct snd_soc_dai *codec_dai;
	int active = 0;
	u16 mute_reg = wm8753_read_reg_cache(codec, WM8753_DAC) & 0xfff7;

	/* the digital mute covers the HiFi and Voice DAC's on the WM8753.
	 * make sure we check if they are not both active when we mute */
	if (mute) {
		list_for_each_entry(codec_dai, &codec->dai_list, list) {
			if (codec_dai->active)
				active++;
		}
		if (active == 0)
			wm8753_write(codec, WM8753_DAC, mute_reg | 0x8);
	} else
		wm8753_write(codec, WM8753_DAC, mute_reg);

	return 0;
}

static int wm8753_set_bias_level(struct snd_soc_codec *codec,
	enum snd_soc_dapm_bias_level level)
{
	u16 pwr_reg = wm8753_read_reg_cache(codec, WM8753_PWR1) & 0xfe3e;

	switch (level) {
	case SND_SOC_BIAS_ON: /* full On */
		/* set vmid to 50k and unmute dac */
		wm8753_write(codec, WM8753_PWR1, pwr_reg | 0x00c0);
		break;
	case SND_SOC_BIAS_PREPARE: /* partial On */
		/* set vmid to 5k for quick power up */
		wm8753_write(codec, WM8753_PWR1, pwr_reg | 0x01c1);
		break;
	case SND_SOC_BIAS_STANDBY: /* Off, with power */
		/* mute dac and set vmid to 500k, enable VREF */
		wm8753_write(codec, WM8753_PWR1, pwr_reg | 0x0141);
		break;
	case SND_SOC_BIAS_OFF: /* Off, without power */
		wm8753_write(codec, WM8753_PWR1, 0x0001);
		break;
	}
	codec->bias_level = level;
	return 0;
}

static void wm8753_work(struct work_struct *work)
{
	struct snd_soc_codec *codec =
		container_of(work, struct snd_soc_codec, delayed_work.work);
	wm8753_set_bias_level(codec, codec->bias_level);
}

static int wm8753_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_codec *codec = platform_get_drvdata(pdev);

	wm8753_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int wm8753_resume(struct platform_device *pdev)
{
	struct snd_soc_codec *codec = platform_get_drvdata(pdev);
	int i;
	u8 data[2];
	u16 *cache = codec->reg_cache;

	/* Sync reg_cache with the hardware */
	for (i = 0; i < ARRAY_SIZE(wm8753_reg); i++) {
		if (i + 1 == WM8753_RESET)
			continue;
		data[0] = ((i + 1) << 1) | ((cache[i] >> 8) & 0x0001);
		data[1] = cache[i] & 0x00ff;
		codec->soc_phys_write(codec->control_data, (long)data, 2);
	}

	wm8753_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/* charge wm8753 caps */
	if (codec->suspend_bias_level == SND_SOC_BIAS_ON) {
		wm8753_set_bias_level(codec, SND_SOC_BIAS_PREPARE);
		codec->bias_level = SND_SOC_BIAS_ON;
		schedule_delayed_work(&codec->delayed_work,
			msecs_to_jiffies(caps_charge));
	}

	return 0;
}

static int wm8753_codec_init(struct snd_soc_codec *codec,
	struct snd_soc_card *soc_card)
{
	int reg;

	wm8753_reset(codec);

	/* charge output caps */
	wm8753_set_bias_level(codec, SND_SOC_BIAS_PREPARE);
	codec->bias_level = SND_SOC_BIAS_STANDBY;
	schedule_delayed_work(&codec->delayed_work,
		msecs_to_jiffies(caps_charge));

	/* set the update bits */
	reg = wm8753_read_reg_cache(codec, WM8753_LDAC);
	wm8753_write(codec, WM8753_LDAC, reg | 0x0100);
	reg = wm8753_read_reg_cache(codec, WM8753_RDAC);
	wm8753_write(codec, WM8753_RDAC, reg | 0x0100);
	reg = wm8753_read_reg_cache(codec, WM8753_LADC);
	wm8753_write(codec, WM8753_LADC, reg | 0x0100);
	reg = wm8753_read_reg_cache(codec, WM8753_RADC);
	wm8753_write(codec, WM8753_RADC, reg | 0x0100);
	reg = wm8753_read_reg_cache(codec, WM8753_LOUT1V);
	wm8753_write(codec, WM8753_LOUT1V, reg | 0x0100);
	reg = wm8753_read_reg_cache(codec, WM8753_ROUT1V);
	wm8753_write(codec, WM8753_ROUT1V, reg | 0x0100);
	reg = wm8753_read_reg_cache(codec, WM8753_LOUT2V);
	wm8753_write(codec, WM8753_LOUT2V, reg | 0x0100);
	reg = wm8753_read_reg_cache(codec, WM8753_ROUT2V);
	wm8753_write(codec, WM8753_ROUT2V, reg | 0x0100);
	reg = wm8753_read_reg_cache(codec, WM8753_LINVOL);
	wm8753_write(codec, WM8753_LINVOL, reg | 0x0100);
	reg = wm8753_read_reg_cache(codec, WM8753_RINVOL);
	wm8753_write(codec, WM8753_RINVOL, reg | 0x0100);

	snd_soc_add_new_controls(soc_card, wm8753_snd_controls, codec,
		ARRAY_SIZE(wm8753_snd_controls));
	wm8753_add_widgets(codec, soc_card);
	return 0;
}

/*
 * This function forces any delayed work to be queued and run.
 */
static int run_delayed_work(struct delayed_work *dwork)
{
	int ret;

	/* cancel any work waiting to be queued. */
	ret = cancel_delayed_work(dwork);

	/* if there was any work waiting then we run it now and
	 * wait for it's completion */
	if (ret) {
		schedule_delayed_work(dwork, 0);
		flush_scheduled_work();
	}
	return ret;
}

static void wm8753_codec_exit(struct snd_soc_codec *codec,
	struct snd_soc_card *soc_card)
{
	run_delayed_work(&codec->delayed_work);
	wm8753_set_bias_level(codec, SND_SOC_BIAS_OFF);
}

#define WM8753_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 | \
		SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000)

#define WM8753_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
	SNDRV_PCM_FMTBIT_S24_LE)

/*
 * The WM8753 supports upto 4 different and mutually exclusive DAI
 * configurations. This gives 2 PCM's available for use, hifi and voice.
 * NOTE: The Voice PCM cannot play or capture audio to the CPU as it's DAI
 * is connected between the wm8753 and a BT codec or GSM modem.
 *
 * 1. Voice over PCM DAI - HIFI DAC over HIFI DAI
 * 2. Voice over HIFI DAI - HIFI disabled
 * 3. Voice disabled - HIFI over HIFI
 * 4. Voice disabled - HIFI over HIFI, uses voice DAI LRC for capture
 */

static int wm8753_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct wm8753_codec_priv *wm8753 = dai->codec->private_data;

	switch(wm8753->codec_dai_mode) {
	case 0:
		return wm8753_mode1h_set_dai_fmt(dai, fmt);
	case 1:
		return wm8753_mode1v_set_dai_fmt(dai, fmt);
	case 2:
		return wm8753_mode2_set_dai_fmt(dai, fmt);
	case 3:
		return wm8753_mode3_4_set_dai_fmt(dai, fmt);
	default:
		return -EINVAL;
	}
}

static struct snd_soc_dai_caps wm8753_hifi_playback = {
	.stream_name	= "HiFi Playback",
	.channels_min	= 1,
	.channels_max	= 2,
	.rates		= WM8753_RATES,
	.formats	= WM8753_FORMATS,
};

static struct snd_soc_dai_caps wm8753_voice_playback = {
	.stream_name	= "Voice Playback",
	.channels_min	= 1,
	.channels_max	= 1,
	.rates		= WM8753_RATES,
	.formats	= WM8753_FORMATS,
};

static struct snd_soc_dai_caps wm8753_capture = {
	.stream_name	= "Capture",
	.channels_min	= 1,
	.channels_max	= 2,
	.rates		= WM8753_RATES,
	.formats	= WM8753_FORMATS,
};

static struct snd_soc_dai_ops wm8753_hifi_dai_ops = {
	/* alsa ops */
	.hw_params	= wm8753_i2s_hw_params,
	/* dai ops */
	.digital_mute	= wm8753_mute,
	.set_fmt	= wm8753_set_dai_fmt,
	.set_clkdiv	= wm8753_set_dai_clkdiv,
	.set_pll	= wm8753_set_dai_pll,
	.set_sysclk	= wm8753_set_dai_sysclk,
};

static struct snd_soc_dai_ops wm8753_voice_dai_ops = {
	/* alsa ops */
	.hw_params	= wm8753_pcm_hw_params,
	/* dai ops */
	.digital_mute	= wm8753_mute,
	.set_fmt	= wm8753_set_dai_fmt,
	.set_clkdiv	= wm8753_set_dai_clkdiv,
	.set_pll	= wm8753_set_dai_pll,
	.set_sysclk	= wm8753_set_dai_sysclk,
};


/* for modprobe */
const char wm8753_codec_id[] = "wm8753-codec";
EXPORT_SYMBOL_GPL(wm8753_codec_id);

const char wm8753_codec_hifi_dai_id[] = "wm8753-codec-hifi-dai";
EXPORT_SYMBOL_GPL(wm8753_codec_hifi_dai_id);

const char wm8753_codec_voice_dai_id[] = "wm8753-codec-voice-dai";
EXPORT_SYMBOL_GPL(wm8753_codec_voice_dai_id);

struct snd_soc_dai_new wm8753_hifi_dai = {
	.name		= wm8753_codec_hifi_dai_id,
	.playback	= &wm8753_hifi_playback,
	.capture	= &wm8753_capture,
	.ops		= &wm8753_hifi_dai_ops,
};

struct snd_soc_dai_new wm8753_voice_dai = {
	.name		= wm8753_codec_voice_dai_id,
	.playback	= &wm8753_voice_playback,
	.capture	= &wm8753_capture,
	.ops		= &wm8753_voice_dai_ops,
};

static struct snd_soc_dai *wm8753_voice_dai_probe(
	struct wm8753_codec_priv *wm8753, struct device *dev)
{
	struct wm8753_dai_priv *dai_priv;
	struct snd_soc_dai *voice_dai;

	dai_priv = kzalloc(sizeof(struct wm8753_dai_priv), GFP_KERNEL);
	if (dai_priv == NULL)
		return NULL;

	voice_dai = snd_soc_register_codec_dai(&wm8753_hifi_dai, dev);
	if (voice_dai == NULL) {
		kfree(dai_priv);
		return NULL;
	}
	voice_dai->private_data = dai_priv;
	return voice_dai;
}

static struct snd_soc_dai *wm8753_hifi_dai_probe(
	struct wm8753_codec_priv *wm8753, struct device *dev)
{
	struct wm8753_dai_priv *dai_priv;
	struct snd_soc_dai *hifi_dai;

	dai_priv = kzalloc(sizeof(struct wm8753_dai_priv), GFP_KERNEL);
	if (dai_priv == NULL)
		return NULL;

	hifi_dai = snd_soc_register_codec_dai(&wm8753_voice_dai, dev);
	if (hifi_dai == NULL) {
		kfree(dai_priv);
		return NULL;
	}
	hifi_dai->private_data = dai_priv;
	return hifi_dai;
}

static struct snd_soc_codec_new wm8753_codec = {
	.name		= wm8753_codec_id,
	.reg_cache_size = sizeof(wm8753_reg),
	.reg_cache_step = 1,
	.set_bias_level	= wm8753_set_bias_level,
	.init		= wm8753_codec_init,
	.exit		= wm8753_codec_exit,
	.codec_read	= wm8753_read_reg_cache,
	.codec_write	= wm8753_write,
};

static int wm8753_codec_probe(struct platform_device *pdev)
{
	struct snd_soc_codec *codec;
	struct wm8753_codec_priv *wm8753;
	int ret;

	info("WM8753 Audio Codec %s", WM8753_VERSION);

	codec = snd_soc_new_codec(&wm8753_codec, (char *) wm8753_reg);
	if (codec == NULL)
		return -ENOMEM;

	wm8753 = kzalloc(sizeof(struct wm8753_codec_priv), GFP_KERNEL);
	if (wm8753 == NULL) {
		ret = -ENOMEM;
		goto err;
	}
	codec->private_data = wm8753;
	INIT_DELAYED_WORK(&codec->delayed_work, wm8753_work);

	ret = snd_soc_register_codec(codec, &pdev->dev);
	if (ret < 0)
		goto codec_err;
	wm8753->hifi_dai = wm8753_hifi_dai_probe(wm8753, &pdev->dev);
	if (wm8753->hifi_dai == NULL)
		goto codec_err;
	wm8753->voice_dai = wm8753_voice_dai_probe(wm8753, &pdev->dev);
	if (wm8753->voice_dai == NULL)
		goto voice_err;

	platform_set_drvdata(pdev, codec);
	return ret;
voice_err:
	kfree(wm8753->hifi_dai->private_data);
	snd_soc_unregister_codec_dai(wm8753->hifi_dai);
codec_err:
	kfree(codec->private_data);
err:
	snd_soc_unregister_codec(codec);
	kfree(codec->reg_cache);
	kfree(codec);
	return ret;
}

static int wm8753_codec_remove(struct platform_device *pdev)
{
	struct snd_soc_codec *codec = platform_get_drvdata(pdev);
	struct wm8753_codec_priv *wm8753 = codec->private_data;

	/* free hifi */
	kfree(wm8753->hifi_dai->private_data);
	snd_soc_unregister_codec_dai(wm8753->hifi_dai);

	/* free voice */
	kfree(wm8753->voice_dai->private_data);
	snd_soc_unregister_codec_dai(wm8753->voice_dai);

	/* free codec */
	kfree(codec->private_data);
	snd_soc_unregister_codec(codec);
	kfree(codec->reg_cache);
	kfree(codec);
	return 0;
}

static struct platform_driver wm8753_codec_driver = {
	.driver = {
		.name		= wm8753_codec_id,
		.owner		= THIS_MODULE,
	},
	.probe		= wm8753_codec_probe,
	.remove		= __devexit_p(wm8753_codec_remove),
	.suspend	= wm8753_suspend,
	.resume		= wm8753_resume,
};

static __init int wm8753_init(void)
{
	return platform_driver_register(&wm8753_codec_driver);
}

static __exit void wm8753_exit(void)
{
	platform_driver_unregister(&wm8753_codec_driver);
}

module_init(wm8753_init);
module_exit(wm8753_exit);

MODULE_DESCRIPTION("ASoC WM8753 driver");
MODULE_AUTHOR("Liam Girdwood");
MODULE_LICENSE("GPL");

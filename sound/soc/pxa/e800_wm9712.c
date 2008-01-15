/*
 * e800-wm9712.c  --  SoC audio for e800
 *
 * Based on tosa.c
 *
 * Copyright 2007 (c) Ian Molton <spyro@f2s.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation; version 2 ONLY.
 *
 *  Revision history
 *    14th Oct 2007   Initial version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/mach-types.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/hardware.h>
#include <asm/arch/audio.h>

#include "../codecs/wm9712.h"
#include "pxa2xx-pcm.h"
#include "pxa2xx-ac97.h"

static struct snd_soc_machine e800;

#define E800_HP        0
#define E800_MIC_INT   1
#define E800_HEADSET   2
#define E800_HP_OFF    3
#define E800_SPK_ON    0
#define E800_SPK_OFF   1

static int e800_jack_func;
static int e800_spk_func;

static void e800_ext_control(struct snd_soc_codec *codec) {
	snd_soc_dapm_set_endpoint(codec, "Speaker", 1);
        snd_soc_dapm_set_endpoint(codec, "Mic (Internal)", 0);
        snd_soc_dapm_set_endpoint(codec, "Headphone Jack", 0);
        snd_soc_dapm_set_endpoint(codec, "Headset Jack", 0);
        snd_soc_dapm_sync_endpoints(codec);
};

static int e800_startup(struct snd_pcm_substream *substream) {
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->socdev->codec;

	/* check the jack status at stream startup */
	e800_ext_control(codec);
	return 0;
}

static struct snd_soc_ops e800_ops = {
        .startup = e800_startup,
};


/* e800 machine DAPM widgets */
static const struct snd_soc_dapm_widget e800_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	//SND_SOC_DAPM_HP("Headset Jack", NULL),
	SND_SOC_DAPM_MIC("Mic (Internal)", NULL),
	SND_SOC_DAPM_SPK("Speaker", NULL),
};


/* e800 audio map */
static const char *audio_map[][3] = {
	/* headphone connected to HPOUTL, HPOUTR */
	{"Headphone Jack", NULL, "HPOUTL"},
	{"Headphone Jack", NULL, "HPOUTR"},

	/* ext speaker connected to LOUT2, ROUT2 */
	{"Speaker", NULL, "LOUT2"},
	{"Speaker", NULL, "ROUT2"},

	/* internal mic is connected to mic1, mic2 differential - with bias */
	{"MIC1", NULL, "Mic Bias"},
	{"MIC2", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Mic (Internal)"},

	{NULL, NULL, NULL},
};

static int e800_ac97_init(struct snd_soc_codec *codec) {
	int i, err;

	snd_soc_dapm_set_endpoint(codec, "OUT3", 0);
	snd_soc_dapm_set_endpoint(codec, "MONOOUT", 0);

	/* add e800 specific widgets */
	for (i = 0; i < ARRAY_SIZE(e800_dapm_widgets); i++) {
		snd_soc_dapm_new_control(codec, &e800_dapm_widgets[i]);
	}

	/* set up e800 specific audio path audio_map */
	for (i = 0; audio_map[i][0] != NULL; i++) {
		snd_soc_dapm_connect_input(codec, audio_map[i][0],
		                           audio_map[i][1], audio_map[i][2]);
	}

	e800_ext_control(codec);

	snd_soc_dapm_sync_endpoints(codec);
	return 0;
}

static struct snd_soc_dai_link e800_dai[] = {
{
        .name = "AC97",
        .stream_name = "AC97 HiFi",
        .cpu_dai = &pxa_ac97_dai[PXA2XX_DAI_AC97_HIFI],
        .codec_dai = &wm9712_dai[WM9712_DAI_AC97_HIFI],
        .init = &e800_ac97_init,
//        .ops = &e800_ops,
},
{
        .name = "AC97 Aux",
        .stream_name = "AC97 Aux",
        .cpu_dai = &pxa_ac97_dai[PXA2XX_DAI_AC97_AUX],
        .codec_dai = &wm9712_dai[WM9712_DAI_AC97_AUX],
//        .ops = &e800_ops,
},
};

static struct snd_soc_machine e800 = {
	.name = "Toshiba e800",
	.dai_link = e800_dai,
	.num_links = ARRAY_SIZE(e800_dai),
//	.ops = &e800_ops,
};

static struct snd_soc_device e800_snd_devdata = {
	.machine = &e800,
	.platform = &pxa2xx_soc_platform,
	.codec_dev = &soc_codec_dev_wm9712,
};

static struct platform_device *e800_snd_device;

static int __init e800_init(void)
{
	int ret;

	if (!machine_is_e800())
		return -ENODEV;

	e800_snd_device = platform_device_alloc("soc-audio", -1);
	if (!e800_snd_device)
		return -ENOMEM;

	platform_set_drvdata(e800_snd_device, &e800_snd_devdata);
	e800_snd_devdata.dev = &e800_snd_device->dev;
	ret = platform_device_add(e800_snd_device);

	if (ret)
		platform_device_put(e800_snd_device);

	return ret;
}

static void __exit e800_exit(void)
{
	platform_device_unregister(e800_snd_device);
}

module_init(e800_init);
module_exit(e800_exit);

/* Module information */
MODULE_AUTHOR("Ian Molton <spyro@f2s.com>");
MODULE_DESCRIPTION("ALSA SoC driver for e800");
MODULE_LICENSE("GPL");

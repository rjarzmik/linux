/*
 * e740-wm9705.c  --  SoC audio for e740
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

#include "../codecs/wm9705.h"
#include "pxa2xx-pcm.h"
#include "pxa2xx-ac97.h"

static struct snd_soc_machine e740;

#define E740_HP        0
#define E740_MIC_INT   1
#define E740_HEADSET   2
#define E740_HP_OFF    3
#define E740_SPK_ON    0
#define E740_SPK_OFF   1

static int e740_jack_func;
static int e740_spk_func;

static void e740_ext_control(struct snd_soc_codec *codec) {
	snd_soc_dapm_set_endpoint(codec, "Speaker", 1);
        snd_soc_dapm_set_endpoint(codec, "Mic (Internal)", 0);
        snd_soc_dapm_set_endpoint(codec, "Headphone Jack", 0);
        snd_soc_dapm_set_endpoint(codec, "Headset Jack", 0);
        snd_soc_dapm_sync_endpoints(codec);
};

static int e740_startup(struct snd_pcm_substream *substream) {
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->socdev->codec;

	/* check the jack status at stream startup */
	e740_ext_control(codec);
	return 0;
}

static struct snd_soc_ops e740_ops = {
        .startup = e740_startup,
};


/* e740 machine DAPM widgets */
static const struct snd_soc_dapm_widget e740_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	//SND_SOC_DAPM_HP("Headset Jack", NULL),
	SND_SOC_DAPM_MIC("Mic (Internal)", NULL),
	SND_SOC_DAPM_SPK("Speaker", NULL),
};


/* e740 audio map */
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

static int e740_ac97_init(struct snd_soc_codec *codec) {
	int i, err;

	snd_soc_dapm_set_endpoint(codec, "OUT3", 0);
	snd_soc_dapm_set_endpoint(codec, "MONOOUT", 0);

	/* add e740 specific widgets */
	for (i = 0; i < ARRAY_SIZE(e740_dapm_widgets); i++) {
		snd_soc_dapm_new_control(codec, &e740_dapm_widgets[i]);
	}

	/* set up e740 specific audio path audio_map */
	for (i = 0; audio_map[i][0] != NULL; i++) {
		snd_soc_dapm_connect_input(codec, audio_map[i][0],
		                           audio_map[i][1], audio_map[i][2]);
	}

	e740_ext_control(codec);

	snd_soc_dapm_sync_endpoints(codec);
	return 0;
}

static struct snd_soc_dai_link e740_dai[] = {
#if 0
{
        .name = "AC97",
        .stream_name = "AC97 HiFi",
        .cpu_dai = &pxa_ac97_dai[PXA2XX_DAI_AC97_HIFI],
        .codec_dai = &wm9705_dai[WM9705_DAI_AC97_HIFI],
        .init = &e740_ac97_init,
//        .ops = &e740_ops,
},
#endif
{
        .name = "AC97 Aux",
        .stream_name = "AC97 Aux",
        .cpu_dai = &pxa_ac97_dai[PXA2XX_DAI_AC97_AUX],
        .codec_dai = &wm9705_dai[WM9705_DAI_AC97_AUX],
//        .ops = &e740_ops,
},
};

static struct snd_soc_machine e740 = {
	.name = "Toshiba e740",
	.dai_link = e740_dai,
	.num_links = ARRAY_SIZE(e740_dai),
//	.ops = &e740_ops,
};

static struct snd_soc_device e740_snd_devdata = {
	.machine = &e740,
	.platform = &pxa2xx_soc_platform,
	.codec_dev = &soc_codec_dev_wm9705,
};

static struct platform_device *e740_snd_device;

static int __init e740_init(void)
{
	int ret;

	if (!machine_is_e740())
		return -ENODEV;

	e740_snd_device = platform_device_alloc("soc-audio", -1);
	if (!e740_snd_device)
		return -ENOMEM;

	platform_set_drvdata(e740_snd_device, &e740_snd_devdata);
	e740_snd_devdata.dev = &e740_snd_device->dev;
	ret = platform_device_add(e740_snd_device);

	if (ret)
		platform_device_put(e740_snd_device);

	return ret;
}

static void __exit e740_exit(void)
{
	platform_device_unregister(e740_snd_device);
}

module_init(e740_init);
module_exit(e740_exit);

/* Module information */
MODULE_AUTHOR("Ian Molton <spyro@f2s.com>");
MODULE_DESCRIPTION("ALSA SoC driver for e740");
MODULE_LICENSE("GPL");

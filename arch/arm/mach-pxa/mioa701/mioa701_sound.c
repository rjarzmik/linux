/*
 * mioa701_sound.c
 *
 * Copyright (c) 2007 Damien Tournoud
 *
 * Based on mainstone_baseband.c :
 *   Copyright 2006 Wolfson Microelectronics PLC.
 *   Author: Liam Girdwood
 *         liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * Big TODO: very preliminary, there are only stub functions in there
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/audio.h>
#include <asm/arch/ssp.h>

#include "../../../../sound/soc/codecs/wm9713.h"
#include "../../../../sound/soc/pxa/pxa2xx-pcm.h"
#include "../../../../sound/soc/pxa/pxa2xx-ac97.h"
#include "../../../../sound/soc/pxa/pxa2xx-ssp.h"

static struct snd_soc_machine mioa701;

/* Do specific baseband PCM voice startup here */
static int baseband_startup(struct snd_pcm_substream *substream)
{
	return 0;
}

/* Do specific baseband PCM voice shutdown here */
static void baseband_shutdown (struct snd_pcm_substream *substream)
{
}

/* Do specific baseband modem PCM voice hw params init here */
static int baseband_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	return 0;
}

/* Do specific baseband modem PCM voice hw params free here */
static int baseband_hw_free(struct snd_pcm_substream *substream)
{
	return 0;
}

/*
 * Baseband Processor DAI
 */
static struct snd_soc_cpu_dai baseband_dai =
{	.name = "Baseband",
	.id = 0,
	.type = SND_SOC_DAI_PCM,
	.playback = {
		.channels_min = 1,
		.channels_max = 1,
		.rates = SNDRV_PCM_RATE_8000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.capture = {
		.channels_min = 1,
		.channels_max = 1,
		.rates = SNDRV_PCM_RATE_8000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.ops = {
		.startup = baseband_startup,
		.shutdown = baseband_shutdown,
		.hw_params = baseband_hw_params,
		.hw_free = baseband_hw_free,
		},
};

/* PM */
static int mioa701_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int mioa701_resume(struct platform_device *pdev)
{
	return 0;
}

static int mioa701_probe(struct platform_device *pdev)
{
	return 0;
}

static int mioa701_remove(struct platform_device *pdev)
{
	return 0;
}

static int mioa701_wm9713_init(struct snd_soc_codec *codec)
{
	return 0;
}

/* the physical audio connections between the WM9713, Baseband and pxa2xx */
static struct snd_soc_dai_link mioa701_dai[] = {
{
	.name = "AC97",
	.stream_name = "AC97 HiFi",
	.cpu_dai = &pxa_ac97_dai[PXA2XX_DAI_AC97_HIFI],
	.codec_dai = &wm9713_dai[WM9713_DAI_AC97_HIFI],
	.init = mioa701_wm9713_init,
},
{
	.name = "AC97 Aux",
	.stream_name = "AC97 Aux",
	.cpu_dai = &pxa_ac97_dai[PXA2XX_DAI_AC97_AUX],
	.codec_dai = &wm9713_dai[WM9713_DAI_AC97_AUX],
},
{
	.name = "Baseband",
	.stream_name = "Voice",
	.cpu_dai = &baseband_dai,
	.codec_dai = &wm9713_dai[WM9713_DAI_PCM_VOICE],
},
};

static struct platform_device mioa701_ac97 = {
	.name 	= "pxa2xx-ac97",
	.id   	= -1,
};

static struct snd_soc_machine mioa701 = {
	.name = "mioa701",
	.probe = mioa701_probe,
	.remove = mioa701_remove,
	.suspend_pre = mioa701_suspend,
	.resume_post = mioa701_resume,
	.dai_link = mioa701_dai,
	.num_links = ARRAY_SIZE(mioa701_dai),
};

static struct snd_soc_device mioa701_snd_ac97_devdata = {
	.machine = &mioa701,
	.platform = &pxa2xx_soc_platform,
	.codec_dev = &soc_codec_dev_wm9713,
};

static struct platform_device *mioa701_snd_ac97_device;

static int __init mioa701_init(void)
{
	int ret;

	/* this initialize the PXA AC97 controller */
	platform_device_register(&mioa701_ac97);

	/* ... and this allocates the ASoC part */
	mioa701_snd_ac97_device = platform_device_alloc("soc-audio", -1);
	if (!mioa701_snd_ac97_device)
		return -ENOMEM;

	platform_set_drvdata(mioa701_snd_ac97_device, &mioa701_snd_ac97_devdata);
	mioa701_snd_ac97_devdata.dev = &mioa701_snd_ac97_device->dev;

	if((ret = platform_device_add(mioa701_snd_ac97_device)) != 0)
		platform_device_put(mioa701_snd_ac97_device);

	return ret;
}

static void __exit mioa701_exit(void)
{
	platform_device_unregister(mioa701_snd_ac97_device);
}

module_init(mioa701_init);
module_exit(mioa701_exit);

/* Module information */
MODULE_AUTHOR("Damien Tournoud <damz@prealable.org>");
MODULE_DESCRIPTION("mioa701 sound implementation");
MODULE_LICENSE("GPL");

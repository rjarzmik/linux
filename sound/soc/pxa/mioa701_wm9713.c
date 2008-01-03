/*
 * mioa701_wm9713.c
 *
 * Robert Jarzmik
 *   inspired by mainstone_wm9713.c by Liam Girdwood
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/audio.h>

#include "../codecs/wm9713.h"
#include "pxa2xx-pcm.h"
#include "pxa2xx-ac97.h"

#define GPIO22_SSP2SYSCLK_MD	(22 | GPIO_ALT_FN_2_OUT)

static struct snd_soc_machine mioa701;

static int mioa701_voice_startup(struct snd_pcm_substream *substream)
{
	/* enable USB on the go MUX so we can use SSPFRM2 */
	//MST_MSCWR2 |= MST_MSCWR2_USB_OTG_SEL;
	//MST_MSCWR2 &= ~MST_MSCWR2_USB_OTG_RST;
	return 0;
}

static void mioa701_voice_shutdown(struct snd_pcm_substream *substream)
{
	/* disable USB on the go MUX so we can use ttyS0 */
	//MST_MSCWR2 &= ~MST_MSCWR2_USB_OTG_SEL;
	//MST_MSCWR2 |= MST_MSCWR2_USB_OTG_RST;
}

static int mioa701_voice_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	unsigned int bclk = 0, pcmdiv = 0;
	int ret = 0;

	switch (params_rate(params)) {
	case 8000:
		pcmdiv = WM9713_PCMDIV(12); /* 2.048 MHz */
		bclk = WM9713_PCMBCLK_DIV_16; /* 128kHz */
		break;
	case 16000:
		pcmdiv = WM9713_PCMDIV(6); /* 4.096 MHz */
		bclk = WM9713_PCMBCLK_DIV_16; /* 256kHz */
		break;
	case 48000:
		pcmdiv = WM9713_PCMDIV(2); /* 12.288 MHz */
		bclk = WM9713_PCMBCLK_DIV_16; /* 512kHz */
		break;
	}

	/* set codec DAI configuration */
	ret = codec_dai->dai_ops.set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_A |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret = cpu_dai->dai_ops.set_fmt(cpu_dai, SND_SOC_DAIFMT_DSP_A |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* set codec BCLK division for sample rate */
	ret = codec_dai->dai_ops.set_clkdiv(codec_dai, WM9713_PCMBCLK_DIV, bclk);
	if (ret < 0)
		return ret;

	/* set codec PCM division for sample rate */
	ret = codec_dai->dai_ops.set_clkdiv(codec_dai, WM9713_PCMCLK_DIV, pcmdiv);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops mioa701_voice_ops = {
	.startup = mioa701_voice_startup,
	.shutdown = mioa701_voice_shutdown,
	.hw_params = mioa701_voice_hw_params,
};

static int test = 0;
static int get_test(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = test;
	return 0;
}

static int set_test(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	test = ucontrol->value.integer.value[0];
	if(test) {

	} else {

	}
	return 0;
}

static long mst_audio_suspend_mask;

static int mioa701_spkfront_power(struct snd_soc_dapm_widget *w, int event)
{
	struct snd_soc_codec *codec = w->codec;
	unsigned short val;

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		val = codec->read(codec, AC97_EXTENDED_MID);
		codec->write(codec, AC97_POWERDOWN, 0x0000);
	}
		//else
		//asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_SPK_PWR_ON, 0);
	return 0;
}


static int mioa701_suspend(struct platform_device *pdev, pm_message_t state)
{
	//mst_audio_suspend_mask = MST_MSCWR2;
	//MST_MSCWR2 |= MST_MSCWR2_AC97_SPKROFF;
	return 0;
}

static int mioa701_resume(struct platform_device *pdev)
{
	//MST_MSCWR2 &= mst_audio_suspend_mask | ~MST_MSCWR2_AC97_SPKROFF;
	return 0;
}

static int mioa701_probe(struct platform_device *pdev)
{
	//MST_MSCWR2 &= ~MST_MSCWR2_AC97_SPKROFF;
	return 0;
}

static int mioa701_remove(struct platform_device *pdev)
{
	//MST_MSCWR2 |= MST_MSCWR2_AC97_SPKROFF;
	return 0;
}

static const char* test_function[] = {"Off", "On"};
static const struct soc_enum mioa701_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, test_function),
};

static const struct snd_kcontrol_new mioa701_controls[] = {
	SOC_ENUM_EXT("ATest Function", mioa701_enum[0], get_test, set_test),
};

/* mioa701 machine dapm widgets */
static const struct snd_soc_dapm_widget mioa701_dapm_widgets[] = {
//	SND_SOC_DAPM_SPK("Speaker", mioa701_spkfront_power),
	SND_SOC_DAPM_MIC("Mic 1", NULL),
	SND_SOC_DAPM_MIC("Mic 2", NULL),
	SND_SOC_DAPM_MIC("Mic 3", NULL),
};

/* example machine audio_mapnections */
static const char* audio_map[][3] = {

	/* mic is connected to mic1 - with bias */
	{"MIC1", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Mic 1"},
	/* mic is connected to mic2A - with bias */
	{"MIC2A", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Mic 2"},
	/* mic is connected to mic2B - with bias */
	{"MIC2B", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Mic 3"},

	{NULL, NULL, NULL},
};

/*
 * This is an example machine initialisation for a wm9713 connected to a
 * Mainstone II. It is missing logic to detect hp/mic insertions and logic
 * to re-route the audio in such an event.
 */
static int mioa701_wm9713_init(struct snd_soc_codec *codec)
{
	int i, err;

	/* set up mioa701 codec pins */
	snd_soc_dapm_set_endpoint(codec, "RXP", 0);
	snd_soc_dapm_set_endpoint(codec, "RXN", 0);
	//snd_soc_dapm_set_endpoint(codec, "MIC2", 0);

	/* Add test specific controls */
	for (i = 0; i < ARRAY_SIZE(mioa701_controls); i++) {
		if ((err = snd_ctl_add(codec->card,
				snd_soc_cnew(&mioa701_controls[i],codec, NULL))) < 0)
			return err;
	}

	/* Add mioa701 specific widgets */
	for(i = 0; i < ARRAY_SIZE(mioa701_dapm_widgets); i++) {
		snd_soc_dapm_new_control(codec, &mioa701_dapm_widgets[i]);
	}

	/* set up mioa701 specific audio path audio_mapnects */
	for(i = 0; audio_map[i][0] != NULL; i++) {
		snd_soc_dapm_connect_input(codec, audio_map[i][0], audio_map[i][1],
			audio_map[i][2]);
	}

	snd_soc_dapm_sync_endpoints(codec);
	return 0;
}

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
};

static struct snd_soc_machine mioa701 = {
	.name = "Mioa701",
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
MODULE_AUTHOR("Robert Jarzmik (rjarzmik@yahoo.fr)");
MODULE_DESCRIPTION("ALSA SoC WM9713 MIO A701");
MODULE_LICENSE("GPL");

// ### Debug awfull stuff ###

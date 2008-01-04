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

static int test = 0;
static int mioa701_get_spk(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = test;
	printk("RJK: MioA701 mioa701_get_spk called\n");
	return 0;
}

static int mioa701_set_spk(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	test = ucontrol->value.integer.value[0];
	if(test) {

	} else {

	}
	printk("RJK: MioA701 mioa701_set_spk called\n");
	return 0;
}

static int mioa701_spkfront_power(struct snd_soc_dapm_widget *w, int event)
{
	struct snd_soc_codec *codec = w->codec;
	unsigned short val;

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		val = codec->read(codec, AC97_EXTENDED_MID);
		printk("RJK: MioA701 front speaker on\n");
		//codec->write(codec, AC97_POWERDOWN, 0x0000);
	}
	else
		printk("RJK: MioA701 front speaker off\n");
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

static const char* spk_function[] = {"Off", "On"};
static const struct soc_enum mioa701_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, spk_function),
};

static const struct snd_kcontrol_new mioa701_controls[] = {
	SOC_ENUM_EXT("Speaker Function", mioa701_enum[0], mioa701_get_spk,
		     mioa701_set_spk),
};

/* mioa701 machine dapm widgets */
static const struct snd_soc_dapm_widget mioa701_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Front Speaker", mioa701_spkfront_power),
	SND_SOC_DAPM_MIC("Mic 1", NULL),
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

	/* headphone connected to HPL, HPRo */
	{"Headphone Jack", NULL, "HPL"},
	{"Headphone Jack", NULL, "HPR"},

	/* front speaker connected to HPL, OUT3 */
	{"Front Speaker", NULL, "HPL"},
	{"Front Speaker", NULL, "OUT3"},

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
	snd_soc_dapm_set_endpoint(codec, "VOUTLHP", 0);
	snd_soc_dapm_set_endpoint(codec, "VOUTRHP", 0);

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

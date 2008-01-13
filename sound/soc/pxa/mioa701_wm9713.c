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
/* define the scenarios */
#define MIO_AUDIO_OFF			0
#define MIO_GSM_CALL_AUDIO_HANDSET	1
#define MIO_GSM_CALL_AUDIO_HEADSET	2
#define MIO_GSM_CALL_AUDIO_BLUETOOTH	3
#define MIO_STEREO_TO_SPEAKERS		4
#define MIO_STEREO_TO_HEADPHONES	5
#define MIO_CAPTURE_HANDSET		6
#define MIO_CAPTURE_HEADSET		7
#define MIO_CAPTURE_BLUETOOTH		8


static struct snd_soc_machine mioa701;
static int mio_scenario = MIO_AUDIO_OFF;

static int phone_stream_start(struct snd_soc_codec *codec);
static int phone_stream_stop(struct snd_soc_codec *codec);

// ### Debug awfull stuff ###
struct mio_mixes_t {
	char *mixname;
	int enum_val;
};
static const struct mio_mixes_t mixes_gsm_call_handset[] = {
	{ "Mono Out Mux", 0 },
	{ "Left Speaker Out Mux", 0 },
	{ "Right Speaker Out Mux", 0 },
	{ "Left Headphone Out Mux", 0 },
	{ "Right Headphone Out Mux", 0 },
	{ "Out 3 Mux", 0 },
	{ "Out 4 Mux", 0 },
	{ "Out 3 Mux", 0 },
	{ "DAC Inv Mux 1", 0 },
	{ "DAC Inv Mux 2", 0 },
	{ "Mic A Source", 0 },
	/* Mic Audio Path */
	{ "Mono Mixer PC Beep Playback Switch", 1 },
	{ "Mic 1 Sidetone Switch", 1 },
	{ "Sidetone Mux", 1 },

	

	{ NULL, 0 }
};

struct snd_kcontrol *mioa701_kctrl_byname(struct snd_soc_codec *codec, char *n)
{
	struct snd_kcontrol *kctl;
	struct snd_ctl_elem_id rid;
	struct snd_ctl_elem_value ucontrol;
	struct snd_card *card = codec->card;

	memset(&rid, 0, sizeof(rid));
	rid.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	strncpy(rid.name, n, sizeof(rid.name));
	kctl = snd_ctl_find_id(card, &rid);

	memset(&ucontrol, 0, sizeof(ucontrol));
	printk("RJK: found snd ctrl for %s at %p\n", n, kctl);

	return kctl;
}



static int set_scenario_endpoints(struct snd_soc_codec *codec, int scenario)
{
	switch(scenario) {
	case MIO_AUDIO_OFF:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   0);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   0);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     0);
		break;
	case MIO_GSM_CALL_AUDIO_HANDSET:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 1);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  1);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   1);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   0);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     1);
		break;
	case MIO_GSM_CALL_AUDIO_HEADSET:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 1);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  1);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   1);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   1);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     0);
		break;
	case MIO_GSM_CALL_AUDIO_BLUETOOTH:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  1);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   1);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   0);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     0);
		break;
	case MIO_STEREO_TO_SPEAKERS:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 1);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   0);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   0);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     0);
		break;
	case MIO_STEREO_TO_HEADPHONES:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   0);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   0);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     0);
		break;
	case MIO_CAPTURE_HANDSET:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   0);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   0);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     1);
		break;
	case MIO_CAPTURE_HEADSET:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   0);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   1);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     0);
		break;
	case MIO_CAPTURE_BLUETOOTH:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   0);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   0);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     0);
		break;
	default:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   0);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   0);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     0);
	}

	snd_soc_dapm_sync_endpoints(codec);

	return 0;
}

static int get_scenario(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mio_scenario;
	return 0;
}

static int set_scenario(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	/* Debug */
	mioa701_kctrl_byname(codec, "Mono Out Mux");

	if (mio_scenario == ucontrol->value.integer.value[0])
		return 0;

	mio_scenario = ucontrol->value.integer.value[0];
	set_scenario_endpoints(codec, mio_scenario);
	switch (mio_scenario) {
	case MIO_GSM_CALL_AUDIO_HANDSET:
	case MIO_GSM_CALL_AUDIO_HEADSET:
	case MIO_GSM_CALL_AUDIO_BLUETOOTH:
		phone_stream_start(codec);
		break;
	default:
		phone_stream_stop(codec);
		break;
	}
	return 1;
}

static int phone_stream_start(struct snd_soc_codec *codec)
{
	snd_soc_dapm_stream_event(codec, "AC97 HiFi",
				  SND_SOC_DAPM_STREAM_START);
	return 0;
}

static int phone_stream_stop(struct snd_soc_codec *codec)
{
	snd_soc_dapm_stream_event(codec, "AC97 HiFi", 
				  SND_SOC_DAPM_STREAM_STOP);
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

static const char *mio_scenarios[] = {
	"Off",
	"GSM Handset",
	"GSM Headset",
	"GSM Bluetooth",
	"Speakers",
	"Headphones",
	"Capture Handset",
	"Capture Headset",
	"Capture Bluetooth"
};
static const struct soc_enum mio_scenario_enum[] = {
	SOC_ENUM_SINGLE_EXT(9, mio_scenarios),
};

static const struct snd_kcontrol_new mioa701_controls[] = {
	SOC_ENUM_EXT("Mio Mode", mio_scenario_enum[0],
		     get_scenario, set_scenario),
};

/* mioa701 machine dapm widgets */
static const struct snd_soc_dapm_widget mioa701_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Front Speaker", NULL),
	SND_SOC_DAPM_SPK("Rear Speaker", NULL),
	SND_SOC_DAPM_LINE("GSM Line Out", NULL),
	SND_SOC_DAPM_LINE("GSM Line In", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Front Mic", NULL),
};

/* example machine audio_mapnections */
static const char* audio_map[][3] = {

	/* Call Mic */
	{"MIC2", NULL, "Mic Bias"},
	{"MIC2N", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Call Mic"},

	/* GSM Module */
	{"Aux DAC", NULL, "GSM Line Out"},
	{"MONO", NULL, "GSM Line In"},

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

	/* initialize mioa701 codec pins */
	set_scenario_endpoints(codec, mio_scenario);

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

	/* Set up mioa701 specific audio path audio_mapnects */
	for(i = 0; audio_map[i][0] != NULL; i++) {
		snd_soc_dapm_connect_input(codec, audio_map[i][0], audio_map[i][1],
			audio_map[i][2]);
	}

	snd_soc_dapm_sync_endpoints(codec);

	/* Debug */
	mioa701_kctrl_byname(codec, "Left Speaker Out Mux");

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
/* { */
/* 	.name = "Phone Virtual", */
/* 	.stream_name = "Phone Virtual", */
/* 	.cpu_dai = &pxa_ac97_dai[PXA2XX_DAI_AC97_HIFI], */
/* 	.codec_dai = &wm9713_dai[WM9713_DAI_AC97_HIFI], */
/* }, */
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


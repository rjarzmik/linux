/*
 * Handles the Mitac mioa701 SoC system
 *
 * Copyright (C) 2008 Robert Jarzmik
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/ac97_codec.h>

#include <asm/arch/audio.h>

#include "pxa2xx-pcm.h"
#include "../codecs/wm9713.h"

#define ARRAY_AND_SIZE(x)	(x), ARRAY_SIZE(x)

#define AC97_GPIO_PULL		0x58

/* define the scenarios */
#define MIO_AUDIO_OFF			0
#define MIO_GSM_AUDIO_HANDSET		1
#define MIO_GSM_AUDIO_HEADSET		2
#define MIO_GSM_AUDIO_HANDSFREE		3
#define MIO_GSM_AUDIO_BLUETOOTH		4
#define MIO_STEREO_TO_SPEAKER		5
#define MIO_STEREO_TO_HEADPHONES	6
#define MIO_CAPTURE_HANDSET		7
#define MIO_CAPTURE_HEADSET		8
#define MIO_CAPTURE_BLUETOOTH		9

static int mio_scenario = MIO_AUDIO_OFF;

static int phone_stream_start(struct snd_soc_card *card);
static int phone_stream_stop(struct snd_soc_card *card);

struct mio_mixes_t {
	char *mixname;
	int  val;
};

static const struct mio_mixes_t mixes_reset_all[] = {
	/* left HP mixer */
	{"Left HP Mixer PC Beep Playback Switch", 0},
	{"Left HP Mixer Voice Playback Switch",	  0},
	{"Left HP Mixer Aux Playback Switch",	  0},
	{"Left HP Mixer Bypass Playback Switch",  0},
	{"Left HP Mixer PCM Playback Switch",	  0},
	{"Left HP Mixer MonoIn Playback Switch",  0},
	{"Left HP Mixer Capture Headphone Mux",	  0},

	/* right HP mixer */
	{"Right HP Mixer PC Beep Playback Switch", 0},
	{"Right HP Mixer Voice Playback Switch",   0},
	{"Right HP Mixer Aux Playback Switch",	   0},
	{"Right HP Mixer Bypass Playback Switch",  0},
	{"Right HP Mixer PCM Playback Switch",	   0},
	{"Right HP Mixer MonoIn Playback Switch",  0},
	{"Right HP Mixer Capture Headphone Mux",   0},

	/* speaker mixer */
	{"Speaker Mixer PC Beep Playback Switch", 0},
	{"Speaker Mixer Voice Playback Switch",	  0},
	{"Speaker Mixer Aux Playback Switch",	  0},
	{"Speaker Mixer Bypass Playback Switch",  0},
	{"Speaker Mixer PCM Playback Switch",	  0},
	{"Speaker Mixer MonoIn Playback Switch",  0},
	{"Speaker Mixer Mic 1 Sidetone Switch",	  0},

	/* mono mixer */
	{"Mono Mixer PC Beep Playback Switch", 0},
	{"Mono Mixer Voice Playback Switch",   0},
	{"Mono Mixer Aux Playback Switch",     0},
	{"Mono Mixer Bypass Playback Switch",  0},
	{"Mono Mixer PCM Playback Switch",     0},
	{"Mono Mixer Capture Mono Mux",	       0},
	{"Mono Mixer MonoIn Playback Switch",  0},
	{"Mono Mixer Mic 1 Sidetone Switch",   0},
	{"Mono Playback Switch",	       0},

	/* headphone muxers */
	{"Left Headphone Out Mux", 0},
	{"Right Headphone Out Mux", 0},

	/* speaker muxer */
	{"Left Speaker Out Mux", 0},
	{"Right Speaker Out Mux", 0},

	/* Out3 muxer */
	{ "Out 3 Mux", 0},

	{ NULL, 0 }
};

static const struct mio_mixes_t mixes_gsm_call_headset[] = {
	/*
	 * GSM Out to Headset HPL Path
	 *   => PCBeep -> Headphone Mixer, Headphone Mixer -> HPL
	 */
	{ "Left HP Mixer PC Beep Playback Switch", 1 },
	{ "Left Headphone Out Mux", 2 },
	/*
	 * GSM Out to Headset HPR Path
	 *   => MonoIn -> Headphone Mixer, Headphone Mixer -> HPR
	 */
	{ "Right HP Mixer MonoIn Playback Switch" , 1 },
	{ "Right Headphone Out Mux", 2 },
	/*
	 * LineL to GSM In
	 * LineL -> MonoMixer, MonoMixer -> Mono, Unmute Mono Mixer
	 */
	{ "Mono Mixer Bypass Playback Switch",	1},
	{ "Mono Out Mux", 2 },
	{ "Mono Playback Switch", 1},
	{ NULL, 0 }
};

static const struct mio_mixes_t mixes_gsm_call_handset[] = {
	/*
	 * GSM Out to Front Speaker HPL Path
	 *   => PCBeep -> Headphone Mixer, Headphone Mixer -> HPL
	 */
	{ "Left HP Mixer PC Beep Playback Switch", 1 },
	{ "Left Headphone Out Mux", 2 },
	/*
	 * GSM Out to Front Speaker Out3 Path
	 *   => MonoIn -> Speaker Mixer, Speaker Mixer -> Inv1, Inv1 -> Out3
	 */
	{ "Speaker Mixer MonoIn Playback Switch" , 1 },
	{ "DAC Inv Mux 1", 2 },
	{ "Out 3 Mux", 2 },
	/*
	 * MIC1 to GSM In
	 *   => MIC1 -> MICA, MICA -> Mono Mixer, Mono Mixer -> MONO,
	 *      UnMute Mono Mixer
	 */
	{ "Mic A Source", 0 },
	{ "Mono Mixer Mic 1 Sidetone Switch", 1 },
	{ "Mono Out Mux", 2 },
	{ "Mono Playback Switch", 1},
	{ NULL, 0 }
};

static const struct mio_mixes_t mixes_gsm_call_handsfree[] = {
	/*
	 * GSM Out to Rear Speaker SPKL Path
	 *   => PCBeep -> Speaker Mixer, Speaker Mixer -> Inv1, Inv1 -> SPKL
	 */
	{ "Speaker Mixer PC Beep Playback Switch", 1 },
	{ "DAC Inv Mux 1", 2 },
	{ "Left Speaker Out Mux", 4 },
	/*
	 * GSM Out to Rear Speaker SPKR Path
	 *   => MonoIn -> Speaker Mixer, Speaker Mixer -> SPKR
	 */
	{ "Speaker Mixer MonoIn Playback Switch" , 1 },
	{ "Right Speaker Out Mux", 3 },
	/*
	 * MIC1 to GSM In
	 *   => MIC1 -> MICA, MICA -> Mono Mixer, Mono Mixer -> MONO,
	 *      Unmute Mono Mixer
	 */
	{ "Mic A Source", 0 },
	{ "Mono Mixer Mic 1 Sidetone Switch", 1 },
	{ "Mono Out Mux", 2 },
	{ "Mono Playback Switch", 1},
	{ NULL, 0 }
};

static const struct mio_mixes_t mixes_stereo_to_rearspeaker[] = {
	/*
	 * PCM to Rear Speakers
	 *   => PCM -> Speaker Mixer, Speaker Mixer -> Inv1, Inv1 -> SPKL,
	 *      Speaker Mixer -> SPKR
	 */
	{ "Speaker Mixer PCM Playback Switch", 1},
	{ "DAC Inv Mux 1", 2 },
	{ "Left Speaker Out Mux", 4 },
	{ "Right Speaker Out Mux", 3 },
	{ NULL, 0 }
};

struct snd_kcontrol *mioa701_kctrl_byname(struct snd_soc_card *card, char *n)
{
	struct snd_ctl_elem_id rid;

	memset(&rid, 0, sizeof(rid));
	rid.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	strncpy(rid.name, n, sizeof(rid.name));
	return snd_ctl_find_id(card->card, &rid);
}

void setup_muxers(struct snd_soc_card *card, const struct mio_mixes_t mixes[])
{
	int pos = 0;
	struct snd_kcontrol *kctl;
	struct snd_ctl_elem_value ucontrol;
	char mname[44];

	while (mixes[pos].mixname) {
		memset(mname, 0, 44);
		strncpy(mname, mixes[pos].mixname, 43);
		kctl = mioa701_kctrl_byname(card, mname);
		memset(&ucontrol, 0, sizeof(ucontrol));
		if (kctl) {
			kctl->get(kctl, &ucontrol);
			ucontrol.value.enumerated.item[0] = mixes[pos].val;
			kctl->put(kctl, &ucontrol);
		}
		pos++;
	}
}

#define NB_ENDP ARRAY_SIZE(endpn)
static int set_scenario_endpoints(struct snd_soc_card *card, int scenario)
{
	static char *endpn[] = { "Front Speaker", "Rear Speaker",
				 "GSM Line Out", "GSM Line In",
				 "Headset Mic", "Front Mic", "Headset" };
	static const int typ_endps[][NB_ENDP] = {
		{ 0, 0, 0, 0, 0, 0, 0 }, /* MIO_AUDIO_OFF		*/
		{ 1, 0, 1, 1, 0, 1, 0 }, /* MIO_GSM_AUDIO_HANDSET	*/
		{ 0, 0, 1, 1, 1, 0, 1 }, /* MIO_GSM_AUDIO_HEADSET	*/
		{ 0, 1, 1, 1, 0, 1, 0 }, /* MIO_GSM_AUDIO_HANDSFREE*/
		{ 0, 0, 1, 1, 0, 0, 0 }, /* MIO_GSM_AUDIO_BLUETOOTH*/
		{ 0, 1, 0, 0, 0, 0, 0 }, /* MIO_STEREO_TO_SPEAKER	*/
		{ 0, 0, 0, 0, 0, 0, 1 }, /* MIO_STEREO_TO_HEADPHONES	*/
		{ 0, 0, 0, 0, 0, 1, 0 }, /* MIO_CAPTURE_HANDSET		*/
		{ 0, 0, 0, 0, 1, 0, 0 }, /* MIO_CAPTURE_HEADSET		*/
		{ 0, 0, 0, 0, 0, 0, 0 }, /* MIO_CAPTURE_BLUETOOTH	*/
	};
	const int *endps = typ_endps[scenario];
	int i;

	for (i = 0; i < NB_ENDP; i++)
		if (endps[i])
			snd_soc_dapm_enable_pin(card, endpn[i]);
		else
			snd_soc_dapm_disable_pin(card, endpn[i]);
	snd_soc_dapm_sync(card);

	return 0;
}

static int get_scenario(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mio_scenario;
	return 0;
}

static int isPhoneMode(int scenario)
{
	int onPhone = 0;

	switch (scenario) {
	case MIO_GSM_AUDIO_HANDSET:
	case MIO_GSM_AUDIO_HEADSET:
	case MIO_GSM_AUDIO_BLUETOOTH:
	case MIO_GSM_AUDIO_HANDSFREE:
		onPhone = 1;
	}

	return onPhone;
}

static void switch_mio_mode(struct snd_soc_card *card, int new_scenario)
{
	int wasPhone = 0, willPhone = 0;

	wasPhone  = isPhoneMode(mio_scenario);
	willPhone = isPhoneMode(new_scenario);

	mio_scenario = new_scenario;
	set_scenario_endpoints(card, mio_scenario);

	if (!wasPhone && willPhone)
		phone_stream_start(card);
	if (wasPhone && !willPhone)
		phone_stream_stop(card);

	setup_muxers(card, mixes_reset_all);
	switch (mio_scenario) {
	case MIO_STEREO_TO_SPEAKER:
		setup_muxers(card, mixes_stereo_to_rearspeaker);
		break;
	case MIO_GSM_AUDIO_HANDSET:
		setup_muxers(card, mixes_gsm_call_handset);
		break;
	case MIO_GSM_AUDIO_HANDSFREE:
		setup_muxers(card, mixes_gsm_call_handsfree);
		break;
	case MIO_GSM_AUDIO_HEADSET:
		setup_muxers(card, mixes_gsm_call_headset);
		break;
	default:
		break;
	}
}

static int set_scenario(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);

	if (mio_scenario == ucontrol->value.integer.value[0])
		return 0;

	switch_mio_mode(card, ucontrol->value.integer.value[0]);
	return 1;
}

static int phone_stream_start(struct snd_soc_card *card)
{
	snd_soc_dapm_stream_event(card, "AC97 HiFi",
				  SND_SOC_DAPM_STREAM_START);
	return 0;
}

static int phone_stream_stop(struct snd_soc_card *card)
{
	snd_soc_dapm_stream_event(card, "AC97 HiFi",
				  SND_SOC_DAPM_STREAM_STOP);
	return 0;
}

/* Use GPIO8 for rear speaker amplificator */
static int rear_amp_power(struct snd_soc_codec *codec, int power)
{
	unsigned short reg;

	if (power) {
		reg = snd_soc_read(codec, AC97_GPIO_CFG);
		snd_soc_write(codec, AC97_GPIO_CFG, reg | 0x0100);
		reg = snd_soc_read(codec, AC97_GPIO_PULL);
		snd_soc_write(codec, AC97_GPIO_PULL, reg | (1<<15));
	} else {
		reg = snd_soc_read(codec, AC97_GPIO_CFG);
		snd_soc_write(codec, AC97_GPIO_CFG, reg & ~0x0100);
		reg = snd_soc_read(codec, AC97_GPIO_PULL);
		snd_soc_write(codec, AC97_GPIO_PULL, reg & ~(1<<15));
	}

	return 0;
}

static int rear_amp_event(struct snd_soc_dapm_widget *widget,
			  struct snd_kcontrol *kctl, int event)
{
	struct snd_soc_codec *codec = widget->codec;
	int rc;

	if (SND_SOC_DAPM_EVENT_ON(event))
		rc = rear_amp_power(codec, 1);
	else
		rc = rear_amp_power(codec, 0);

	return rc;
}

static const char *mio_scenarios[] = {
	"Off",
	"GSM Handset",
	"GSM Headset",
	"GSM Handsfree",
	"GSM Bluetooth",
	"PCM Speaker",
	"Headphones",
	"Capture Handset",
	"Capture Headset",
	"Capture Bluetooth"
};
static const struct soc_enum mio_scenario_enum[] = {
	SOC_ENUM_SINGLE_EXT(10, mio_scenarios),
};

static const struct snd_kcontrol_new mioa701_controls[] = {
	SOC_ENUM_EXT("Mio Mode", mio_scenario_enum[0],
		     get_scenario, set_scenario),
};

/* mioa701 machine dapm widgets */
static const struct snd_soc_dapm_widget mioa701_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Front Speaker", NULL),
	SND_SOC_DAPM_SPK("Rear Speaker", rear_amp_event),
	SND_SOC_DAPM_MIC("Headset", NULL),
	SND_SOC_DAPM_LINE("GSM Line Out", NULL),
	SND_SOC_DAPM_LINE("GSM Line In", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Front Mic", NULL),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* Call Mic */
	{"Mic Bias", NULL, "Front Mic"},
	{"MIC1", NULL, "Mic Bias"},

	/* Headset Mic */
	{"LINEL", NULL, "Headset Mic"},
	{"LINER", NULL, "Headset Mic"},

	/* GSM Module */
	{"MONOIN", NULL, "GSM Line Out"},
	{"PCBEEP", NULL, "GSM Line Out"},
	{"GSM Line In", NULL, "MONO"},

	/* headphone connected to HPL, HPR */
	{"Headset", NULL, "HPL"},
	{"Headset", NULL, "HPR"},

	/* front speaker connected to HPL, OUT3 */
	{"Front Speaker", NULL, "HPL"},
	{"Front Speaker", NULL, "OUT3"},

	/* rear speaker connected to SPKL, SPKR */
	{"Rear Speaker", NULL, "SPKL"},
	{"Rear Speaker", NULL, "SPKR"},
};

static int mioa701_wm9713_init(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec;
	struct snd_ac97_bus_ops *ac97_ops;
	int i, ret;
	unsigned short reg;

	codec = snd_soc_card_get_codec(card, wm9713_codec_id, 0);
	if (codec == NULL)
		return -ENODEV;

	ac97_ops = snd_soc_card_get_ac97_ops(card, pxa_ac97_hifi_dai_id);
	if (!ac97_ops) {
		printk(KERN_ERR "Unable to obtain AC97 operations\n");
		return -ENODEV;
	}

	/* register with AC97 bus for ad-hoc driver access */
	ret = snd_soc_new_ac97_codec(codec, ac97_ops, card->card, 0, 0);
	if (ret < 0)
		return ret;

	/* do a cold reset for the controller and then try
	 * a warm reset followed by an optional cold reset for codec */
	ac97_ops->reset(codec->ac97);
	ac97_ops->warm_reset(codec->ac97);
	if (ac97_ops->read(codec->ac97, AC97_VENDOR_ID1) == 0) {
		printk(KERN_ERR "AC97 link error\n");
		return ret;
	}

	snd_soc_card_init_codec(codec, card);

	/* initialize mioa701 codec pins */
	set_scenario_endpoints(card, mio_scenario);

	/* Add mioa701 specific controls */
	for (i = 0; i < ARRAY_SIZE(mioa701_controls); i++) {
		ret = snd_ctl_add(card->card, snd_soc_cnew(&mioa701_controls[i],
							   card, NULL));
		if (ret)
			goto out;
	}

	/* Add mioa701 specific widgets */
	ret = snd_soc_dapm_new_controls(card, codec,
					ARRAY_AND_SIZE(mioa701_dapm_widgets));
	if (ret)
		goto out;

	/* Set up mioa701 specific audio path audio_mapnects */
	ret = snd_soc_dapm_add_routes(card, ARRAY_AND_SIZE(audio_map));
	if (ret)
		goto out;

	/* Prepare MIC input */
	reg = snd_soc_read(codec, AC97_3D_CONTROL);
	snd_soc_write(codec, AC97_3D_CONTROL, reg | 0xc000);

	snd_soc_dapm_sync(card);

	return 0;
out:
	return ret;
}

static void mioa701_wm9713_exit(struct snd_soc_card *card)
{
}

#ifdef CONFIG_PM
static int mioa701_wm9713_suspend(struct platform_device *pdev,
				  pm_message_t state)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;

	codec = snd_soc_card_get_codec(card, wm9713_codec_id, 0);
	if (codec == NULL)
		return -ENODEV;

	rear_amp_power(codec, 0);
	return 0; /* snd_soc_card_suspend_pcms(card, state) doesn't work */
}

static int mioa701_wm9713_resume(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;

	codec = snd_soc_card_get_codec(card, wm9713_codec_id, 0);
	if (codec == NULL)
		return -ENODEV;

	rear_amp_power(codec, 0);
	return 0; /* snd_soc_card_resume_pcms(card) doesn't work */
}

#else
#define mioa701_wm9713_suspend NULL
#define mioa701_wm9713_resume  NULL
#endif

static struct snd_soc_ops mioa701_hifi_ops = {
};

static struct snd_soc_ops mioa701_aux_ops = {
};

static struct snd_soc_pcm_config pcm_configs[] = {
	{
		.name		= "Aux",
		.codec		= wm9713_codec_id,
		.codec_dai	= wm9713_codec_aux_dai_id,
		.platform	= pxa_platform_id,
		.cpu_dai	= pxa_ac97_aux_dai_id,
		.playback	= 1,
		.ops		= &mioa701_aux_ops,
	},
	{
		.name		= "HiFi",
		.codec		= wm9713_codec_id,
		.codec_dai	= wm9713_codec_hifi_dai_id,
		.platform	= pxa_platform_id,
		.cpu_dai	= pxa_ac97_hifi_dai_id,
		.playback	= 1,
		.capture	= 1,
		.ops		= &mioa701_hifi_ops,
	},
};

static int mioa701_wm9713_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	int ret;

	card = snd_soc_card_create("mioa701", &pdev->dev,
				   SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (card == NULL)
		return -ENOMEM;

	card->longname = "WM9713";
	card->init = mioa701_wm9713_init;
	card->exit = mioa701_wm9713_exit;
	card->private_data = pdev;
	platform_set_drvdata(pdev, card);

	ret = snd_soc_card_create_pcms(card, ARRAY_AND_SIZE(pcm_configs));
	if (ret < 0)
		goto errpcms;

	ret = snd_soc_card_register(card);
	if (ret < 0)
		goto errcard;
	return ret;

errpcms:
	snd_soc_card_free(card);
errcard:
	return ret;
}

static int __devexit mioa701_wm9713_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_card_free(card);
	return 0;
}

static struct platform_driver mioa701_wm9713_driver = {
	.probe		= mioa701_wm9713_probe,
	.remove		= __devexit_p(mioa701_wm9713_remove),
	.suspend	= mioa701_wm9713_suspend,
	.resume		= mioa701_wm9713_resume,
	.driver		= {
		.name		= "mioa701-wm9713",
		.owner		= THIS_MODULE,
	},
};

static int __init mioa701_asoc_init(void)
{
	return platform_driver_register(&mioa701_wm9713_driver);
}

static void __exit mioa701_asoc_exit(void)
{
	platform_driver_unregister(&mioa701_wm9713_driver);
}

module_init(mioa701_asoc_init);
module_exit(mioa701_asoc_exit);

/* Module information */
MODULE_AUTHOR("Robert Jarzmik (rjarzmik@free.fr)");
MODULE_DESCRIPTION("ALSA SoC WM9713 MIO A701");
MODULE_LICENSE("GPL");

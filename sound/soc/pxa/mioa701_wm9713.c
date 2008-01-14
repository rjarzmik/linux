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
#define MIO_GSM_CALL_AUDIO_HANDSFREE	3
#define MIO_GSM_CALL_AUDIO_BLUETOOTH	4
#define MIO_STEREO_TO_SPEAKERS		5
#define MIO_STEREO_TO_HEADPHONES	6
#define MIO_CAPTURE_HANDSET		7
#define MIO_CAPTURE_HEADSET		8
#define MIO_CAPTURE_BLUETOOTH		9


static struct snd_soc_machine mioa701;
static int mio_scenario = MIO_AUDIO_OFF;

static int phone_stream_start(struct snd_soc_codec *codec);
static int phone_stream_stop(struct snd_soc_codec *codec);

// ### Debug awfull stuff ###
struct mio_mixes_t {
	char *mixname;
	int  val;
};

static const struct mio_mixes_t mixes_reset_all[] = {
	/* left HP mixer */
	{"Left HP Mixer PC Beep Playback Switch", 0},
	{"Left HP Mixer Voice Playback Switch",   0},
	{"Left HP Mixer Aux Playback Switch",     0},
	{"Left HP Mixer Bypass Playback Switch",  0},
	{"Left HP Mixer PCM Playback Switch",     0},
	{"Left HP Mixer MonoIn Playback Switch",  0},
	{"Left HP Mixer Capture Headphone Mux",   0},

	/* right HP mixer */
	{"Right HP Mixer PC Beep Playback Switch", 0},
	{"Right HP Mixer Voice Playback Switch",   0},
	{"Right HP Mixer Aux Playback Switch",     0},
	{"Right HP Mixer Bypass Playback Switch",  0},
	{"Right HP Mixer PCM Playback Switch",     0},
	{"Right HP Mixer MonoIn Playback Switch",  0},
	{"Right HP Mixer Capture Headphone Mux",   0},

	/* speaker mixer */
	{"Speaker Mixer PC Beep Playback Switch", 0},
	{"Speaker Mixer Voice Playback Switch",   0},
	{"Speaker Mixer Aux Playback Switch",     0},
	{"Speaker Mixer Bypass Playback Switch",  0},
	{"Speaker Mixer PCM Playback Switch",     0},
	{"Speaker Mixer MonoIn Playback Switch",  0},

	/* mono mixer */
	{"Mono Mixer PC Beep Playback Switch", 0},
	{"Mono Mixer Voice Playback Switch",   0},
	{"Mono Mixer Aux Playback Switch",     0},
	{"Mono Mixer Bypass Playback Switch",  0},
	{"Mono Mixer PCM Playback Switch",     0},
	{"Mono Mixer Capture Mono Mux",        0},

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

static const struct mio_mixes_t mixes_gsm_call_handset[] = {
	/* GSM Out to Front Speaker HPL Path */
	{ "Left HP Mixer PC Beep Playback Switch", 1 },
	{ "Left Headphone Out Mux", 2 }, // wm9713_spk_gpa = "Headphone"
	/* GSM Out to Front Speaker Out3 Path */
	{ "Speaker Mixer MonoIn Playback Switch" , 1 },
	{ "DAC Inv Mux 1", 2 }, // wm9713_dac_inv = "Speaker"
	{ "Out 3 Mux", 2 },     // wm9713_out3_pga = "Inv 1"
	/* MIC1 to GSM In */
	{ "Mic A Source", 0 },  // wm9713_mic_select = "Mic 1"
	{ "Mic 1 Sidetone Switch", 1 }, // wm9713_mono_mixer_controls = "Mic 1..."
	{ "Mono Out Mux", 2 },  // wm9713_mono_pga = "Mono"
	{ "Mono Mixer PC Beep Playback Switch", 1 },
	{ NULL, 0 }
};

static const struct mio_mixes_t mixes_gsm_call_handsfree[] = {
	/* GSM Out to Rear Speaker SPKL Path */
	{ "Speaker Mixer PC Beep Playback Switch", 1 },
	{ "DAC Inv Mux 1", 2 }, // wm9713_dac_inv = "Speaker"
	{ "Left Speaker Out Mux", 3 }, // wm9713_spk_pga = "Speaker"
	/* GSM Out to Rear Speaker SPKR Path */
	{ "Speaker Mixer MonoIn Playback Switch" , 1 },
	{ "Right Speaker Out Mux", 3 }, // wm9713_spk_pga = "Speaker"
	/* MIC1 to GSM In */
	{ "Mic A Source", 0 },  // wm9713_mic_select = "Mic 1"
	{ "Mic 1 Sidetone Switch", 1 }, // wm9713_mono_mixer_controls = "Mic 1..."
	{ "Mono Out Mux", 2 },  // wm9713_mono_pga = "Mono"
	{ "Mono Mixer PC Beep Playback Switch", 1 },
	{ NULL, 0 }
};

static const struct mio_mixes_t mixes_stereo_to_speakers[] = {
	{"Left HP Mixer PCM Playback Switch", 1},  // PCM -> Headphone Mixer
	{"Right HP Mixer PCM Playback Switch", 1}, // PCM -> Headphone Mixer
	{"Left Speaker Out Mux", 3 },		// Headphone Mixer -> SPKL
	{"Right Speaker Out Mux", 3 },		// Headphone Mixer -> SPKR
	{"Left Headphone Out Mux", 3 },		// Headphone Mixer -> HPL
	{"Right Headphone Out Mux", 3 },	// Headphone Mixer -> HPR
	{ "Out 3 Mux", 2 },			// Inv1 -> OUT3
};

struct snd_kcontrol *mioa701_kctrl_byname(struct snd_soc_codec *codec, char *n)
{
	struct snd_kcontrol *kctl;
	struct snd_ctl_elem_id rid;
	struct snd_card *card = codec->card;

	memset(&rid, 0, sizeof(rid));
	rid.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	strncpy(rid.name, n, sizeof(rid.name));
	kctl = snd_ctl_find_id(card, &rid);

	return kctl;
}

void setup_muxers(struct snd_soc_codec *codec, const struct mio_mixes_t mixes[])
{
	int pos = 0;
	struct snd_kcontrol *kctl;
	struct snd_ctl_elem_value ucontrol;
	char mname[32];

	while (mixes[pos].mixname) {
		memset(mname, 0, 32);
		strncpy(mname, mixes[pos].mixname, 31);
		kctl = mioa701_kctrl_byname(codec, mname);
		memset(&ucontrol, 0, sizeof(ucontrol));
		if (kctl) {
			kctl->get(kctl, &ucontrol);
			printk ("RJK: changing val of %s from %d to %d\n",
				kctl->id.name,
				ucontrol.value.enumerated.item[0],
				mixes[pos].val);
			ucontrol.value.enumerated.item[0] = mixes[pos].val;
			kctl->put(kctl, &ucontrol);
		}
		pos++;
	}

	/* Debug controls
	struct snd_ctl_elem_id rid;
	struct snd_card *card = codec->card;
	list_for_each_entry(kctl, &card->controls, list) {
		printk("Control : %s\n", kctl->id.name);
	}
	*/
}


static int set_scenario_endpoints(struct snd_soc_codec *codec, int scenario)
{
	/* endps[] = Front Speaker, Rear Speaker, GSM Line Out, GSM Line In,
	             Headset Mic, Front Mic */
	int *endps;
	static const int typ_endps[][6] = {
		{ 0, 0, 0, 0, 0, 0 }, // MIO_AUDIO_OFF
		{ 1, 0, 1, 1, 0, 1 }, // MIO_GSM_CALL_AUDIO_HANDSET
		{ 0, 1, 1, 1, 0, 1 }, // MIO_GSM_CALL_AUDIO_HANDSFREE
		{ 0, 0, 1, 1, 1, 0 }, // MIO_GSM_CALL_AUDIO_HEADSET
		{ 0, 0, 1, 1, 0, 0 }, // MIO_GSM_CALL_AUDIO_BLUETOOTH
		{ 1, 1, 0, 0, 0, 0 }, // MIO_STEREO_TO_SPEAKERS
		{ 0, 0, 0, 0, 0, 0 }, // MIO_STEREO_TO_HEADPHONES
		{ 0, 0, 0, 0, 0, 1 }, // MIO_CAPTURE_HANDSET
		{ 0, 0, 0, 0, 1, 0 }, // MIO_CAPTURE_HEADSET
		{ 0, 0, 0, 0, 0, 0 }, // MIO_CAPTURE_BLUETOOTH
	};

	endps = typ_endps[scenario];
/*
	switch(scenario) {
	case MIO_AUDIO_OFF:
		endps = { 0, 0, 0, 0, 0, 0 };
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 0);
		snd_soc_dapm_set_endpoint(codec, "Rear Speaker",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   0);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   0);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     0);
		break;
	case MIO_GSM_CALL_AUDIO_HANDSET:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 1);
		snd_soc_dapm_set_endpoint(codec, "Rear Speaker",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  1);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   1);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   0);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     1);
		break;
	case MIO_GSM_CALL_AUDIO_HANDSFREE:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 0);
		snd_soc_dapm_set_endpoint(codec, "Rear Speaker",  1);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  1);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   1);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   0);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     1);
		break;
	case MIO_GSM_CALL_AUDIO_HEADSET:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 0);
		snd_soc_dapm_set_endpoint(codec, "Rear Speaker",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  1);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   1);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   1);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     0);
		break;
	case MIO_GSM_CALL_AUDIO_BLUETOOTH:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 0);
		snd_soc_dapm_set_endpoint(codec, "Rear Speaker",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  1);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   1);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   0);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     0);
		break;
	case MIO_STEREO_TO_SPEAKERS:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 1);
		snd_soc_dapm_set_endpoint(codec, "Rear Speaker",  1);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   0);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   0);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     0);
		break;
	case MIO_STEREO_TO_HEADPHONES:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 0);
		snd_soc_dapm_set_endpoint(codec, "Rear Speaker",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   0);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   0);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     0);
		break;
	case MIO_CAPTURE_HANDSET:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 0);
		snd_soc_dapm_set_endpoint(codec, "Rear Speaker",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   0);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   0);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     1);
		break;
	case MIO_CAPTURE_HEADSET:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 0);
		snd_soc_dapm_set_endpoint(codec, "Rear Speaker",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   0);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   1);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     0);
		break;
	case MIO_CAPTURE_BLUETOOTH:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 0);
		snd_soc_dapm_set_endpoint(codec, "Rear Speaker",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   0);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   0);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     0);
		break;
	default:
		snd_soc_dapm_set_endpoint(codec, "Front Speaker", 0);
		snd_soc_dapm_set_endpoint(codec, "Rear Speaker",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  0);
		snd_soc_dapm_set_endpoint(codec, "GSM Line In",   0);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic",   0);
		snd_soc_dapm_set_endpoint(codec, "Front Mic",     0);
	}
*/
	snd_soc_dapm_set_endpoint(codec, "Front Speaker", endps[0]);
	snd_soc_dapm_set_endpoint(codec, "Rear Speaker",  endps[1]);
	snd_soc_dapm_set_endpoint(codec, "GSM Line Out",  endps[2]);
	snd_soc_dapm_set_endpoint(codec, "GSM Line In",   endps[3]);
	snd_soc_dapm_set_endpoint(codec, "Headset Mic",   endps[4]);
	snd_soc_dapm_set_endpoint(codec, "Front Mic",     endps[5]);
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
	case MIO_GSM_CALL_AUDIO_HANDSFREE:
		phone_stream_start(codec);
	break;
	default:
		phone_stream_stop(codec);
		break;
	}
	switch (mio_scenario) {
	case MIO_AUDIO_OFF:
		setup_muxers(codec, mixes_reset_all);
	case MIO_STEREO_TO_SPEAKERS:
		setup_muxers(codec, mixes_stereo_to_speakers);
	case MIO_GSM_CALL_AUDIO_HANDSET:
		setup_muxers(codec, mixes_gsm_call_handset);
		break;
	case MIO_GSM_CALL_AUDIO_HANDSFREE:
		setup_muxers(codec, mixes_gsm_call_handsfree);
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

static const char *mio_scenarios[] = {
	"Off",
	"GSM Handset",
	"GSM Headset",
	"GSM Handsfree",
	"GSM Bluetooth",
	"Speakers",
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
	SND_SOC_DAPM_SPK("Rear Speaker", NULL),
	SND_SOC_DAPM_LINE("GSM Line Out", NULL),
	SND_SOC_DAPM_LINE("GSM Line In", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Front Mic", NULL),
};

/* example machine audio_mapnections */
static const char* audio_map[][3] = {

	/* Call Mic */
	{"Mic Bias", NULL, "Call Mic"},
	{"MIC1", NULL, "Front Mic"},

	/* GSM Module */
	{"MONOIN", NULL, "GSM Line Out"},
	{"PCBEEP", NULL, "GSM Line Out"},
	{"GSM Line In", NULL, "MONO"},

	/* headphone connected to HPL, HPR */
	{"Headphone Jack", NULL, "HPL"},
	{"Headphone Jack", NULL, "HPR"},

	/* front speaker connected to HPL, OUT3 */
	{"Front Speaker", NULL, "HPL"},
	{"Front Speaker", NULL, "OUT3"},

	/* front speaker connected to SPKL, SPKR */
	{"Rear Speaker", NULL, "SPKL"},
	{"Rear Speaker", NULL, "SPKR"},

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


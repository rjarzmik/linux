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
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/arch/pxa-regs.h>
#include <asm/gpio.h>
#include <asm/arch/audio.h>

#include "../codecs/wm9713.h"
#include "pxa2xx-pcm.h"
#include "pxa2xx-ac97.h"
#include "../../../arch/arm/mach-pxa/mioa701/mioa701.h"

#define GPIO22_SSP2SYSCLK_MD	(22 | GPIO_ALT_FN_2_OUT)
#define AC97_GPIO_PULL		0x58
/* define the scenarios */
#define MIO_AUDIO_OFF			0
#define MIO_GSM_CALL_AUDIO_HANDSET	1
#define MIO_GSM_CALL_AUDIO_HEADSET	2
#define MIO_GSM_CALL_AUDIO_HANDSFREE	3
#define MIO_GSM_CALL_AUDIO_BLUETOOTH	4
#define MIO_STEREO_TO_SPEAKER		5
#define MIO_STEREO_TO_HEADPHONES	6
#define MIO_CAPTURE_HANDSET		7
#define MIO_CAPTURE_HEADSET		8
#define MIO_CAPTURE_BLUETOOTH		9

extern int mioa701_master_init(struct snd_soc_codec *codec);
extern void mioa701_master_change(int scenario);

static struct snd_soc_machine mioa701;
static int mio_scenario = MIO_AUDIO_OFF;

static int phone_stream_start(struct snd_soc_codec *codec);
static int phone_stream_stop(struct snd_soc_codec *codec);
static int setup_jack_isr(struct platform_device *pdev);
static void remove_jack_isr(struct platform_device *pdev);
static void hpjack_toggle(struct work_struct *unused);

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
	{"Speaker Mixer Mic 1 Sidetone Switch",   0},

	/* mono mixer */
	{"Mono Mixer PC Beep Playback Switch", 0},
	{"Mono Mixer Voice Playback Switch",   0},
	{"Mono Mixer Aux Playback Switch",     0},
	{"Mono Mixer Bypass Playback Switch",  0},
	{"Mono Mixer PCM Playback Switch",     0},
	{"Mono Mixer Capture Mono Mux",        0},
	{"Mono Mixer MonoIn Playback Switch",  0},
	{"Mono Mixer Mic 1 Sidetone Switch",   0},
	{"Mono Playback Switch",               0},

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
	/* GSM Out to Headset HPL Path */
	{ "Left HP Mixer PC Beep Playback Switch", 1 }, // PCBeep->Headphone Mix
	{ "Left Headphone Out Mux", 2 },		// Headphone Mixer->HPL
	/* GSM Out to Headset HPR Path */
	{ "Right HP Mixer MonoIn Playback Switch" , 1 }, //MonoIn->HP Mixer
	{ "Right Headphone Out Mux", 2 },		// Headphone Mixer->HPR
	/* LineL to GSM In */
	{ "Mono Mixer Bypass Playback Switch",  1},	// LineL -> MonoMixer
	{ "Mono Out Mux", 2 },				// Mono Mixer -> Mono
	{ "Mono Playback Switch", 1},			// Unmute Mono Mixer
	{ NULL, 0 }
};

static const struct mio_mixes_t mixes_gsm_call_handset[] = {
	/* GSM Out to Front Speaker HPL Path */
	{ "Left HP Mixer PC Beep Playback Switch", 1 }, // PCBeep->Headphone Mix
	{ "Left Headphone Out Mux", 2 },		// Headphone Mixer->HPL
	/* GSM Out to Front Speaker Out3 Path */
	{ "Speaker Mixer MonoIn Playback Switch" , 1 }, // MonoIn->Speaker Mixer
	{ "DAC Inv Mux 1", 2 },				// Speaker Mix -> Inv1
	{ "Out 3 Mux", 2 },				// Inv1 -> Out3
	/* MIC1 to GSM In */
	{ "Mic A Source", 0 },				// MIC1 -> MICA
	{ "Mono Mixer Mic 1 Sidetone Switch", 1 },	// MICA -> Mono Mixer
	{ "Mono Out Mux", 2 },				// Mono Mixer -> MONO
	{ "Mono Playback Switch", 1},			// Unmute Mono Mixer
	{ NULL, 0 }
};

static const struct mio_mixes_t mixes_gsm_call_handsfree[] = {
	/* GSM Out to Rear Speaker SPKL Path */
	{ "Speaker Mixer PC Beep Playback Switch", 1 }, // PCBeep->Speaker Mix
	{ "DAC Inv Mux 1", 2 },				// Speaker Mixer -> Inv1
	{ "Left Speaker Out Mux", 4 },			// Inv1 -> SPKL
	/* GSM Out to Rear Speaker SPKR Path */
	{ "Speaker Mixer MonoIn Playback Switch" , 1 },	// MonoIn->Speaker Mix
	{ "Right Speaker Out Mux", 3 },			// Speaker Mixer -> SPKR
	/* MIC1 to GSM In */
	{ "Mic A Source", 0 },				// MIC1 -> MICA
	{ "Mono Mixer Mic 1 Sidetone Switch", 1 },	// MICA -> Mono Mixer
	{ "Mono Out Mux", 2 },				// Mono Mixer -> MONO
	{ "Mono Playback Switch", 1},			// Unmute Mono Mixer
	{ NULL, 0 }
};

static const struct mio_mixes_t mixes_stereo_to_rearspeaker[] = {
	{ "Speaker Mixer PCM Playback Switch", 1},	// PCM -> Speaker Mixer
	{ "DAC Inv Mux 1", 2 },				// Speaker Mixer -> Inv1
	{ "Left Speaker Out Mux", 4 },			// Inv1 -> SPKL
	{ "Right Speaker Out Mux", 3 },			// Speaker Mixer -> SPKR
	{ NULL, 0 }
};

struct snd_kcontrol *mioa701_kctrl_byname(struct snd_soc_codec *codec, char *n)
{
	struct snd_ctl_elem_id rid;

	memset(&rid, 0, sizeof(rid));
	rid.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	strncpy(rid.name, n, sizeof(rid.name));
	return snd_ctl_find_id(codec->card, &rid);
}

void setup_muxers(struct snd_soc_codec *codec, const struct mio_mixes_t mixes[])
{
	int pos = 0;
	struct snd_kcontrol *kctl;
	struct snd_ctl_elem_value ucontrol;
	char mname[44];

	while (mixes[pos].mixname) {
		memset(mname, 0, 44);
		strncpy(mname, mixes[pos].mixname, 43);
		kctl = mioa701_kctrl_byname(codec, mname);
		memset(&ucontrol, 0, sizeof(ucontrol));
		if (kctl) {
			kctl->get(kctl, &ucontrol);
			/* RJK debug to be removed
			printk("setup_mixers: control : %s %d->%d\n", 
			       kctl->id.name, ucontrol.value.enumerated.item[0],
			       mixes[pos].val);
			*/
			ucontrol.value.enumerated.item[0] = mixes[pos].val;
			kctl->put(kctl, &ucontrol);
		}
		pos++;
	}
}

#define NB_ENDP sizeof(endpn)/sizeof(char *)
static int set_scenario_endpoints(struct snd_soc_codec *codec, int scenario)
{
	static char *endpn[] = { "Front Speaker", "Rear Speaker", 
				 "GSM Line Out", "GSM Line In",
				 "Headset Mic", "Front Mic", "Headset" };
	static const int typ_endps[][NB_ENDP] = {
		{ 0, 0, 0, 0, 0, 0, 0 }, // MIO_AUDIO_OFF
		{ 1, 0, 1, 1, 0, 1, 0 }, // MIO_GSM_CALL_AUDIO_HANDSET
		{ 0, 0, 1, 1, 1, 0, 1 }, // MIO_GSM_CALL_AUDIO_HEADSET
		{ 0, 1, 1, 1, 0, 1, 0 }, // MIO_GSM_CALL_AUDIO_HANDSFREE
		{ 0, 0, 1, 1, 0, 0, 0 }, // MIO_GSM_CALL_AUDIO_BLUETOOTH
		{ 0, 1, 0, 0, 0, 0, 0 }, // MIO_STEREO_TO_SPEAKER
		{ 0, 0, 0, 0, 0, 0, 1 }, // MIO_STEREO_TO_HEADPHONES
		{ 0, 0, 0, 0, 0, 1, 0 }, // MIO_CAPTURE_HANDSET
		{ 0, 0, 0, 0, 1, 0, 0 }, // MIO_CAPTURE_HEADSET
		{ 0, 0, 0, 0, 0, 0, 0 }, // MIO_CAPTURE_BLUETOOTH
	};
	const int *endps = typ_endps[scenario];
	int i;

	for (i=0; i < NB_ENDP; i++)
		snd_soc_dapm_set_endpoint(codec, endpn[i], endps[i]);
	snd_soc_dapm_sync_endpoints(codec);

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
	case MIO_GSM_CALL_AUDIO_HANDSET:
	case MIO_GSM_CALL_AUDIO_HEADSET:
	case MIO_GSM_CALL_AUDIO_BLUETOOTH:
	case MIO_GSM_CALL_AUDIO_HANDSFREE:
		onPhone = 1;
	}

	return onPhone;
}

static void switch_mio_mode(struct snd_soc_codec *codec, int new_scenario)
{
	int wasPhone = 0, willPhone = 0;

	wasPhone  = isPhoneMode(mio_scenario);
	willPhone = isPhoneMode(new_scenario);

	mio_scenario = new_scenario;
	set_scenario_endpoints(codec, mio_scenario);

	if (!wasPhone && willPhone)
		phone_stream_start(codec);
	if (wasPhone && !willPhone)
		phone_stream_stop(codec);

	setup_muxers(codec, mixes_reset_all);
	switch (mio_scenario) {
	case MIO_STEREO_TO_SPEAKER:
		setup_muxers(codec, mixes_stereo_to_rearspeaker);
		break;
	case MIO_GSM_CALL_AUDIO_HANDSET:
		setup_muxers(codec, mixes_gsm_call_handset);
		break;
	case MIO_GSM_CALL_AUDIO_HANDSFREE:
		setup_muxers(codec, mixes_gsm_call_handsfree);
		break;
	case MIO_GSM_CALL_AUDIO_HEADSET:
		setup_muxers(codec, mixes_gsm_call_headset);
		break;
	default:
		break;
	}

	mioa701_master_change(mio_scenario);
}

static int set_scenario(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	if (mio_scenario == ucontrol->value.integer.value[0])
		return 0;
	
	switch_mio_mode(codec, ucontrol->value.integer.value[0]);
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

/* Use GPIO8 for rear speaker amplificator */
static int rear_amp_power(struct snd_soc_codec *codec, int power)
{
	unsigned short reg;

	if (power) {
		reg = codec->read(codec, AC97_GPIO_CFG);
		codec->write(codec, AC97_GPIO_CFG, reg | 0x0100);
		reg = codec->read(codec, AC97_GPIO_PULL);
		codec->write(codec, AC97_GPIO_PULL, reg | (1<<15));
	}
	else {
		reg = codec->read(codec, AC97_GPIO_CFG);
		codec->write(codec, AC97_GPIO_CFG, reg & ~0x0100);
		reg = codec->read(codec, AC97_GPIO_PULL);
		codec->write(codec, AC97_GPIO_PULL, reg & ~(1<<15));
	}

	return 0;
}

static int rear_amp_event(struct snd_soc_dapm_widget *widget, int event)
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

/* example machine audio_mapnections */
static const char* audio_map[][3] = {
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

	{NULL, NULL, NULL},
};

/* Headphone Jack functions */
static struct snd_soc_codec *hpjack_codec = NULL;
DECLARE_WORK(hpjack_wq, hpjack_toggle);

static void hpjack_on(struct snd_soc_codec *codec)
{
	switch (mio_scenario) {
	case MIO_GSM_CALL_AUDIO_HANDSET:
	case MIO_GSM_CALL_AUDIO_HANDSFREE:
		switch_mio_mode(codec, MIO_GSM_CALL_AUDIO_HEADSET);
		break;
	case MIO_STEREO_TO_SPEAKER:
		switch_mio_mode(codec, MIO_STEREO_TO_HEADPHONES);
		break;
	}
}

static void hpjack_off(struct snd_soc_codec *codec)
{
	switch (mio_scenario) {
	case MIO_GSM_CALL_AUDIO_HEADSET:
		switch_mio_mode(codec, MIO_GSM_CALL_AUDIO_HANDSET);
		break;
	case MIO_STEREO_TO_HEADPHONES:
		switch_mio_mode(codec, MIO_STEREO_TO_SPEAKER);
		break;
	}
}

static void hpjack_toggle(struct work_struct *unused)
{
	int val = gpio_get_value(MIO_GPIO_HPJACK_INSERT);

	if (!hpjack_codec)
		return;

	if (val)
		hpjack_on(hpjack_codec);
	else
		hpjack_off(hpjack_codec);
}

static irqreturn_t hpjack_isr(int irq, void *irq_desc)
{
	schedule_work(&hpjack_wq);
	return IRQ_HANDLED;
}

static int setup_jack_isr(struct platform_device *pdev)
{
	int irq = gpio_to_irq(MIO_GPIO_HPJACK_INSERT);
	int ret;

	pxa_gpio_mode(MIO_GPIO_HPJACK_INSERT);
	ret = request_irq(irq, hpjack_isr, 
			  IRQF_DISABLED | IRQF_TRIGGER_LOW | IRQF_TRIGGER_HIGH,
			  "hearphone jack",
			  &hpjack_codec);
	set_irq_type(irq, IRQ_TYPE_EDGE_BOTH);
	return ret;
}

static void remove_jack_isr(struct platform_device *pdev)
{
	int irq = gpio_to_irq(MIO_GPIO_HPJACK_INSERT);

	free_irq(irq, &hpjack_codec);
	hpjack_codec = NULL;
}

static int mioa701_wm9713_init(struct snd_soc_codec *codec)
{
	int i, err;
	unsigned short reg;

	/* initialize mioa701 codec pins */
	set_scenario_endpoints(codec, mio_scenario);

	/* Add test specific controls */
	for (i = 0; i < ARRAY_SIZE(mioa701_controls); i++) {
		if ((err = snd_ctl_add(codec->card,
				snd_soc_cnew(&mioa701_controls[i],codec, NULL))) < 0)
			return err;
	}

	/* Add Mio Masters */
	mioa701_master_init(codec);

	/* Add mioa701 specific widgets */
	for(i = 0; i < ARRAY_SIZE(mioa701_dapm_widgets); i++) {
		snd_soc_dapm_new_control(codec, &mioa701_dapm_widgets[i]);
	}

	/* Set up mioa701 specific audio path audio_mapnects */
	for(i = 0; audio_map[i][0] != NULL; i++) {
		snd_soc_dapm_connect_input(codec, audio_map[i][0], audio_map[i][1],
			audio_map[i][2]);
	}

	/* Prepare MIC input */
	reg = codec->read(codec, AC97_3D_CONTROL);
	codec->write(codec, AC97_3D_CONTROL, reg | 0xc000);

	snd_soc_dapm_sync_endpoints(codec);
	hpjack_codec = codec;

	return 0;
}

static int mioa701_suspend(struct platform_device *pdev, pm_message_t state)
{
 	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;

	rear_amp_power(codec, 0);
	return 0;
}

static int mioa701_resume(struct platform_device *pdev)
{
 	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;

	rear_amp_power(codec, 0);
	return 0;
}

static int mioa701_probe(struct platform_device *pdev)
{
	setup_jack_isr(pdev);
	return 0;
}

static int mioa701_remove(struct platform_device *pdev)
{
	remove_jack_isr(pdev);
	return 0;
}

/* Add some hook for wm9713 reset procedure to setup wm9713 registers */

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

	if ((ret = platform_device_add(mioa701_snd_ac97_device)) > 0)
		goto devadderr;

	platform_device_put(mioa701_snd_ac97_device);

devadderr:
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

/* Git AsoC v2 */
/* git://opensource.wolfsonmicro.com/linux-2.6-asoc asoc-v2-dev */

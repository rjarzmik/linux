/*
 * mioa701_wm9713.c
 *
 * Robert Jarzmik
 *
 * Licenced under GPLv2
 */

#include <linux/vmalloc.h>
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

#define MASTER_MAX_CTL 2
#define MAST2CODEC(m) ((struct kctl_master_t *)m)->codec
#define KCTL2MAST(k) (struct kctl_master_t *) k->private_data
struct kctl_master_t {
	unsigned int count;
	snd_ctl_elem_type_t type;
	char *names[MASTER_MAX_CTL];
	int nidx[MASTER_MAX_CTL];
	struct snd_soc_codec *codec;
};
#define MVOL2(name1, idx1, name2, idx2) \
{	.count = 2, .names = { name1, name2 }, .nidx = { idx1, idx2 }, \
	.type = SNDRV_CTL_ELEM_TYPE_INTEGER, \
}
#define MMUTE2(name1, idx1, name2, idx2) \
{	.count = 2, .names = { name1, name2 }, .nidx = { idx1, idx2 }, \
	.type = SNDRV_CTL_ELEM_TYPE_BOOLEAN, \
}

#define MVOL2_DEFAULT MVOL2(NULL, 0, NULL, 0)
#define MMUTE2_DEFAULT MMUTE2(NULL, 0, NULL, 0)

static struct snd_kcontrol *kctrl_byname(struct snd_soc_codec *codec, char *n)
{
	struct snd_ctl_elem_id rid;

	memset(&rid, 0, sizeof(rid));
	rid.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	strncpy(rid.name, n, sizeof(rid.name));
	return snd_ctl_find_id(codec->card, &rid);
}

static int master_find(struct snd_soc_codec *codec, char *name,
		int *max, struct snd_kcontrol **kctl)
{
	struct snd_ctl_elem_info info;

	memset(&info, 0, sizeof(struct snd_ctl_elem_info));
	*kctl = NULL;

	if (name)
		*kctl = kctrl_byname(codec, name);

	if (*kctl) {
		(*kctl)->info(*kctl, &info);
		*max = info.value.integer.max;
	}

	return (*kctl != NULL);
}

static int master_info(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_info *uinfo)
{
	struct kctl_master_t *master = KCTL2MAST(kcontrol);

	uinfo->type = master->type;
	uinfo->count = master->count;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0;
	if (master->type & SNDRV_CTL_ELEM_TYPE_BOOLEAN)
		uinfo->value.integer.max = 1;
	if (master->type & SNDRV_CTL_ELEM_TYPE_INTEGER)
		uinfo->value.integer.max = 256;
	return 0;
}

static int master_get(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_kcontrol *kctl;
	struct snd_ctl_elem_value uc;
	struct kctl_master_t *master = KCTL2MAST(kcontrol);
	struct snd_soc_codec *codec = MAST2CODEC(master);
	int i, ind, max, old;

	for (i=0; i<master->count; i++) {
		max = old = 0;
		ind = master->nidx[i];
		if (!master_find(codec, master->names[i], &max, &kctl))
			continue;
		if (kctl->get(kctl, &uc) < 0)
			continue;
		old = uc.value.integer.value[ind];
		if (master->type == SNDRV_CTL_ELEM_TYPE_INTEGER)
			old = old * 256 / (max+1);
		printk("master_get: %s[%d] max=%d: %ld->%d\n", kctl->id.name, 
		       ind, max, ucontrol->value.integer.value[i], old);
		ucontrol->value.integer.value[i] = old;
	}

	return 0;
}

static int master_put(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_kcontrol *kctl;
	struct snd_ctl_elem_value uc;
	struct kctl_master_t *master = KCTL2MAST(kcontrol);
	struct snd_soc_codec *codec = MAST2CODEC(master);

	int i, ind, max, new;

	for (i=0; i<master->count; i++) {
		max = new = 0;
		ind = master->nidx[i];
		if (!master_find(codec, master->names[i], &max, &kctl))
			continue;
		new = ucontrol->value.integer.value[i];
		if (master->type == SNDRV_CTL_ELEM_TYPE_INTEGER)
			new = new * (max+1) / 256;

		if (kctl->get(kctl, &uc) < 0)
			continue;
		printk("master_put: %s[%d] max=%d %ld->%d\n", kctl->id.name, 
		       ind, max, uc.value.integer.value[ind], new);

		uc.value.integer.value[ind] = new;
		if (kctl->put(kctl, &uc) < 0)
			continue;
	}

	return 0;
}

/* Beware : masterconf must be of same type, and have same count as the old 
 * master
 */
void master_change(struct snd_kcontrol *k, struct kctl_master_t *masterconf)
{
	if (masterconf && k)
		masterconf->codec = MAST2CODEC(k->private_data);
	if (k)
		k->private_data = masterconf;
}

struct snd_kcontrol *master_init(struct snd_soc_codec *codec, char *mname,
				 struct kctl_master_t *master)
{
	struct snd_kcontrol_new *kctln= NULL;
	struct snd_kcontrol *kctl = NULL;
	int err;

	kctln = kmalloc(sizeof(struct snd_kcontrol_new), GFP_KERNEL);
	if (!kctln)
		return NULL;

	memset(kctln, 0, sizeof(struct snd_kcontrol_new));
	kctln->iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	kctln->name = mname;
	kctln->info = master_info;
	kctln->get = master_get;
	kctln->put = master_put;
	master->codec = codec;

	kctl = snd_soc_cnew(kctln, master, NULL);

	if ((err = snd_ctl_add(codec->card, kctl)) < 0) {
		kfree(kctln);
		kctl = NULL;
	}

	return kctl;
}

/* MIO Specific */
static struct kctl_master_t miomastervol[] = {
	MVOL2_DEFAULT,				//MIO_AUDIO_OFF
	MVOL2("Headphone Playback Volume", 0,	//MIO_GSM_CALL_AUDIO_HANDSET 
	      "Out3 Playback Volume", 0),
	MVOL2("Headphone Playback Volume", 0,	//MIO_GSM_CALL_AUDIO_HEADSET
	      "Headphone Playback Volume", 1),
	MVOL2("Speaker Playback Volume", 0,	//MIO_GSM_CALL_AUDIO_HANDSFREE
	      "Speaker Playback Volume", 1),
	MVOL2_DEFAULT,				//MIO_GSM_CALL_AUDIO_BLUETOOTH
	MVOL2("Speaker Playback Volume", 0,	//MIO_STEREO_TO_SPEAKER
	      "Speaker Playback Volume", 1),
	MVOL2("Headphone Playback Volume", 0,	//MIO_STEREO_TO_HEADPHONES
	      "Headphone Playback Volume", 1),
	MVOL2_DEFAULT,				//MIO_CAPTURE_HANDSET
	MVOL2_DEFAULT,				//MIO_CAPTURE_HEADSET
	MVOL2_DEFAULT,				//MIO_CAPTURE_BLUETOOTH
};

static struct kctl_master_t miomastermute[] = {
	MMUTE2_DEFAULT,				//MIO_AUDIO_OFF
	MMUTE2("Headphone Playback Switch", 0,	//MIO_GSM_CALL_AUDIO_HANDSET 
	      "Out3 Playback Switch", 0),
	MMUTE2("Headphone Playback Switch", 0,	//MIO_GSM_CALL_AUDIO_HEADSET
	       "Headphone Playback Switch", 1),
	MMUTE2("Speaker Playback Switch", 0,	//MIO_GSM_CALL_AUDIO_HANDSFREE
	       "Speaker Playback Switch", 1),
	MMUTE2_DEFAULT,				//MIO_GSM_CALL_AUDIO_BLUETOOTH
	MMUTE2("Speaker Playback Switch", 0,	//MIO_STEREO_TO_SPEAKER
	       "Speaker Playback Switch", 1),
	MMUTE2("Headphone Playback Switch", 0,	//MIO_STEREO_TO_HEADPHONES
	       "Headphone Playback Switch", 1),
	MMUTE2_DEFAULT,				//MIO_CAPTURE_HANDSET
	MMUTE2_DEFAULT,				//MIO_CAPTURE_HEADSET
	MMUTE2_DEFAULT,				//MIO_CAPTURE_BLUETOOTH
};

static struct snd_kcontrol *miovol, *miomute;

int mioa701_master_init(struct snd_soc_codec *codec)
{
	miovol =  master_init(codec, "Mio Volume", &miomastervol[0]);
	miomute = master_init(codec, "Mio Switch", &miomastermute[0]);

	return 0;
}

void mioa701_master_change(int scenario)
{
	printk(KERN_NOTICE "mioa701_master_change(%d)\n", scenario);
	master_change(miovol, &miomastervol[scenario]);
	master_change(miomute, &miomastermute[scenario]);
}

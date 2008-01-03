/*
 *  Copyright (c) by Jaroslav Kysela <perex@suse.cz>
 *  Universal interface for Audio Codec '97
 *
 *  For more details look to AC '97 component specification revision 2.2
 *  by Intel Corporation (http://developer.intel.com).
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <sound/driver.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include <sound/core.h>
//#include <sound/ac97_codec.h>
#include <sound/asoundef.h>
#include <sound/soc.h>

/*
 * proc interface
 */

#ifdef CONFIG_SND_DEBUG

static struct snd_info_entry *glob_entry;

unsigned short rjk_ac97_read(struct snd_soc_codec *codec, unsigned short reg)
{
        return codec->read(codec, reg);
}

void rjk_ac97_write(struct snd_soc_codec *codec, unsigned short reg, unsigned short val)
{
	codec->write(codec, reg, val);
}


/* direct register write for debugging */
static void rjk_ac97_proc_regs_write(struct snd_info_entry *entry, struct snd_info_buffer *buffer)
{
	struct snd_soc_codec *codec = entry->private_data;
	char line[64];
	unsigned int reg, val;
	while (!snd_info_get_line(buffer, line, sizeof(line))) {
		if (sscanf(line, "%x %x", &reg, &val) != 2)
			continue;
                printk("RJK2: writing ac97 reg %x : %x\n", reg, val);
		/* register must be even */
		if (reg < 0x80 && (reg & 1) == 0 && val <= 0xffff)
			rjk_ac97_write(codec, reg, val);
	}
}
#endif

static void rjk_ac97_proc_regs_read_main(struct snd_soc_codec *codec, struct snd_info_buffer *buffer, int subidx)
{
	int reg, val;

	for (reg = 0; reg < 0x80; reg += 2) {
		val = rjk_ac97_read(codec, reg);
		snd_iprintf(buffer, "%i:%02x = %04x\n", subidx, reg, val);
	}
}

static void rjk_ac97_proc_regs_read(struct snd_info_entry *entry, 
				    struct snd_info_buffer *buffer)
{
	struct snd_soc_codec *codec = entry->private_data;

        rjk_ac97_proc_regs_read_main(codec, buffer, 0);
}

void rjk_ac97_proc_init(struct snd_soc_codec *codec)
{
	struct snd_info_entry *entry;
	char name[32];
	const char *prefix;

	printk("rjk_ac97_proc_init called\n");

	prefix = "ac97";
	sprintf(name, "%s#+regs", prefix);
	if ((entry = snd_info_create_card_entry(codec->card, name, codec->card->proc_root)) != NULL) {
		snd_info_set_text_ops(entry, codec, rjk_ac97_proc_regs_read);
#ifdef CONFIG_SND_DEBUG
		entry->mode |= S_IWUSR;
		entry->c.text.write = rjk_ac97_proc_regs_write;
#endif
		if (snd_info_register(entry) < 0) {
			snd_info_free_entry(entry);
			entry = NULL;
		}
	}
	glob_entry = entry;
}

void rjk_ac97_proc_done(struct snd_ac97 * ac97)
{
	printk("rjk_ac97_proc_done called\n");

	if (glob_entry)
		snd_info_free_entry(glob_entry);
	glob_entry = NULL;
}

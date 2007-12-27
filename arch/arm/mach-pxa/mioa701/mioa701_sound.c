/* Sound interface for the MIO A701
 *
 * 2007-12-26 Robert Jarzmik
 *
 * This code is licenced under the GPLv2.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/audio.h>
#include <asm/arch/ssp.h>

#include "mioa701.h"

static pxa2xx_audio_ops_t mioa701_audio_ops = {};

static struct platform_device mioa701_ac97 = {
	.name 	= "pxa2xx-ac97",
	.id   	= -1,
        .dev    = { .platform_data = &mioa701_audio_ops },
};

static void __init mioa701_init(void)
{
	int ret;

	/* this initialize the PXA AC97 controller */
	pxa_gpio_mode(GPIO89_AC97_SYSCLK_MD);
	platform_device_register(&mioa701_ac97);

	return;
}

static void __exit mioa701_exit(void)
{
	platform_device_unregister(&mioa701_ac97);
}

module_init(mioa701_init);
module_exit(mioa701_exit);

/* Module information */
MODULE_AUTHOR("Robert Jarzmik <rjarzmik@yahoo.fr>");
MODULE_DESCRIPTION("mioa701 sound implementation");
MODULE_LICENSE("GPL");

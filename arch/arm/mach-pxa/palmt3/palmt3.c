/*
 * Hardware definitions for Palm Tungsten T3
 *
 * Based on Palm LD patch
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <asm/arch/hardware.h>
#include <asm/mach/map.h>
#include <asm/domain.h>

#include <linux/device.h>
#include <linux/fb.h>

#include <asm/arch/pxa-dmabounce.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/pxa-regs.h>

#include "../generic.h"

#define DEBUG

static void palm_backlight_power(int on)
{
	/* TODO */
}

static struct pxafb_mach_info palmt3lcd = {
	.pixclock		= 80000,
	.xres			= 320,
	.yres			= 480,
	.bpp			= 16,
	.hsync_len		= 4,
	.left_margin		= 31,
	.right_margin		= 3,
	.vsync_len		= 1,
	.upper_margin		= 8,
	.lower_margin		= 7,
	.sync			= FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,

	.lccr0 = 0x003008F9,
	.lccr3 = 0x03700004,

	.pxafb_backlight_power	= palm_backlight_power,
};

static void __init palm_init(void)
{
	set_pxa_fb_info(&palmt3lcd);
}

MACHINE_START(T3XSCALE, "Palm Tungsten T3")
	/* Maintainer: Vladimir Pouzanov <farcaller@gmail.com> */
	.phys_ram	= 0xa0000000,
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.map_io		= pxa_map_io,
	.init_irq	= pxa_init_irq,
	.timer		= &pxa_timer,
	.init_machine	= palm_init
MACHINE_END

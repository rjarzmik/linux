/*
 * linux/arch/arm/mach-pxa/palm/palm.c
 *
 *  Support for the Intel XScale based Palm PDAs. Only the LifeDrive is
 *  supported at the moment.
 *
 *  Author: Alex Osborne <bobofdoom@gmail.com>
 *
 *  USB stubs based on aximx30.c (Michael Opdenacker)
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fb.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <asm/arch/hardware.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/udc.h>
#include <asm/arch/palmld-gpio.h>

#include "../generic.h"

static void palm_backlight_power(int on)
{
	/**
	  * TODO: check which particular PWM controls the backlight on the LifeDrive
	  * and enable and disable it here. It's PWM1 on the Tungsten T3. Quite
	  * likely to be the same.
	  */
}


/* USB Device Controller */

static int udc_is_connected(void)
{
	return GPLR(GPIO_PALMLD_USB_DETECT) & GPIO_bit(GPIO_PALMLD_USB_DETECT);
}

static void udc_enable(int cmd) 
{
	/**
	  * TODO: find the GPIO line which powers up the USB.
	  */
	switch (cmd)
	{
		case PXA2XX_UDC_CMD_DISCONNECT:
			printk (KERN_NOTICE "USB cmd disconnect\n");
			/* SET_X30_GPIO(USB_PUEN, 0); */
			break;

		case PXA2XX_UDC_CMD_CONNECT:
			printk (KERN_NOTICE "USB cmd connect\n");
			/* SET_X30_GPIO(USB_PUEN, 1); */
			break;
	}
}
static struct pxa2xx_udc_mach_info palmld_udc_mach_info = {
	.udc_is_connected = udc_is_connected,
	.udc_command      = udc_enable,
};


static struct pxafb_mach_info palmld_lcd = {
	/* pixclock is set by lccr3 below */
	.pixclock		= 0,		
	.xres			= 320,
	.yres			= 480,
	.bpp			= 16,
	.hsync_len		= 4,
	.vsync_len		= 1,

	/* fixme: these are the margins PalmOS has set,
	  * 	   they seem to work but could be better.
	  */
	.left_margin		= 31,
	.right_margin		= 3,
	.upper_margin		= 7, //5,
	.lower_margin		= 8, //3,

	.sync			= FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
	
	/* fixme: this is a hack, use constants instead. */
	.lccr0			= 0x7b008f8,
	.lccr3			= 0x4700004,
	
	.pxafb_backlight_power	= palm_backlight_power,
};

static void __init palmld_init(void)
{
	set_pxa_fb_info( &palmld_lcd );
	pxa_set_udc_info( &palmld_udc_mach_info );
}

MACHINE_START(XSCALE_PALMLD, "Palm LifeDrive")
	.phys_ram	= 0xa0000000,
	.phys_io	= 0x40000000,
	.io_pg_offst	= io_p2v(0x40000000),
	.boot_params	= 0xa0000100,
	.map_io 	= pxa_map_io,
	.init_irq	= pxa_init_irq,
	.timer  	= &pxa_timer,
	.init_machine	= palmld_init,
MACHINE_END


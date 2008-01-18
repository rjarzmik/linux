/*
 * Hardware definitions for MIO A701
 *
 * 2008-1-1 Robert Jarzmik
 *          inspired by initial work of Damien Tournoud
 *
 * This code is licenced under the GPLv2.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/corgi_bl.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/gpio_keys.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/gpio.h>

#include <asm/mach-types.h>
#include <asm/mach/irq.h>
#include <asm/mach/arch.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/serial.h>
#include <asm/arch/pxa27x_keyboard.h>
#include <asm/arch/pxafb.h>

#include "../generic.h"
#include "mioa701.h"

#define MIO_SIMPLE_DEV(var, strname, pdata)	\
static struct platform_device var = {		\
        .name   = strname,			\
	.dev = {				\
		.platform_data = pdata,		\
	},					\
};

/* LCD Screen and Backlight */
static void mioa701_backlight_set_intensity(int intensity)
{
	if (intensity > 0) {
		pxa_gpio_mode(GPIO16_PWM0_MD);
		pxa_set_cken(CKEN0_PWM0, 1);
		PWM_CTRL0 = 0;
		PWM_PERVAL0 = 0x3ff;
		PWM_PWDUTY0 = intensity;
	}
	else {
		PWM_CTRL0 = 0;
		PWM_PWDUTY0 = 0x0;
		pxa_set_cken(CKEN0_PWM0, 0);
	}
}

/*
 * LTM0305A776C LCD panel timings
 *
 * see:
 *  - the LTM0305A776C datasheet,
 *  - and the PXA27x Programmers' manual
 */
static struct pxafb_mode_info mioa701_ltm0305a776c = {
	.pixclock		= 220000,
	.xres			= 240,
	.yres			= 320,
	.bpp			= 16,
	.hsync_len		= 4,
	.vsync_len		= 2,
	.left_margin		= 6,
	.right_margin		= 4,
	.upper_margin		= 5,
	.lower_margin		= 3,
	// .sync			= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
};

static struct pxafb_mach_info mioa701_pxafb_info = {
	.modes			= &mioa701_ltm0305a776c,
	.num_modes		= 1,
	.lccr0			= LCCR0_Act,
	.lccr3			= LCCR3_PCP,
};

static struct corgibl_machinfo mioa701_backlight_info = {
	.max_intensity = 0x3ff,
	.default_intensity = 0x3ff,
	.limit_mask = 0x0f,
	.set_bl_intensity = mioa701_backlight_set_intensity,
};

/**
 * Keyboard configuration
 */
static struct pxa27x_keyboard_platform_data mioa701_kbd = {
	.nr_rows = 3,
	.nr_cols = 3,
	.keycodes = {
	{
		/* row 0 */
		KEY_UP,
		KEY_RIGHT,
		-1,		// Media player
	}, {
		/* row 1 */
		KEY_DOWN,
		KEY_ENTER,
		-1,		// GPS
	}, {
		/* row 2 */
		KEY_LEFT,
		-1,		// Phone Green
		-1,		// Camera
	},
	},
	.gpio_modes = {
		100 | GPIO_ALT_FN_1_IN,   // KP_MKIN1<0>
		101 | GPIO_ALT_FN_1_IN,   // KP_MKIN1<1>
		102 | GPIO_ALT_FN_1_IN,   // KP_MKIN1<2>,
		103 | GPIO_ALT_FN_2_OUT,  // KP_MKOUT<0>
		104 | GPIO_ALT_FN_2_OUT,  // KP_MKOUT<1>
		105 | GPIO_ALT_FN_2_OUT,  // KP_MKOUT<2>
	}
};

struct platform_device mioa701_pxa_keyboard = {
	.name   = "pxa27x-keyboard",
	.id     = -1,
	.dev	=  {
		.platform_data	= &mioa701_kbd,
	},
};

/**
 * GPIO Key Configuration
 */
static struct gpio_keys_button mioa701_button_table[] = {
	{KEY_F8, 0 /* yes, this is GPIO<0> ! */, 0},  // red phone key
	{KEY_PAGEUP, 93, 0},                          // volume-up
	{KEY_PAGEDOWN, 94, 0},                        // volume-down
};

static struct gpio_keys_platform_data mioa701_gpio_keys_data = {
	.buttons  = mioa701_button_table,
	.nbuttons = ARRAY_SIZE(mioa701_button_table),
};

/* Bluetooth */
struct platform_pxa_serial_funcs mioa701_bt_funcs;
EXPORT_SYMBOL(mioa701_bt_funcs);

/* Phone */
struct platform_pxa_serial_funcs mioa701_phone_funcs;
EXPORT_SYMBOL(mioa701_phone_funcs);

/* GPS */
struct platform_pxa_serial_funcs mioa701_gps_funcs;
EXPORT_SYMBOL(mioa701_gps_funcs);

/* Devices */
MIO_SIMPLE_DEV(mioa701_backlight, "corgi-bl",      &mioa701_backlight_info)
MIO_SIMPLE_DEV(mioa701_gpio_keys, "gpio-keys",     &mioa701_gpio_keys_data)
MIO_SIMPLE_DEV(mioa701_phone,     "mioa701-phone", &mioa701_phone_funcs)
MIO_SIMPLE_DEV(mioa701_bt,        "mioa701-bt",    &mioa701_bt_funcs)
MIO_SIMPLE_DEV(mioa701_gps,       "mioa701-gps",   &mioa701_gps_funcs)
MIO_SIMPLE_DEV(mioa701_udc,       "mioa701-udc",   NULL)
MIO_SIMPLE_DEV(mioa701_pm,        "mioa701-pm",    NULL)
MIO_SIMPLE_DEV(mioa701_led,       "mioa701-leds",  NULL)

static struct platform_device *devices[] __initdata = {
	&mioa701_pxa_keyboard,
	&mioa701_gpio_keys,
        &mioa701_backlight,
	&mioa701_udc,
        &mioa701_phone,
        &mioa701_bt,
        &mioa701_gps,
        &mioa701_led,
	&mioa701_pm,
};

static void __init mioa701_init(void)
{
	set_pxa_fb_info(&mioa701_pxafb_info);

        platform_add_devices(devices, ARRAY_SIZE(devices));
}

MACHINE_START(MIOA701, "MIO A701")
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= &pxa_map_io,
	.init_irq	= &pxa_init_irq,
	.init_machine	= mioa701_init,
	.timer		= &pxa_timer,
MACHINE_END

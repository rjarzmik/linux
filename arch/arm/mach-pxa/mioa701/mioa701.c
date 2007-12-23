/*
 * Hardware definitions for MIO A701
 *
 * Copyright (c) 2007 Damien Tournoud
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
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
#include <linux/power_supply.h>
#include <linux/wm97xx.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/gpio.h>

#include <asm/mach-types.h>
#include <asm/mach/irq.h>
#include <asm/mach/arch.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/bitfield.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/udc.h>
#include <asm/arch/serial.h>
#include <asm/arch/pxa27x_keyboard.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/mmc.h>
#include <asm/arch/irda.h>
#include <asm/arch/ohci.h>

#include "../generic.h"
#include "mioa701.h"

/**
 * LCD Screen and Backlight
 */

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

static struct platform_device mioa701_backlight = {
	.name = "corgi-bl",
	.id = -1,
	.dev = {
		.platform_data = &mioa701_backlight_info,
	},
};

/**
 * USB Client Controller
 */

static int mioa701_udc_is_connected(void)
{
	return gpio_get_value(MIOA701_USB_DETECT) == 0;
}

static void mioa701_udc_command(int cmd) {
	switch (cmd) {
	case PXA2XX_UDC_CMD_DISCONNECT:
		printk("UDC disconnected.\n");
		break;
	case PXA2XX_UDC_CMD_CONNECT:
		printk("UDC connect.\n");
		break;
	default:
		printk("_udc_control: unknown command (0x%x)!\n", cmd);
		break;
	}
}

static struct pxa2xx_udc_mach_info mioa701_udc_info __initdata = {
	.udc_is_connected 	= mioa701_udc_is_connected,
	.udc_command      	= mioa701_udc_command,
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

static struct platform_device mioa701_gpio_keys = {
	.name = "gpio-keys",
	.dev  = {
		.platform_data = &mioa701_gpio_keys_data,
	},
	.id   = -1,
};

/**
 * SDIO/MMC Card controller
 */
static int mioa701_mci_init(struct device *dev, irq_handler_t detect_int, void *data)
{
	int err;
  
	/*
	* setup GPIO
	*/
	pxa_gpio_mode(92 | GPIO_ALT_FN_1_OUT);    // MMDAT<0>
	pxa_gpio_mode(109 | GPIO_ALT_FN_1_OUT);   // MMDAT<1>
	pxa_gpio_mode(110 | GPIO_ALT_FN_1_OUT);   // MMDAT<2> MMCCS<0>
	pxa_gpio_mode(111 | GPIO_ALT_FN_1_OUT);   // MMDAT<3> MMCCS<1>
	pxa_gpio_mode(112 | GPIO_ALT_FN_1_OUT);   // MMCMD
	pxa_gpio_mode(32 | GPIO_ALT_FN_2_OUT);    // MMCLK

	/* enable RE/FE interrupt on card insertion and removal */
	GRER0 |= 1 << MIOA701_SDIO_INSERT;
	GFER0 |= 1 << MIOA701_SDIO_INSERT;
  
	err = request_irq(gpio_to_irq(MIOA701_SDIO_INSERT), detect_int,
		IRQF_DISABLED | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
		"MMC card detect", data);
	if (err) {
		printk(KERN_ERR "mioa701_mci_init: MMC/SD: can't request MMC card detect IRQ\n");
		return -1;
	}

	return 0;
}

static void mioa701_mci_setpower(struct device *dev, unsigned int vdd)
{
	struct pxamci_platform_data* p_d = dev->platform_data;
	if ((1 << vdd) & p_d->ocr_mask)
		gpio_set_value(MIOA701_SDIO_EN, 1);    // enable SDIO slot power
	else
		gpio_set_value(MIOA701_SDIO_EN, 0);    // disable SDIO slot power
}

static int mioa701_mci_get_ro(struct device *dev)
{
	return gpio_get_value(MIOA701_SDIO_RO);
}

static void mioa701_mci_exit(struct device *dev, void *data)
{
	free_irq(gpio_to_irq(MIOA701_SDIO_INSERT), data);
}

static struct pxamci_platform_data mioa701_mci_info = {
	.ocr_mask = MMC_VDD_32_33 | MMC_VDD_33_34,
	.init     = mioa701_mci_init,
	.get_ro   = mioa701_mci_get_ro,
	.setpower = mioa701_mci_setpower,
	.exit     = mioa701_mci_exit,
};


/* 
 * Bluetooth
 */
static struct mioa701_bt_funcs bt_funcs;

static void
mioa701_bt_configure( int state )
{
	if (bt_funcs.configure != NULL)
		bt_funcs.configure( state );
}

static struct platform_pxa_serial_funcs mioa701_pxa_bt_funcs = {
        .configure = mioa701_bt_configure,
};

static struct platform_device mioa701_bt = {
	.name = "mioa701-bt",
	.id = -1,
	.dev = {
		.platform_data = &bt_funcs,
	},
};

/*
 * Phone
 */
static struct mioa701_phone_funcs phone_funcs;

static void
mioa701_phone_configure( int state )
{
	if (phone_funcs.configure != NULL)
		phone_funcs.configure( state );
}

static struct platform_pxa_serial_funcs mioa701_pxa_phone_funcs = {
        .configure = mioa701_phone_configure,
};

static struct platform_device mioa701_phone = {
	.name = "mioa701-phone",
	.id = -1,
	.dev = {
		.platform_data = &phone_funcs,
	},
};

/*
 * GPS
 */
static struct mioa701_gps_funcs gps_funcs;

static void
mioa701_gps_configure( int state )
{
	if (gps_funcs.configure != NULL)
		gps_funcs.configure( state );
}

static struct platform_pxa_serial_funcs mioa701_pxa_gps_funcs = {
        .configure = mioa701_gps_configure,
};

static struct platform_device mioa701_gps = {
	.name = "mioa701-gps",
	.id = -1,
	.dev = {
		.platform_data = &gps_funcs,
	},
};


static void __init mioa701_map_io(void)
{
	pxa_map_io();

        pxa_set_ffuart_info(&mioa701_pxa_phone_funcs);
	pxa_set_btuart_info(&mioa701_pxa_bt_funcs);
	pxa_set_stuart_info(&mioa701_pxa_gps_funcs);
}

static struct platform_device *devices[] __initdata = {
	&mioa701_pxa_keyboard,
	&mioa701_gpio_keys,
        &mioa701_backlight,
        &mioa701_phone,
        &mioa701_bt,
        &mioa701_gps,
};

static void __init mioa701_init(void)
{
	set_pxa_fb_info(&mioa701_pxafb_info);

	/* XXX: does this turns on USB ? */
	gpio_set_value(MIOA701_USB_EN0, 1);
	gpio_set_value(MIOA701_USB_EN1, 1);

        platform_add_devices(devices, ARRAY_SIZE(devices));

	pxa_set_udc_info(&mioa701_udc_info);
	pxa_set_mci_info(&mioa701_mci_info);
}

MACHINE_START(MIOA701, "MIO A701")
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= &mioa701_map_io,
	.init_irq	= &pxa_init_irq,
	.init_machine	= mioa701_init,
	.timer		= &pxa_timer,
MACHINE_END


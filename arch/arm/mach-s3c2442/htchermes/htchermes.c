/* Code to support the HTC Hermes phone.
 *
 * (c) Copyright 2007 Kevin O'Connor <kevin@koconnor.net>
 *
 * Based on the code by Ben Dooks <ben@simtec.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License.
 *
 */

#include <linux/kernel.h>
#include <asm/mach-types.h> // MACH_TYPE_HTCHERMES
#include <asm/mach/arch.h> // MACHINE_START
#include <asm/mach/map.h> // struct map_desc
#include <linux/platform_device.h> // struct platform_device

#include <linux/mtd/nand.h> // MTD_WRITEABLE
#include <linux/mtd/partitions.h> // struct mtd_partition
#include <asm/arch/nand.h> // struct s3c2410_nand_set

#include <linux/serial_core.h> // for regs-serial.h
#include <asm/arch/regs-serial.h> // struct s3c24xx_uart_clksrc

#include <asm/plat-s3c24xx/devs.h> // s3c_device_*
#include <asm/plat-s3c24xx/cpu.h> // struct s3c24xx_board
#include <asm/plat-s3c24xx/pm.h> // s3c2410_pm_init
#include <asm/arch/udc.h> // S3C2410_UDC_P_ENABLE

#include <asm/arch/gpio.h> // gpio_get_value
#include <linux/input.h> // KEY_*
#include <linux/gpio_keys.h> // struct gpio_keys_platform_data
#include <linux/pda_power.h> // struct pda_power_pdata
#include <linux/leds.h> // struct gpio_led
#include <linux/mfd/htc-egpio.h> // struct htc_egpio_platform_data
#include <linux/mfd/htc-bbkeys.h> // struct htc_bbkeys_platform_data

#include "htchermes-gpio.h" // GPIO_NR_*

// Ughh
#define IRQ_BOARD_START (IRQ_S3C2440_AC97+1)


/****************************************************************
 * EGPIO
 ****************************************************************/

// Location of the egpio chip in physical ram.
enum { EGPIO_BASE = 0x08000000 };

static struct resource egpio_resources[] = {
	[0] = {
		.start  = EGPIO_BASE,
		.end    = EGPIO_BASE + 0x10,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = IRQ_NR_HTCHERMES_EGPIO_IRQ,
		.end    = IRQ_NR_HTCHERMES_EGPIO_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

struct htc_egpio_pinInfo pins[] = {
	// Input pins with IRQs
	{.gpio = EGPIO_NR_HTCHERMES_PWR_IN_PWR
	 , .type = HTC_EGPIO_TYPE_INPUT
	 , .input_irq = EGPIO_NR_HTCHERMES_PWR_IRQ},
	// Output pins that default on
	{.gpio = EGPIO_NR_HTCHERMES_PWR_CHARGE, .type = HTC_EGPIO_TYPE_OUTPUT
	 , .output_initial = 1}, // Disable charger
	{.gpio = HTCHERMES_EGPIO(0, 9), .type = HTC_EGPIO_TYPE_OUTPUT
	 , .output_initial = 1}, // XXX - must be on - reason unknown
};

struct htc_egpio_platform_data egpio_data = {
	.irq_base = IRQ_BOARD_START,
	.gpio_base = HTCHERMES_EGPIO_BASE,
	.nrRegs = 5,
	.ackRegister = 2,
	.pins = pins,
	.nr_pins = ARRAY_SIZE(pins),
};

static struct platform_device egpio = {
	.name = "htc-egpio",
	.id = -1,
	.resource = egpio_resources,
	.num_resources = ARRAY_SIZE(egpio_resources),
	.dev    = {
		.platform_data = &egpio_data,
	},
};

// Compatibility wrappers
#define IRQ_EGPIO(x)   (IRQ_BOARD_START + (x) - HTCHERMES_EGPIO_BASE)


/****************************************************************
 * Front keypad
 ****************************************************************/

#define HTCHERMES_BBKEYS_BASE (HTCHERMES_EGPIO_BASE + GPIO_BASE_INCREMENT)
#define BBKEYS_GPIO(reg,bit) (HTCHERMES_BBKEYS_BASE + 8*(reg) + (bit))

static struct resource bbkeys_resources[] = {
	[0] = {
		.start  = IRQ_NR_HTCHERMES_FK_IRQ,
		.end    = IRQ_NR_HTCHERMES_FK_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct htc_bbkeys_platform_data bbkeys_data = {
	.sda_gpio = GPIO_NR_HTCHERMES_FK_SDA,
	.scl_gpio = GPIO_NR_HTCHERMES_FK_SCL,
	.gpio_base = HTCHERMES_BBKEYS_BASE,

	.ackreg = 3,
	.key1reg = 0,
	.key2reg = 1,
	.buttons = {
		-1,
		KEY_LEFTMETA,  // Windows key
		KEY_OK,        // Ok
		KEY_ENTER,     // Joystick press
		KEY_DOWN,      // Joystick down
		KEY_LEFT,      // Joystick left
		KEY_UP,        // Joystick up
		KEY_RIGHT,     // Joystick right
		-1,
		KEY_RIGHTCTRL, // Right menu
		-1,
		KEY_LEFTCTRL,  // Left menu
		KEY_WWW,       // Browser button
		KEY_EMAIL,     // Mail button
		KEY_PHONE,     // Call button
		KEY_ESC,       // Hangup
	}
};

static struct platform_device bbkeys = {
	.name   = "htc-bbkeys",
	.id     = -1,
	.resource = bbkeys_resources,
	.num_resources = ARRAY_SIZE(bbkeys_resources),
	.dev	=  {
		.platform_data	= &bbkeys_data,
	},
};


/****************************************************************
 * LEDS
 ****************************************************************/

struct gpio_led hermes_leds[] = {
	{ .name = "hermes:vibra", .gpio = GPIO_NR_HTCHERMES_VIBRA },
	{ .name = "hermes:kbd_bl", .gpio = EGPIO_NR_HTCHERMES_KBD_BACKLIGHT },
	{ .name = "hermes:flashlight", .gpio = GPIO_NR_HTCHERMES_FLASHLIGHT },
#if 0
	{ .name = "hermes:phone_bl", .gpio = BBKEYS_GPIO(1, 5) },

	{ .name = "hermesLeft:green", .gpio = BBKEYS_GPIO(2, 2) },
	{ .name = "hermesLeft:blue", .gpio = BBKEYS_GPIO(2, 3) },
	{ .name = "hermesLeft:alter", .gpio = BBKEYS_GPIO(2, 4) },
	{ .name = "hermesRight:green", .gpio = BBKEYS_GPIO(2, 5) },
	{ .name = "hermesRight:red", .gpio = BBKEYS_GPIO(2, 6) },
	{ .name = "hermesRight:alter", .gpio = BBKEYS_GPIO(2, 7) },
#endif
};

static struct gpio_led_platform_data hermes_led_info = {
	.leds = hermes_leds,
	.num_leds = ARRAY_SIZE(hermes_leds),
};

static struct platform_device leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev = {
		.platform_data = &hermes_led_info,
	}
};


/****************************************************************
 * Serial
 ****************************************************************/

static struct s3c24xx_uart_clksrc htchermes_serial_clocks[] = {
	[0] = {
		.name		= "fclk",
		.divisor	= 1,
		.min_baud	= 0,
		.max_baud	= 0,
	},
	[1] = {
		.name		= "hclk",
		.divisor	= 1,
		.min_baud	= 0,
		.max_baud	= 0,
	},
	[2] = {
		.name		= "pclk",
		.divisor	= 1,
		.min_baud	= 0,
		.max_baud	= 0,
	},
};

static struct s3c2410_uartcfg htchermes_uartcfgs[] = {
	/* bluetooth */
	[0] = {
		.hwport	     = 0,
		.flags	     = 0,
		.ucon	     = 0x3fc5,
		.ulcon	     = 0x03,
		.ufcon	     = 0xf1,
		.clocks	     = htchermes_serial_clocks,
		.clocks_size = ARRAY_SIZE(htchermes_serial_clocks),
	},
	/* phone */
	[1] = {
		.hwport	     = 1,
		.flags	     = 0,
		.ucon	     = 0xfc5,
		.ulcon	     = 0x03,
		.ufcon	     = 0xf1,
		.clocks	     = htchermes_serial_clocks,
		.clocks_size = ARRAY_SIZE(htchermes_serial_clocks),
	},
	/* IR port */
	[2] = {
		.hwport	     = 2,
		.uart_flags  = UPF_CONS_FLOW,
		.ucon	     = 0x8fc5,
		.ulcon	     = 0x43,
		.ufcon	     = 0xf1,
		.clocks	     = htchermes_serial_clocks,
		.clocks_size = ARRAY_SIZE(htchermes_serial_clocks),
	}
};


/****************************************************************
 * Nand
 ****************************************************************/

static struct mtd_partition htchermes_nand_part[] = {
	[0] = {
		.name		= "Whole Flash",
		.offset		= 0,
		.size		= MTDPART_SIZ_FULL,
		.mask_flags	= MTD_WRITEABLE,
	}
};

static struct s3c2410_nand_set htchermes_nand_sets[] = {
	[0] = {
		.name		= "Internal",
		.nr_chips	= 1,
		.nr_partitions	= ARRAY_SIZE(htchermes_nand_part),
		.partitions	= htchermes_nand_part,
	},
};

static struct s3c2410_platform_nand htchermes_nand_info = {
	.tacls		= 10,	// 1
	.twrph0		= 30,   // 2
	.twrph1		= 20,    // 1
	.nr_sets	= ARRAY_SIZE(htchermes_nand_sets),
	.sets		= htchermes_nand_sets,
};


/****************************************************************
 * Buttons
 ****************************************************************/

static struct gpio_keys_button htchermes_gpio_keys_table[] = {
	{KEY_POWER,	S3C2410_GPF0,	1,	"Power button"},
	{KEY_CAMERA,	S3C2410_GPF4,	1,	"Camera button"},
	{KEY_SETUP,	S3C2410_GPG3,	1,	"Comm mgr button"},
	{KEY_ENTER,	S3C2410_GPG6,	1,	"Jog wheel click"},
	{KEY_OK,	S3C2410_GPG1,	1,	"Side ok button"},
	{KEY_AUDIO,	S3C2410_GPG5,	1,	"PTT button"},
};

static struct gpio_keys_platform_data htchermes_gpio_keys_data = {
	.buttons	= htchermes_gpio_keys_table,
	.nbuttons	= ARRAY_SIZE(htchermes_gpio_keys_table),
};

static struct platform_device gpiokeys = {
	.name		= "gpio-keys",
	.dev = { .platform_data = &htchermes_gpio_keys_data, }
};


/****************************************************************
 * LCD
 ****************************************************************/

static struct platform_device htchermes_lcd = {
	.name = "vsfb",
	.id = -1,
	.dev = {
		.platform_data = NULL,
	},
};


/****************************************************************
 * Power management
 ****************************************************************/

static int ac_on(void)
{
	int haspower = !gpio_get_value(EGPIO_NR_HTCHERMES_PWR_IN_PWR);
	int hashigh = !gpio_get_value(EGPIO_NR_HTCHERMES_PWR_IN_HIGHPWR);
	return haspower && hashigh;
}

static int usb_on(void)
{
	int haspower = !gpio_get_value(EGPIO_NR_HTCHERMES_PWR_IN_PWR);
	int hashigh = !gpio_get_value(EGPIO_NR_HTCHERMES_PWR_IN_HIGHPWR);
	return haspower && !hashigh;
}

static struct pda_power_pdata power_pdata = {
	.is_ac_online	= ac_on,
	.is_usb_online	= usb_on,
};

static struct resource power_resources[] = {
	[0] = {
		.name	= "ac",
		.start	= IRQ_EGPIO(EGPIO_NR_HTCHERMES_PWR_IN_PWR),
		.end	= IRQ_EGPIO(EGPIO_NR_HTCHERMES_PWR_IN_PWR),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device power_dev = {
	.name		= "pda-power",
	.id		= -1,
	.resource	= power_resources,
	.num_resources	= ARRAY_SIZE(power_resources),
	.dev = {
		.platform_data	= &power_pdata,
	},
};


/****************************************************************
 * USB client controller
 ****************************************************************/

static void udc_command(enum s3c2410_udc_cmd_e cmd)
{
	switch (cmd) {
	case S3C2410_UDC_P_ENABLE:
		printk(KERN_NOTICE "USB cmd disconnect\n");
		gpio_set_value(GPIO_NR_HTCHERMES_USB_PUEN, 1);
		break;
	case S3C2410_UDC_P_DISABLE:
		printk(KERN_NOTICE "USB cmd connect\n");
		gpio_set_value(GPIO_NR_HTCHERMES_USB_PUEN, 0);
		break;
	default:
		break;
	}
}

static struct s3c2410_udc_mach_info udc_cfg = {
	.udc_command      = udc_command,
};


/****************************************************************
 * Init
 ****************************************************************/

static struct platform_device *htchermes_devices[] __initdata = {
	&s3c_device_usb,
	&s3c_device_wdt,
	&s3c_device_i2c,
	&s3c_device_iis,
//	&s3c_device_nand,
	&s3c_device_rtc,
	&s3c_device_usbgadget,
	&s3c_device_sdi,
	&egpio,
//	&bbkeys,
//	&htchermes_lcd,
	&leds,
	&gpiokeys,
	&power_dev,
};

static struct map_desc htchermes_iodesc[] __initdata = {
};

static struct s3c24xx_board htchermes_board __initdata = {
	.devices       = htchermes_devices,
	.devices_count = ARRAY_SIZE(htchermes_devices)
};

static void __init htchermes_map_io(void)
{
//	s3c_device_nand.dev.platform_data = &htchermes_nand_info;

	s3c24xx_init_io(htchermes_iodesc, ARRAY_SIZE(htchermes_iodesc));
	s3c24xx_init_clocks(16934000);
	s3c24xx_init_uarts(htchermes_uartcfgs, ARRAY_SIZE(htchermes_uartcfgs));
	s3c24xx_set_board(&htchermes_board);
}

static void __init htchermes_init_machine(void)
{
	s3c24xx_udc_set_platdata(&udc_cfg);
	s3c2410_pm_init();
}


MACHINE_START(HTCHERMES, "HTC Hermes")
	.phys_io	= S3C2410_PA_UART,
	.io_pg_offst	= (((u32)S3C24XX_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S3C2410_SDRAM_PA + 0x100,
	.map_io		= htchermes_map_io,
	.init_irq	= s3c24xx_init_irq,
	.init_machine	= htchermes_init_machine,
	.timer		= &s3c24xx_timer,
MACHINE_END

/* Phone interface driver for Sagem XS200 on MIO A701
 *
 * 2007-12-12 Robert Jarzmik
 *
 * This code is licenced under the GPLv2.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <asm/gpio.h>
#include <asm/hardware.h>
#include <asm/arch/serial.h>

#include "mioa701.h"

extern struct platform_pxa_serial_funcs mioa701_phone_funcs;

static void mioa701_phone_configure(int state)
{
	int tries;

	//printk( KERN_NOTICE "mioa701 configure phone: %d\n", state );
	switch (state) {
	
	case PXA_UART_CFG_PRE_STARTUP:
		break;

	case PXA_UART_CFG_POST_STARTUP:
		/* pre-serial-up hardware configuration */
		gpio_set_value(MIO_GPIO_GSM_UNKNOWN2, 1);
		gpio_set_value(MIO_GPIO_GSM_UNKNOWN1, 1);
		gpio_set_value(MIO_GPIO_GSM_PREP, 1);
		gpio_set_value(MIO_GPIO_GSM_OFF, 0);
		msleep(100);
		gpio_set_value(MIO_GPIO_GSM_PREP, 0);
		msleep(10);
		gpio_set_value(MIO_GPIO_GSM_PREP, 1);

		tries = 0;
		do {
			msleep(10);
		} while ( (FFMSR & MSR_CTS) && (tries++ < 300) );
		if (tries >= 300) {
			printk("MioA701 phone: expect garbage at GSM start.\n");
		} else {
			printk("MioA701 phone: GSM turned on.\n");
		}
		try_module_get(THIS_MODULE);
		break;

	case PXA_UART_CFG_PRE_SHUTDOWN:
		gpio_set_value(MIO_GPIO_GSM_UNKNOWN1, 1);
		msleep(1);
		gpio_set_value(MIO_GPIO_GSM_PREP, 1);
		msleep(1);
		gpio_set_value(MIO_GPIO_GSM_OFF, 1);
		gpio_set_value(MIO_GPIO_GSM_UNKNOWN1, 0);
		msleep(1);
		gpio_set_value(MIO_GPIO_GSM_UNKNOWN1, 1);
		gpio_set_value(MIO_GPIO_GSM_UNKNOWN2, 0);
		msleep(1);
		gpio_set_value(MIO_GPIO_GSM_UNKNOWN2, 1);
		printk("MioA701 phone: GSM turned off.\n");
		break;

	case PXA_UART_CFG_POST_SHUTDOWN:
		module_put(THIS_MODULE);

	default:
		break;
	}
}


static int mioa701_phone_probe(struct platform_device *dev)
{
	/* configure phone GPIOS */
	pxa_gpio_mode(MIO_GPIO_GSM_OFF | GPIO_OUT | GPIO_DFLT_LOW);
	pxa_gpio_mode(MIO_GPIO_GSM_PREP | GPIO_OUT);
	pxa_gpio_mode(MIO_GPIO_GSM_UNKNOWN1 | GPIO_OUT);
	pxa_gpio_mode(MIO_GPIO_GSM_UNKNOWN2 | GPIO_OUT);
	pxa_gpio_mode(MIO_GPIO_GSM_EVENT | GPIO_IN);

	/* configure phone UART */
	pxa_gpio_mode(MIO_GPIO_GSM_RXD_MD);
	pxa_gpio_mode(MIO_GPIO_GSM_CTS_MD);
	pxa_gpio_mode(MIO_GPIO_GSM_DCD_MD);
	pxa_gpio_mode(MIO_GPIO_GSM_DSR_MD);
	pxa_gpio_mode(MIO_GPIO_GSM_TXD_MD);
	pxa_gpio_mode(MIO_GPIO_GSM_DTR_MD);
	pxa_gpio_mode(MIO_GPIO_GSM_RTS_MD);

	return 0;
}

static int
mioa701_phone_remove(struct platform_device *dev)
{
	return 0;
}

static struct platform_driver phone_driver = {
	.driver	  = {
		.name	  = "mioa701-phone",
	},
	.probe	  = mioa701_phone_probe,
	.remove	  = mioa701_phone_remove,
};

static int __init mioa701_phone_init( void )
{
	printk(KERN_NOTICE "mioa701 Phone Driver\n");
	mioa701_phone_funcs.configure = mioa701_phone_configure;
	pxa_set_ffuart_info(&mioa701_phone_funcs);
	
	return platform_driver_register( &phone_driver );
}

static void __exit mioa701_phone_exit( void )
{
	pxa_set_ffuart_info(NULL);
	mioa701_phone_funcs.configure = NULL;
	platform_driver_unregister(&phone_driver);
}

module_init(mioa701_phone_init);
module_exit(mioa701_phone_exit);

MODULE_AUTHOR("Robert Jarzmik");
MODULE_DESCRIPTION("MIO A701 Phone Support Driver");
MODULE_LICENSE("GPL");

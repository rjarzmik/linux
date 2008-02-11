/* Bluetooth interface driver for TI BRF6150 on MIO A701
 *
 * 2007-12-12 Robert Jarzmik
 *
 * This code is licenced under the GPLv2.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/leds.h>

#include <asm/gpio.h>
#include <asm/hardware.h>
#include <asm/arch/serial.h>
#include "mioa701.h"

extern struct platform_pxa_serial_funcs mioa701_bt_funcs;

static void mioa701_bt_configure(int state)
{
	int tries;

	// printk( KERN_NOTICE "mioa701 configure bluetooth: %d\n", state );
	switch (state) {
	
	case PXA_UART_CFG_PRE_STARTUP:
		break;

	case PXA_UART_CFG_POST_STARTUP:
		/* pre-serial-up hardware configuration */
		gpio_set_value(MIO_GPIO_BT_ON, 1);

		/*
		 * BRF6150's RTS goes low when firmware is ready
		 * so check for CTS=1 (nCTS=0 -> CTS=1). Typical 150ms
		 */
		tries = 0;
		do {
			msleep(10);
		} while ((BTMSR & MSR_CTS) == 0 && tries++ < 50);
		try_module_get(THIS_MODULE);
		break;

	case PXA_UART_CFG_PRE_SHUTDOWN:
		gpio_set_value(MIO_GPIO_BT_ON, 0);
		break;

	case PXA_UART_CFG_POST_SHUTDOWN:
		module_put(THIS_MODULE);
	default:
		break;
	}
}


static int mioa701_bt_probe(struct platform_device *pdev)
{
	/* configure bluetooth UART */
	pxa_gpio_mode(MIO_GPIO_BT_RXD_MD);
	pxa_gpio_mode(MIO_GPIO_BT_TXD_MD);
	pxa_gpio_mode(MIO_GPIO_BT_CTS_MD);
	pxa_gpio_mode(MIO_GPIO_BT_RTS_MD);
	pxa_gpio_mode(MIO_GPIO_BT_ON | GPIO_OUT);
	return 0;
}

static int mioa701_bt_remove(struct platform_device *pdev)
{
	return 0;
}

static int mioa701_bt_suspend(struct platform_device *dev, pm_message_t state)
{
	PGSR1 |= GPIO_bit(MIO_GPIO_BT_RXD_MD) | GPIO_bit(MIO_GPIO_BT_TXD_MD)
		| GPIO_bit(MIO_GPIO_BT_CTS_MD);
	PGSR2 |= GPIO_bit(MIO_GPIO_BT_ON);
	return 0;
}

static struct platform_driver bt_driver = {
	.driver = {
		.name	  = "mioa701-bt",
	},
	.probe	  = mioa701_bt_probe,
	.remove	  = mioa701_bt_remove,
	.suspend  = mioa701_bt_suspend,
};

static int __init mioa701_bt_init(void)
{
	printk(KERN_NOTICE "mioa701 Bluetooth Driver\n");
	mioa701_bt_funcs.configure = mioa701_bt_configure;
	pxa_set_btuart_info(&mioa701_bt_funcs);

	return platform_driver_register( &bt_driver );
}

static void __exit mioa701_bt_exit(void)
{
	pxa_set_btuart_info(NULL);
	mioa701_bt_funcs.configure = NULL;
	platform_driver_unregister( &bt_driver );
}

module_init(mioa701_bt_init);
module_exit(mioa701_bt_exit);

MODULE_AUTHOR("Robert Jarzmik");
MODULE_DESCRIPTION("mioa701 Bluetooth Support Driver");
MODULE_LICENSE("GPL");

/* Bluetooth interface driver for TI BRF6150 on MIO A701
 *
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


static void
mioa701_bt_configure( int state )
{
	int tries;

	// printk( KERN_NOTICE "mioa701 configure bluetooth: %d\n", state );
	switch (state) {
	
	case PXA_UART_CFG_PRE_STARTUP:
		break;

	case PXA_UART_CFG_POST_STARTUP:
		/* pre-serial-up hardware configuration */
		gpio_set_value(GPIO_NR_MIOA701_BT_ON, 1);
		gpio_set_value(GPIO_NR_MIOA701_BT_RESET, 1);
		mdelay(1);
		gpio_set_value(GPIO_NR_MIOA701_BT_RESET, 0);

		/*
		 * BRF6150's RTS goes low when firmware is ready
		 * so check for CTS=1 (nCTS=0 -> CTS=1). Typical 150ms
		 */
		tries = 0;
		do {
			mdelay(10);
		} while ((BTMSR & MSR_CTS) == 0 && tries++ < 50);
		break;

	case PXA_UART_CFG_PRE_SHUTDOWN:
		gpio_set_value(GPIO_NR_MIOA701_BT_ON, 0);
		gpio_set_value(GPIO_NR_MIOA701_BT_RESET, 0);
		mdelay(1);
		gpio_set_value(GPIO_NR_MIOA701_BT_RESET, 1);
		break;

	default:
		break;
	}
}


static int
mioa701_bt_probe( struct platform_device *pdev )
{
	struct mioa701_bt_funcs *funcs = pdev->dev.platform_data;

	/* configure bluetooth UART */
	pxa_gpio_mode( GPIO_NR_MIOA701_BT_RXD_MD );
	pxa_gpio_mode( GPIO_NR_MIOA701_BT_TXD_MD );
	pxa_gpio_mode( GPIO_NR_MIOA701_BT_UART_CTS_MD );
	pxa_gpio_mode( GPIO_NR_MIOA701_BT_UART_RTS_MD );

	funcs->configure = mioa701_bt_configure;

	return 0;
}

static int
mioa701_bt_remove( struct platform_device *pdev )
{
	struct mioa701_bt_funcs *funcs = pdev->dev.platform_data;

	funcs->configure = NULL;

	return 0;
}

static struct platform_driver bt_driver = {
	.driver = {
		.name     = "mioa701-bt",
	},
	.probe    = mioa701_bt_probe,
	.remove   = mioa701_bt_remove,
};

static int __init
mioa701_bt_init( void )
{
	printk(KERN_NOTICE "mioa701 Bluetooth Driver\n");
	return platform_driver_register( &bt_driver );
}

static void __exit
mioa701_bt_exit( void )
{
	platform_driver_unregister( &bt_driver );
}

module_init( mioa701_bt_init );
module_exit( mioa701_bt_exit );

MODULE_AUTHOR("Robert Jarzmik");
MODULE_DESCRIPTION("mioa701 Bluetooth Support Driver");
MODULE_LICENSE("GPL");

/* USB UDC interface enabler from MIO A701
 *
 * 2007-1-12 Robert Jarzmik
 *
 * This code is licenced under the GPLv2.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/udc.h>
#include <asm/gpio.h>

#include "mioa701.h"

static void mioa701_udc_command(int cmd)
{
	switch (cmd) {
	case PXA2XX_UDC_CMD_DISCONNECT:
		printk("UDC disconnected.\n");
		break;
	case PXA2XX_UDC_CMD_CONNECT:
		printk("UDC connect.\n");
		break;
	default:
		printk("udc_control: unknown command (0x%x)!\n", cmd);
		break;
	}
}

static int mioa701_udc_is_connected(void)
{
	return gpio_get_value(MIO_GPIO_USB_DETECT) == 0;
}

static struct pxa2xx_udc_mach_info mioa701_udc_info __initdata = {
	.udc_is_connected = mioa701_udc_is_connected,
	.udc_command      = mioa701_udc_command,
};

static struct pxa2xx_udc_mach_info empty_udc_info __initdata;

static int mioa701_udc_probe(struct platform_device * dev)
{
	pxa_set_udc_info(&mioa701_udc_info);
	return 0;
}

static int mioa701_udc_remove(struct platform_device * dev)
{
	pxa_set_udc_info(&empty_udc_info);
	return 0;
}

static struct platform_driver mioa701_udc_driver = {
	.driver	  = {
		.name     = "mioa701_udc",
	},
	.probe    = mioa701_udc_probe,
	.remove   = mioa701_udc_remove,
};

static int __init mioa701_udc_init(void)
{
	return platform_driver_register(&mioa701_udc_driver);
}

static void __exit mioa701_udc_exit( void )
{
        platform_driver_unregister(&mioa701_udc_driver);
        return;
}


module_init(mioa701_udc_init);
module_exit(mioa701_udc_exit);
MODULE_AUTHOR("Robert Jarzmik");
MODULE_DESCRIPTION("MIO A701 UDC Support Driver");
MODULE_LICENSE("GPL");

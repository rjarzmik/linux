/* Phone interface driver for GPS SIRF-III
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

#define OUTLO (GPIO_OUT | GPIO_DFLT_LOW)
#define OUTHI (GPIO_OUT | GPIO_DFLT_HIGH)

extern struct platform_pxa_serial_funcs mioa701_gps_funcs;

void gps_init_gpios()
{
	/* configure SIRF.III GPIOS */
	pxa_gpio_mode(MIO_GPIO_GPS_UNKNOWN1 | OUTHI);
	pxa_gpio_mode(MIO_GPIO_GPS_UNKNOWN2 | OUTLO);
	pxa_gpio_mode(MIO_GPIO_GPS_UNKNOWN3 | OUTLO);
	pxa_gpio_mode(MIO_GPIO_GPS_ON       | OUTLO);
	pxa_gpio_mode(MIO_GPIO_GPS_RESET    | OUTLO);
	/* configure gps UART */
	pxa_gpio_mode(MIO_GPIO_GPS_RXD_MD);
	pxa_gpio_mode(MIO_GPIO_GPS_TXD_MD);
}

void gps_on()
{
	gpio_set_value(MIO_GPIO_GPS_ON, 1);
	msleep(200);
	gpio_set_value(MIO_GPIO_GPS_RESET, 1);
	printk("MioA701 gps: GPS turned on.\n");
}

void gps_off()
{
	gpio_set_value(MIO_GPIO_GPS_ON, 0);
	gpio_set_value(MIO_GPIO_GPS_RESET, 0);
	printk("MioA701 gps: GPS turned off.\n");
}

void gps_reset()
{
	gps_off();
	gps_on();
}

static void mioa701_gps_configure(int state)
{
	//printk( KERN_NOTICE "mioa701 configure gps: %d\n", state );
	switch (state) {
	
	case PXA_UART_CFG_PRE_STARTUP:
		break;

	case PXA_UART_CFG_POST_STARTUP:
		/* pre-serial-up hardware configuration */
                try_module_get(THIS_MODULE);
		break;

	case PXA_UART_CFG_PRE_SHUTDOWN:
		break;

	case PXA_UART_CFG_POST_SHUTDOWN:
                module_put(THIS_MODULE);

	default:
		break;
	}
}


static int mioa701_gps_probe(struct platform_device *dev)
{
	gps_init_gpios();
	gps_on();

	return 0;
}

static int mioa701_gps_remove(struct platform_device *dev)
{
	gps_off();

	return 0;
}

static int mioa701_gps_suspend(struct platform_device *dev, pm_message_t state)
{
	PGSR0 |= GPIO_bit(MIO_GPIO_GPS_UNKNOWN1);
	PGSR1 |= GPIO_bit(MIO_GPIO_GPS_RXD_MD);

	return 0;
}

static struct platform_driver gps_driver = {
	.driver   = {
		.name     = "mioa701-gps",
	},
	.probe    = mioa701_gps_probe,
	.remove   = mioa701_gps_remove,
	.suspend  = mioa701_gps_suspend,
};

static int __init mioa701_gps_init(void)
{
	printk(KERN_NOTICE "mioa701 Gps Driver\n");
	mioa701_gps_funcs.configure = mioa701_gps_configure;
	pxa_set_stuart_info(&mioa701_gps_funcs);
	return platform_driver_register( &gps_driver );
}

static void __exit mioa701_gps_exit(void)
{
	pxa_set_stuart_info(NULL);
	mioa701_gps_funcs.configure = NULL;
	platform_driver_unregister( &gps_driver );
}

module_init(mioa701_gps_init);
module_exit(mioa701_gps_exit);

MODULE_AUTHOR("Robert Jarzmik");
MODULE_DESCRIPTION("MIO A701 GPS Support Driver");
MODULE_LICENSE("GPL");

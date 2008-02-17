/* USB UDC interface enabler from MIO A701
 *
 * 2007-1-12 Robert Jarzmik
 *
 * This code is licenced under the GPLv2.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/input.h>
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

static inline void input_report_misc(struct input_dev *dev, unsigned int code, int value)
{
	input_event(dev, EV_MSC, code, !!value);
}

static irqreturn_t detect_usb_power(int irq, void *dev_id)
{
	int power =  mioa701_udc_is_connected();

	if (mioa701_evdev) {
		input_report_misc(mioa701_evdev, MSC_SERIAL, power);
		input_sync(mioa701_evdev);
	}

	return IRQ_HANDLED;
}

static int mioa701_udc_probe(struct platform_device * dev)
{
	int irq = gpio_to_irq(MIO_GPIO_USB_DETECT);
	int err;

	pxa_set_udc_info(&mioa701_udc_info);
	pxa_gpio_mode(MIO_GPIO_USB_DETECT | GPIO_IN);
	err = request_irq(irq, detect_usb_power,
                          IRQF_TRIGGER_RISING
			  | IRQF_TRIGGER_FALLING,
                          "USB power detect", &mioa701_udc_info);
	set_irq_type(irq, IRQ_TYPE_EDGE_BOTH);

	return err;
}

static int mioa701_udc_remove(struct platform_device * dev)
{
	int irq = gpio_to_irq(MIO_GPIO_USB_DETECT);

	pxa_set_udc_info(&empty_udc_info);
	free_irq(irq, &mioa701_udc_info);
	return 0;
}

static int mioa701_udc_suspend(struct platform_device *dev, pm_message_t state)
{
	PWER |= GPIO_bit(MIO_GPIO_USB_DETECT);
	PFER |= GPIO_bit(MIO_GPIO_USB_DETECT);
	PRER |= GPIO_bit(MIO_GPIO_USB_DETECT);
	return 0;
}

static struct platform_driver mioa701_udc_driver = {
	.driver	  = {
		.name     = "mioa701-udc",
	},
	.probe    = mioa701_udc_probe,
	.remove   = mioa701_udc_remove,
	.suspend  = mioa701_udc_suspend,
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

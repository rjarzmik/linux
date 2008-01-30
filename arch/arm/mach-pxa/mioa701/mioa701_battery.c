/* Battery driver from MIO A701
 *
 * 2007-1-12 Robert Jarzmik
 *
 * This code is licenced under the GPLv2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pda_power.h>
#include <linux/power_supply.h>
#include <asm/gpio.h>
#include <linux/wm97xx.h>

#include "mioa701.h"

static struct wm97xx *battery_wm;

static int is_usb_connected(void)
{
	return gpio_get_value(MIO_GPIO_USB_DETECT) == 0;
}

static struct pda_power_pdata power_pdata = {
	.is_usb_online	= is_usb_connected,
};

static struct resource power_resources[] = {
	[0] = {
		.name	= "usb",
		.start	= gpio_to_irq(MIO_GPIO_USB_DETECT),
		.end	= gpio_to_irq(MIO_GPIO_USB_DETECT),
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE | IORESOURCE_IRQ_LOWEDGE,
	},
};

static struct platform_device power_dev = 
{
	.name		= "pda-power",
	.id		= -1,
	.resource	= power_resources,
	.num_resources	= ARRAY_SIZE(power_resources),
	.dev = 
	 {
		.platform_data	= &power_pdata,
	 },
};

static enum power_supply_property battery_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static int get_battery_voltage(void)
{
	int adc = -1;

	if (battery_wm)
		adc = wm97xx_read_aux_adc(battery_wm, WM97XX_AUX_ID1);
	return adc;
}

static int get_property(struct power_supply *b,
			enum power_supply_property psp,
			union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = 0xfd0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = 0xc00;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = get_battery_voltage();
		break;
	default:
		val->intval = -1;
		break;
	}
	return 0;
};

static struct power_supply battery_ps = {
	.name = "battery",
	.get_property = get_property,
	.properties = battery_props,
	.num_properties = ARRAY_SIZE(battery_props),
};

static struct wm97xx_mach_ops battery_mach_ops;

static int battery_probe(struct device *dev)
{
	struct wm97xx *wm = dev->driver_data;

	battery_wm = wm;
	printk(KERN_NOTICE "wm97xx-battery: mioa701 battery probed\n");
	return wm97xx_register_mach_ops (wm, &battery_mach_ops);
}

static int battery_remove(struct device *dev)
{
	struct wm97xx *wm = dev->driver_data;

	battery_wm = NULL;
	wm97xx_unregister_mach_ops(wm);
	return 0;
}

static struct device_driver mioa701_battery_driver = {
	.name = "wm97xx-battery",
	.bus = &wm97xx_bus_type,
	.owner = THIS_MODULE,
	.probe = battery_probe,
	.remove = battery_remove
};

static int __init mioa701_battery_init(void)
{
	int ret;

	if ((ret = driver_register(&mioa701_battery_driver)))
		printk(KERN_ERR "Couldn't register mioa701 battery driver\n");
	if (!ret)
		ret = power_supply_register(NULL, &battery_ps);
	if (ret)
		printk(KERN_ERR "Could not register mioa701 battery class\n");
	if (!ret)
		ret = platform_device_register(&power_dev);
	if (!ret)
		printk(KERN_NOTICE "mioa701 Battery driver loaded\n");
	return ret;
}

static void __exit mioa701_battery_exit(void)
{
	driver_unregister(&mioa701_battery_driver);
	power_supply_unregister(&battery_ps);
	platform_device_unregister(&power_dev);
	printk(KERN_NOTICE "mioa701 Battery driver unloaded\n");
}

module_init(mioa701_battery_init);
module_exit(mioa701_battery_exit);

MODULE_AUTHOR("Robert Jarzmik");
MODULE_DESCRIPTION("MIO A701 Battery Support Driver");
MODULE_LICENSE("GPL");

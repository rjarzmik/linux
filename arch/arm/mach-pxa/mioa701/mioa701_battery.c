/* Battery driver from MIO A701
 *
 * 2007-1-12 Robert Jarzmik
 *
 * This code is licenced under the GPLv2.
 *
 * The battery voltage is mesured on the wm9713 chip. The range mesured is
 * beetwen 3100 mV and 3810 mV.
 * The observed voltage curve is approximated by 2 linear functions :
 *   - V(in mV) = -t  (in mins) + 3810 when 0 <= t < 270 mins
 *   - V(in mV) = -5*t(in mins) + 4800 when 270 mins <= t < 340 mins
 * According charge is between 0 and 100, and charge should be linear regarding
 * time, discharging gives : C = 100 * (1 - t/240)
 * Applying above linear functions, this gives :
 *   - C = 100 when V >= 3810
 *   - C = 100 * (1 - (3810 - V)/240) when 3450 <= V < 3810
 *   - C = 100 * (1 - (4800-V)/5/240) when 3100 <= V < 3450
 *   - C = 0 when V < 3100
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pda_power.h>
#include <linux/power_supply.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <asm/gpio.h>
#include <linux/wm97xx.h>

#include "mioa701.h"

static struct wm97xx *battery_wm;
static struct power_supply *mio_power_supply;

static int is_usb_connected(void)
{
	return gpio_get_value(MIO_GPIO_USB_DETECT) == 0;
}

static char *supplicants[] = {
	"mioa701_battery"
};

static struct pda_power_pdata power_pdata = {
	.is_usb_online	= is_usb_connected,
	.is_ac_online	= is_usb_connected,
	.supplied_to = supplicants,
	.num_supplicants = ARRAY_SIZE(supplicants),
};

static struct resource power_resources[] = {
	[0] = {
		.name	= "ac",
		.start	= gpio_to_irq(MIO_GPIO_USB_DETECT),
		.end	= gpio_to_irq(MIO_GPIO_USB_DETECT),
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE | IORESOURCE_IRQ_LOWEDGE,
	},
	[1] = {
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
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,	// Necessary for apm !!!
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_EMPTY,
	POWER_SUPPLY_PROP_CHARGE_NOW,
};

/* Remember formulas :
 *   - C = 100 when V >= 3810
 *   - C = 100 * (1 - (3810 - V)/240) when 3450 <= V < 3810
 *   - C = 100 * (1 - (4800-V)/5/240) when 3100 <= V < 3450
 *   - C = 0 when V < 3100
 */
static int remain_charge(int voltage)
{
	int cap = -1;

	if (voltage < 3100)
		cap = 0;
	else if (voltage > 3810)
		cap = 100;
	else if (voltage <3450)
		cap = 100 - (4800-voltage)/(5*240/100);
	else
		cap = 100 - (3810-voltage)*100/240;
	return cap;
}


static int get_battery_voltage(void)
{
	int adc = -1;

	if (battery_wm)
		adc = wm97xx_read_aux_adc(battery_wm, WM97XX_AUX_ID1);
	return adc;
}

static int get_battery_status(struct power_supply *b)
{
	int status;

	if (is_usb_connected())
		status = POWER_SUPPLY_STATUS_CHARGING;
	else
		status = POWER_SUPPLY_STATUS_DISCHARGING;

	return status;
}

static int get_property(struct power_supply *b,
			enum power_supply_property psp,
			union power_supply_propval *val)
{
	int ret = 0;

	if (!mio_power_supply)
		mio_power_supply = b;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = get_battery_status(b);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = 0xfd0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = 0xc00;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = get_battery_voltage();
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = 100;
		break;
	case POWER_SUPPLY_PROP_CHARGE_EMPTY:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = remain_charge(get_battery_voltage());
		break;
	default:
		val->intval = -1;
		ret = -1;
	}

	return ret;
};

static struct power_supply battery_ps = {
	.name = "mioa701_battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
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
	mio_power_supply = NULL;
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

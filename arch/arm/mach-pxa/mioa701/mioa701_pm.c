/* Power managment interface from MIO A701
 *
 * 2007-12-12 Robert Jarzmik
 *
 * This code is licenced under the GPLv2.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/pda_power.h>

#include <asm/gpio.h>
#include <asm/hardware.h>
#include <asm/arch/serial.h>

#include "mioa701.h"


static void mioa701_release(struct device *dev)
{
        return;
}

static int mioa701_is_ac_online(void)
{
        return 0;
}

static int mioa701_is_usb_online(void)
{
        return 0;
}

static void mioa701_set_charge(int flags)
{
        return;
}

static char *mioa701_supplicants[] = {
        "ds2760-battery.0", "backup-battery"
};

static struct pda_power_pdata mioa701_power_pdata = {
        .is_ac_online = mioa701_is_ac_online,
        .is_usb_online = mioa701_is_usb_online,
        .set_charge = mioa701_set_charge,
        .supplied_to = mioa701_supplicants,
        .num_supplicants = ARRAY_SIZE(mioa701_supplicants),
};

static struct resource mioa701_power_resourses[] = {
        {},
/*                .name = "ac",
                .flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE |
                IORESOURCE_IRQ_LOWEDGE, */
};

static struct platform_device mioa701_power_pdev = {
        .name = "pda-power",
        .id = -1,
        .resource = mioa701_power_resourses,
        .num_resources = ARRAY_SIZE(mioa701_power_resourses),
        .dev = {
                .platform_data = &mioa701_power_pdata,
                .release = mioa701_release,
        },
};

static int __init
mioa701_pm_init( void )
{
        int ret;
        ret = platform_device_register(&mioa701_power_pdev);
        if (ret)
                printk(KERN_NOTICE ": registration failed\n");

        return ret;
}

static void __exit
mioa701_pm_exit( void )
{
        platform_device_unregister(&mioa701_power_pdev);
        return;
}

module_init( mioa701_pm_init );
module_exit( mioa701_pm_exit );

MODULE_AUTHOR("Robert Jarzmik");
MODULE_DESCRIPTION("MIO A701 Power Managment Support Driver");
MODULE_LICENSE("GPL");

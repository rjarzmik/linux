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
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxa-pm_ll.h>

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

static int mioa701_pm_probe(struct platform_device *dev)
{
	//pxa_gpio_mode(18 | GPIO_OUT | GPIO_DFLT_LOW);
	return 0;
}

static int mioa701_pm_remove(struct platform_device *dev)
{
	return 0;
}

static int mioa701_pm_suspend(struct platform_device *dev, pm_message_t state)
{
	PWER |=   PWER_GPIO0 
		| PWER_GPIO1; /* reset */
	// PFER |= PWER_GPIO0 | PWER_GPIO1;
	// PRER |= PWER_GPIO0 | PWER_GPIO1;

	// haret: PWER = 0x8010e801 (WERTC | WEUSIM | WE15 | WE14 | WE13 | WE11 | WE0
	// haret: PSSR = 0x00000005
	// haret: PRER = 0x0000e001 (RE15 | RE14 | RE13 | RE0)
	PGSR0 = 0x02b0401a;
	PGSR1 = 0x02525c96;
	PGSR2 = 0x054d2000;
	PGSR3 = 0x007e038c;

	/* 3.6864 MHz oscillator power-down enable */
	PCFR  = 0x000004f1; // PCFR_FVC | PCFR_DCEN || PCFR_PI2CEN | PCFR_GPREN | PCFR_OPDE
	//PCFR = PCFR_OPDE | PCFR_PI2CEN | PCFR_GPROD | PCFR_GPR_EN;

	/* */
	PSLR  = 0xff100000; // PSLR_SYSDEL=125ms, PSLR_PWRDEL=125ms, PSLR_SL_ROD=1

	return 0;
}

static int mioa701_pm_resume(struct platform_device *dev)
{
	return 0;
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

static struct platform_driver mioa701_pm = {
        .driver = {
		.name = "mioa701-pm",
        },
	.probe = mioa701_pm_probe,
	.remove = mioa701_pm_remove,
	.suspend = mioa701_pm_suspend,
	.resume = mioa701_pm_resume,
};

static int __init mioa701_pm_init( void )
{
        int ret;
        ret = platform_driver_register(&mioa701_pm);
        if (ret)
                printk(KERN_NOTICE ": registration failed\n");

        return ret;
}

static void __exit mioa701_pm_exit( void )
{
        platform_driver_unregister(&mioa701_pm);
        return;
}

module_init( mioa701_pm_init );
module_exit( mioa701_pm_exit );

MODULE_AUTHOR("Robert Jarzmik");
MODULE_DESCRIPTION("MIO A701 Power Managment Support Driver");
MODULE_LICENSE("GPL");

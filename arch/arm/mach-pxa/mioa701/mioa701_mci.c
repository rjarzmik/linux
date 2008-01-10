/* PCMCIA SD-Card interface for MIO A701
 *
 * 2007-1-12 Robert Jarzmik
 *
 * This code is licenced under the GPLv2.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <asm/gpio.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/mmc.h> // MMC_VDD_32_33

#include "mioa701.h"

static struct pxamci_platform_data mioa701_mci_info;

/**
 * SDIO/MMC Card controller
 * The card detect interrupt isn't debounced so we delay it by 250ms
 * to give the card a chance to fully insert/eject.
 */
static int mioa701_mci_init(struct device *dev, irq_handler_t detect_int, 
			    void *data)
{
	int err;
  
	/*
	 * setup GPIO
	 */
	pxa_gpio_mode(92 | GPIO_ALT_FN_1_OUT);    // MMDAT<0>
	pxa_gpio_mode(109 | GPIO_ALT_FN_1_OUT);   // MMDAT<1>
	pxa_gpio_mode(110 | GPIO_ALT_FN_1_OUT);   // MMDAT<2> MMCCS<0>
	pxa_gpio_mode(111 | GPIO_ALT_FN_1_OUT);   // MMDAT<3> MMCCS<1>
	pxa_gpio_mode(112 | GPIO_ALT_FN_1_OUT);   // MMCMD
	pxa_gpio_mode(32 | GPIO_ALT_FN_2_OUT);    // MMCLK

	/* enable RE/FE interrupt on card insertion and removal */
	GRER0 |= 1 << MIOA701_SDIO_INSERT;
	GFER0 |= 1 << MIOA701_SDIO_INSERT;
  
	err = request_irq(gpio_to_irq(MIOA701_SDIO_INSERT), detect_int,
			  IRQF_DISABLED | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			  "MMC card detect", data);
	if (err) {
		printk(KERN_ERR "mioa701_mci_init: MMC/SD: can't request MMC card detect IRQ\n");
		return -1;
	}
	mioa701_mci_info.detect_delay = msecs_to_jiffies(250);

	return 0;
}

static void mioa701_mci_setpower(struct device *dev, unsigned int vdd)
{
	struct pxamci_platform_data* p_d = dev->platform_data;
	if ((1 << vdd) & p_d->ocr_mask)
		gpio_set_value(MIOA701_SDIO_EN, 1);    // enable SDIO slot power
	else
		gpio_set_value(MIOA701_SDIO_EN, 0);    // disable SDIO slot power
}

static int mioa701_mci_get_ro(struct device *dev)
{
	return gpio_get_value(MIOA701_SDIO_RO);
}

static void mioa701_mci_exit(struct device *dev, void *data)
{
	free_irq(gpio_to_irq(MIOA701_SDIO_INSERT), data);
}

static struct pxamci_platform_data mioa701_mci_info = {
	.ocr_mask = MMC_VDD_32_33 | MMC_VDD_33_34,
	.init     = mioa701_mci_init,
	.get_ro   = mioa701_mci_get_ro,
	.setpower = mioa701_mci_setpower,
	.exit     = mioa701_mci_exit,
};

static int __init mioa701_mcimod_init(void)
{
	pxa_set_mci_info(&mioa701_mci_info);
	return 0;
}

static void __exit mioa701_mcimod_exit(void)
{
	pxa_set_mci_info(NULL);
	return;
}

module_init(mioa701_mcimod_init);
module_exit(mioa701_mcimod_exit);

/* Module information */
MODULE_AUTHOR("Robert Jarzmik (rjarzmik@yahoo.fr)");
MODULE_DESCRIPTION("MMC/SD manager for MIO A701");
MODULE_LICENSE("GPL");

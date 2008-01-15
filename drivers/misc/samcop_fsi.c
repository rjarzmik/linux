/*
 * Driver for the samcop fcd/fsi interface
 *
 * Copyright 2005 Erik Hovland
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/hardware/ipaq-samcop.h>
#include <linux/mfd/samcop_base.h>
#include <asm/hardware/samcop-fsi.h>

#include "samcop_fsi.h"

static inline void
samcop_fsi_write_register (struct samcop_fsi *fsi, u32 reg, u32 val)
{
	fsi->fsi_data->write_reg (fsi->me, reg, val);
}

static inline u32
samcop_fsi_read_register (struct samcop_fsi *fsi, u32 reg)
{
	return fsi->fsi_data->read_reg (fsi->me, reg);
}

/*
 * Helper functions that provide samcop access to the fcd and fsi parts of the
 * fingerprint scanner
 */

void
samcop_fsi_set_control (struct samcop_fsi *fsi, u32 val)
{
	val &= 0xff;
	samcop_fsi_write_register (fsi, _SAMCOP_FSI_Control, val);
}

void
samcop_fsi_set_prescaler (struct samcop_fsi *fsi, u32 val)
{
	samcop_fsi_write_register (fsi, _SAMCOP_FSI_Prescaler, val);
}

void
samcop_fsi_set_dmc (struct samcop_fsi *fsi, u32 val)
{
	samcop_fsi_write_register (fsi, _SAMCOP_FSI_DMC, val);
}

void
samcop_fsi_set_fifo_control (struct samcop_fsi *fsi, u32 val)
{
	samcop_fsi_write_register (fsi, _SAMCOP_FSI_FIFO_Control, val);
}

u32
samcop_fsi_get_control (struct samcop_fsi *fsi)
{
	return samcop_fsi_read_register (fsi, _SAMCOP_FSI_Control);
}

void
samcop_fsi_up (struct samcop_fsi *fsi)
{
	/* Reset fcd registers */
	samcop_reset_fcd (fsi->parent);

	/* enable PCLK */
	clk_enable(fsi->clk);

	/* reset fsi h/w */
	samcop_fsi_set_control(fsi, SAMCOP_FSI_CONTROL_RESET);

	/* set prescaler, 19 FM */
	samcop_fsi_set_prescaler(fsi, SAMCOP_FSI_PRESCALE_50KHZ);

	/* set up dummy counter */
	samcop_fsi_set_dmc(fsi, SAMCOP_FSI_DMC_1MBSEC);

	/* clear then reset fifo fifo */
	samcop_fsi_set_fifo_control(fsi, SAMCOP_FSI_FIFO_RESET_COUNT_NOW);
	samcop_fsi_set_fifo_control(fsi, SAMCOP_FSI_FIFO_RESET_COUNT_NORMAL_MODE);
}

void
samcop_fsi_down (struct samcop_fsi *fsi)
{
	/* tell scanner to sleep */
	u32 control_register = 0;
	control_register = samcop_fsi_get_control(fsi);
	control_register &= ~SAMCOP_FSI_CONTROL_TEMP_PWR_ON; // |= was here originally
	samcop_fsi_set_control(fsi, control_register);

	/* disable PCLK */
	clk_disable(fsi->clk);
}

void
samcop_fsi_set_status (struct samcop_fsi *fsi, u32 val)
{
	samcop_fsi_write_register (fsi, _SAMCOP_FSI_STA, val);
}

u32
samcop_fsi_get_status (struct samcop_fsi *fsi)
{
	return samcop_fsi_read_register (fsi, _SAMCOP_FSI_STA);
}

u32
samcop_fsi_read_data (struct samcop_fsi *fsi)
{
	return samcop_fsi_read_register (fsi, _SAMCOP_FSI_DAT);
}

static int
samcop_fsi_probe (struct platform_device *pdev)
{
	int result = 0;
	struct samcop_fsi *fsi;

	fsi = kmalloc (sizeof (*fsi), GFP_KERNEL);
	if (!fsi)
		return -ENOMEM;
	memset (fsi, 0, sizeof (*fsi));

	fsi->parent = pdev->dev.parent;
	platform_set_drvdata(pdev, fsi);

	/* Is this really correct? Some other drivers use this approach, but
	 * this was already once ioremapped by samcop_base, so we could possibly
	 * use that mapping and not create new one (approach similar to
	 * samcop_sdi).
	 */

	fsi->map = ioremap(pdev->resource[0].start, 
			pdev->resource[0].end - pdev->resource[0].start + 1);
	fsi->irq = pdev->resource[1].start;
	pr_debug("FSI at 0x%p, IRQ %d\n", fsi->map, fsi->irq);

	fsi->clk = clk_get(&pdev->dev, "fsi");
	if (!fsi->clk) {
		printk(KERN_ERR "samcop_fsi: Could not get fsi clock");
		iounmap(fsi->map);
		kfree(fsi);
		result = -EBUSY;
	};

	fsi->fsi_data = pdev->dev.platform_data;
	fsi->me = &pdev->dev;
/*	
	fsi->set_control = samcop_fsi_set_control;
	fsi->get_control = samcop_fsi_get_control;
	fsi->set_status = samcop_fsi_set_status;
	fsi->get_status = samcop_fsi_get_status;
	fsi->read_data = samcop_fsi_read_data;
	fsi->up = samcop_fsi_up;
	fsi->down = samcop_fsi_down;
	fsi->set_fifo_control = samcop_fsi_set_fifo_control;
	fsi->set_prescaler = samcop_fsi_set_prescaler;
	fsi->set_dmc = samcop_fsi_set_dmc;
*/

	if (IS_ERR(fsi->clk)) {
		printk(KERN_ERR "failed to get fsi clock\n");
		kfree (fsi);
		return -EBUSY;
	}

	if ((result = fsi_attach(fsi)) != 0) {
		printk("couldn't attach fsi driver: error %d\n", result);
		kfree (fsi);
	}
	

	return result;
}

static int
samcop_fsi_remove (struct platform_device *dev)
{
	struct samcop_fsi *fsi = platform_get_drvdata(dev);
	int result = 0;
	result = fsi_detach();
	if (result != 0)
		return result;
	iounmap(fsi->map);
	kfree (fsi);
	return result;
}

static int
samcop_fsi_suspend (struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int
samcop_fsi_resume (struct platform_device *dev)
{
	return 0;
}

struct platform_driver samcop_fsi_device_driver = {
	.driver = {
		.name    = "samcop fsi",
	},
	.probe   = samcop_fsi_probe,
	.remove  = samcop_fsi_remove,
	.suspend = samcop_fsi_suspend,
	.resume  = samcop_fsi_resume
};

static int samcop_fsi_init (void)
{
	return platform_driver_register (&samcop_fsi_device_driver);
}

static void samcop_fsi_cleanup (void)
{
	platform_driver_unregister (&samcop_fsi_device_driver);
}

module_init(samcop_fsi_init);
module_exit(samcop_fsi_cleanup);

MODULE_AUTHOR("Erik Hovland <erik@hovland.org>");
MODULE_DESCRIPTION("SAMCOP FCD/FSI controls for the HP iPAQ");
MODULE_LICENSE("GPL");

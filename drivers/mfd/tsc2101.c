/*
 * TI TSC2102 Common Code
 *
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Richard Purdie <richard@o-hand.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mfd/tsc2101.h> 
#include "tsc2101.h"

extern void tsc2101_ts_setup(struct device *dev);
extern void tsc2101_ts_report(struct tsc2101_data *devdata, int x, int y, int p, int pendown);

static int tsc2101_regread(struct tsc2101_data *devdata, int regnum)
{
	int reg;
	devdata->platform->send(TSC2101_READ, regnum, &reg, 1);
	return reg;
}

static void tsc2101_regwrite(struct tsc2101_data *devdata, int regnum, int value)
{
	int reg=value;
	devdata->platform->send(TSC2101_WRITE, regnum, &reg, 1);
	return;
}

static int tsc2101_ts_penup(struct tsc2101_data *devdata)
{
	return !( tsc2101_regread(devdata, TSC2101_REG_STATUS) & TSC2101_STATUS_DAVAIL);
}

#define TSC2101_ADC_DEFAULT (TSC2101_ADC_RES(TSC2101_ADC_RES_12BITP) | TSC2101_ADC_AVG(TSC2101_ADC_4AVG) | TSC2101_ADC_CL(TSC2101_ADC_CL_1MHZ_12BIT) | TSC2101_ADC_PV(TSC2101_ADC_PV_500us) | TSC2101_ADC_AVGFILT_MEAN) 

static void tsc2101_readdata(struct tsc2101_data *devdata, struct tsc2101_ts_event *ts_data)
{
	int z1,z2,fixadc=0;
	u32 values[4],status;

	status=tsc2101_regread(devdata, TSC2101_REG_STATUS);
    
	if (status & (TSC2101_STATUS_XSTAT | TSC2101_STATUS_YSTAT | TSC2101_STATUS_Z1STAT 
		| TSC2101_STATUS_Z2STAT)) {

		/* Read X, Y, Z1 and Z2 */
		devdata->platform->send(TSC2101_READ, TSC2101_REG_X, &values[0], 4);

		ts_data->x=values[0];
		ts_data->y=values[1];
		z1=values[2];
		z2=values[3];

		/* Calculate Pressure */
		if ((z1 != 0) && (ts_data->x!=0) && (ts_data->y!=0)) 
			ts_data->p = ((ts_data->x * (z2 -z1) / z1));
		else
			ts_data->p=0;
	}
	
    if (status & TSC2101_STATUS_BSTAT) {
		devdata->platform->send(TSC2101_READ, TSC2101_REG_BAT, &values[0], 1);
	   	devdata->miscdata.bat=values[0];
	   	fixadc=1;
	}	
	if (status & TSC2101_STATUS_AX1STAT) {
		devdata->platform->send(TSC2101_READ, TSC2101_REG_AUX1, &values[0], 1);
		devdata->miscdata.aux1=values[0];
		fixadc=1;
	}
    if (status & TSC2101_STATUS_AX2STAT) {
    	devdata->platform->send(TSC2101_READ, TSC2101_REG_AUX2, &values[0], 1);
    	devdata->miscdata.aux2=values[0];
    	fixadc=1;
	}
    if (status & TSC2101_STATUS_T1STAT) {
    	devdata->platform->send(TSC2101_READ, TSC2101_REG_TEMP1, &values[0], 1);
    	devdata->miscdata.temp1=values[0];
    	fixadc=1;
	}
    if (status & TSC2101_STATUS_T2STAT) {
    	devdata->platform->send(TSC2101_READ, TSC2101_REG_TEMP2, &values[0], 1);
    	devdata->miscdata.temp2=values[0];
    	fixadc=1;
    }
    if (fixadc) {
		/* Switch back to touchscreen autoscan */
		tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_PSM | TSC2101_ADC_ADMODE(0x2));
	}    	
}

static void tsc2101_ts_enable(struct tsc2101_data *devdata)
{
	//tsc2101_regwrite(devdata, TSC2101_REG_RESETCTL, 0xbb00);
	
	/* PINTDAV is data available only */
	tsc2101_regwrite(devdata, TSC2101_REG_STATUS, 0x4000);

	/* disable buffer mode */
	tsc2101_regwrite(devdata, TSC2101_REG_BUFMODE, 0x0);

	/* use internal reference, 100 usec power-up delay,
	 * power down between conversions, 1.25V internal reference */
	tsc2101_regwrite(devdata, TSC2101_REG_REF, 0x16);
			   
	/* enable touch detection, 84usec precharge time, 32 usec sense time */
	tsc2101_regwrite(devdata, TSC2101_REG_CONFIG, 0x08);
			   
	/* 3 msec conversion delays  */
	tsc2101_regwrite(devdata, TSC2101_REG_DELAY, 0x0900);
	
	/*
	 * TSC2101-controlled conversions
	 * 12-bit samples
	 * continuous X,Y,Z1,Z2 scan mode
	 * average (mean) 4 samples per coordinate
	 * 1 MHz internal conversion clock
	 * 500 usec panel voltage stabilization delay
	 */
	tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_PSM | TSC2101_ADC_ADMODE(0x2));

	return;
}

static void tsc2101_ts_disable(struct tsc2101_data *devdata)
{
	/* stop conversions and power down */
	tsc2101_regwrite(devdata, TSC2101_REG_ADC, 0x4000);
}

static void ts_interrupt_main(struct tsc2101_data *devdata, int isTimer, struct pt_regs *regs)
{
	unsigned long flags;
	struct tsc2101_ts_event ts_data;

	spin_lock_irqsave(&devdata->lock, flags);

	//if (!tsc2101_ts_penup(devdata)) {
	if (devdata->platform->pendown()) {
		devdata->pendown = 1;
		tsc2101_readdata(devdata, &ts_data);
		tsc2101_ts_report(devdata, ts_data.x, ts_data.y, ts_data.p, 1);
		mod_timer(&(devdata->ts_timer), jiffies + HZ / 100);
	} else if (devdata->pendown > 0 && devdata->pendown < 3) {
		mod_timer(&(devdata->ts_timer), jiffies + HZ / 100);
		devdata->pendown++;
	} else {
		if (devdata->pendown) 
			tsc2101_ts_report(devdata, 0, 0, 0, 0);

		devdata->pendown = 0;

		set_irq_type(devdata->platform->irq,IRQT_FALLING);

		/* This must be checked after set_irq_type() to make sure no data was missed */
		if (devdata->platform->pendown()) {
			tsc2101_readdata(devdata, &ts_data);
			mod_timer(&(devdata->ts_timer), jiffies + HZ / 100);
		} 
	}
	
	spin_unlock_irqrestore(&devdata->lock, flags);
}


static void tsc2101_timer(unsigned long data)
{
	struct tsc2101_data *devdata = (struct tsc2101_data *) data;

	ts_interrupt_main(devdata, 1, NULL);
}

static irqreturn_t tsc2101_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	struct tsc2101_data *devdata = dev_id;
	
	set_irq_type(devdata->platform->irq,IRQT_NOEDGE);
	ts_interrupt_main(devdata, 0, regs);
	return IRQ_HANDLED;
}


static void tsc2101_get_miscdata(struct tsc2101_data *devdata)
{
	static int i=0;
	unsigned long flags;

	if (devdata->pendown == 0) {
		i++;
		spin_lock_irqsave(&devdata->lock, flags);
		if (i==1) 
			tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_ADMODE(0x6));
		else if (i==2) 
			tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_ADMODE(0x7));
		else if (i==3)
			tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_ADMODE(0x8));
		else if (i==4)
			tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_ADMODE(0xa));
		else if (i>=5) {
			tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_ADMODE(0xc));
			i=0;
		}
		spin_unlock_irqrestore(&devdata->lock, flags);
	}
}


static void tsc2101_misc_timer(unsigned long data)
{
	
	struct tsc2101_data *devdata = (struct tsc2101_data *) data;
	tsc2101_get_miscdata(devdata);
	mod_timer(&(devdata->misc_timer), jiffies + HZ);	
}

void tsc2101_print_miscdata(struct device *dev)
{
	struct tsc2101_data *devdata = dev_get_drvdata(dev);

	printk(KERN_ERR "TSC2101 Bat:   %04x\n",devdata->miscdata.bat);
	printk(KERN_ERR "TSC2101 Aux1:  %04x\n",devdata->miscdata.aux1);
	printk(KERN_ERR "TSC2101 Aux2:  %04x\n",devdata->miscdata.aux2);
	printk(KERN_ERR "TSC2101 Temp1: %04x\n",devdata->miscdata.temp1);
	printk(KERN_ERR "TSC2101 Temp2: %04x\n",devdata->miscdata.temp2);
}

static int tsc2101_suspend(struct device *dev, u32 state, u32 level)
{
	struct tsc2101_data *devdata = dev_get_drvdata(dev);
	
	if (level == SUSPEND_POWER_DOWN) {
		tsc2101_ts_disable(devdata);
		devdata->platform->suspend();
	}
	
	return 0;
}

static int tsc2101_resume(struct device *dev, u32 level)
{
	struct tsc2101_data *devdata = dev_get_drvdata(dev);

	if (level == RESUME_POWER_ON) {
		devdata->platform->resume();
		tsc2101_ts_enable(devdata);
	}

	return 0;
}


static int __init tsc2101_probe(struct device *dev)
{
	struct tsc2101_data *devdata;
	struct tsc2101_ts_event ts_data;

	if (!(devdata = kcalloc(1, sizeof(struct tsc2101_data), GFP_KERNEL)))
		return -ENOMEM;
	
	dev_set_drvdata(dev,devdata);
	spin_lock_init(&devdata->lock);
	devdata->platform = dev->platform_data;

	init_timer(&devdata->ts_timer);
	devdata->ts_timer.data = (unsigned long) devdata;
	devdata->ts_timer.function = tsc2101_timer;

	init_timer(&devdata->misc_timer);
	devdata->misc_timer.data = (unsigned long) devdata;
	devdata->misc_timer.function = tsc2101_misc_timer;

	/* request irq */
	if (request_irq(devdata->platform->irq, tsc2101_handler, 0, "tsc2101", devdata)) {
		printk(KERN_ERR "tsc2101: Could not allocate touchscreen IRQ!\n");
		kfree(devdata);
		return -EINVAL;
	}
	
	tsc2101_ts_setup(dev);
	tsc2101_ts_enable(devdata);

	set_irq_type(devdata->platform->irq,IRQT_FALLING);
	/* Check there is no pending data */
	tsc2101_readdata(devdata, &ts_data);

	mod_timer(&(devdata->misc_timer), jiffies + HZ);	
	
	return 0;
}


static  int __exit tsc2101_remove(struct device *dev)
{
	struct tsc2101_data *devdata = dev_get_drvdata(dev);

	free_irq(devdata->platform->irq, devdata);
	del_timer_sync(&devdata->ts_timer);
	del_timer_sync(&devdata->misc_timer);
	input_unregister_device(&devdata->inputdevice);
	tsc2101_ts_disable(devdata);
	kfree(devdata);
	return 0;
}

static struct device_driver tsc2101_driver = {
	.name 		= "tsc2101",
	.bus 		= &platform_bus_type,
	.probe 		= tsc2101_probe,
	.remove 	= __exit_p(tsc2101_remove),
	.suspend 	= tsc2101_suspend,
	.resume 	= tsc2101_resume,
};

static int __init tsc2101_init(void)
{
	return driver_register(&tsc2101_driver);
}

static void __exit tsc2101_exit(void)
{
	driver_unregister(&tsc2101_driver);
}

module_init(tsc2101_init);
module_exit(tsc2101_exit);

MODULE_LICENSE("GPL");

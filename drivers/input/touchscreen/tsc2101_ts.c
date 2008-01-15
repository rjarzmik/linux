/*
 * Texas Instruments TSC2101 Touchscreen Driver
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
#include <linux/module.h>
#include <linux/mfd/tsc2101.h>

#define X_AXIS_MAX		3830
#define X_AXIS_MIN		150
#define Y_AXIS_MAX		3830
#define Y_AXIS_MIN		190
#define PRESSURE_MIN	0
#define PRESSURE_MAX	20000

void tsc2101_ts_report(struct tsc2101_data *tsc2101_ts, int x, int y, int p, int pendown)
{
	input_report_abs(&(tsc2101_ts->inputdevice), ABS_X, x);
	input_report_abs(&(tsc2101_ts->inputdevice), ABS_Y, y);
	input_report_abs(&(tsc2101_ts->inputdevice), ABS_PRESSURE, p);
	input_report_key(&(tsc2101_ts->inputdevice), BTN_TOUCH, pendown);
	input_sync(&(tsc2101_ts->inputdevice));

	return;
}

void tsc2101_ts_setup(struct device *dev)
{
	struct tsc2101_data *tsc2101_ts = dev_get_drvdata(dev);
	
	init_input_dev(&(tsc2101_ts->inputdevice));
	tsc2101_ts->inputdevice.name = "tsc2101_ts";
	tsc2101_ts->inputdevice.evbit[0] = BIT(EV_KEY) | BIT(EV_ABS);
	tsc2101_ts->inputdevice.keybit[LONG(BTN_TOUCH)] |= BIT(BTN_TOUCH);
	input_set_abs_params(&tsc2101_ts->inputdevice, ABS_X, X_AXIS_MIN, X_AXIS_MAX, 0, 0);
	input_set_abs_params(&tsc2101_ts->inputdevice, ABS_Y, Y_AXIS_MIN, Y_AXIS_MAX, 0, 0);
	input_set_abs_params(&tsc2101_ts->inputdevice, ABS_PRESSURE, PRESSURE_MIN, PRESSURE_MAX, 0, 0);
	input_register_device(&(tsc2101_ts->inputdevice));

	printk("tsc2101 touchscreen driver initialized\n");

	return 0;
}


/*
 * Driver for diagonal joypads on GPIO lines capable of generating interrupts.
 *
 * Copyright 2007 Milan Plzik, based on gpio-keys.c (c) Phil Blundell 2005
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/sort.h>
#include <linux/gpiodev.h>
#include <linux/gpiodev_keys2.h>

struct gpiodev_keys2_drvdata {
	struct input_dev *input;
	struct timer_list debounce_timer;
	struct gpiodev_keys2_gpio ***button_gpios;
	int *gpio_delays;
	struct gpiodev_keys2_button **buttons_by_prio;
	int *gpio_used_prio;
};


static struct gpiodev_keys2_gpio *lookup_gpio (struct gpiodev_keys2_platform_data *pdata,
		const char *name)
{
	int i;
	
	for (i = 0; i < pdata->ngpios; i++) {
		if (!strcmp (name, pdata->gpios[i].name)) 
			return &pdata->gpios[i];
	}

	return NULL;
}

static int count_gpios (struct gpiodev_keys2_button *btn)
{
	int i = 0;
	const char **gpio = btn->gpio_names;
	
	while (gpio[i]) i++;

	return i;
}


static irqreturn_t gpiodev_keys2_isr(int irq, void *data)
{
	int i;
	struct platform_device *pdev = data;
	struct gpiodev_keys2_platform_data *pdata = pdev->dev.platform_data;
	struct gpiodev_keys2_drvdata *drvdata = platform_get_drvdata(pdev);

	/* Find out which GPIO has been asserted and the corresponding delay for
	 * debouncing. */
	for (i = 0; i < pdata->ngpios; i++)
		if (irq == gpiodev_to_irq(&pdata->gpios[i].gpio)) {
			break;
		};
	
	/* Prepare delayed call of debounce handler */
	if (time_before(drvdata->debounce_timer.expires, jiffies + 
			msecs_to_jiffies(drvdata->gpio_delays[i])) ||
	    !timer_pending(&drvdata->debounce_timer))
		mod_timer(&drvdata->debounce_timer, 
				jiffies + msecs_to_jiffies(drvdata->gpio_delays[i]));

	return IRQ_HANDLED;
}

static void debounce_handler(unsigned long data)
{
	struct platform_device *pdev = (void*)data;
	struct gpiodev_keys2_platform_data *pdata = pdev->dev.platform_data;
	struct gpiodev_keys2_drvdata *drvdata = platform_get_drvdata(pdev);
	struct gpiodev_keys2_button *button;
	struct gpiodev_keys2_gpio **gpio;
	int i;

	memset(drvdata->gpio_used_prio, 0, sizeof(drvdata->gpio_used_prio[0])*pdata->ngpios);

	/* Clear reported buttons */
	for (i = 0; i < pdata->nbuttons; i++)
		input_report_key(drvdata->input, pdata->buttons[i].keycode, 0);

	for (i = 0; i < pdata->nbuttons; i++) {
		int button_idx = drvdata->buttons_by_prio[i] - &pdata->buttons[0];
		button = drvdata->buttons_by_prio[i];
		gpio = drvdata->button_gpios[button_idx];

		/* are all required GPIOs asserted? */
		do {
			int gpio_idx = gpio[0] - pdata->gpios; /* index in pdata->gpios */
			if (((gpiodev_get_value(&gpio[0]->gpio) ? 1 : 0) ^ (gpio[0]->active_low)) &&
				/* this is only true when gpio prio is 0,
				 * because the buttons are ordered descending by
				 * priority */
				drvdata->gpio_used_prio[gpio_idx]<=button->priority)
				continue;
			else 
				break;
		} while ((++gpio)[0] != NULL);

		if (gpio[0] == NULL) {
			/* all gpios are asserted and unused otherwise, so key
			 * can be pressed and GPIOs will be marked as used. */
			gpio = drvdata->button_gpios[button_idx];
			do {
				int gpio_idx = gpio[0] - pdata->gpios;
				drvdata->gpio_used_prio[gpio_idx] = button->priority;
			} while (((++gpio)[0]) != NULL);
			if (button->keycode != -1)
				input_report_key(drvdata->input, button->keycode, 1);
		}
	};
	input_sync(drvdata->input);
}


static int btnprio_cmp (const void *a, const void *b)
{
	const struct gpiodev_keys2_button *b1, *b2;
	
	b1 = *((const struct gpiodev_keys2_button**)a);
	b2 = *((const struct gpiodev_keys2_button**)b);

	return b2->priority - b1->priority;
}

/* Builds array, which on i-th position contains pointers to all GPIOs used by
 * i-th button in pdata->buttons, and also finds maximal debounce time for each
 * gpio.
 */

static int gpiodev_keys2_init_drvdata(struct platform_device *pdev)
{
	struct gpiodev_keys2_platform_data *pdata = pdev->dev.platform_data;
	struct gpiodev_keys2_drvdata *drvdata = platform_get_drvdata(pdev);
	struct gpiodev_keys2_gpio *gpio;
	int i, j;
	int button_gpios;

	if (drvdata->button_gpios) {
		printk (KERN_ERR "gpiodev-keys2: Attempting to re-create drvdata\n");
		return -EBUSY;
	};
	
	drvdata->button_gpios = kzalloc(
			pdata->nbuttons*sizeof(*drvdata->button_gpios), 
			GFP_KERNEL);
	drvdata->gpio_delays = kzalloc(pdata->ngpios*sizeof(*drvdata->gpio_delays), GFP_KERNEL);
	drvdata->buttons_by_prio = kzalloc(pdata->nbuttons*sizeof(*drvdata->buttons_by_prio), GFP_KERNEL);
	drvdata->gpio_used_prio = kzalloc(pdata->ngpios*sizeof(*drvdata->gpio_used_prio), GFP_KERNEL);

	for (i = 0; i < pdata->nbuttons; i++) {
		button_gpios = count_gpios(&pdata->buttons[i]);
		drvdata->button_gpios[i] = kzalloc((button_gpios+1)*sizeof(drvdata->button_gpios[i]), GFP_KERNEL);

		/* Put all GPIOs used by i-th button into button_gpios[i] */
		for (j = 0; j < button_gpios; j++) {
			gpio = lookup_gpio(pdata, pdata->buttons[i].gpio_names[j]);
			if (!gpio) {
				printk(KERN_WARNING "gpiodev-keys2: Unknown gpio '%s'\n", pdata->buttons[i].gpio_names[j]);
				continue;
			};

			/* See whether actual button has delay, which is bigger
			 * than delay GPIO. If so, update GPIOs delay.
			 */
			drvdata->gpio_delays[gpio - pdata->gpios] = max(
				drvdata->gpio_delays[gpio - pdata->gpios],
				pdata->buttons[i].debounce_ms);

			drvdata->button_gpios[i][j] = gpio;
		};


	};

	/* Create list of buttons sorted descending by priority */
	for (i = 0; i < pdata->nbuttons; i++)
		drvdata->buttons_by_prio[i] = &pdata->buttons[i];
	
	sort(drvdata->buttons_by_prio, pdata->nbuttons, sizeof(drvdata->buttons_by_prio[0]), btnprio_cmp, NULL);
	for (i = 0; i <pdata->nbuttons; i++)
		printk (KERN_INFO "- button '%s', prio %d\n", drvdata->buttons_by_prio[i]->gpio_names[0], drvdata->buttons_by_prio[i]->priority);

	return 0;
}

static void gpiodev_keys2_free_mapping(struct platform_device *pdev)
{
	struct gpiodev_keys2_platform_data *pdata = pdev->dev.platform_data;
	struct gpiodev_keys2_drvdata *drvdata = platform_get_drvdata(pdev);
	int i;

	if (!drvdata)
		return;

	for (i = 0; i < pdata->nbuttons; i++) 
		kfree(drvdata->button_gpios[i]);

	kfree(drvdata->button_gpios);
	kfree(drvdata->gpio_delays);
	kfree(drvdata->buttons_by_prio);
	kfree(drvdata->gpio_used_prio);
}


static int __devinit gpiodev_keys2_probe(struct platform_device *pdev)
{
	struct gpiodev_keys2_drvdata *drvdata;
	struct gpiodev_keys2_platform_data *pdata = pdev->dev.platform_data;
	struct input_dev *input;
	int i, error;

	input = input_allocate_device();
	if (!input)
		return -ENOMEM;

	input->evbit[0] = BIT(EV_KEY) | BIT(EV_REP);
	input->name = pdev->name;
	input->phys = "gpiodev-keys2/input0";
	input->cdev.dev = &pdev->dev;
	// input->private // do we need this?
	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	drvdata = kzalloc(sizeof(struct gpiodev_keys2_drvdata), GFP_KERNEL);
	drvdata->input = input;
	platform_set_drvdata(pdev, drvdata);

	setup_timer(&drvdata->debounce_timer, debounce_handler, (unsigned long) pdev);
	
	for (i = 0; i < pdata->nbuttons; i++)
		set_bit(pdata->buttons[i].keycode, input->keybit);
	
	error = input_register_device(input);
	if (error) {
		printk(KERN_ERR "Unable to register gpiodev-keys2 input device\n");
		goto fail;
	}

	gpiodev_keys2_init_drvdata(pdev);


	for (i = 0; i < pdata->ngpios; i++) {
		int irq = gpiodev_to_irq(&(pdata->gpios[i].gpio));

		set_irq_type(irq, IRQ_TYPE_EDGE_BOTH);
		error = request_irq(irq, gpiodev_keys2_isr, 
			IRQF_SAMPLE_RANDOM | IRQF_SHARED,
			pdata->gpios[i].name ? 
			pdata->gpios[i].name : "gpiodev_keys2", pdev);

		if (error) {
			printk(KERN_ERR "gpiodev-keys: unable to claim irq %d; error %d\n", irq, error);
			goto fail;
		}
	}


	return 0;

fail:
	for (i = i - 1; i >= 0; i--) 
		free_irq(gpiodev_to_irq(&pdata->gpios[i].gpio),pdev);

	input_free_device(input);

	del_timer_sync(&drvdata->debounce_timer);

	return error;
}

static int __devexit gpiodev_keys2_remove(struct platform_device *pdev)
{
	struct gpiodev_keys2_drvdata *drvdata = platform_get_drvdata(pdev);
	struct gpiodev_keys2_platform_data *pdata = pdev->dev.platform_data;
	int i;

	del_timer_sync(&drvdata->debounce_timer);

	for (i = 0; i < pdata->ngpios; i++) 
		free_irq(gpiodev_to_irq(&pdata->gpios[i].gpio), pdev);

	input_unregister_device(drvdata->input);
	input_free_device(drvdata->input);
	gpiodev_keys2_free_mapping(pdev);
	kfree(drvdata);

	return 0;
}


struct platform_driver gpiodev_keys2_device_driver = {
	.probe 	= gpiodev_keys2_probe,
	.remove = __devexit_p(gpiodev_keys2_remove),
	.driver	= {
		.name = "gpiodev-keys2",
	}
};

static int __init gpiodev_keys2_init(void)
{
	return platform_driver_register(&gpiodev_keys2_device_driver);
}

static void __exit gpiodev_keys2_exit(void)
{
	platform_driver_unregister(&gpiodev_keys2_device_driver);
}

module_init(gpiodev_keys2_init);
module_exit(gpiodev_keys2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Milan Plzik <milan.plzik@gmail.com>");
MODULE_DESCRIPTION("Keyboard driver for GPIO");

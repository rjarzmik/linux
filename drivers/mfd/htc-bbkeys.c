/*
 * Support for the "mysterious" micro-controller that does "bit
 * banging" on a pair of gpio pins.  This is known to be present on
 * HTC Apache and HTC Hermes phones.  This controller controls the lcd
 * power, the front keypad, and front LEDs.
 *
 * (c) Copyright 2007 Kevin O'Connor <kevin@koconnor.net>
 *
 * This file may be distributed under the terms of the GNU GPL license.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h> /* struct platform_device */
#include <linux/interrupt.h> /* irqreturn_t */
#include <linux/input.h> /* input_report_key */
#include <linux/spinlock.h> /* spinlock_t */

#include <asm/gpio.h> /* gpio_set_value */
#include <linux/mfd/htc-bbkeys.h>

/* Some HTC phones have a device attached to gpio pins that appears to
 * follow the i2c system with some minor deviations.  It uses a 4bit
 * register number instead of a 7 bit device-id address. */

#define dprintf1(fmt, args...) printk(KERN_DEBUG fmt "\n" , ##args)
#define dprintf(fmt, args...)

struct bbkeys_info {
	spinlock_t lock;
	int sda_gpio, scl_gpio;
	int irq;
	struct input_dev *input;
	u8 outcache[8];
};


/****************************************************************
 * Chip register access
 ****************************************************************/

/* Wrappers around gpio functions */
static inline int get_sda(struct bbkeys_info *bb)
{
	return gpio_get_value(bb->sda_gpio);
}
static inline void set_sda(struct bbkeys_info *bb, int value)
{
	gpio_set_value(bb->sda_gpio, value);
}
static inline void set_scl(struct bbkeys_info *bb, int value)
{
	gpio_set_value(bb->scl_gpio, value);
}
static inline void set_sda_input(struct bbkeys_info *bb)
{
	gpio_direction_input(bb->sda_gpio);
}
static inline void set_sda_output(struct bbkeys_info *bb)
{
	gpio_direction_output(bb->sda_gpio, 0);
}

/* Read 'count' bits from SDA pin.  The caller must ensure the SDA pin
 * is set to input mode before calling.  The SCL pin must be low. */
static inline u32 read_bits(struct bbkeys_info *bb, int count)
{
	u32 data = 0;
	int i = count;

	for (; i>0; i--) {
		data <<= 1;

		set_scl(bb, 1);

		if (get_sda(bb))
			data |= 1;
		dprintf(2, "Read bit of %d", data & 1);

		set_scl(bb, 0);
	}
	dprintf(1, "Read %d bits: 0x%08x", count, data);
	return data;
}

/* Write 'count' bits to SDA pin.  The caller must ensure the SDA pin
 * is set to output mode before calling.  The SCL pin must be low. */
static inline void write_bits(struct bbkeys_info *bb, u32 data, int count)
{
	u32 bit = 1<<(count-1);
	dprintf(1, "Writing %d bits: 0x%08x", count, data);

	for (; count>0; count--) {
		set_sda(bb, data & bit);
		dprintf(2, "Write bit of %d", !!(data & bit));

		set_scl(bb, 1);
		set_scl(bb, 0);

		data <<= 1;
	}
}

/* Init the gpio lines and send a stop followed by a start. */
static inline void stopstart(struct bbkeys_info *bb)
{
	dprintf(1, "Stop Start");

	/* Init lines */
	set_scl(bb, 0);
	set_sda_output(bb);

	/* Send stop */
	set_scl(bb, 1);
	set_sda(bb, 1);

	/* Send start */
	set_sda(bb, 0);
	set_scl(bb, 0);
}

/* Read a register from the bit-banging interface. */
static u32 __bbkeys_read(struct bbkeys_info *bb, u32 reg)
{
	u32 val;
	stopstart(bb);

	/* Set read indicator as low order bit. */
	val = reg << 1;
	val |= 1;

	/* Send register */
	write_bits(bb, val, 5);

	/* Get ack. */
	set_sda_input(bb);
	val = read_bits(bb, 1);
	if (! val)
		goto fail;

	/* Read register value. */
	val = read_bits(bb, 9);
	if (!(val & 1))
		goto fail;
	val >>= 1;

	return val;
fail:
	printk(KERN_WARNING "BBKEYS - Missing ack\n");
	return 0;
}

static u32 bbkeys_read(struct bbkeys_info *bb, u32 reg)
{
	u32 ret;
	unsigned long flag;
	spin_lock_irqsave(&bb->lock, flag);
	ret = __bbkeys_read(bb, reg);
	spin_unlock_irqrestore(&bb->lock, flag);
	return ret;
}

/* Write a register on the bit-banging interface. */
static void __bbkeys_write(struct bbkeys_info *bb, u32 reg, u32 data)
{
	stopstart(bb);

	/* Set write indicator as low order bit. */
	reg <<= 1;

	/* Send register */
	write_bits(bb, reg, 5);

	/* Get ack. */
	set_sda_input(bb);
	reg = read_bits(bb, 1);
	if (! reg)
		goto fail;
	set_sda_output(bb);

	/* write register value. */
	write_bits(bb, data, 8);
	set_sda_input(bb);
	reg = read_bits(bb, 1);
	if (!reg)
		goto fail;
	return;
fail:
	printk(KERN_WARNING "BBKEYS - Missing ack\n");
}

static void bbkeys_write(struct bbkeys_info *bb, u32 reg, u32 data)
{
	unsigned long flag;
	spin_lock_irqsave(&bb->lock, flag);
	__bbkeys_write(bb, reg, data);
	spin_unlock_irqrestore(&bb->lock, flag);
}


/****************************************************************
 * Keypad
 ****************************************************************/

static irqreturn_t bbkeys_isr(int irq, void *dev_id)
{
	int i;
	struct platform_device *pdev = dev_id;
	struct htc_bbkeys_platform_data *pdata = pdev->dev.platform_data;
	struct bbkeys_info *bb = platform_get_drvdata(pdev);
	u32 regs;

	/* Ack the interrupt. */
	bbkeys_write(bb, pdata->ackreg, 0);

	/* Read the key status registers */
	regs = ~(bbkeys_read(bb, pdata->key1reg)
		 | (bbkeys_read(bb, pdata->key2reg) << 8));

	/* Register the state of the keys */
	for (i = 0; i < BBKEYS_MAXKEY; i++) {
		int key = pdata->buttons[i];
		int state = !!(regs & (1<<i));
		if (key < 0)
			continue;
		input_report_key(bb->input, key, state);
		input_sync(bb->input);
	}

	return IRQ_HANDLED;
}

/* Delayed input driver data. */
static int InputReady;
static struct platform_device *Early_pdev;

static int key_setup(struct platform_device *pdev)
{
	struct bbkeys_info *bb = platform_get_drvdata(pdev);
	struct htc_bbkeys_platform_data *pdata = pdev->dev.platform_data;
	struct input_dev *input;
	struct resource *res;
	int i, ret;

	if (! InputReady) {
		printk("Delaying registration of bbkeys input driver\n");
		Early_pdev = pdev;
		return 0;
	}

	/* Find keyboard irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res)
		return -EINVAL;
	bb->irq = res->start;

	input = input_allocate_device();
	if (!input)
		return -ENOMEM;

	set_bit(EV_KEY, input->evbit);

	input->name = pdev->name;
	bb->input = input;

	for (i = 0; i < BBKEYS_MAXKEY; i++) {
		int code = pdata->buttons[i];
		if (code >= 0)
			set_bit(code, input->keybit);
	}

	ret = input_register_device(input);
	if (ret < 0) {
		printk(KERN_ERR "Unable to register bbkeys input device\n");
		goto fail;
	}

        ret = request_irq(bb->irq, bbkeys_isr
			  , IRQF_DISABLED | IRQF_TRIGGER_RISING
			    | IRQF_SAMPLE_RANDOM
			  , pdev->name, pdev);
	if (ret)
		goto fail;

	/* Clear any queued up key events. */
	bbkeys_write(bb, pdata->ackreg, 0);

	return 0;

fail:
	input_free_device(input);
	return ret;
}


/****************************************************************
 * GPIOs
 ****************************************************************/

static inline int u8pos(unsigned gpio) {
	return (gpio & GPIO_BASE_MASK) / 8;
}
static inline int u8bit(unsigned gpio) {
	return 1<<(gpio & (8-1));
}

/* Check an input pin to see if it is active. */
static int bbkeys_get(struct device *dev, unsigned gpio)
{
	struct bbkeys_info *bb = dev_get_drvdata(dev);
	u32 readval = bbkeys_read(bb, u8pos(gpio));
	return !!(readval & u8bit(gpio));
}

/* Set the direction of an output pin. */
static void bbkeys_set(struct device *dev, unsigned gpio, int val)
{
	struct bbkeys_info *bb = dev_get_drvdata(dev);
	int pos = u8pos(gpio);
	unsigned long flag;

	spin_lock_irqsave(&bb->lock, flag);
	if (val)
		bb->outcache[pos] |= u8bit(gpio);
	else
		bb->outcache[pos] &= ~u8bit(gpio);
	__bbkeys_write(bb, pos, bb->outcache[pos]);
	spin_unlock_irqrestore(&bb->lock, flag);
}

/* Dummy handler - this device doesn't support irqs. */
static int bbkeys_to_irq(struct device *dev, unsigned gpio_no)
{
	return -ENODEV;
}


/****************************************************************
 * Setup
 ****************************************************************/

static int bbkeys_probe(struct platform_device *pdev)
{
	struct htc_bbkeys_platform_data *pdata = pdev->dev.platform_data;
	struct bbkeys_info *bb;
	int ret;
	struct gpio_ops ops;

	/* Initialize bb data structure. */
	bb = kzalloc(sizeof(*bb), GFP_KERNEL);
	if (!bb)
		return -ENOMEM;

	spin_lock_init(&bb->lock);

	bb->sda_gpio = pdata->sda_gpio;
	bb->scl_gpio = pdata->scl_gpio;

	platform_set_drvdata(pdev, bb);

	/* Setup input handling */
	ret = key_setup(pdev);
	if (ret)
		goto fail;

	ops.get_value = bbkeys_get;
	ops.set_value = bbkeys_set;
	ops.to_irq = bbkeys_to_irq;
	gpiodev_register(pdata->gpio_base, &pdev->dev, &ops);

	return 0;
fail:
	printk(KERN_NOTICE "BBKEYS failed to setup\n");
	kfree(bb);
	return ret;
}

static int bbkeys_remove(struct platform_device *pdev)
{
	struct bbkeys_info *bb = platform_get_drvdata(pdev);
        free_irq(bb->irq, pdev);
	input_unregister_device(bb->input);
	kfree(bb);
	return 0;
}

static struct platform_driver bbkeys_driver = {
	.driver = {
		.name           = "htc-bbkeys",
	},
	.probe          = bbkeys_probe,
	.remove         = bbkeys_remove,
};

static int __init bbkeys_init(void)
{
	return platform_driver_register(&bbkeys_driver);
}

static void __exit bbkeys_exit(void)
{
	platform_driver_unregister(&bbkeys_driver);
}

/* Ugh.  The input subsystem comes up later in the boot process, but
 * several devices need the bbkeys gpio driver earlier.  So, if this
 * driver comes up early, we delay input registration until later in
 * the boot. */
static int __init bbkeys_late_init(void)
{
	InputReady = 1;
	if (Early_pdev) {
		printk("Registering delayed bbkeys input driver\n");
		key_setup(Early_pdev);
	}
	Early_pdev = NULL;
	return 0;
}

module_init(bbkeys_late_init)
#ifdef MODULE
module_init(bbkeys_init);
#else   /* start early for dependencies */
subsys_initcall(bbkeys_init);
#endif
module_exit(bbkeys_exit)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kevin O'Connor <kevin@koconnor.net>");

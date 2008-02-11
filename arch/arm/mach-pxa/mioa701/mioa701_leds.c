/* Leds interface driver on MIO A701
 *
 * 2008-1-1 Robert Jarzmik
 *
 * This code is licenced under the GPLv2.
 */
 
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/leds.h>

#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/mach-types.h>
#include "mioa701.h"


struct mioa701_leds {
	int gpio;
	struct led_classdev led;
};

#define ledtoml(Led) container_of((Led), struct mioa701_leds, led)
static void mioa701_led_brightness_set(struct led_classdev *led_cdev, enum led_brightness value);
#define PGSRn(n) __REG2(0x40F00020, (n<<2))

static void PGSRset(int gpio, int val)
{
	int reg;
	
	if (gpio >= 96)
		reg = 3;
	else if (gpio >= 64)
		reg = 2;
	else if (gpio >= 32)
		reg = 1;
	else
		reg = 0;
		
	if (val) {
		PGSRn(reg) |= GPIO_bit(gpio);
		printk(KERN_NOTICE "PGSRset(%d,%d)\n", gpio, val);
	}
	else {
		PGSRn(reg) &= ~GPIO_bit(gpio);
		printk(KERN_NOTICE "PGSRset(%d,%d)\n", gpio, val);
	}
}

static struct mioa701_leds leds[] = {
	{
		.gpio   = MIO_GPIO_LED_nBLUE,
		.led  = {
			.name	       = "mioa701:blue",
			.brightness_set = mioa701_led_brightness_set,
		}
	},
	{
		.gpio   = MIO_GPIO_LED_nOrange,
		.led  = {
			.name	       = "mioa701:orange",
			.brightness_set = mioa701_led_brightness_set,
		}
	},
	{
		.gpio   = MIO_GPIO_LED_nVibra,
		.led  = {
			.name	       = "mioa701:vibra",
			.brightness_set = mioa701_led_brightness_set,
		}
	},
	{
		.gpio   = MIO_GPIO_LED_nKeyboard,
		.led  = {
			.name	       = "mioa701:keyboard",
			.brightness_set = mioa701_led_brightness_set,
		}
	},
};

static void mioa701_led_brightness_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	struct mioa701_leds *ml = ledtoml(led_cdev);
	int gpioval = (value == 0);

	gpio_set_value(ml->gpio, gpioval);
}

static int mioa701_led_probe(struct platform_device *dev)
{
	int i,ret=0;

	for (i = 0; i < ARRAY_SIZE(leds); i++) {
		ret = led_classdev_register(&dev->dev, &leds[i].led);
		pxa_gpio_mode(leds[i].gpio | GPIO_OUT | GPIO_DFLT_HIGH);
		leds[i].led.brightness_set(&leds[i].led, LED_OFF);

		if (unlikely(ret)) {
			printk(KERN_WARNING "Unable to register mioa701_led %s\n", leds[i].led.name);
		}
	}
	return ret;
}

static int mioa701_led_remove(struct platform_device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(leds); i++) {
		leds[i].led.brightness_set(&leds[i].led, LED_OFF);
		led_classdev_unregister(&leds[i].led);
	}

	return 0;
}

static int mioa701_led_suspend(struct platform_device *dev, pm_message_t state)
{
	int i;
	int gpio, val;

	for (i = 0; i < ARRAY_SIZE(leds); i++) {
		gpio = leds[i].gpio;
		val = gpio_get_value(gpio);
		PGSRset(gpio, val);
		led_classdev_suspend(&leds[i].led);
	}

	return 0;
}

static int mioa701_led_resume(struct platform_device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(leds); i++)
		led_classdev_resume(&leds[i].led);
	
	return 0;
}

static struct platform_driver mioa701_led_driver = {
	.driver = {
		.name   = "mioa701-leds",
	},
	.probe  = mioa701_led_probe,
	.remove = mioa701_led_remove,
	.suspend = mioa701_led_suspend,
	.resume = mioa701_led_resume,
};

static int mioa701_led_init (void)
{
        return platform_driver_register(&mioa701_led_driver);
}

static void mioa701_led_exit (void)
{
	platform_driver_unregister(&mioa701_led_driver);
}

module_init (mioa701_led_init);
module_exit (mioa701_led_exit);

MODULE_AUTHOR("Robert Jarzmik");
MODULE_DESCRIPTION("mioa701 Led Support Driver");
MODULE_LICENSE("GPL");

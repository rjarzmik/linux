#ifndef __GPIODEV_KEYS2_H
#define __GPIODEV_KEYS2_H

#include <linux/gpiodev.h>

#define MAX_GPIOS_PER_BUTTON 5

struct gpiodev_keys2_gpio {
	const char *name;	/* GPIO identifier */
	struct gpio gpio;	/* gpiodev descriptor */
	int active_low;		
};

struct gpiodev_keys2_button {
	int keycode;		/* key code for the button */
	
	const char *gpio_names[MAX_GPIOS_PER_BUTTON];
				/* which GPIOs have to be asserted in order 
				 * to emit keypress. This is NULL-terminated 
				 * array of string GPIO identifiers */
	
	int debounce_ms;	/* Value for this button's debounce will be at
				 * least this number of milliseconds. */

	int priority;		/* If more buttons are likely to be pressed, 
				 * only those with highest priority will
				 * generate keypress, others will be released */
};

struct gpiodev_keys2_platform_data {
	struct gpiodev_keys2_gpio *gpios;
	int ngpios;
	struct gpiodev_keys2_button *buttons;
	int nbuttons;
};

#endif

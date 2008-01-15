/* HTC Hermes GPIOs */
#ifndef _HTCHERMES_GPIO_H_
#define _HTCHERMES_GPIO_H_

#include "linux/gpiodev2.h" // GPIO_BASE_INCREMENT


/****************************************************************
 * EGPIO
 ****************************************************************/

#define IRQ_NR_HTCHERMES_EGPIO_IRQ	IRQ_EINT1
#define HTCHERMES_EGPIO_BASE		GPIO_BASE_INCREMENT
#define HTCHERMES_EGPIO(reg,bit) (HTCHERMES_EGPIO_BASE + 16*(reg) + (bit))


/****************************************************************
 * Front keypad
 ****************************************************************/

#define GPIO_NR_HTCHERMES_FK_SDA	S3C2410_GPC12
#define GPIO_NR_HTCHERMES_FK_SCL	S3C2410_GPC11
#define IRQ_NR_HTCHERMES_FK_IRQ		IRQ_EINT3


/****************************************************************
 * Power
 ****************************************************************/

#define EGPIO_NR_HTCHERMES_PWR_IRQ		HTCHERMES_EGPIO(2, 3)
#define EGPIO_NR_HTCHERMES_PWR_IN_PWR		HTCHERMES_EGPIO(4, 2)
#define EGPIO_NR_HTCHERMES_PWR_IN_HIGHPWR	HTCHERMES_EGPIO(4, 1)
#define EGPIO_NR_HTCHERMES_PWR_CHARGE		HTCHERMES_EGPIO(0, 10)


/****************************************************************
 * LEDS
 ****************************************************************/

#define GPIO_NR_HTCHERMES_FLASHLIGHT		S3C2410_GPC6
#define GPIO_NR_HTCHERMES_VIBRA	 		S3C2410_GPB4
#define EGPIO_NR_HTCHERMES_KBD_BACKLIGHT	HTCHERMES_EGPIO(1, 3)


/****************************************************************
 * USB
 ****************************************************************/

#define GPIO_NR_HTCHERMES_USB_PUEN		S3C2410_GPH10


#endif /* htchermes-gpio.h */

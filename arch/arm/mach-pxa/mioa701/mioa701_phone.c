/* Phone interface driver for Sagem XS200 on MIO A701
 *
 * 2007-12-12 Robert Jarzmik
 *
 * This code is licenced under the GPLv2.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <asm/gpio.h>
#include <asm/hardware.h>
#include <asm/arch/serial.h>

#include "mioa701.h"

#define PWER_GPIO113 (0x2 << 19)
#define OUTLO (GPIO_OUT | GPIO_DFLT_LOW)
#define OUTHI (GPIO_OUT | GPIO_DFLT_HIGH)

extern struct platform_pxa_serial_funcs mioa701_phone_funcs;

static int is_phone_on(void)
{
	return (gpio_get_value(MIO_GPIO_GSM_MOD_ON_STATE) != 0);
}

irqreturn_t phone_power_irq(int irq, void* dev_id, struct pt_regs *regs)
{
	printk("Mioa701: GSM power changed to %s\n",
	       is_phone_on() ? "on" : "off");
	return IRQ_HANDLED;
}

static void phone_init_gpios(void)
{
	/* configure XS200 GPIOS */
	pxa_gpio_mode(MIO_GPIO_GSM_nMOD_ON_CMD         | OUTHI);
	pxa_gpio_mode(MIO_GPIO_GSM_nMOD_OFF_CMD        | OUTHI);
	pxa_gpio_mode(MIO_GPIO_GSM_nMOD_DTE_UART_STATE | OUTHI);
	pxa_gpio_mode(MIO_GPIO_GSM_MOD_RESET_CMD       | OUTLO);

	pxa_gpio_mode(MIO_GPIO_GSM_EVENT | GPIO_IN);
}

static void ffuart_init_gpios(void)
{
	pxa_gpio_mode(MIO_GPIO_GSM_RXD_MD);
	pxa_gpio_mode(MIO_GPIO_GSM_CTS_MD);
	pxa_gpio_mode(MIO_GPIO_GSM_DCD_MD);
	pxa_gpio_mode(MIO_GPIO_GSM_DSR_MD);
	pxa_gpio_mode(MIO_GPIO_GSM_TXD_MD);
	pxa_gpio_mode(MIO_GPIO_GSM_DTR_MD);
	pxa_gpio_mode(MIO_GPIO_GSM_RTS_MD);
}

static void phone_reset(void)
{
	phone_init_gpios();
	msleep(20);
	gpio_set_value(MIO_GPIO_GSM_MOD_RESET_CMD, 1);
	msleep(110);
	gpio_set_value(MIO_GPIO_GSM_MOD_RESET_CMD, 0);
}

static void phone_off(void)
{
	gpio_set_value(MIO_GPIO_GSM_nMOD_DTE_UART_STATE, 0);
	msleep(110);
	gpio_set_value(MIO_GPIO_GSM_nMOD_DTE_UART_STATE, 1);
	gpio_set_value(MIO_GPIO_GSM_nMOD_OFF_CMD, 0);
	msleep(110);
	gpio_set_value(MIO_GPIO_GSM_nMOD_OFF_CMD, 1);
	msleep(110);
	printk("MioA701 phone: GSM turned off.\n");
}

static void phone_on(void)
{
	int tries;

	phone_init_gpios();
	if (is_phone_on())
		phone_off();
	phone_reset();
	msleep(300);
	gpio_set_value(MIO_GPIO_GSM_nMOD_ON_CMD, 0);
	msleep(110);
	gpio_set_value(MIO_GPIO_GSM_nMOD_ON_CMD, 1);

	tries = 0;
	do {
		msleep(50);
	} while ( (FFMSR & MSR_CTS) && (tries++ < 300) );
	if (tries >= 300) {
		printk("MioA701 phone: expect garbage at GSM start.\n");
	} else {
		printk("MioA701 phone: GSM turned on in %d ms\n", tries*50);
	}
}

static void mioa701_phone_configure(int state)
{
	switch (state) {
	
	case PXA_UART_CFG_PRE_STARTUP:
		break;

	case PXA_UART_CFG_POST_STARTUP:
		//phone_on();
		try_module_get(THIS_MODULE);
		break;

	case PXA_UART_CFG_PRE_SHUTDOWN:
		//phone_off();
		break;

	case PXA_UART_CFG_POST_SHUTDOWN:
		module_put(THIS_MODULE);

	default:
		break;
	}
}


static int mioa701_phone_probe(struct platform_device *dev)
{
	phone_init_gpios();
	ffuart_init_gpios();
	phone_on();

	set_irq_type (gpio_to_irq(MIO_GPIO_GSM_MOD_ON_STATE), IRQT_BOTHEDGE);
	return request_irq(gpio_to_irq(MIO_GPIO_GSM_MOD_ON_STATE), phone_power_irq,
			  IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			  "GSM XS200 Power Irq", NULL);
}

static int
mioa701_phone_remove(struct platform_device *dev)
{
	phone_off();
	free_irq(gpio_to_irq(MIO_GPIO_GSM_MOD_ON_STATE), NULL);
	return 0;
}

static int mioa701_phone_suspend(struct platform_device *dev, pm_message_t state)
{
	PWER |= PWER_GPIO113;
	return 0;
}

static struct platform_driver phone_driver = {
	.driver	  = {
		.name	  = "mioa701-phone",
	},
	.probe	  = mioa701_phone_probe,
	.remove	  = mioa701_phone_remove,
	.suspend  = mioa701_phone_suspend,
};

static int __init mioa701_phone_init( void )
{
	printk(KERN_NOTICE "mioa701 Phone Driver\n");
	mioa701_phone_funcs.configure = mioa701_phone_configure;
	pxa_set_ffuart_info(&mioa701_phone_funcs);

	/* If out of windows through Haret, let's do some cleanup */
	phone_init_gpios();
	phone_off();
	phone_reset();

	return platform_driver_register( &phone_driver );
}

static void __exit mioa701_phone_exit( void )
{
	pxa_set_ffuart_info(NULL);
	mioa701_phone_funcs.configure = NULL;
	platform_driver_unregister(&phone_driver);
}

module_init(mioa701_phone_init);
module_exit(mioa701_phone_exit);

MODULE_AUTHOR("Robert Jarzmik");
MODULE_DESCRIPTION("MIO A701 Phone Support Driver");
MODULE_LICENSE("GPL");

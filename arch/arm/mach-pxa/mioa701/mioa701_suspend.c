/* Power managment interface from MIO A701
 *
 * 2007-1-12 Robert Jarzmik
 *
 * This code is licenced under the GPLv2.
 *
 * MIO A701 reboot sequence is highly ROM dependant. From the one dissassembled,
 * this sequence is as follows :
 *   - disables interrupts
 *   - initialize SDRAM (self refresh RAM into active RAM)
 *   - initialize GPIOs (depends on value at 0xa020b020)
 *   - initialize coprossessors
 *   - if edge detect on PWR_SCL(GPIO3), then proceed to cold start
 *   - or if value at 0xa020b000 not equal to 0x0f0f0f0f, proceed to cold start
 *   - else do a resume, ie. jump to addr 0xa0100000
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <asm/gpio.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxa-pm_ll.h>

#include "mioa701.h"

#define RESUME_ENABLE_ADDR 0xa020b000
#define RESUME_ENABLE_VAL  0x0f0f0f0f
#define RESUME_BT_ADDR     0xa020b020
#define RESUME_VECTOR_ADDR 0xa0100000
#define BOOTSTRAP_WORDS    mioa701_bootstrap_lg/4

/* Assembler externals mioa701_bootresume.S */
extern u32 mioa701_bootstrap;
extern u32 mioa701_jumpaddr;
extern u32 mioa701_bootstrap_lg;


static u32 *save_buffer;

static void install_bootstrap(unsigned long resume_addr)
{
	int i;
	u32 *rom_bootstrap  = phys_to_virt(RESUME_VECTOR_ADDR);
	u32 *src = &mioa701_bootstrap;

	mioa701_jumpaddr = resume_addr;
	for (i=0; i < BOOTSTRAP_WORDS; i++)
		rom_bootstrap[i] = src[i];
}


static void mioa701_pxa_ll_pm_suspend(unsigned long resume_addr)
{
	int i = 0;
	u32 *mem_resume_vector  = phys_to_virt(RESUME_VECTOR_ADDR);
	u32 *mem_resume_enabler = phys_to_virt(RESUME_ENABLE_ADDR);
	u32 *mem_resume_bt      = phys_to_virt(RESUME_BT_ADDR);

	for (i=0; i< BOOTSTRAP_WORDS; i++)
		save_buffer[i] = mem_resume_vector[i];
	save_buffer[i++] = *mem_resume_enabler;
	save_buffer[i++] = *mem_resume_bt;

	*mem_resume_enabler = RESUME_ENABLE_VAL;
	*mem_resume_bt      = 0;

	install_bootstrap(resume_addr);
}

static void mioa701_pxa_ll_pm_resume(void)
{
	int i = 0;
	u32 *mem_resume_vector  = phys_to_virt(RESUME_VECTOR_ADDR);
	u32 *mem_resume_enabler = phys_to_virt(RESUME_ENABLE_ADDR);
	u32 *mem_resume_bt      = phys_to_virt(RESUME_BT_ADDR);

	for (i=0; i<BOOTSTRAP_WORDS; i++)
		mem_resume_vector[i] = save_buffer[i];
	*mem_resume_enabler = save_buffer[i++];
	*mem_resume_bt      = save_buffer[i++];

	OSCC |= OSCC_TOUT_EN | OSCC_OON;
}

static struct pxa_ll_pm_ops mioa701_ll_pm_ops = {
	.suspend = mioa701_pxa_ll_pm_suspend,
	.resume  = mioa701_pxa_ll_pm_resume,
};

static int mioa701_inputdev_init(void)
{
	int rc;

	/* Register the input device */
	mioa701_evdev = input_allocate_device();
	if (!mioa701_evdev)
		return -ENOMEM;
	mioa701_evdev->name = "Mioa701 Events";
	mioa701_evdev->phys = "mioa701/input0";
	mioa701_evdev->id.bustype = BUS_HOST;
	mioa701_evdev->id.vendor = 0x0001;
	mioa701_evdev->id.product = 0x0001;
	mioa701_evdev->id.version = 0x0100;

	mioa701_evdev->evbit[0] = BIT(EV_SW) | BIT(EV_MSC);
	set_bit(SW_HEADPHONE_INSERT, mioa701_evdev->swbit);
	set_bit(MSC_SERIAL, mioa701_evdev->mscbit);

	rc = input_register_device(mioa701_evdev);
	if (rc) {
		input_free_device(mioa701_evdev);
		mioa701_evdev = NULL;
	}

	return rc;
}

static int mioa701_pm_probe(struct platform_device *dev)
{
	if (mioa701_inputdev_init())
		printk(KERN_NOTICE
		       "Mioa701: Couldn't register mioa701 main input device");
	return 0;
}

static int mioa701_pm_remove(struct platform_device *dev)
{
	return 0;
}

static int mioa701_pm_suspend(struct platform_device *dev, pm_message_t state)
{
	PWER |=   PWER_GPIO0 
		| PWER_GPIO1 /* reset */
		| PWER_RTC;
	PRER |=   PWER_GPIO0
		| PWER_GPIO1
		| PWER_RTC;
	// haret: PWER = 0x8010e801 (WERTC | WEUSIM | WE15 | WE14 | WE13 | WE11 | WE0
	// haret: PSSR = 0x00000005
	// haret: PRER = 0x0000e001 (RE15 | RE14 | RE13 | RE0)

	// PGSR0 = 0x02b0401a; // GPIOs: 1, 3, 4, 14, 20, 21, 23(gps), 25(gsm)
	/* I2C bus */
	PGSR0 |= GPIO_bit(3) | GPIO_bit(4);
	/* Unknown GPIO14, GPIO20, GPIO21 */
	PGSR0 |= GPIO_bit(14) | GPIO_bit(20) | GPIO_bit(21);

	// PGSR1 = 0x02525c96; // GPIOs: 33, 34(gsm), 36(gsm), 39(gsm), 42(bt),
	//                               43(bt), 44(bt), 46(gps), 49, 52, 54, 57
	/* Unknown GPIO33, GPIO49, GPIO57 */
	PGSR1 |= GPIO_bit(33) | GPIO_bit(49) | GPIO_bit(57);

	// PGSR2 = 0x054d2000; // GPIOs: 77, 80, 82(leds), 83(bt), 86, 88(gsm), 90(gsm)
	/* Unknown GPIO77, GPIO80, GPIO86 */
	PGSR2 |= GPIO_bit(77) | GPIO_bit(80) | GPIO_bit(86) ;

	// PGSR3 = 0x007e038c; // GPIOs: 98(leds), 99(sound), 103(key), 104(key)
	//				105(key), 113(gsm), 114(gsm), 115(led), 
	//				116, 117(i2c), 118(i2c)
	/* Keyboard */
	PGSR3 |= GPIO_bit(103) | GPIO_bit(104) | GPIO_bit(105);
	/* Power I2C */
	PGSR3 |= GPIO_bit(117) | GPIO_bit(118);
	/* Unknown GPIO 116 */
	PGSR3 |= GPIO_bit(116);

	/*
	PGSR0 = 0x02b0401a; // GPIOs: 1, 3, 4, 14, 20, 21, 23(gps), 25(gsm)
	PGSR1 = 0x02525c96; // GPIOs: 33, 34(gsm), 36(gsm), 39(gsm), 42(bt),
	PGSR2 = 0x054d2000; // GPIOs: 77, 80, 82(leds), 83(bt), 86, 88(gsm), 90(gsm)
	PGSR3 = 0x007e038c; // GPIOs: 98(leds), 99(sound), 103(key), 104(key)
	*/


	/* 3.6864 MHz oscillator power-down enable */
	//PCFR  = 0x000004f1; // PCFR_FVC | PCFR_DCEN || PCFR_PI2CEN | PCFR_GPREN | PCFR_OPDE
	PCFR = PCFR_OPDE | PCFR_PI2CEN | PCFR_GPROD | PCFR_GPR_EN;

	PSLR  = 0xff100000; /* SYSDEL=125ms, PWRDEL=125ms, PSLR_SL_ROD=1 */

	return 0;
}

static int mioa701_pm_resume(struct platform_device *dev)
{
	return 0;
}

static struct platform_driver mioa701_pm = {
        .driver = {
		.name = "mioa701-pm",
        },
	.probe   = mioa701_pm_probe,
	.remove  = mioa701_pm_remove,
	.suspend = mioa701_pm_suspend,
	.resume  = mioa701_pm_resume,
};

static void init_registers(void)
{
	PWER = 0;
	PFER = 0;
	PRER = 0;
	PGSR0 = 0;
	PGSR1 = 0;
	PGSR2 = 0;
	PGSR3 = 0;
}

int mioa701_suspend_init(void)
{
	save_buffer = kmalloc(mioa701_bootstrap_lg+2*sizeof(u32), GFP_KERNEL);
	if (!save_buffer)
		return -ENOMEM;
	pxa_pm_set_ll_ops(&mioa701_ll_pm_ops);
	init_registers();
        return platform_driver_register(&mioa701_pm);
}

void mioa701_suspend_exit(void)
{
	platform_driver_unregister(&mioa701_pm);
	if (save_buffer)
		kfree(save_buffer);
	printk(KERN_CRIT "Unregistering mioa701 suspend will hang next"
	       "resume !!!\n");
}

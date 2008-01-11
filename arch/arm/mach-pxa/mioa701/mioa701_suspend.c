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

static int mioa701_pm_probe(struct platform_device *dev)
{
	return 0;
}

static int mioa701_pm_remove(struct platform_device *dev)
{
	return 0;
}

static int mioa701_pm_suspend(struct platform_device *dev, pm_message_t state)
{
	PWER |=   PWER_GPIO0 
		| PWER_GPIO1; /* reset */
	// haret: PWER = 0x8010e801 (WERTC | WEUSIM | WE15 | WE14 | WE13 | WE11 | WE0
	// haret: PSSR = 0x00000005
	// haret: PRER = 0x0000e001 (RE15 | RE14 | RE13 | RE0)
	PGSR0 = 0x02b0401a;
	PGSR1 = 0x02525c96;
	PGSR2 = 0x054d2000;
	PGSR3 = 0x007e038c;

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
	.probe = mioa701_pm_probe,
	.remove = mioa701_pm_remove,
	.suspend = mioa701_pm_suspend,
	.resume = mioa701_pm_resume,
};

int mioa701_suspend_init(void)
{
	save_buffer = kmalloc(mioa701_bootstrap_lg+2*sizeof(u32), GFP_KERNEL);
	if (!save_buffer)
		return -ENOMEM;
	pxa_pm_set_ll_ops(&mioa701_ll_pm_ops);
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

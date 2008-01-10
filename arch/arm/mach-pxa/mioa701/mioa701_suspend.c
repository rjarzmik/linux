/* Power managment interface from MIO A701
 *
 * 2007-1-12 Robert Jarzmik
 *
 * This code is licenced under the GPLv2.
 *
 * MIO A701 reboot sequence is highly ROM dependant. From the one dissambled,
 * this sequence is as follows :
 *   - disables interrupts
 *   - initialize SDRAM
 *   - initialize GPIOs (depends on value at 0xa020b020)
 *   - initialize coprossessors
 *   - if edge detect on PWR_SCL(GPIO3), the proceed to cold start
 *   - or if value at 0xa020b000 not equal to 0x0f0f0f0f, proceed to cold start
 *   - else do a resume, ie. jump to addr 0xa0100000
 */

#include <linux/module.h>
#include <asm/gpio.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxa-pm_ll.h>

#include "mioa701.h"

#define RESUME_ENABLE_ADDR 0xa020b000
#define RESUME_ENABLE_VAL  0x0f0f0f0f
#define RESUME_GSM_ADDR    0xa020b020
#define RESUME_VECTOR_ADDR 0xa0100000


#define nbu32_vibrate_nommu 23
static u32 save_buffer[nbu32_vibrate_nommu+3];

unsigned long inresume = 0;

static	u32 vibrate_nommu[] = {
//00000060 <rjk_vibrate_nommu>:
	0xe3a0120a, //        mov     r1, #-1610612736        ; 0xa0000000
	0xe3811602, //        orr     r1, r1, #2097152        ; 0x200000
	0xe3811a0b, //        orr     r1, r1, #45056  ; 0xb000
	0xe3a00000, //        mov     r0, #0  ; 0x0
	0xe5810000, //        str     r0, [r1]
	0xea000001, //        b       80 <n_begin1>

//00000078 <n_v1>:
	0x00000000, //        .word   0x0
	0x40e00004, //        .word   0x40e00004

//00000080 <n_begin1>:
	0xe3a01101, //        mov     r1, #1073741824 ; 0x40000000
	0xe3811000, //        orr     r1, r1, #0      ; 0x0
	0xe381160e, //        orr     r1, r1, #14680064       ; 0xe00000
	0xe1a02001, //        mov     r2, r1
	0xe281102c, //        add     r1, r1, #44     ; 0x2c
	0xe2822014, //        add     r2, r2, #20     ; 0x14
	0xe5910000, //        ldr     r0, [r1]
	0xe3800701, //        orr     r0, r0, #262144 ; 0x40000

//	0xe5810000, //        str     r0, [r1]
	0xe3811000, //        orr     r1, r1, #0      ; 0x0

	0xe5920000, //        ldr     r0, [r2]
	0xe3800701, //        orr     r0, r0, #262144 ; 0x40000

//	0xe5810000, //        str     r0, [r1]
	0xe3811000, //        orr     r1, r1, #0      ; 0x0

//000000b0 <n_jump>:
	0xe51f0040, //        ldr     r0, [pc, #-64]  ; 78 <n_v1>
	0xe1a0f000, //        mov     pc, r0
	0xeafffffe, //        b       b8 <n_jump+0x8>
};

static	u32 vibrate_mmu[] = {
	0xe3a0120f, //        mov     r1, #-268435456         ; 0xf0000000
	0xe3811402, //        orr     r1, r1, #33554432       ; 0x02000000
	0xe381160e, //        orr     r1, r1, #14680064       ; 0x00e00000
	0xe1a02001, //        mov     r2, r1
	0xe281102c, //        add     r1, r1, #44     ; 0x2c
	0xe2822014, //        add     r2, r2, #20     ; 0x14
	0xe5910000, //        ldr     r0, [r1]
	0xe3800701, //        orr     r0, r0, #262144 ; 0x40000
	0xe5810000, //        str     r0, [r1]
	0xe5920000, //        ldr     r0, [r2]
	0xe3800701, //        orr     r0, r0, #262144 ; 0x40000
	0xe5810000, //        str     r0, [r1]
	0xeafffffe, //        b       38 <m_l1+0x18>
};

void mioa701_pxa_ll_pm_suspend(unsigned long resume_addr)
{
	int i = 0;
	u32 tmp, pc, sp, mmu;
	u32 *mem_resume_vector  = phys_to_virt(RESUME_VECTOR_ADDR);
	u32 *mem_resume_enabler = phys_to_virt(RESUME_ENABLE_ADDR);
	u32 *mem_resume_gsm     = phys_to_virt(RESUME_GSM_ADDR);

	/* RJK */
	printk("RJK: ready to die in an endless loop\n");

	for (i=0; i< nbu32_vibrate_nommu; i++)
		save_buffer[i] = mem_resume_vector[i];
	save_buffer[i++] = *mem_resume_enabler;
	save_buffer[i++] = *mem_resume_gsm;

	/* RJK */
	asm("mov %0, pc":"=r"(tmp)); pc = tmp;
	asm("mov %0, sp":"=r"(tmp)); sp = tmp;
	asm("mrc p15, 0, %0, c2, c0, 0":"=r"(tmp)); mmu = tmp;
	printk ("RJK_SUSPEND: cp=%p, sp=%p, mmu=%p\n", pc, sp, mmu);
		
	*mem_resume_enabler = RESUME_ENABLE_VAL;
	*mem_resume_gsm     = 1;

	vibrate_nommu[6] = resume_addr;
	for (i=0; i < nbu32_vibrate_nommu; i++)
		mem_resume_vector[i] = vibrate_nommu[i];
}

static void mioa701_pxa_ll_pm_resume(void)
{
	int i = 0;
	u32 *mem_resume_vector  = phys_to_virt(RESUME_VECTOR_ADDR);
	u32 *mem_resume_enabler = phys_to_virt(RESUME_ENABLE_ADDR);
	u32 *mem_resume_gsm     = phys_to_virt(RESUME_GSM_ADDR);

	for (i=0; i<nbu32_vibrate_nommu; i++)
		mem_resume_vector[i] = save_buffer[i];
	*mem_resume_enabler = save_buffer[i++];
	*mem_resume_gsm     = save_buffer[i++];

	OSCC |= OSCC_TOUT_EN | OSCC_OON;
	inresume = 1;
 	//printk("RJK: Are you ready to vibrate ?n");
	/*__asm__ __volatile__ (
	   "mov r1, %0\n"
	   "mov pc, r1\n"
	   :
	   : "r" (vibrate_mmu)
	   : "r1"); */
	//gpio_set_value(GPIO_MIOA701_LED_nOrange, 0);
}

static struct pxa_ll_pm_ops mioa701_ll_pm_ops = {
	.suspend = mioa701_pxa_ll_pm_suspend,
	.resume  = mioa701_pxa_ll_pm_resume,
};

void mioa701_ll_pm_init(void) {
	printk("RJK: suspend under work ... \n");
	pxa_pm_set_ll_ops(&mioa701_ll_pm_ops);
}

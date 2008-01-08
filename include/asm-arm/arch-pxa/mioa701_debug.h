/*
 * @author Robert Jarzmik
 * @brief Debug include to track resume failures
 * Vibrator is actived in MMU mode, but can work with IRQs disabled
 */

#ifndef MIOA071_DEBUG_H
#define MIOA071_DEBUG_H

/* RJK */
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#define OSCR_FREQ		CLOCK_TICK_RATE
/* RJK */

static int *mysleep(unsigned int secs, int r) {
	/* Assume 524 MHz */
	unsigned long i,j;
	static int tab[4] = {1, 5, 3, 9};

	for (i=0; i<5*secs; i++) {
		tab[0] = tab[i%r] * tab[i%r];
		for (j=0; j<1000000; j++)
			tab[r] = tab[j%r];
	}
	return tab;
}

static void vibrate_once(int duration)
{
	GPCR2 |= 0x40000;
	mysleep(duration,1);
	GPSR2 |= 0x40000;
	mysleep(3,2);
}

static void do_orangeled_on(void) {
	GPCR3 |= 0x4;
	GPDR3 |= 0x4;
}


static void do_vibrate(void) {
	__asm__ __volatile__("                \n\
	.align	5                             \n\
	mov	r1,     #0xf0000000           \n\
	orr	r1, r1, #0x02000000           \n\
	orr	r1, r1, #0x00e00000           \n\
	mov	r2, r1                        \n\
	add	r1, r1, #44 ; r1 = 0xf20e002c \n\
	add	r2, r2, #20 ; r2 = 0xf20e0014 \n\
	ldr	r0, [r1]                      \n\
	orr	r0, r0, #0x40000              \n\
	str	r0, [r1]                      \n\
	ldr	r0, [r2]                      \n\
	orr	r0, r0, #0x40000              \n\
	str	r0, [r2]                      \n\
;vib_loop:                                     \n\
;	b	vib_loop"
			      : // output regs
			      : // input regs
			      : "r0", "r1", "r2");
	return;
}

static const unsigned char *morse[] = {
	".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", /* A-H */
	"..", ".---.", "-.-", ".-..", "--", "-.", "---", ".--.", /* I-P */
	"--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", /* Q-X */
	"-.--", "--..", /* Y-Z */
	"-----", ".----", "..---", "...--", "....-", /* 0-4 */
	".....", "-....", "--...", "---..", "----." /* 5-9 */
};

static void say_car_morse(const char *sign)
{
	unsigned int i = 0;
	char c;

	while (c = sign[i++])
		switch (c) {
		case '.':
			vibrate_once(1);
			break;
		case '-':
			vibrate_once(3);
			break;
		}
}

static void talk_morse(const char *talk)
{
	unsigned char c, *mcode;
	unsigned int pos = 0;

	while (c = talk[pos++]) {
		mcode = NULL;
		if ((c >= 'a') && (c <= 'z'))
			c -= ('a' - 'A');
		if ((c >= 'A') && (c <= 'Z'))
			mcode = morse[c - 'A'];
		if ((c >= '0') && (c <= '9'))
			mcode = morse[c - '0' + 26];
		if (mcode) {
			say_car_morse(mcode);
			mysleep(2,1);
		}
	}
}

#endif

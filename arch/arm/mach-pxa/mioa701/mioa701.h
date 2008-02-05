#ifndef _MIOA701_H_
#define _MIOA701_H_

#include <asm/arch/pxa-regs.h>

/* detect the connexion of an USB cable */
#define MIO_GPIO_USB_DETECT		13

/* probably (XXX: untested) linked to USB enable */
#define MIO_GPIO_USB_EN0		9
#define MIO_GPIO_USB_EN1		22

/* SDIO bits */
#define MIO_GPIO_SDIO_RO		78
#define MIO_GPIO_SDIO_INSERT		15
#define MIO_GPIO_SDIO_EN		91

/* Bluetooth */
#define MIO_GPIO_BT_RXD_MD		GPIO42_BTRXD_MD
#define MIO_GPIO_BT_TXD_MD		GPIO43_BTTXD_MD
#define MIO_GPIO_BT_CTS_MD		GPIO44_BTCTS_MD
#define MIO_GPIO_BT_RTS_MD		GPIO45_BTRTS_MD
#define MIO_GPIO_BT_ON			83

/* GPS */
#define MIO_GPIO_GPS_RXD_MD		GPIO46_STRXD_MD
#define MIO_GPIO_GPS_TXD_MD		GPIO47_STTXD_MD

/* GSM */
#define MIO_GPIO_GSM_OFF		24
#define MIO_GPIO_GSM_PREP		88
#define MIO_GPIO_GSM_EVENT		113
#define MIO_GPIO_GSM_UNKNOWN1		114
#define MIO_GPIO_GSM_UNKNOWN2		90
#define MIO_GPIO_GSM_RXD_MD		GPIO34_FFRXD_MD
#define MIO_GPIO_GSM_CTS_MD		GPIO35_FFCTS_MD
#define MIO_GPIO_GSM_DCD_MD		GPIO36_FFDCD_MD
#define MIO_GPIO_GSM_DSR_MD		GPIO37_FFDSR_MD
#define MIO_GPIO_GSM_TXD_MD		GPIO39_FFTXD_MD
#define MIO_GPIO_GSM_DTR_MD		GPIO40_FFDTR_MD
#define MIO_GPIO_GSM_RTS_MD		GPIO41_FFRTS_MD

/* SOUND */
#define GPIO89_AC97_SYSCLK_MD			(89 | GPIO_ALT_FN_1_OUT)
#define MIO_GPIO_HPJACK_INSERT		12

/* LEDS */
#define MIO_GPIO_LED_nBLUE		97
#define MIO_GPIO_LED_nOrange		98
#define MIO_GPIO_LED_nVibra		82
#define MIO_GPIO_LED_nKeyboard		115

#endif /* _MIOA701_H */

#ifndef _MIOA701_H_
#define _MIOA701_H_

/* detect the connexion of an USB cable */
#define MIOA701_USB_DETECT 13

/* probably (XXX: untested) linked to USB enable */
#define MIOA701_USB_EN0 9
#define MIOA701_USB_EN1 22
#define MIOA701_GSM_EN0 25

/* SDIO bits */
#define MIOA701_SDIO_RO     78
#define MIOA701_SDIO_INSERT 15
#define MIOA701_SDIO_EN     91

/* Bluetooth */
#define GPIO_NR_MIOA701_BT_RXD_MD		(42 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_MIOA701_BT_TXD_MD		(43 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_MIOA701_BT_UART_CTS_MD		(44 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_MIOA701_BT_UART_RTS_MD		(45 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_MIOA701_BT_ON			83

/* GPS */
#define GPIO_NR_MIOA701_GPS_UART_RXD		(46 | GPIO_ALT_FN_2_IN)
#define GPIO_NR_MIOA701_GPS_UART_TXD		(47 | GPIO_ALT_FN_1_OUT)

/* GSM */
#define GPIO_NR_MIOA701_GSM_OFF			24
#define GPIO_NR_MIOA701_GSM_PREP		88
#define GPIO_NR_MIOA701_GSM_UART_RXD		(34 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_MIOA701_GSM_UART_CTS		(35 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_MIOA701_GSM_UART_DCD		(36 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_MIOA701_GSM_UART_DSR		(37 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_MIOA701_GSM_UART_TXD		(39 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_MIOA701_GSM_UART_DTR		(40 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_MIOA701_GSM_UART_RTS		(41 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_MIOA701_GSM_EVENT		113
#define GPIO_NR_MIOA701_GSM_UNKNOWN1		114
#define GPIO_NR_MIOA701_GSM_UNKNOWN2		90

/* SOUND */
#define GPIO89_AC97_SYSCLK_MD			(89 | GPIO_ALT_FN_1_OUT)

/* LEDS */
#define GPIO_MIOA701_LED_nBLUE			97
#define GPIO_MIOA701_LED_nOrange		98
#define GPIO_MIOA701_LED_nVibra			82
#define GPIO_MIOA701_LED_nKeyboard		115


struct mioa701_bt_funcs {
	void (*configure) ( int state );
};

struct mioa701_phone_funcs {
	void (*configure) ( int state );
};

struct mioa701_gps_funcs {
	void (*configure) ( int state );
};

#endif /* _MIOA701_H */

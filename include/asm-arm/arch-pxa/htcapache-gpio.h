/* HTC Apache GPIOs */
#ifndef _HTCAPACHE_GPIO_H_
#define _HTCAPACHE_GPIO_H_


/****************************************************************
 * Micro-controller interface
 ****************************************************************/

#define GPIO_NR_HTCAPACHE_MC_SDA	56
#define GPIO_NR_HTCAPACHE_MC_SCL	57
#define GPIO_NR_HTCAPACHE_MC_IRQ	14


/****************************************************************
 * EGPIO
 ****************************************************************/

#define GPIO_NR_HTCAPACHE_EGPIO_IRQ	15
#define HTCAPACHE_EGPIO_BASE		0x100 /* GPIO_BASE_INCREMENT */
#define HTCAPACHE_EGPIO(reg,bit) (HTCAPACHE_EGPIO_BASE + 16*(reg) + (bit))


/****************************************************************
 * Power
 ****************************************************************/

#define EGPIO_NR_HTCAPACHE_PWR_IN_PWR		HTCAPACHE_EGPIO(0, 0)
#define EGPIO_NR_HTCAPACHE_PWR_IN_HIGHPWR	HTCAPACHE_EGPIO(0, 7)
#define EGPIO_NR_HTCAPACHE_PWR_CHARGE		HTCAPACHE_EGPIO(2, 8)
#define EGPIO_NR_HTCAPACHE_PWR_HIGHCHARGE	HTCAPACHE_EGPIO(2, 7)


/****************************************************************
 * Sound
 ****************************************************************/

#define EGPIO_NR_HTCAPACHE_SND_POWER	HTCAPACHE_EGPIO(1, 5)  // XXX - not sure
#define EGPIO_NR_HTCAPACHE_SND_RESET	HTCAPACHE_EGPIO(1, 8)  // XXX - not sure
#define EGPIO_NR_HTCAPACHE_SND_PWRJACK	HTCAPACHE_EGPIO(1, 7)
#define EGPIO_NR_HTCAPACHE_SND_PWRSPKR	HTCAPACHE_EGPIO(1, 6)
#define EGPIO_NR_HTCAPACHE_SND_IN_JACK	HTCAPACHE_EGPIO(0, 2)


/****************************************************************
 * Touchscreen
 ****************************************************************/

#define GPIO_NR_HTCAPACHE_TS_PENDOWN	36
#define GPIO_NR_HTCAPACHE_TS_DAV	114
#define GPIO_NR_HTCAPACHE_TS_ALERT	20  // XXX - not sure


/****************************************************************
 * LEDS
 ****************************************************************/

#define GPIO_NR_HTCAPACHE_LED_FLASHLIGHT	87
#define EGPIO_NR_HTCAPACHE_LED_VIBRA	 	HTCAPACHE_EGPIO(2, 3)
#define EGPIO_NR_HTCAPACHE_LED_KBD_BACKLIGHT	HTCAPACHE_EGPIO(2, 2)


/****************************************************************
 * BlueTooth
 ****************************************************************/

#define EGPIO_NR_HTCAPACHE_BT_POWER	HTCAPACHE_EGPIO(1, 11)
#define EGPIO_NR_HTCAPACHE_BT_RESET	HTCAPACHE_EGPIO(1, 10)


/****************************************************************
 * Wifi
 ****************************************************************/

#define EGPIO_NR_HTCAPACHE_WIFI_POWER1	HTCAPACHE_EGPIO(1, 4)
#define EGPIO_NR_HTCAPACHE_WIFI_POWER2	HTCAPACHE_EGPIO(1, 1)
#define EGPIO_NR_HTCAPACHE_WIFI_POWER3	HTCAPACHE_EGPIO(1, 0)
#define EGPIO_NR_HTCAPACHE_WIFI_RESET	HTCAPACHE_EGPIO(1, 3)
#define EGPIO_NR_HTCAPACHE_WIFI_IN_IRQ	HTCAPACHE_EGPIO(0, 5)


/****************************************************************
 * Mini-SD card
 ****************************************************************/

#define GPIO_NR_HTCAPACHE_SD_CARD_DETECT_N	13
#define GPIO_NR_HTCAPACHE_SD_POWER_N		89


/****************************************************************
 * USB
 ****************************************************************/

#define GPIO_NR_HTCAPACHE_USB_PUEN	99
#define EGPIO_NR_HTCAPACHE_USB_PWR	HTCAPACHE_EGPIO(2, 5)


/****************************************************************
 * Buttons on side
 ****************************************************************/

#define GPIO_NR_HTCAPACHE_BUTTON_POWER		0
#define GPIO_NR_HTCAPACHE_BUTTON_RECORD		95
#define GPIO_NR_HTCAPACHE_BUTTON_VOLUP		9
#define GPIO_NR_HTCAPACHE_BUTTON_VOLDOWN	10
#define GPIO_NR_HTCAPACHE_BUTTON_BROWSER	94
#define GPIO_NR_HTCAPACHE_BUTTON_CAMERA		93


/****************************************************************
 * Pull out keyboard
 ****************************************************************/

#define GPIO_NR_HTCAPACHE_KP_PULLOUT	116

#define GPIO_NR_HTCAPACHE_KP_PULLOUT	116

#define GPIO_NR_HTCAPACHE_KP_MKIN0	100
#define GPIO_NR_HTCAPACHE_KP_MKIN1	101
#define GPIO_NR_HTCAPACHE_KP_MKIN2	102
#define GPIO_NR_HTCAPACHE_KP_MKIN3	37
#define GPIO_NR_HTCAPACHE_KP_MKIN4	38
#define GPIO_NR_HTCAPACHE_KP_MKIN5	90
#define GPIO_NR_HTCAPACHE_KP_MKIN6	91

#define GPIO_NR_HTCAPACHE_KP_MKOUT0	103
#define GPIO_NR_HTCAPACHE_KP_MKOUT1	104
#define GPIO_NR_HTCAPACHE_KP_MKOUT2	105
#define GPIO_NR_HTCAPACHE_KP_MKOUT3	106
#define GPIO_NR_HTCAPACHE_KP_MKOUT4	107
#define GPIO_NR_HTCAPACHE_KP_MKOUT5	108
#define GPIO_NR_HTCAPACHE_KP_MKOUT6	40

#define GPIO_NR_HTCAPACHE_KP_MKIN0_MD 	(GPIO_NR_HTCAPACHE_KP_MKIN0 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_HTCAPACHE_KP_MKIN1_MD 	(GPIO_NR_HTCAPACHE_KP_MKIN1 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_HTCAPACHE_KP_MKIN2_MD 	(GPIO_NR_HTCAPACHE_KP_MKIN2 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_HTCAPACHE_KP_MKIN3_MD 	(GPIO_NR_HTCAPACHE_KP_MKIN3 | GPIO_ALT_FN_3_IN)
#define GPIO_NR_HTCAPACHE_KP_MKIN4_MD 	(GPIO_NR_HTCAPACHE_KP_MKIN4 | GPIO_ALT_FN_2_IN)
#define GPIO_NR_HTCAPACHE_KP_MKIN5_MD 	(GPIO_NR_HTCAPACHE_KP_MKIN5 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_HTCAPACHE_KP_MKIN6_MD 	(GPIO_NR_HTCAPACHE_KP_MKIN6 | GPIO_ALT_FN_1_IN)

#define GPIO_NR_HTCAPACHE_KP_MKOUT0_MD (GPIO_NR_HTCAPACHE_KP_MKOUT0 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_HTCAPACHE_KP_MKOUT1_MD (GPIO_NR_HTCAPACHE_KP_MKOUT1 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_HTCAPACHE_KP_MKOUT2_MD (GPIO_NR_HTCAPACHE_KP_MKOUT2 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_HTCAPACHE_KP_MKOUT3_MD (GPIO_NR_HTCAPACHE_KP_MKOUT3 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_HTCAPACHE_KP_MKOUT4_MD (GPIO_NR_HTCAPACHE_KP_MKOUT4 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_HTCAPACHE_KP_MKOUT5_MD (GPIO_NR_HTCAPACHE_KP_MKOUT5 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_HTCAPACHE_KP_MKOUT6_MD (GPIO_NR_HTCAPACHE_KP_MKOUT6 | GPIO_ALT_FN_1_OUT)

#endif

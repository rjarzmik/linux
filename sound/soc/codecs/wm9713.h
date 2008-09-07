/*
 * wm9713.h  --  WM9713 Soc Audio driver
 */

#ifndef _WM9713_H
#define _WM9713_H

/* MCLK clock inputs */
#define WM9713_CLKA_PIN			0
#define WM9713_CLKB_PIN			1

/* clock divider ID's */
#define WM9713_PCMCLK_DIV		0
#define WM9713_CLKA_MULT		1
#define WM9713_CLKB_MULT		2
#define WM9713_HIFI_DIV			3
#define WM9713_PCMBCLK_DIV		4

/* PCM clk div */
#define WM9713_PCMDIV(x)		((x - 1) << 8)

/* HiFi Div */
#define WM9713_HIFIDIV(x)		((x - 1) << 12)

/* MCLK clock mulitipliers */
#define WM9713_CLKA_X1			(0 << 1)
#define WM9713_CLKA_X2			(1 << 1)
#define WM9713_CLKB_X1			(0 << 2)
#define WM9713_CLKB_X2			(1 << 2)

/* MCLK clock MUX */
#define WM9713_CLK_MUX_A		(0 << 0)
#define WM9713_CLK_MUX_B		(1 << 0)

/* Voice DAI BCLK divider */
#define WM9713_PCMBCLK_DIV_1		(0 << 9)
#define WM9713_PCMBCLK_DIV_2		(1 << 9)
#define WM9713_PCMBCLK_DIV_4		(2 << 9)
#define WM9713_PCMBCLK_DIV_8		(3 << 9)
#define WM9713_PCMBCLK_DIV_16		(4 << 9)

/* WM9713 DAI ID's */
#define WM9713_DAI_AC97_HIFI		0
#define WM9713_DAI_AC97_AUX		1
#define WM9713_DAI_PCM_VOICE		2

extern const char wm9713_codec_id[];
extern const char wm9713_codec_hifi_dai_id[];
extern const char wm9713_codec_aux_dai_id[];
extern const char wm9713_codec_voice_dai_id[];

#endif

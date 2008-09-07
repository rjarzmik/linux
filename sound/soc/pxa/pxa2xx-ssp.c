/*
 * pxa2xx-ssp.c  --  ALSA Soc Audio Layer
 *
 * Copyright 2005 Wolfson Microelectronics PLC.
 * Author: Liam Girdwood
 *         liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  Revision history
 *    12th Aug 2005   Initial version.
 *
 * TODO:
 *  o Test network mode for > 16bit sample size
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <mach/hardware.h>
#include <mach/pxa-regs.h>
#include <mach/audio.h>
#include <mach/ssp.h>

#include "pxa2xx-pcm.h"

#define PXA_SSP_DEBUG 0

#if PXA_SSP_DEBUG
#define dbg(format, arg...) \
	printk(KERN_DEBUG format "\n" , ## arg)
#else
#define dbg(format, arg...) do {} while (0)
#endif

/*
 * SSP audio private data
 */
struct ssp_priv {
	unsigned int sysclk;
	struct ssp_dev dev;
#ifdef CONFIG_PM
	struct ssp_state state;
#endif
};

static struct pxa2xx_pcm_dma_params pxa2xx_ssp1_pcm_mono_out = {
	.name			= "SSP1 PCM Mono out",
	.dev_addr		= __PREG(SSDR_P1),
	.drcmr			= &DRCMRTXSSDR,
	.dcmd			= DCMD_INCSRCADDR | DCMD_FLOWTRG |
				  DCMD_BURST16 | DCMD_WIDTH2,
};

static struct pxa2xx_pcm_dma_params pxa2xx_ssp1_pcm_mono_in = {
	.name			= "SSP1 PCM Mono in",
	.dev_addr		= __PREG(SSDR_P1),
	.drcmr			= &DRCMRRXSSDR,
	.dcmd			= DCMD_INCTRGADDR | DCMD_FLOWSRC |
				  DCMD_BURST16 | DCMD_WIDTH2,
};

static struct pxa2xx_pcm_dma_params pxa2xx_ssp1_pcm_stereo_out = {
	.name			= "SSP1 PCM Stereo out",
	.dev_addr		= __PREG(SSDR_P1),
	.drcmr			= &DRCMRTXSSDR,
	.dcmd			= DCMD_INCSRCADDR | DCMD_FLOWTRG |
				  DCMD_BURST16 | DCMD_WIDTH4,
};

static struct pxa2xx_pcm_dma_params pxa2xx_ssp1_pcm_stereo_in = {
	.name			= "SSP1 PCM Stereo in",
	.dev_addr		= __PREG(SSDR_P1),
	.drcmr			= &DRCMRRXSSDR,
	.dcmd			= DCMD_INCTRGADDR | DCMD_FLOWSRC |
				  DCMD_BURST16 | DCMD_WIDTH4,
};

static struct pxa2xx_pcm_dma_params pxa2xx_ssp2_pcm_mono_out = {
	.name			= "SSP2 PCM Mono out",
	.dev_addr		= __PREG(SSDR_P2),
	.drcmr			= &DRCMRTXSS2DR,
	.dcmd			= DCMD_INCSRCADDR | DCMD_FLOWTRG |
				  DCMD_BURST16 | DCMD_WIDTH2,
};

static struct pxa2xx_pcm_dma_params pxa2xx_ssp2_pcm_mono_in = {
	.name			= "SSP2 PCM Mono in",
	.dev_addr		= __PREG(SSDR_P2),
	.drcmr			= &DRCMRRXSS2DR,
	.dcmd			= DCMD_INCTRGADDR | DCMD_FLOWSRC |
				  DCMD_BURST16 | DCMD_WIDTH2,
};

static struct pxa2xx_pcm_dma_params pxa2xx_ssp2_pcm_stereo_out = {
	.name			= "SSP2 PCM Stereo out",
	.dev_addr		= __PREG(SSDR_P2),
	.drcmr			= &DRCMRTXSS2DR,
	.dcmd			= DCMD_INCSRCADDR | DCMD_FLOWTRG |
				  DCMD_BURST16 | DCMD_WIDTH4,
};

static struct pxa2xx_pcm_dma_params pxa2xx_ssp2_pcm_stereo_in = {
	.name			= "SSP2 PCM Stereo in",
	.dev_addr		= __PREG(SSDR_P2),
	.drcmr			= &DRCMRRXSS2DR,
	.dcmd			= DCMD_INCTRGADDR | DCMD_FLOWSRC |
				  DCMD_BURST16 | DCMD_WIDTH4,
};

static struct pxa2xx_pcm_dma_params pxa2xx_ssp3_pcm_mono_out = {
	.name			= "SSP3 PCM Mono out",
	.dev_addr		= __PREG(SSDR_P3),
	.drcmr			= &DRCMRTXSS3DR,
	.dcmd			= DCMD_INCSRCADDR | DCMD_FLOWTRG |
				  DCMD_BURST16 | DCMD_WIDTH2,
};

static struct pxa2xx_pcm_dma_params pxa2xx_ssp3_pcm_mono_in = {
	.name			= "SSP3 PCM Mono in",
	.dev_addr		= __PREG(SSDR_P3),
	.drcmr			= &DRCMRRXSS3DR,
	.dcmd			= DCMD_INCTRGADDR | DCMD_FLOWSRC |
				  DCMD_BURST16 | DCMD_WIDTH2,
};

static struct pxa2xx_pcm_dma_params pxa2xx_ssp3_pcm_stereo_out = {
	.name			= "SSP3 PCM Stereo out",
	.dev_addr		= __PREG(SSDR_P3),
	.drcmr			= &DRCMRTXSS3DR,
	.dcmd			= DCMD_INCSRCADDR | DCMD_FLOWTRG |
				  DCMD_BURST16 | DCMD_WIDTH4,
};

static struct pxa2xx_pcm_dma_params pxa2xx_ssp3_pcm_stereo_in = {
	.name			= "SSP3 PCM Stereo in",
	.dev_addr		= __PREG(SSDR_P3),
	.drcmr			= &DRCMRRXSS3DR,
	.dcmd			= DCMD_INCTRGADDR | DCMD_FLOWSRC |
				  DCMD_BURST16 | DCMD_WIDTH4,
};

static struct pxa2xx_pcm_dma_params *ssp_dma_params[3][4] = {
	{&pxa2xx_ssp1_pcm_mono_out, &pxa2xx_ssp1_pcm_mono_in,
	&pxa2xx_ssp1_pcm_stereo_out,&pxa2xx_ssp1_pcm_stereo_in,},
	{&pxa2xx_ssp2_pcm_mono_out, &pxa2xx_ssp2_pcm_mono_in,
	&pxa2xx_ssp2_pcm_stereo_out, &pxa2xx_ssp2_pcm_stereo_in,},
	{&pxa2xx_ssp3_pcm_mono_out, &pxa2xx_ssp3_pcm_mono_in,
	&pxa2xx_ssp3_pcm_stereo_out,&pxa2xx_ssp3_pcm_stereo_in,},
};

static inline int dai_to_port(struct snd_soc_dai_runtime *cpu_dai)
{
	return (cpu_dai->dai->id - PXA2XX_DAI_SSP1) + 1;
}

static int pxa2xx_ssp_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai_runtime *dai)
{
	struct ssp_priv *ssp = dai->private_data;
	int ret = 0;

	if (!dai->active) {
		ret = ssp_init (&ssp->dev, dai_to_port(dai), SSP_NO_IRQ);
		if (ret < 0)
			return ret;
		ssp_disable(&ssp->dev);
	}
	return ret;
}

static void pxa2xx_ssp_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai_runtime *dai)
{
	struct ssp_priv *ssp = dai->private_data;

	if (!dai->active) {
		ssp_disable(&ssp->dev);
		ssp_exit(&ssp->dev);
	}
}

#if defined (CONFIG_PXA27x)
static int cken[3] = {CKEN_SSP1, CKEN_SSP2, CKEN_SSP3};
#else
static int cken[3] = {CKEN_SSP, CKEN_NSSP, CKEN_ASSP};
#endif

#ifdef CONFIG_PM
int pxa2xx_ssp_suspend(struct snd_soc_dai_runtime *rdai,
	pm_message_t state)
{
	struct ssp_priv *ssp = rdai->private_data;

	if (!rdai->active)
		return 0;

	ssp_save_state(&ssp->dev, &ssp->state);
	pxa_set_cken(cken[rdai->dai->id], 0);
	return 0;
}

int pxa2xx_ssp_resume(struct snd_soc_dai_runtime *rdai)
{
	struct ssp_priv *ssp = rdai->private_data;

	if (!rdai->active)
		return 0;

	pxa_set_cken(cken[rdai->dai->id], 1);
	ssp_restore_state(&ssp->dev, &ssp->state);
	ssp_enable(&ssp->dev);

	return 0;
}

#else
#define pxa2xx_ssp_suspend	NULL
#define pxa2xx_ssp_resume	NULL
#endif

/*
 * Set the SSP ports SYSCLK.
 */
static int pxa2xx_ssp_set_dai_sysclk(struct snd_soc_dai_runtime *cpu_dai,
	int clk_id, unsigned int freq, int dir)
{
	struct ssp_priv *ssp = cpu_dai->private_data;
	int port = dai_to_port(cpu_dai);

	u32 sscr0 = SSCR0_P(port) &
		~(SSCR0_ECS |  SSCR0_NCS | SSCR0_MOD | SSCR0_ADC);

	dbg("pxa2xx_ssp_set_dai_sysclk id: %d, clk_id %d, freq %d",
		cpu_dai->dai->id, clk_id, freq);

	switch (clk_id) {
	case PXA2XX_SSP_CLK_NET_PLL:
		sscr0 |= SSCR0_MOD;
	case PXA2XX_SSP_CLK_PLL:
		/* Internal PLL is fixed on pxa25x and pxa27x */
#ifdef CONFIG_PXA27x
		ssp->sysclk = 13000000;
#else
		ssp->sysclk = 1843200;
#endif
		break;
	case PXA2XX_SSP_CLK_EXT:
		ssp->sysclk = freq;
		sscr0 |= SSCR0_ECS;
		break;
	case PXA2XX_SSP_CLK_NET:
		ssp->sysclk = freq;
		sscr0 |= SSCR0_NCS | SSCR0_MOD;
		break;
	case PXA2XX_SSP_CLK_AUDIO:
		ssp->sysclk = 0;
		SSCR0_P(port) |= SSCR0_SerClkDiv(1);
		sscr0 |= SSCR0_ADC;
		break;
	default:
		return -ENODEV;
	}

	/* the SSP CKEN clock must be disabled when changing SSP clock mode */
	pxa_set_cken(cken[cpu_dai->dai->id], 0);
	SSCR0_P(port) |= sscr0;
	pxa_set_cken(cken[cpu_dai->dai->id], 1);
	return 0;
}

/*
 * Set the SSP clock dividers.
 */
static int pxa2xx_ssp_set_dai_clkdiv(struct snd_soc_dai_runtime *cpu_dai,
	int div_id, int div)
{
	int port = dai_to_port(cpu_dai);

	switch (div_id) {
	case PXA2XX_SSP_AUDIO_DIV_ACDS:
		SSACD_P(port) &= ~ 0x7;
		SSACD_P(port) |= SSACD_ACDS(div);
		break;
	case PXA2XX_SSP_AUDIO_DIV_SCDB:
		SSACD_P(port) &= ~0x8;
		if (div == PXA2XX_SSP_CLK_SCDB_1)
			SSACD_P(port) |= SSACD_SCDB;
		break;
	case PXA2XX_SSP_DIV_SCR:
		SSCR0_P(port) &= ~SSCR0_SCR;
		SSCR0_P(port) |= SSCR0_SerClkDiv(div);
		break;
	default:
		return -ENODEV;
	}

	return 0;
}

/*
 * Configure the PLL frequency pxa27x and (afaik - pxa320 only)
 */
static int pxa2xx_ssp_set_dai_pll(struct snd_soc_dai_runtime *cpu_dai,
	int pll_id, unsigned int freq_in, unsigned int freq_out)
{
	int port = dai_to_port(cpu_dai);

	SSACD_P(port) &= ~0x70;
	switch (freq_out) {
	case 5622000:
		break;
	case 11345000:
		SSACD_P(port) |= (0x1 << 4);
		break;
	case 12235000:
		SSACD_P(port) |= (0x2 << 4);
		break;
	case 14857000:
		SSACD_P(port) |= (0x3 << 4);
		break;
	case 32842000:
		SSACD_P(port) |= (0x4 << 4);
		break;
	case 48000000:
		SSACD_P(port) |= (0x5 << 4);
		break;
	}
	return 0;
}

/*
 * Set the active slots in TDM/Network mode
 */
static int pxa2xx_ssp_set_dai_tdm_slot(struct snd_soc_dai_runtime *cpu_dai,
	unsigned int mask, int slots)
{
	int port = dai_to_port(cpu_dai);

	SSCR0_P(port) &= ~SSCR0_SlotsPerFrm(7);

	/* set number of active slots */
	SSCR0_P(port) |= SSCR0_SlotsPerFrm(slots);

	/* set active slot mask */
	SSTSA_P(port) = mask;
	SSRSA_P(port) = mask;
	return 0;
}

/*
 * Tristate the SSP DAI lines
 */
static int pxa2xx_ssp_set_dai_tristate(struct snd_soc_dai_runtime *cpu_dai,
	int tristate)
{
	int port = dai_to_port(cpu_dai);

	if (tristate)
		SSCR1_P(port) &= ~SSCR1_TTE;
	else
		SSCR1_P(port) |= SSCR1_TTE;

	return 0;
}

/*
 * Set up the SSP DAI format.
 * The SSP Port must be inactive before calling this function as the
 * physical interface format is changed.
 */
static int pxa2xx_ssp_set_dai_fmt(struct snd_soc_dai_runtime *cpu_dai,
		unsigned int fmt)
{
	int port = dai_to_port(cpu_dai);

	/* we can only change the settings if the port is not in use */
	if (SSCR0_P(port) & SSCR0_SSE)
		return 0;

	/* reset port settings */
	SSCR0_P(port) = 0;
	SSCR1_P(port) = 0;
	SSPSP_P(port) = 0;

	/* NOTE: I2S emulation is still very much work in progress here */

	/* FIXME: this is what wince uses for msb */
	if ((fmt & SND_SOC_DAIFMT_FORMAT_MASK) == SND_SOC_DAIFMT_MSB) {
		SSCR0_P(port) = SSCR0_EDSS | SSCR0_TISSP | SSCR0_DataSize(16);
		goto master;
	}

	/* check for I2S emulation mode - handle it separately  */
	if ((fmt & SND_SOC_DAIFMT_FORMAT_MASK) == SND_SOC_DAIFMT_I2S) {
		/* 8.4.11 */

		/* Only SSCR0[NCS] or SSCR0[ECS] bit fields settings are optional */
		SSCR0_P(port) = SSCR0_EDSS | SSCR0_PSP | SSCR0_DataSize(16);

		/* set FIFO thresholds */
		SSCR1_P(port) = SSCR1_RxTresh(14) | SSCR1_TxTresh(1);

		/* normal: */
		/* all bit fields must be cleared except: FSRT = 1 and
		 * SFRMWDTH = 16, DMYSTART=0,1) */
		SSPSP_P(port) = SSPSP_FSRT | SSPSP_SFRMWDTH(16) | SSPSP_DMYSTRT(0);
		goto master;
	}

	SSCR0_P(port) |= SSCR0_PSP;
	SSCR1_P(port) = SSCR1_RxTresh(14) | SSCR1_TxTresh(1) |
		SSCR1_TRAIL | SSCR1_RWOT;

master:
	switch(fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		SSCR1_P(port) |= (SSCR1_SCLKDIR | SSCR1_SFRMDIR);
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		SSCR1_P(port) |= SSCR1_SCLKDIR;
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		SSCR1_P(port) |= SSCR1_SFRMDIR;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		SSPSP_P(port) |= SSPSP_SFRMP | SSPSP_FSRT;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
		SSPSP_P(port) |= SSPSP_DMYSTRT(1);
	case SND_SOC_DAIFMT_DSP_B:
		SSPSP_P(port) |= SSPSP_SCMODE(2);
		break;
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_MSB:
		/* handled above */
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/*
 * Set the SSP audio DMA parameters and sample size.
 * Can be called multiple times by oss emulation.
 */
static int pxa2xx_ssp_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai_runtime *dai)
{
	int dma = 0, chn = params_channels(params);
	int port = dai_to_port(dai);

	/* select correct DMA params */
	if (substream->stream != SNDRV_PCM_STREAM_PLAYBACK)
		dma = 1; /* capture DMA offset is 1,3 */
	if (chn == 2)
		dma += 2; /* stereo DMA offset is 2, mono is 0 */
	dai->dma_data = ssp_dma_params[dai->dai->id][dma];

	dbg("pxa2xx_ssp_hw_params: dma %d", dma);

	/* we can only change the settings if the port is not in use */
	if (SSCR0_P(port) & SSCR0_SSE)
		return 0;

	/* clear selected SSP bits */
	SSCR0_P(port) &= ~(SSCR0_DSS | SSCR0_EDSS);

	/* bit size */
	switch(params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		SSCR0_P(port) |= SSCR0_DataSize(16);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		SSCR0_P(port) |=(SSCR0_EDSS | SSCR0_DataSize(8));
		/* we must be in network mode (2 slots) for 24 bit stereo */
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		SSCR0_P(port) |= (SSCR0_EDSS | SSCR0_DataSize(16));
		/* we must be in network mode (2 slots) for 32 bit stereo */
		break;
	}

	dbg("SSCR0 0x%08x SSCR1 0x%08x SSTO 0x%08x SSPSP 0x%08x SSSR 0x%08x SSACD 0x%08x",
		SSCR0_P(port), SSCR1_P(port),
		SSTO_P(port), SSPSP_P(port),
		SSSR_P(port), SSACD_P(port));

	return 0;
}

static int pxa2xx_ssp_trigger(struct snd_pcm_substream *substream, int cmd,
	struct snd_soc_dai_runtime *dai)
{
	struct ssp_priv *ssp = dai->private_data;
	int ret = 0;
	int port = dai_to_port(dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_RESUME:
		ssp_enable(&ssp->dev);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			SSCR1_P(port) |= SSCR1_TSRE;
		else
			SSCR1_P(port) |= SSCR1_RSRE;
		SSSR_P(port) |= SSSR_P(port);
		break;
	case SNDRV_PCM_TRIGGER_START:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			SSCR1_P(port) |= SSCR1_TSRE;
		else
			SSCR1_P(port) |= SSCR1_RSRE;
		ssp_enable(&ssp->dev);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			SSCR1_P(port) &= ~SSCR1_TSRE;
		else
			SSCR1_P(port) &= ~SSCR1_RSRE;
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
		ssp_disable(&ssp->dev);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			SSCR1_P(port) &= ~SSCR1_TSRE;
		else
			SSCR1_P(port) &= ~SSCR1_RSRE;
		break;

	default:
		ret = -EINVAL;
	}

	dbg("SSCR0 0x%08x SSCR1 0x%08x SSTO 0x%08x SSPSP 0x%08x SSSR 0x%08x",
		SSCR0_P(port), SSCR1_P(port),
		SSTO_P(port), SSPSP_P(port),
		SSSR_P(port));

	return ret;
}

#define PXA2XX_SSP_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 | \
		SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000)

#define PXA2XX_SSP_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static int pxa2xx_ssp_new(struct snd_soc_dai_runtime *dai)
{
	struct ssp_priv *ssp;

	ssp = kzalloc(sizeof(struct ssp_priv), GFP_KERNEL);
	if (ssp == NULL)
		return -ENOMEM;

	dai->private_data = ssp;
	return 0;
}

static void pxa2xx_ssp_free(struct snd_soc_dai_runtime *dai)
{
	kfree(dai->private_data);
}

struct snd_soc_dai pxa2xx_ssp[] = {
{
	.name	= "pxa2xx-ssp1",
	.id	= PXA2XX_DAI_SSP1,

	.new	= pxa2xx_ssp_new,
	.free	= pxa2xx_ssp_free,

	.playback = {
		.stream_name	= "Playback",
		.channels_min	= 1,
		.channels_max	= 2,
		.rates		= PXA2XX_SSP_RATES,
		.formats	= PXA2XX_SSP_FORMATS,
	},
	.capture = {
		.stream_name	= "Capture",
		.channels_min	= 1,
		.channels_max	= 2,
		.rates		= PXA2XX_SSP_RATES,
		.formats	= PXA2XX_SSP_FORMATS,
	},

	/* alsa ops */
	.startup	= pxa2xx_ssp_startup,
	.shutdown	= pxa2xx_ssp_shutdown,
	.trigger	= pxa2xx_ssp_trigger,
	.hw_params	= pxa2xx_ssp_hw_params,

	/* dai ops */
	.set_sysclk	= pxa2xx_ssp_set_dai_sysclk,
	.set_clkdiv	= pxa2xx_ssp_set_dai_clkdiv,
	.set_pll	= pxa2xx_ssp_set_dai_pll,
	.set_fmt	= pxa2xx_ssp_set_dai_fmt,
	.set_tdm_slot	= pxa2xx_ssp_set_dai_tdm_slot,
	.set_tristate	= pxa2xx_ssp_set_dai_tristate,
},
{
	.name	= "pxa2xx-ssp2",
	.id	= PXA2XX_DAI_SSP2,

	.new	= pxa2xx_ssp_new,
	.free	= pxa2xx_ssp_free,

	.playback = {
		.stream_name	= "Playback",
		.channels_min	= 1,
		.channels_max	= 2,
		.rates		= PXA2XX_SSP_RATES,
		.formats	= PXA2XX_SSP_FORMATS,
	},
	.capture = {
		.stream_name	= "Capture",
		.channels_min	= 1,
		.channels_max	= 2,
		.rates		= PXA2XX_SSP_RATES,
		.formats	= PXA2XX_SSP_FORMATS,
	},

	/* alsa ops */
	.startup	= pxa2xx_ssp_startup,
	.shutdown	= pxa2xx_ssp_shutdown,
	.trigger	= pxa2xx_ssp_trigger,
	.hw_params	= pxa2xx_ssp_hw_params,

	/* dai ops */
	.set_sysclk	= pxa2xx_ssp_set_dai_sysclk,
	.set_clkdiv	= pxa2xx_ssp_set_dai_clkdiv,
	.set_pll	= pxa2xx_ssp_set_dai_pll,
	.set_fmt	= pxa2xx_ssp_set_dai_fmt,
	.set_tdm_slot	= pxa2xx_ssp_set_dai_tdm_slot,
	.set_tristate	= pxa2xx_ssp_set_dai_tristate,
},
{
	.name	= "pxa2xx-ssp3",
	.id	= PXA2XX_DAI_SSP3,

	.new	= pxa2xx_ssp_new,
	.free	= pxa2xx_ssp_free,

	.playback = {
		.stream_name	= "Playback",
		.channels_min	= 1,
		.channels_max	= 2,
		.rates		= PXA2XX_SSP_RATES,
		.formats	= PXA2XX_SSP_FORMATS,
	},
	.capture = {
		.stream_name	= "Capture",
		.channels_min	= 1,
		.channels_max	= 2,
		.rates		= PXA2XX_SSP_RATES,
		.formats	= PXA2XX_SSP_FORMATS,
	},

	/* alsa ops */
	.startup	= pxa2xx_ssp_startup,
	.shutdown	= pxa2xx_ssp_shutdown,
	.trigger	= pxa2xx_ssp_trigger,
	.hw_params	= pxa2xx_ssp_hw_params,

	/* dai ops */
	.set_sysclk	= pxa2xx_ssp_set_dai_sysclk,
	.set_clkdiv	= pxa2xx_ssp_set_dai_clkdiv,
	.set_pll	= pxa2xx_ssp_set_dai_pll,
	.set_fmt	= pxa2xx_ssp_set_dai_fmt,
	.set_tdm_slot	= pxa2xx_ssp_set_dai_tdm_slot,
	.set_tristate	= pxa2xx_ssp_set_dai_tristate,
},
};
EXPORT_SYMBOL_GPL(pxa2xx_ssp);

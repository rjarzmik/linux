/*
 * include/linux/rtc/sa1100.h
 *
 * Definitions for the platform data of sa1100/pxa RTC chip driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_RTC_SA1100_H_
#define _LINUX_RTC_SA1100_H_

/**
 * struct rtc_sa1100_platform_data - sa1100 platform data
 * @pxa_use_rdcr: uses pxa specific registers RDCR and RYCR
 */
struct rtc_sa1100_platform_data {
	unsigned int pxa_use_rdcr:1;
};

#endif

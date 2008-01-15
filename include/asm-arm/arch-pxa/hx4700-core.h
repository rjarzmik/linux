/* Core Hardware driver for Hx4700 (ASIC3, EGPIOs)
 *
 * Copyright (c) 2005-2006 SDG Systems, LLC
 *
 * 2005-03-29   Todd Blumer       Converted basic structure to support hx4700
 */

#ifndef _HX4700_CORE_H
#define _HX4700_CORE_H
#include <linux/leds.h>

struct hx4700_core_funcs {
    int (*udc_detect)( void );
};

EXTERN_LED_TRIGGER_SHARED(hx4700_radio_trig);

#endif /* _HX4700_CORE_H */

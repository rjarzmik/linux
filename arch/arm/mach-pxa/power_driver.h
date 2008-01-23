/*
 *  Jeremy Compostella <jeremy.compostella@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; Version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#ifndef POWER_DRIVER_H
#define POWER_DRIVER_H

#include <linux/platform_device.h>

enum power_driver_state {
	none = -2,
	managed = -1,
	off = 0,
	cold,
	warm,
	on
};

struct power_policy {
	enum power_driver_state init;
	enum power_driver_state exit;
	enum power_driver_state suspend;
	enum power_driver_state resume;
};

struct power_driver {
	struct platform_driver *pdriver;
	struct power_policy power_policy;
	int (*set_state)(struct platform_device *pdriver,
			 enum power_driver_state state);
	int (*reset)(struct platform_device *pdriver);
};

extern int power_driver_register(struct power_driver *pd);
extern void power_driver_unregister(struct power_driver *pd);

extern enum power_driver_state power_driver_get_state(struct platform_device *pdev);
extern int power_driver_set_state(struct platform_device *pdev,
				  enum power_driver_state state);

#endif /* POWER_DRIVER_H */

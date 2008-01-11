/* Power managment interface from MIO A701
 *
 * 2007-12-12 Robert Jarzmik
 *
 * This code is licenced under the GPLv2.
 */

#include <linux/module.h>
#include <linux/kernel.h>

#include "mioa701.h"

extern int mioa701_suspend_init(void);
extern void mioa701_suspend_exit(void);

static int __init mioa701_pm_init( void )
{
	return mioa701_suspend_init();
}

static void __exit mioa701_pm_exit( void )
{
	mioa701_suspend_exit();
        return;
}

module_init( mioa701_pm_init );
module_exit( mioa701_pm_exit );

MODULE_AUTHOR("Robert Jarzmik");
MODULE_DESCRIPTION("MIO A701 Power Managment Support Driver");
MODULE_LICENSE("GPL");

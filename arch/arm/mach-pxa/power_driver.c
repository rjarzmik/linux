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

#include "power_driver.h"

#include <linux/sysfs.h>
#include <linux/list.h>

struct pri_power_driver {
	struct list_head list;
	struct power_driver pwdrv;
	int (*probe)(struct platform_device *);
	int (*remove)(struct platform_device *);
};
static struct list_head drivers = LIST_HEAD_INIT(drivers);

struct pri_power_device {
	struct class_device class_dev;
	struct list_head list;
	struct platform_device *pdev;
	enum power_driver_state state;
	struct power_policy power_policy;
	struct pri_power_driver *driver;
};
static struct list_head devices = LIST_HEAD_INIT(devices);

static void power_driver_class_release(struct class_device *cdev);
static struct class power_driver_class = {
	.name = "power_driver",
	.release = power_driver_class_release
};

/* Tools */
static int power_driver_suspend(struct platform_device *pdev, pm_message_t state);
static int power_driver_resume(struct platform_device *pdev);
static void power_driver_set_trigger(struct pri_power_device *device,
				     const char *trigger, const char *state);
static int power_driver_transition(struct pri_power_device *device,
				   enum power_driver_state to_state);
static struct pri_power_driver *find_driver_by_name(const char *name);
static struct pri_power_device *find_device_by_pdev(struct platform_device *pdev);
static const char *state2name(enum power_driver_state state);
static enum power_driver_state name2state(const char *name, size_t count);

/* SYSFS triggers */
ssize_t power_driver_state_show(struct class_device *cdev, char *buf)
{
	struct pri_power_device *device = (struct pri_power_device *)cdev;
	return sprintf(buf, "%s\n", state2name(device->state));
}

ssize_t power_driver_state_store(struct class_device *cdev, const char *buf,
				 size_t count)
{
	struct pri_power_device *device = (struct pri_power_device *)cdev;
	enum power_driver_state state = name2state(buf, count);

	power_driver_transition(device, state);

	return count;
}

ssize_t power_driver_policy_show(struct class_device *cdev, char *buf)
{
	struct pri_power_device *device = (struct pri_power_device *)cdev;
	return sprintf(buf, "init:%s\nexit:%s\nsuspend:%s\nresume:%s\n",
		       state2name(device->power_policy.init),
		       state2name(device->power_policy.exit),
		       state2name(device->power_policy.suspend),
		       state2name(device->power_policy.resume));
	return 0;
}

ssize_t power_driver_policy_store(struct class_device *cdev, const char *buf,
				  size_t count)
{
	struct pri_power_device *device = (struct pri_power_device *)cdev;
	char *copy = (char *)kmalloc(sizeof(char) * count, GFP_KERNEL);
	char *trigger = copy, *state = NULL;
	int i;
	
	strncpy(copy, buf, count);

	for (i = 0 ; i < count ; i++) {
		if (state == NULL && copy[i] == ':') {
			copy[i] = 0;
			state = copy + (++i);
		} else if (state && (i == count - 1 || copy[i] == 0 ||
				     copy[i] == '\n')) {
			copy[i] = 0;
			power_driver_set_trigger(device, trigger, state);
			state = NULL;
			trigger = copy + (++i);
		}
	}

	return i;
}

ssize_t power_driver_reset_store(struct class_device *cdev, const char *buf,
				 size_t count)
{
	struct pri_power_device *device = (struct pri_power_device *)cdev;
	if (device->driver->pwdrv.reset)
		device->driver->pwdrv.reset(device->pdev);
	else {
		power_driver_transition(device, off);
		power_driver_transition(device, on);
	}
	return count;
}
struct class_device_attribute power_state =
    __ATTR(power_state, 0644, power_driver_state_show, power_driver_state_store);
struct class_device_attribute power_policy =
    __ATTR(power_policy, 0644, power_driver_policy_show, power_driver_policy_store);
struct class_device_attribute power_reset =
    __ATTR(power_reset, 0200, NULL, power_driver_reset_store);

static void power_driver_class_release(struct class_device *cdev)
{
	kfree(cdev);
}

static int power_driver_probe(struct platform_device *pdev)
{
	struct pri_power_device *device =
		(struct pri_power_device *)kzalloc(sizeof(struct pri_power_device),
						   GFP_KERNEL);
	struct pri_power_driver *driver;
	int ret = 0;

	if (device == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	device->pdev = pdev;
	device->state = off;
	device->class_dev.class = &power_driver_class;

	driver = find_driver_by_name(pdev->name);
	if (driver == NULL) {
		printk("power_driver: driver not found on probe\n");
		ret = -EINVAL;
		goto free;
	}
	class_set_devdata(&device->class_dev, driver);
	device->driver = driver;
	device->power_policy = driver->pwdrv.power_policy;

	strlcpy(device->class_dev.class_id, device->pdev->name, KOBJ_NAME_LEN);
	list_add(&device->list, &devices);

	ret = class_device_register(&device->class_dev);
	if (ret)
		goto free;

	if ((ret = class_device_create_file(&device->class_dev, &power_state))) {
		printk("power_driver: unable to create power_state file\n");
		goto unregister;
	}
	if ((ret = class_device_create_file(&device->class_dev, &power_reset))) {
		printk("power_driver: unable to create power_reset file\n");
		class_device_remove_file(&device->class_dev, &power_state);
		goto unregister;
	}
	if ((ret = class_device_create_file(&device->class_dev, &power_policy))) {
		printk("power_driver: unable to create power_policy file\n");
		class_device_remove_file(&device->class_dev, &power_reset);
		class_device_remove_file(&device->class_dev, &power_state);
		goto unregister;
	}

	if (driver->probe)
		driver->probe(pdev);
	power_driver_transition(device, device->power_policy.init);

	goto out;
 unregister:
	class_device_unregister(&device->class_dev);
 free:
	kfree(device);
 out:
	return ret;
}

static int power_driver_remove(struct platform_device *pdev)
{
	struct pri_power_device *device = find_device_by_pdev(pdev);
	if (device == NULL) {
		printk("power_driver: unable to find device on remove\n");
		return -EINVAL;
	}

	power_driver_transition(device, device->power_policy.exit);
	if (device->driver->remove)
		device->driver->remove(pdev);

	list_del(&device->list);

	class_device_remove_file(&device->class_dev, &power_policy);
	class_device_remove_file(&device->class_dev, &power_reset);
	class_device_remove_file(&device->class_dev, &power_state);
	class_device_unregister(&device->class_dev);

	return 0;
}

int power_driver_register(struct power_driver *pwdrv)
{
	struct pri_power_driver *driver;

	if (!pwdrv->pdriver || !pwdrv->set_state)
		return -EINVAL;

	driver = (struct pri_power_driver *)kzalloc(sizeof(struct pri_power_driver),
						    GFP_KERNEL);
	if (driver == NULL)
		return -ENOMEM;

	driver->pwdrv = *pwdrv;

	list_add(&driver->list, &drivers);

	if (driver->pwdrv.pdriver->suspend)
		driver->pwdrv.power_policy.suspend = managed;
	else
		driver->pwdrv.pdriver->suspend = power_driver_suspend;

	if (driver->pwdrv.pdriver->resume)
		driver->pwdrv.power_policy.resume = managed;
	else
		driver->pwdrv.pdriver->resume = power_driver_resume;

	driver->probe = pwdrv->pdriver->probe;
	driver->remove = pwdrv->pdriver->remove;
	pwdrv->pdriver->probe = power_driver_probe;
	pwdrv->pdriver->remove = power_driver_remove;

	return  0;
}
EXPORT_SYMBOL(power_driver_register);

void power_driver_unregister(struct power_driver *pwdrv)
{
	struct pri_power_driver *driver =
		find_driver_by_name(pwdrv->pdriver->driver.name);

	if (driver == NULL) {
		printk("power_driver: private structure not found\n");
		return;
	}

	if (driver->pwdrv.power_policy.suspend != managed)
		driver->pwdrv.pdriver->suspend = NULL;
	if (driver->pwdrv.power_policy.resume != managed)
		driver->pwdrv.pdriver->resume = NULL;

	list_del(&driver->list);
	kfree(driver);
}
EXPORT_SYMBOL(power_driver_unregister);

extern enum power_driver_state power_driver_get_state(struct platform_device *pdev)
{
	struct pri_power_device *device = find_device_by_pdev(pdev);
	if (device == NULL) {
		printk("power_driver: device not found\n");
		return none;
	}
	return device->state;
}
EXPORT_SYMBOL(power_driver_get_state);

int power_driver_set_state(struct platform_device *pdev,
			   enum power_driver_state state)
{
	struct pri_power_device *device = find_device_by_pdev(pdev);
	if (device == NULL) {
		printk("power_driver: device not found\n");
		return none;
	}
	return power_driver_transition(device, state);
}
EXPORT_SYMBOL(power_driver_set_state);

static int power_driver_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct pri_power_device *device = find_device_by_pdev(pdev);
	return power_driver_transition(device, device->power_policy.suspend);
}

static int power_driver_resume(struct platform_device *pdev)
{
	struct pri_power_device *device = find_device_by_pdev(pdev);
	return power_driver_transition(device, device->power_policy.resume);
}

static void power_driver_set_trigger(struct pri_power_device *device,
				     const char *trigger, const char *state)
{
	enum power_driver_state to_state = name2state(state, strlen(state));
	if (to_state != managed) {
		if (strcmp(trigger, "init") == 0)
			device->power_policy.init = to_state;
		else if (strcmp(trigger, "exit") == 0)
			device->power_policy.exit = to_state;
		else if (strcmp(trigger, "suspend") == 0)
			device->power_policy.suspend = to_state;
		else if (strcmp(trigger, "resume") == 0)
			device->power_policy.resume = to_state;
	}
}

static int power_driver_transition(struct pri_power_device *device,
				   enum power_driver_state to_state)
{
	int ret = 0;

	if (to_state < 0 || device->state == to_state)
		return ret;

	ret = device->driver->pwdrv.set_state(device->pdev, to_state);
	device->state = ret != 0 ? device->state : to_state;
	return ret;
}

static struct pri_power_driver *find_driver_by_name(const char *name)
{
	struct pri_power_driver *cur;

	list_for_each_entry(cur, &drivers, list)
		if (strcmp(cur->pwdrv.pdriver->driver.name, name) == 0)
			return cur;

	return NULL;
}

static struct pri_power_device *find_device_by_pdev(struct platform_device *pdev)
{
	struct pri_power_device *cur;

	list_for_each_entry(cur, &devices, list)
		if (cur->pdev == pdev)
			return cur;

	return NULL;
}

static char *states_name[] = { "none", "managed", "off", "cold", "warm", "on" };
static const char *state2name(enum power_driver_state state)
{
	if (state >= none && state <= on)
		return states_name[state - none];
	return "unknown";
}
static enum power_driver_state name2state(const char *name, size_t count)
{
	int i;
	for (i = 0 ; i < ARRAY_SIZE(states_name) ; ++i)
		if (strncmp(name, states_name[i],
			    min(count, strlen(states_name[i]))) == 0)
			return i + none;
	return none;
}

static int __init power_driver_init(void)
{
	printk(KERN_NOTICE "power_driver: advanced power driver loaded\n");
	return class_register(&power_driver_class);
}

static void __exit power_driver_exit(void)
{	
	class_unregister(&power_driver_class);
	printk(KERN_NOTICE "power_driver: advanced power driver unloaded\n");
}

module_init(power_driver_init);
module_exit(power_driver_exit);

MODULE_AUTHOR("Jeremy Compostella");
MODULE_DESCRIPTION("An advanced power driver");
MODULE_LICENSE("GPL");

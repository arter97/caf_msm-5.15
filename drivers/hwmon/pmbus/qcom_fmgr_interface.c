// SPDX-License-Identifier: GPL-2.0-only
/*
 * Userspace interface for /dev/fmgr_interface_device
 *
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/compat.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/of_platform.h>
#include <linux/poll.h>
#include <linux/idr.h>
#include <linux/qcom_fmgr_interface.h>

#include "pmbus.h"

#define MAX_DEVICES 64

struct fmgr_interface {
	struct i2c_client *client;
	struct cdev cdev;
	struct device *dev;
	struct pmbus_driver_info info;
	struct wait_queue_head  waitq;
	struct mutex queue_lock;
	uint8_t addr;
};

static struct class *dev_class;
static dev_t fmgr_interface_major;
static DEFINE_IDA(fmgr_minor_ida);

/*
 * fmgr_interface_open: open function for dev node.
 * @inode: Pointer to inode.
 * @file: Pointer to file descriptor.
 *
 * This function will be called when we open the dev node
 * from user-space application.
 *
 * Return: 0 for success.
 */
static int fmgr_interface_open(struct inode *inode, struct file *file)
{
	struct cdev *cdev = inode->i_cdev;
	struct  fmgr_interface *fmgr_interface = container_of(cdev,
			struct fmgr_interface, cdev);

	file->private_data = fmgr_interface;

	return 0;
}

/*
 * fmgr_interface_release: close function for dev node.
 * @inode: pointer to inode.
 * @file: Pointer to file descriptor.
 *
 * This function will be called when we close the dev node
 * from user-space application.
 *
 * Return: 0 for success.
 */
static int fmgr_interface_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;

	return 0;
}

/*
 * fmgr_interface_poll: poll() syscall for I2C slave.
 * @file: Pointer to the file structure.
 * @wait: Pointer to Poll table.
 *
 * This function is used to poll on the I2C slave device.
 * when userspace client do a poll() system call.
 *
 * Return: POLLIN in case of SMB alert with slave address.
 */
static __poll_t fmgr_interface_poll(struct file *file,
		struct poll_table_struct *wait)
{
	struct fmgr_interface *fmgr_interface = file->private_data;
	__poll_t mask = 0;

	poll_wait(file, &fmgr_interface->waitq, wait);

	if (fmgr_interface->addr)
		mask |= POLLIN;

	return mask;
}

/*
 * fmgr_interface_alert: alert call of client devices.
 * @client : i2c client
 * @type   : protocol type
 * @data: data of alerted slave device
 */
static void fmgr_interface_alert(struct i2c_client *client,
			   enum i2c_alert_protocol type, unsigned int data)
{
	struct fmgr_interface *fmgr_interface;
	const struct pmbus_driver_info *info = pmbus_get_driver_info(client);

	fmgr_interface = container_of(info, struct fmgr_interface, info);

	fmgr_interface->addr = (uint8_t)client->addr;
	wake_up_interruptible(&fmgr_interface->waitq);
}

/*
 * fmgr_interface_ioctl: fmgr ioctl function.
 * @file: Pointer to file descriptor.
 * @command: PMbus command code
 * @arg: Pointer to client data
 *
 * Based on read_write flag, it will call read
 * write function.
 *
 * Return: 0 or positive value for success, Negative number for error condition.
 */
static long fmgr_interface_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct fmgr_interface *fmgr_interface = file->private_data;
	struct fmgr_ioctl_pmbus_msg msg;
	int ret = 0;

	switch (cmd) {
	case PMBUS_READ:
		ret = copy_from_user(&msg, (struct fmgr_ioctl_pmbus_msg __user *)arg,
				   sizeof(struct fmgr_ioctl_pmbus_msg));
		if (ret < 0)
			return ret;

		if (msg.size == PMBUS_BYTE) {
			ret = pmbus_read_byte_data(fmgr_interface->client,
					msg.page, msg.reg);
			msg.buf.byte = ret;
		} else if (msg.size == PMBUS_WORD) {
			ret = pmbus_read_word_data(fmgr_interface->client,
					msg.page, msg.phase, msg.reg);
			msg.buf.word = ret;
		} else {
			ret = i2c_smbus_read_i2c_block_data(
					fmgr_interface->client,
					msg.reg, msg.size + 1, msg.buf.block);
		}

		if (ret < 0)
			return ret;

		ret = copy_to_user((struct fmgr_ioctl_pmbus_msg *)arg, &msg,
				sizeof(struct fmgr_ioctl_pmbus_msg));

		break;

	case PMBUS_WRITE:
		ret = copy_from_user(&msg,
				   (struct fmgr_ioctl_pmbus_msg __user *)arg,
				   sizeof(struct fmgr_ioctl_pmbus_msg));
		if (ret < 0)
			return ret;
		if (msg.size == PMBUS_BYTE)
			ret = pmbus_write_byte_data(fmgr_interface->client,
					msg.page, msg.reg, msg.buf.byte);

		else if (msg.size == PMBUS_WORD)
			ret = pmbus_write_word_data(fmgr_interface->client,
					msg.page, msg.reg, msg.buf.word);

		else
			ret = i2c_smbus_write_block_data(fmgr_interface->client,
					msg.reg, msg.size, msg.buf.block);

		break;

	case ALERT_ADDR:
		if (copy_to_user((uint8_t *)arg, &fmgr_interface->addr,
					sizeof(fmgr_interface->addr)))
			return -EFAULT;
		break;

	case CLEAR_FAULTS:
		fmgr_interface->addr = 0;

		break;

	default:
		dev_err(fmgr_interface->dev, "Invalid command\n");
		ret = -EINVAL;
	}

	return ret;
}

static const struct file_operations fmgr_interface_fops = {
	.owner		=	THIS_MODULE,
	.unlocked_ioctl	=	fmgr_interface_ioctl,
	.open		=	fmgr_interface_open,
	.poll		=	fmgr_interface_poll,
	.release	=	fmgr_interface_release,
};

/*
 * fmgr_find_sensor_groups:
 * find sensor groups and status registers on each page.
 * @client : i2c client member
 * @pmbus_info : pmbus driver info
 */
static void fmgr_find_sensor_groups(struct i2c_client *client,
				     struct pmbus_driver_info *pmbus_info)
{
	int page;

	/* Sensors detected on page 0 only */
	if (pmbus_check_word_register(client, 0, PMBUS_READ_VIN))
		pmbus_info->func[0] |= PMBUS_HAVE_VIN;

	if (pmbus_check_word_register(client, 0, PMBUS_READ_VCAP))
		pmbus_info->func[0] |= PMBUS_HAVE_VCAP;

	if (pmbus_check_word_register(client, 0, PMBUS_READ_IIN))
		pmbus_info->func[0] |= PMBUS_HAVE_IIN;

	if (pmbus_check_word_register(client, 0, PMBUS_READ_PIN))
		pmbus_info->func[0] |= PMBUS_HAVE_PIN;

	if (pmbus_info->func[0]
	    && pmbus_check_byte_register(client, 0, PMBUS_STATUS_INPUT))
		pmbus_info->func[0] |= PMBUS_HAVE_STATUS_INPUT;

	if (pmbus_check_byte_register(client, 0, PMBUS_FAN_CONFIG_12) &&
	    pmbus_check_word_register(client, 0, PMBUS_READ_FAN_SPEED_1)) {
		pmbus_info->func[0] |= PMBUS_HAVE_FAN12;
		if (pmbus_check_byte_register(client, 0, PMBUS_STATUS_FAN_12))
			pmbus_info->func[0] |= PMBUS_HAVE_STATUS_FAN12;
	}

	if (pmbus_check_byte_register(client, 0, PMBUS_FAN_CONFIG_34) &&
	    pmbus_check_word_register(client, 0, PMBUS_READ_FAN_SPEED_3)) {
		pmbus_info->func[0] |= PMBUS_HAVE_FAN34;
		if (pmbus_check_byte_register(client, 0, PMBUS_STATUS_FAN_34))
			pmbus_info->func[0] |= PMBUS_HAVE_STATUS_FAN34;
	}

	if (pmbus_check_word_register(client, 0, PMBUS_READ_TEMPERATURE_1))
		pmbus_info->func[0] |= PMBUS_HAVE_TEMP;

	if (pmbus_check_word_register(client, 0, PMBUS_READ_TEMPERATURE_2))
		pmbus_info->func[0] |= PMBUS_HAVE_TEMP2;

	if (pmbus_check_word_register(client, 0, PMBUS_READ_TEMPERATURE_3))
		pmbus_info->func[0] |= PMBUS_HAVE_TEMP3;

	if (pmbus_info->func[0] & (PMBUS_HAVE_TEMP | PMBUS_HAVE_TEMP2 |
			PMBUS_HAVE_TEMP3) &&
			pmbus_check_byte_register(client, 0, PMBUS_STATUS_TEMPERATURE))
		pmbus_info->func[0] |= PMBUS_HAVE_STATUS_TEMP;

	/* Sensors detected on all pages */
	for (page = 0; page < pmbus_info->pages; page++) {
		if (pmbus_check_word_register(client, page, PMBUS_READ_VOUT)) {
			pmbus_info->func[page] |= PMBUS_HAVE_VOUT;
			if (pmbus_check_byte_register(client, page,
						      PMBUS_STATUS_VOUT))
				pmbus_info->func[page] |= PMBUS_HAVE_STATUS_VOUT;
		}

		if (pmbus_check_word_register(client, page, PMBUS_READ_IOUT)) {
			pmbus_info->func[page] |= PMBUS_HAVE_IOUT;
			if (pmbus_check_byte_register(client, 0,
						      PMBUS_STATUS_IOUT))
				pmbus_info->func[page] |= PMBUS_HAVE_STATUS_IOUT;
		}

		if (pmbus_check_word_register(client, page, PMBUS_READ_POUT))
			pmbus_info->func[page] |= PMBUS_HAVE_POUT;
	}
}

/*
 * fmgr_indentify : Identifies chip parameters.
 * @client : i2c client member
 * @pmbus_info : pmbus driver information
 */
static int fmgr_identify(struct i2c_client *client,
			  struct pmbus_driver_info *pmbus_info)
{
	if (!pmbus_info->pages) {
		/*
		 * Check if PAGE command is supported.
		 * To know number of pages supported by chip, keep setting
		 * the page number until it fails or until the
		 * maximum number of pages has been reached.
		 */
		if (pmbus_check_byte_register(client, 0, PMBUS_PAGE)) {
			int page;

			for (page = 1; page < PMBUS_PAGES; page++) {
				if (pmbus_set_page(client, page, 0xff) < 0)
					break;
			}
			pmbus_set_page(client, 0, 0xff);
			pmbus_info->pages = page;
		} else {
			pmbus_info->pages = 1;
		}

		pmbus_clear_faults(client);
	}

	if (pmbus_check_byte_register(client, 0, PMBUS_VOUT_MODE)) {
		int vout_mode, i;

		vout_mode = pmbus_read_byte_data(client, 0, PMBUS_VOUT_MODE);
		if (vout_mode >= 0 && vout_mode != 0xff) {
			switch (vout_mode >> 5) {
			case 0:
				break;
			case 1:
				pmbus_info->format[PSC_VOLTAGE_OUT] = vid;
				for (i = 0; i < pmbus_info->pages; i++)
					pmbus_info->vrm_version[i] = vr11;
				break;
			case 2:
				pmbus_info->format[PSC_VOLTAGE_OUT] = direct;
				break;
			default:
				return -ENODEV;
			}
		}
	}

	/*
	 * Check if the COEFFICIENTS register is supported.
	 * If it is, and the chip is configured for direct mode, we can read
	 * the coefficients from the chip, one set per group of sensor
	 * registers.
	 *
	 * To do this, we will need access to a chip which actually supports the
	 * COEFFICIENTS command, since the command is too complex to implement
	 * without testing it. Until then, abort if a chip configured for direct
	 * mode was detected.
	 */
	if (pmbus_info->format[PSC_VOLTAGE_OUT] == direct)
		return -ENODEV;

	/* Find sensor groups  */
	fmgr_find_sensor_groups(client, pmbus_info);

	return 0;
}

/*
 * fmgr_interface_probe: Driver Probe function.
 * @pdev: Pointer to platform device structure.
 *
 * This function will performs pre-initialization tasks such as
 * allocating memory, creating user-space dev node to support
 * poll function on the fm interface device.
 * Return: 0 for success, negative number for error condition.
 */
static int fmgr_interface_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct fmgr_interface *fmgr_interface;
	struct device *dev_ret, *dev;
	int minor;
	int ret = 0;
	char name[32] = {'\0'};

	fmgr_interface = devm_kzalloc(&i2c->dev, sizeof(*fmgr_interface), GFP_KERNEL);
	if (!fmgr_interface)
		return -ENOMEM;

	fmgr_interface->dev = &i2c->dev;
	dev = fmgr_interface->dev;

	cdev_init(&fmgr_interface->cdev, &fmgr_interface_fops);
	fmgr_interface->cdev.owner = THIS_MODULE;
	fmgr_interface->cdev.ops = &fmgr_interface_fops;

	minor = ida_simple_get(&fmgr_minor_ida, 0, MAX_DEVICES, GFP_KERNEL);
	if (minor < 0)
		return minor;

	ret = cdev_add(&fmgr_interface->cdev, MKDEV(MAJOR(fmgr_interface_major), minor), 1);
	if (ret < 0) {
		dev_err(dev, "Cannot add device to system\n");
		ida_simple_remove(&fmgr_minor_ida,  minor);
		return ret;
	}

	snprintf(name, sizeof(name), "fmgr_%x", i2c->addr);

	dev_ret = device_create(dev_class, NULL, MKDEV(MAJOR(fmgr_interface_major), minor), NULL,
		name);
	if (IS_ERR_OR_NULL(dev_ret)) {
		dev_err(dev, "Could not create fmgr_interface device\n");
		ret = PTR_ERR(dev_ret);
		ida_simple_remove(&fmgr_minor_ida, minor);
		return ret;
	}

	init_waitqueue_head(&fmgr_interface->waitq);

	fmgr_interface->client = i2c;
	fmgr_interface->info.pages = 0;
	fmgr_interface->info.identify = fmgr_identify;

	pmbus_do_probe(i2c, &fmgr_interface->info);
	return ret;
}

static int fmgr_interface_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id fmgr_interface_dev_id[] = {
	{"fmgr_interface", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, fmgr_interface_dev_id);

static const struct of_device_id fmgr_interface_of_match[] = {
	{ .compatible = "qcom,fmgr-interface" },
	{ }
};
MODULE_DEVICE_TABLE(of, fmgr_interface_of_match);

static struct i2c_driver fmgr_interface_driver = {
	.driver = {
		.name = "fmgr_interface",
		.of_match_table = of_match_ptr(fmgr_interface_of_match),
	 },
	.probe = fmgr_interface_probe,
	.alert = fmgr_interface_alert,
	.id_table = fmgr_interface_dev_id,
	.remove = fmgr_interface_remove,
};

static void __exit fmgr_exit(void)
{
	class_destroy(dev_class);
	unregister_chrdev_region(MAJOR(fmgr_interface_major), 64);
}

static int __init fmgr_init(void)
{
	int ret = 0;

	ret = alloc_chrdev_region(&fmgr_interface_major, 0, MAX_DEVICES, "fmgr_interface");
	if (ret < 0) {
		pr_err("Could not allocate major number\n");
		return ret;
	}

	dev_class = class_create(THIS_MODULE, "fmgr_interface");
	if (IS_ERR_OR_NULL(dev_class)) {
		pr_err("Could not create fmgr_interface class\n");
		ret = PTR_ERR(dev_class);
		goto class_err;
	}

	ret = i2c_add_driver(&fmgr_interface_driver);

	return ret;

class_err:
	unregister_chrdev_region(MAJOR(fmgr_interface_major), 1);
	return ret;
}
module_init(fmgr_init);
module_exit(fmgr_exit);

MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(PMBUS);

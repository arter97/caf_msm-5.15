// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2019, 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <linux/of_address.h>
#include <linux/nvmem-consumer.h>
#ifdef CONFIG_POWER_RESET_QCOM_RESET_REASON
#include <linux/reset_reason.h>
#include <linux/slab.h>
#endif

#define INTENT_BIT_SHIFT	6	/* SDAM bit[7] reserved for reboot intent flag */
#define REASON_MASK		0x3F	/* Lower 6 bits for reboot reason */

enum reboot_reason_category {
	REBOOT_UNINTENTIONAL = 0,
	REBOOT_INTENTIONAL = 1,
};

struct qcom_reboot_reason {
	struct device *dev;
	struct notifier_block reboot_nb;
	struct nvmem_cell *nvmem_cell;
#ifdef CONFIG_POWER_RESET_QCOM_RESET_REASON
	struct kobject kobj;
	unsigned int last_reset_reason;
#endif
};

struct poweroff_reason {
	const char *cmd;
	unsigned char pon_reason;
	enum reboot_reason_category category;
};

static struct poweroff_reason reasons[] = {
	{ "recovery",			0x01,	REBOOT_INTENTIONAL },
	{ "bootloader",			0x02,	REBOOT_INTENTIONAL },
	{ "rtc",			0x03,	REBOOT_INTENTIONAL },
	{ "dm-verity device corrupted",	0x04,	REBOOT_UNINTENTIONAL },
	{ "dm-verity enforcing",	0x05,	REBOOT_INTENTIONAL },
	{ "keys clear",			0x06,	REBOOT_INTENTIONAL },
#if defined(CONFIG_POWER_RESET_QCOM_RESET_REASON)
	{ "panic",			0x07,	REBOOT_UNINTENTIONAL },
	{ "watchdog bark",		0x08,	REBOOT_UNINTENTIONAL },
	{ "user",			0x10,	REBOOT_INTENTIONAL },
	{ "system-normal",		0x11,	REBOOT_INTENTIONAL },
	{ "system-abnormal",		0x12,	REBOOT_UNINTENTIONAL },
#endif
	{}
};

#ifdef CONFIG_POWER_RESET_QCOM_RESET_REASON
/* interface for exporting attributes */
struct reset_attribute {
	struct attribute        attr;
	ssize_t (*show)(struct kobject *kobj, struct attribute *attr,
			char *buf);
};
#define to_reset_attr(_attr) \
	container_of(_attr, struct reset_attribute, attr)

static ssize_t attr_show(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	struct reset_attribute *reset_attr = to_reset_attr(attr);
	ssize_t ret = -EIO;

	if (reset_attr->show)
		ret = reset_attr->show(kobj, attr, buf);

	return ret;
}

static const struct sysfs_ops reset_sysfs_ops = {
	.show   = attr_show,
};

static struct kobj_type qcom_reset_kobj_type = {
	.sysfs_ops      = &reset_sysfs_ops,
};

static ssize_t reset_reason_show(struct kobject *kobj,
				struct attribute *this,
				char *buf)
{
	struct qcom_reboot_reason *reboot = container_of(kobj, struct qcom_reboot_reason, kobj);
	const char *reset_reason = "unknown";
	struct poweroff_reason *reason;

	for (reason = reasons; reason->pon_reason; reason++) {
		if (reason->pon_reason == reboot->last_reset_reason) {
			reset_reason = reason->cmd;
			break;
		}
	}

	return scnprintf(buf, PAGE_SIZE, "Last reset reason: %s\n", reset_reason);
}
static struct reset_attribute attr_reset_reason = __ATTR_RO(reset_reason);

static struct attribute *qcom_reset_attrs[] = {
	&attr_reset_reason.attr,
	NULL
};
static struct attribute_group qcom_reset_attr_group = {
	.attrs = qcom_reset_attrs,
};

struct nvmem_cell *reset_reason_nvmem;

static int reset_reason_sysfs(struct qcom_reboot_reason *reboot)
{
	int ret;
	u8 *buf;
	size_t len;
	unsigned char reason_unknown = 0x0;

	ret = kobject_init_and_add(&reboot->kobj, &qcom_reset_kobj_type,
			kernel_kobj, "reset_reason");
	if (ret) {
		pr_err("%s: Error in creation kobject_add\n", __func__);
		kobject_put(&reboot->kobj);
		return ret;
	}

	ret = sysfs_create_group(&reboot->kobj, &qcom_reset_attr_group);
	if (ret) {
		pr_err("%s: Error in creation sysfs_create_group\n", __func__);
		kobject_del(&reboot->kobj);
		return ret;
	}

	reset_reason_nvmem = reboot->nvmem_cell;
	buf = nvmem_cell_read(reboot->nvmem_cell, &len);
	if (IS_ERR(buf))
		return PTR_ERR(buf);

	nvmem_cell_write(reboot->nvmem_cell, &reason_unknown, sizeof(reason_unknown));
	reboot->last_reset_reason = buf[0];
	kfree(buf);

	return 0;
}

void store_reset_reason(char *cmd, struct nvmem_cell *nvmem_cell)
#else
static void store_reset_reason(char *cmd, struct nvmem_cell *nvmem_cell)
#endif
{
	struct poweroff_reason *reason;
	unsigned char val;
	int ret;

#ifdef CONFIG_POWER_RESET_QCOM_RESET_REASON
	if (!reset_reason_nvmem)
		return;

	if (!nvmem_cell)
		nvmem_cell = reset_reason_nvmem;
#endif
	for (reason = reasons; reason->cmd; reason++) {
		if (strcmp(cmd, reason->cmd))
			continue;

		/*
		 * SDAM 0x7148 layout:
		 * bit[7] -> Intentional reboot flag (0 = unintentional, 1 = intentional)
		 * bits[1:6] -> Reboot reason code
		 *
		 * Combine category and reason into one byte:
		 * - Shift category by INTENT_BIT_SHIFT to position bit[7]
		 * - Mask reason with REASON_MASK to keep lower 6 bits
		 */
		val = ((reason->category & 0x01) << INTENT_BIT_SHIFT) |
			(reason->pon_reason & REASON_MASK);
		ret = nvmem_cell_write(nvmem_cell, &val, sizeof(val));
		pr_info("%s: Value 0x%x, ret %d\n", __func__, val, ret);
		break;
	}
}
#ifdef CONFIG_POWER_RESET_QCOM_RESET_REASON
EXPORT_SYMBOL_GPL(store_reset_reason);
#endif

static int qcom_reboot_reason_reboot(struct notifier_block *this,
				     unsigned long event, void *ptr)
{
	char *cmd = ptr;
	struct qcom_reboot_reason *reboot = container_of(this,
		struct qcom_reboot_reason, reboot_nb);

	if (!cmd)
		return NOTIFY_OK;

	store_reset_reason(cmd, reboot->nvmem_cell);

	return NOTIFY_OK;
}

static int qcom_reboot_reason_probe(struct platform_device *pdev)
{
	struct qcom_reboot_reason *reboot;

	reboot = devm_kzalloc(&pdev->dev, sizeof(*reboot), GFP_KERNEL);
	if (!reboot)
		return -ENOMEM;

	reboot->dev = &pdev->dev;

	reboot->nvmem_cell = nvmem_cell_get(reboot->dev, "restart_reason");

	if (IS_ERR(reboot->nvmem_cell))
		return PTR_ERR(reboot->nvmem_cell);

#ifdef CONFIG_POWER_RESET_QCOM_RESET_REASON
	reset_reason_sysfs(reboot);
#endif
	reboot->reboot_nb.notifier_call = qcom_reboot_reason_reboot;
	reboot->reboot_nb.priority = 255;
	register_reboot_notifier(&reboot->reboot_nb);

	platform_set_drvdata(pdev, reboot);

	return 0;
}

static int qcom_reboot_reason_remove(struct platform_device *pdev)
{
	struct qcom_reboot_reason *reboot = platform_get_drvdata(pdev);

	unregister_reboot_notifier(&reboot->reboot_nb);

	return 0;
}

static const struct of_device_id of_qcom_reboot_reason_match[] = {
	{ .compatible = "qcom,reboot-reason", },
	{},
};
MODULE_DEVICE_TABLE(of, of_qcom_reboot_reason_match);

static struct platform_driver qcom_reboot_reason_driver = {
	.probe = qcom_reboot_reason_probe,
	.remove = qcom_reboot_reason_remove,
	.driver = {
		.name = "qcom-reboot-reason",
		.of_match_table = of_match_ptr(of_qcom_reboot_reason_match),
	},
};

module_platform_driver(qcom_reboot_reason_driver);

MODULE_DESCRIPTION("MSM Reboot Reason Driver");
MODULE_LICENSE("GPL v2");

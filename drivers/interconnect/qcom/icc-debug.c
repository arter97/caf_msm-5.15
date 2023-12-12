// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2021, The Linux Foundation. All rights reserved. */

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/interconnect-provider.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/syscore_ops.h>
#include <trace/events/power.h>

#include <dt-bindings/interconnect/qcom,icc.h>

#include "../internal.h"

static LIST_HEAD(icc_providers);
static DEFINE_MUTEX(debug_lock);
static struct dentry *dentry_suspend;
static bool debug_suspend;

struct qcom_icc_debug_provider {
	struct list_head list;
	struct icc_provider *provider;
};

static int icc_print_enabled(void)
{
	struct qcom_icc_debug_provider *dp;
	struct icc_provider *provider;
	struct icc_node *n;
	struct icc_req *r;
	u32 avg_bw, peak_bw;
	int cnt = 0;

	pr_info(" node                                  tag          avg         peak\n");
	pr_info("--------------------------------------------------------------------\n");

	list_for_each_entry(dp, &icc_providers, list) {
		provider = dp->provider;

		list_for_each_entry(n, &provider->nodes, node_list) {
			if (!n->avg_bw && !n->peak_bw)
				continue;
			pr_info("%-42s %12u %12u\n",
				n->name, n->avg_bw, n->peak_bw);

			hlist_for_each_entry(r, &n->req_list, req_node) {
				if (!r->dev)
					continue;

				if (r->enabled) {
					avg_bw = r->avg_bw;
					peak_bw = r->peak_bw;
				} else {
					avg_bw = 0;
					peak_bw = 0;
				}

				if (avg_bw || peak_bw) {
					pr_info("  %-27s %12u %12u %12u\n",
						dev_name(r->dev), r->tag, avg_bw, peak_bw);
					if (!(r->tag == QCOM_ICC_TAG_ACTIVE_ONLY))
						cnt++;
				}
			}
		}
	}

	return cnt;
}

static int icc_debug_suspend_get(void *data, u64 *val)
{
	*val = debug_suspend;

	return 0;
}

static void icc_debug_suspend_trace_probe(void *unused, const char *action,
					  int val, bool start)
{
	if (start && val > 0 && !strcmp("machine_suspend", action)) {
		pr_info("Enabled interconnect votes:\n");
		icc_print_enabled();
	}
}

static int icc_debug_suspend_set(void *data, u64 val)
{
	int ret;

	val = !!val;
	if (val == debug_suspend)
		return 0;

	if (val)
		ret = register_trace_suspend_resume(icc_debug_suspend_trace_probe, NULL);
	else
		ret = unregister_trace_suspend_resume(icc_debug_suspend_trace_probe, NULL);

	if (ret) {
		pr_err("%s: Failed to %sregister suspend trace callback, ret=%d\n",
		       __func__, val ? "" : "un", ret);
		return ret;
	}

	debug_suspend = val;

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(icc_debug_suspend_fops, icc_debug_suspend_get,
			 icc_debug_suspend_set, "%llu\n");

int qcom_icc_debug_register(struct icc_provider *provider)
{
	struct qcom_icc_debug_provider *dp;

	dp = kzalloc(sizeof(*dp), GFP_KERNEL);
	if (!dp)
		return -ENOMEM;

	dp->provider = provider;

	mutex_lock(&debug_lock);
	list_add_tail(&dp->list, &icc_providers);
	mutex_unlock(&debug_lock);

	return 0;
}
EXPORT_SYMBOL(qcom_icc_debug_register);

int qcom_icc_debug_unregister(struct icc_provider *provider)
{
	struct qcom_icc_debug_provider *dp, *temp;

	mutex_lock(&debug_lock);

	list_for_each_entry_safe(dp, temp, &icc_providers, list) {
		if (dp->provider == provider) {
			list_del(&dp->list);
			kfree(dp);
		}
	}

	mutex_unlock(&debug_lock);

	return 0;
}
EXPORT_SYMBOL(qcom_icc_debug_unregister);

static int icc_debug_suspend(struct device *dev)
{
	int cnt;

	cnt = icc_print_enabled();
	if (cnt)
		pr_err("Enabled icc count: %d\n", cnt);
	else
		pr_info("No interconnects enabled.\n");

	return cnt;
}

static int icc_debug_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops icc_debug_dev_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(icc_debug_suspend,
			icc_debug_resume)
};

static struct platform_device icc_debug_device = {
	.name = "icc_debug",
	.id = -1,
};

static int icc_debug_driver_probe(struct platform_device *pdev)
{
	return 0;
}

static int icc_debug_driver_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver icc_debug_driver = {
	.probe = icc_debug_driver_probe,
	.remove = icc_debug_driver_remove,
	.driver = {
		.name = "icc_debug",
		.pm = &icc_debug_dev_pm_ops,
	},
};

static int icc_syscore_suspend(void)
{
	int cnt;

	cnt = icc_print_enabled();
	if (cnt)
		pr_err("Enabled icc count: %d\n", cnt);
	else
		pr_info("No interconnects enabled.\n");

	return cnt;
}

static struct syscore_ops icc_sleep_syscore_ops = {
	.suspend = icc_syscore_suspend,
};

static int __init qcom_icc_debug_init(void)
{
	static struct dentry *dir;
	int ret;

	ret = platform_device_register(&icc_debug_device);
	if (ret)
		return -ENODEV;

	ret = platform_driver_register(&icc_debug_driver);
	if (ret) {
		pr_err("%s: failed to register icc_debug device driver\n",
				__func__);
		return -ENODEV;
	}

	dir = debugfs_lookup("interconnect", NULL);
	if (IS_ERR_OR_NULL(dir)) {
		ret = PTR_ERR(dir);
		pr_err("%s: unable to find root interconnect debugfs directory, ret=%d\n",
		       __func__, ret);
		return 0;
	}

	dentry_suspend = debugfs_create_file_unsafe("debug_suspend",
						    0644, dir, NULL,
						    &icc_debug_suspend_fops);

	register_syscore_ops(&icc_sleep_syscore_ops);

	return 0;
}
module_init(qcom_icc_debug_init);

static void __exit qcom_icc_debug_exit(void)
{
	debugfs_remove(dentry_suspend);
	if (debug_suspend)
		unregister_trace_suspend_resume(icc_debug_suspend_trace_probe, NULL);
}
module_exit(qcom_icc_debug_exit);

MODULE_DESCRIPTION("QCOM ICC debug library");
MODULE_LICENSE("GPL v2");

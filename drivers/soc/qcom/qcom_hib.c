// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/cpuidle.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <trace/hooks/cpuidle_psci.h>
#include <trace/hooks/bl_hib.h>
#include <linux/blkdev.h>
#include <linux/swap.h>

static struct block_device *hiber_bdev;

static void save_hib_resume_bdev(void *data, struct block_device *hib_resume_bdev)
{
	hiber_bdev = hib_resume_bdev;
}

static void check_hibernation_swap(void *data, struct block_device *dev,
			bool *hib_swap)
{
	if (dev == hiber_bdev)
		*hib_swap = true;
	else
		*hib_swap = false;
}

static void save_cpu_resume(void *data, u64 *addr, u64 phys_addr)
{
	*addr = phys_addr;
}

static int __init init_s2d_hooks(void)
{
	register_trace_android_vh_save_hib_resume_bdev(save_hib_resume_bdev, NULL);
	register_trace_android_vh_check_hibernation_swap(check_hibernation_swap, NULL);
	register_trace_android_vh_save_cpu_resume(save_cpu_resume, NULL);
	return 0;
}

module_init(init_s2d_hooks);
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Bootloader Hibernation Vendor hooks");
MODULE_LICENSE("GPL");

/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Userspace interface for /dev/fmgr_interface_device
 *
 * This file can be used by applications that need to communicate with the fmgr
 * interface via the ioctl interface
 *
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef FM_INTERFACE_H
#define FM_INTERFACE_H
#include <linux/ioctl.h>
/* TODO: Modify below structure based on requirement
 * Add comments for specific members
 */
struct fmgr_ioctl_pmbus_msg {
	uint16_t addr;
	uint32_t reg;
	uint8_t phase;
	uint8_t page;
	uint32_t size;
	union i2c_smbus_data buf;
};

#define PMBUS_READ _IOWR('F', 801, struct fmgr_ioctl_pmbus_msg)
#define PMBUS_WRITE _IOWR('F', 802, struct fmgr_ioctl_pmbus_msg)
#define ALERT_ADDR 0x3
#define CLEAR_FAULTS 0x4
#define ALERT 0x5

#define PMBUS_BYTE 0x1
#define PMBUS_WORD 0x2
#define PMBUS_BLOCK 0x3
#endif


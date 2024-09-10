/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Userspace interface for /dev/rmt_err_sys_evt
 *
 * This file can be used by applications that need to communicate with the
 * sideband driver via the ioctl interface.
 *
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SB_CFG_H
#define __SB_CFG_H

#include <linux/ioctl.h>

/* Private IOCTL codes:
 *
 * SIDEBAND_REQUEST_REMOTE_WAKEUP : Request for the remote processor wakeup
 */

#define SIDEBAND        's'

#define SIDEBAND_REQUEST_REMOTE_WAKEUP _IO(SIDEBAND, 1)

#endif /* __SB_CFG_H */

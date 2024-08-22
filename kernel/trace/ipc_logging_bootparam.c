// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/moduleparam.h>

static bool enabled;
module_param(enabled, bool, 0444);

bool ipc_logging_enabled(void)
{
	return enabled;
}

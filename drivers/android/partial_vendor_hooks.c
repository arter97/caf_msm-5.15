// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define CREATE_TRACE_POINTS
#include <linux/tracepoint.h>
/*
 * Export tracepoints that act as a bare tracehook (ie: have no trace event
 * associated with them) to allow external modules to probe them.
 */

/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM cpuidle_psci
#define TRACE_INCLUDE_PATH trace/hooks
#if !defined(_TRACE_HOOK_CPUIDLE_PSCI_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_CPUIDLE_PSCI_H
#if defined(CONFIG_CPU_IDLE_GOV_QCOM_LPM)
#include <trace/hooks/vendor_hooks_declare.h>
#else
#include <trace/hooks/vendor_hooks.h>
#endif
/*
 * Following tracepoints are not exported in tracefs and provide a
 * mechanism for vendor modules to hook and extend functionality
 */

/* struct cpuidle_device */
#include <linux/cpuidle.h>
DECLARE_HOOK(android_vh_cpuidle_psci_enter,
	TP_PROTO(struct cpuidle_device *dev, bool s2idle),
	TP_ARGS(dev, s2idle));

DECLARE_HOOK(android_vh_cpuidle_psci_exit,
	TP_PROTO(struct cpuidle_device *dev, bool s2idle),
	TP_ARGS(dev, s2idle));

#endif /* _TRACE_HOOK_CPUIDLE_PSCI_H */
/* This part must be outside protection */
#include <trace/define_trace.h>

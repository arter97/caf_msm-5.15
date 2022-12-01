// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define CREATE_TRACE_POINTS
#include <linux/tracepoint.h>
#if defined(CONFIG_CPU_IDLE_GOV_QCOM_LPM)
#include <trace/hooks/cpuidle.h>
#include <trace/hooks/pm_domain.h>
#include <trace/hooks/cpuidle_psci.h>
#endif
#if defined(CONFIG_REMOTEPROC)
#include <trace/hooks/remoteproc.h>
#endif

/*
 * Export tracepoints that act as a bare tracehook (ie: have no trace event
 * associated with them) to allow external modules to probe them.
 */
#if defined(CONFIG_CPU_IDLE_GOV_QCOM_LPM)
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_cpu_idle_enter);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_cpu_idle_exit);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_allow_domain_state);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_cpuidle_psci_enter);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_cpuidle_psci_exit);
#endif
#if defined(CONFIG_REMOTEPROC)
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_rproc_recovery);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_rproc_recovery_set);
#endif

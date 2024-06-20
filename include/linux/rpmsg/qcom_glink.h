/* SPDX-License-Identifier: GPL-2.0
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _LINUX_RPMSG_QCOM_GLINK_H
#define _LINUX_RPMSG_QCOM_GLINK_H

#include <linux/device.h>

struct qcom_glink;

#if IS_ENABLED(CONFIG_RPMSG_QCOM_GLINK)
void qcom_glink_ssr_notify(const char *ssr_name);
void glink_ssr_notify_rpm(void);
#else
static inline void qcom_glink_ssr_notify(const char *ssr_name) {}
#endif

#if IS_ENABLED(CONFIG_RPMSG_QCOM_GLINK_SMEM)

struct qcom_glink *qcom_glink_smem_register(struct device *parent,
					    struct device_node *node);
void qcom_glink_smem_unregister(struct qcom_glink *glink);
int qcom_glink_smem_start(struct qcom_glink *glink);
bool qcom_glink_is_wakeup(bool reset);
void qcom_glink_early_ssr_notify(void *data);

#else

static inline struct qcom_glink *
qcom_glink_smem_register(struct device *parent,
			 struct device_node *node)
{
	return NULL;
}

static inline void qcom_glink_smem_unregister(struct qcom_glink *glink) {}
static inline void qcom_glink_early_ssr_notify(void *data) {}

int qcom_glink_smem_start(struct qcom_glink *glink)
{
	return -ENXIO;
}

static inline bool qcom_glink_is_wakeup(bool reset)
{
	return false;
}
#endif


#if IS_ENABLED(CONFIG_RPMSG_QCOM_GLINK_SPSS)

struct qcom_glink *qcom_glink_spss_register(struct device *parent,
					    struct device_node *node);
void qcom_glink_spss_unregister(struct qcom_glink *glink);

#else

static inline struct qcom_glink *
qcom_glink_spss_register(struct device *parent,
			 struct device_node *node)
{
	return NULL;
}

static inline void qcom_glink_spss_unregister(struct qcom_glink *glink) {}

#endif


#if IS_ENABLED(CONFIG_RPMSG_QCOM_GLINK_SPI)

struct glink_spi *qcom_glink_spi_register(struct device *parent,
					       struct device_node *node);
void qcom_glink_spi_unregister(struct glink_spi *glink);

#else

static inline struct glink_spi *
qcom_glink_spi_register(struct device *parent, struct device_node *node)
{
	return NULL;
}

static inline void qcom_glink_spi_unregister(struct glink_spi *glink) {}

#endif


#endif

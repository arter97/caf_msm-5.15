/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 Synopsys, Inc. and/or its affiliates.
 * Synopsys DesignWare XPCS helpers
 */
/* Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved. */

#ifndef __LINUX_PCS_XPCS_QCOM_H
#define __LINUX_PCS_XPCS_QCOM_H

#include <linux/phy.h>
#include <linux/phylink.h>

/* AN mode */
#define DW_AN_C73			1
#define DW_AN_C37_SGMII			2
#define DW_2500BASEX			3
#define DW_AN_C37_USXGMII		4

struct xpcs_id;

struct dw_xpcs_qcom {
	const struct xpcs_id *id;
	struct phylink_pcs pcs;
	void __iomem *addr;
};

int qcom_xpcs_get_an_mode(struct dw_xpcs_qcom *xpcs,
			  phy_interface_t interface);
void qcom_xpcs_link_up(struct phylink_pcs *pcs, unsigned int mode,
		       phy_interface_t interface, int speed,
		       int duplex);
void qcom_xpcs_validate(struct dw_xpcs_qcom *xpcs, unsigned long *supported,
		   struct phylink_link_state *state);
int qcom_xpcs_config_eee(struct dw_xpcs_qcom *xpcs, int mult_fact_100ns,
		    int enable);
struct dw_xpcs_qcom *qcom_xpcs_create(void __iomem *addr,
				      phy_interface_t interface);
void qcom_xpcs_destroy(struct dw_xpcs_qcom *xpcs);
void xpcs_link_up_usxgmii(struct dw_xpcs_qcom *xpcs, int speed);
void xpcs_link_up_sgmii(struct dw_xpcs_qcom *xpcs, unsigned int mode,
			int speed, int duplex);
int qcom_xpcs_poll_reset(struct dw_xpcs_qcom *xpcs, unsigned int offset,
			 unsigned int field);
const struct xpcs_compat *xpcs_find_compat(const struct xpcs_id *id,
					   phy_interface_t interface);
int qcom_xpcs_enable_pcs_loopback(struct dw_xpcs_qcom *xpcs);
int qcom_xpcs_disable_pcs_loopback(struct dw_xpcs_qcom *xpcs);
int qcom_xpcs_enable_serdes_loopback(struct dw_xpcs_qcom *xpcs, bool speed_1g);
int qcom_xpcs_disable_serdes_loopback(struct dw_xpcs_qcom *xpcs);
void qcom_xpcs_handle_an_intr(struct work_struct *work);
int qcom_xpcs_check_aneg_ioc(void);
#endif /* __LINUX_PCS_XPCS_QCOM_H */

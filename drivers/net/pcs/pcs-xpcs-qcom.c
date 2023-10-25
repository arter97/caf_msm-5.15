// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Synopsys, Inc. and/or its affiliates.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * Synopsys DesignWare XPCS helpers
 *
 * Author: Jose Abreu <Jose.Abreu@synopsys.com>
 */

#include <linux/delay.h>
#include <linux/pcs-xpcs-qcom.h>
#include <linux/mdio.h>
#include <linux/phylink.h>
#include <linux/workqueue.h>
#include "pcs-xpcs-qcom.h"

#ifdef CONFIG_MSM_BOOT_TIME_MARKER
#include <soc/qcom/boot_stats.h>
#endif

#define phylink_pcs_to_xpcs(pl_pcs) \
	container_of((pl_pcs), struct dw_xpcs_qcom, pcs)

#define DRV_NAME "qcom-xpcs"

#define XPCSINFO(fmt, args...) \
do {\
  pr_info(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
} while (0)

#define XPCSERR(fmt, args...) \
do {\
  pr_err(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
} while (0)

phy_interface_t g_interface;
static const int xpcs_usxgmii_features[] = {
	ETHTOOL_LINK_MODE_Pause_BIT,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT,
	ETHTOOL_LINK_MODE_Autoneg_BIT,
	ETHTOOL_LINK_MODE_10baseT_Full_BIT,
	ETHTOOL_LINK_MODE_100baseT_Full_BIT,
	ETHTOOL_LINK_MODE_1000baseKX_Full_BIT,
	ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
	ETHTOOL_LINK_MODE_2500baseX_Full_BIT,
	ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
	ETHTOOL_LINK_MODE_5000baseT_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseKR_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseT_Full_BIT,
	__ETHTOOL_LINK_MODE_MASK_NBITS,
};

static const int xpcs_sgmii_features[] = {
	ETHTOOL_LINK_MODE_Pause_BIT,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT,
	ETHTOOL_LINK_MODE_Autoneg_BIT,
	ETHTOOL_LINK_MODE_10baseT_Half_BIT,
	ETHTOOL_LINK_MODE_10baseT_Full_BIT,
	ETHTOOL_LINK_MODE_100baseT_Half_BIT,
	ETHTOOL_LINK_MODE_100baseT_Full_BIT,
	ETHTOOL_LINK_MODE_1000baseT_Half_BIT,
	ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
	ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
	__ETHTOOL_LINK_MODE_MASK_NBITS,
};

static const int xpcs_2500basex_features[] = {
	ETHTOOL_LINK_MODE_Pause_BIT,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT,
	ETHTOOL_LINK_MODE_Autoneg_BIT,
	ETHTOOL_LINK_MODE_2500baseX_Full_BIT,
	ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
	__ETHTOOL_LINK_MODE_MASK_NBITS,
};

static const phy_interface_t xpcs_usxgmii_interfaces[] = {
	PHY_INTERFACE_MODE_USXGMII,
};

static const phy_interface_t xpcs_sgmii_interfaces[] = {
	PHY_INTERFACE_MODE_SGMII,
};

static const phy_interface_t xpcs_2500basex_interfaces[] = {
	PHY_INTERFACE_MODE_2500BASEX,
};

enum {
	DW_XPCS_USXGMII,
	DW_XPCS_SGMII,
	DW_XPCS_2500BASEX,
	DW_XPCS_INTERFACE_MAX,
};

struct xpcs_id {
	u32 id;
	u32 mask;
	const struct xpcs_compat *compat;
};

const struct xpcs_compat *xpcs_find_compat(const struct xpcs_id *id,
					   phy_interface_t interface)
{
	int i, j;

	for (i = 0; i < DW_XPCS_INTERFACE_MAX; i++) {
		const struct xpcs_compat *compat = &id->compat[i];

		for (j = 0; j < compat->num_interfaces; j++)
			if (compat->interface[j] == interface)
				return compat;
	}

	return NULL;
}

int qcom_xpcs_get_an_mode(struct dw_xpcs_qcom *xpcs, phy_interface_t interface)
{
	const struct xpcs_compat *compat;

	compat = xpcs_find_compat(xpcs->id, interface);
	if (!compat)
		return -ENODEV;

	return compat->an_mode;
}
EXPORT_SYMBOL_GPL(qcom_xpcs_get_an_mode);

static int qcom_xpcs_read(struct dw_xpcs_qcom *xpcs, u32 reg)
{
	return readl(xpcs->addr + reg);
}

static int qcom_xpcs_write(struct dw_xpcs_qcom *xpcs, u32 reg, u16 val)
{
	writel(val, xpcs->addr + reg);
	return 0;
}

static int qcom_xpcs_poll_reset(struct dw_xpcs_qcom *xpcs, unsigned int offset,
			 unsigned int field)
{
	unsigned int retries = 32;
	int ret;

	do {
		usleep_range(1, 20);
		ret = qcom_xpcs_read(xpcs, offset);
		if (ret < 0)
			return ret;
	} while (ret & field && --retries);

	return (ret & field) ? -ETIMEDOUT : 0;
}

static int xpcs_soft_reset(struct dw_xpcs_qcom *xpcs,
			   const struct xpcs_compat *compat)
{
	int ret;

	switch (compat->an_mode) {
	case DW_AN_C73:
		XPCSERR("CL73 Autonegotiation is not supported\n");
		return -EINVAL;
	case DW_AN_C37_USXGMII:
	case DW_AN_C37_SGMII:
	case DW_2500BASEX:
		break;
	default:
		XPCSERR("Incompatible Autonegotiation mode\n");
		return -EINVAL;
	}

	ret = qcom_xpcs_read(xpcs, DW_SR_MII_PCS_CTRL1);
	if (ret < 0)
		return ret;

	ret = qcom_xpcs_write(xpcs, DW_SR_MII_PCS_CTRL1, ret | SOFT_RST);

	if (compat->an_mode == DW_AN_C37_USXGMII) {
		ret = qcom_xpcs_poll_reset(xpcs, DW_SR_MII_PCS_CTRL1, SW_RST_BIT_STATUS);

		ret = qcom_xpcs_read(xpcs, DW_VR_MII_DIG_CTRL1);
		if (ret < 0)
			return ret;

		ret = qcom_xpcs_write(xpcs, DW_VR_MII_DIG_CTRL1, ret | SOFT_RST);
		return qcom_xpcs_poll_reset(xpcs, DW_VR_MII_DIG_CTRL1, SW_RST_BIT_STATUS);
	}

	return qcom_xpcs_poll_reset(xpcs, DW_SR_MII_PCS_CTRL1, SW_RST_BIT_STATUS);
}

void qcom_xpcs_validate(struct dw_xpcs_qcom *xpcs, unsigned long *supported,
			struct phylink_link_state *state)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(xpcs_supported);
	const struct xpcs_compat *compat;
	int i;

	/* phylink expects us to report all supported modes with
	 * PHY_INTERFACE_MODE_NA, just don't limit the supported and
	 * advertising masks and exit.
	 */
	if (state->interface == PHY_INTERFACE_MODE_NA)
		return;

	bitmap_zero(xpcs_supported, __ETHTOOL_LINK_MODE_MASK_NBITS);

	compat = xpcs_find_compat(xpcs->id, state->interface);

	/* Populate the supported link modes for this
	 * PHY interface type
	 */
	if (compat)
		for (i = 0; compat->supported[i] != __ETHTOOL_LINK_MODE_MASK_NBITS; i++)
			set_bit(compat->supported[i], xpcs_supported);

	linkmode_and(supported, supported, xpcs_supported);
	linkmode_and(state->advertising, state->advertising, xpcs_supported);
}
EXPORT_SYMBOL_GPL(qcom_xpcs_validate);

int qcom_xpcs_serdes_loopback(struct dw_xpcs_qcom *xpcs, bool on)
{
	int ret;

	ret = qcom_xpcs_read(xpcs, DW_SR_MII_MMD_CTRL);
	if (ret < 0) {
		XPCSERR("Register read failed\n");
		return ret;
	}

	if (on)
		ret |= DW_R2TLBE;
	else
		ret &= ~DW_R2TLBE;

	return qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, ret);
}
EXPORT_SYMBOL_GPL(qcom_xpcs_serdes_loopback);

/* To enable or disable Network-PCS loopback. */
int qcom_xpcs_pcs_loopback(struct dw_xpcs_qcom *xpcs, bool on)
{
	int ret, pmbl_ctrl = 0;

	ret = qcom_xpcs_read(xpcs, DW_VR_MII_PCS_DIG_CTRL1);
	if (ret < 0)
		goto out;

	if (on) {
		if (xpcs->sgmii_2p5g_en) {
			XPCSINFO("Enabling 2.5Gbps-SGMII XPCS loopback\n");
			pmbl_ctrl = qcom_xpcs_read(xpcs, DW_VR_MII_PCS_DEBUG_CTRL);
			if (pmbl_ctrl < 0)
				goto out;

			pmbl_ctrl = qcom_xpcs_write(xpcs, DW_VR_MII_PCS_DEBUG_CTRL,
						    pmbl_ctrl | TX_PMBL_CTRL);
		}
		ret |= DW_R2TLBE;
	} else {
		if (xpcs->sgmii_2p5g_en) {
			XPCSINFO("Disabling 2.5Gbps-SGMII XPCS loopback\n");
			pmbl_ctrl = qcom_xpcs_read(xpcs, DW_VR_MII_PCS_DEBUG_CTRL);
			if (pmbl_ctrl < 0)
				goto out;

			pmbl_ctrl &= ~TX_PMBL_CTRL;
			pmbl_ctrl = qcom_xpcs_write(xpcs, DW_VR_MII_PCS_DEBUG_CTRL,
						    pmbl_ctrl);
		}
		ret &= ~DW_R2TLBE;
	}

	return qcom_xpcs_write(xpcs, DW_VR_MII_PCS_DIG_CTRL1, ret);

out:
	XPCSERR("Register read failed\n");
	return ret;
}
EXPORT_SYMBOL_GPL(qcom_xpcs_pcs_loopback);

/* KREEE: USXGMII 10GBase-R/KR mode
 * KX4EEE: SGMII 10GBase-X/KX4 mode
 */
int qcom_xpcs_config_eee(struct dw_xpcs_qcom *xpcs, int mult_fact_100ns, int enable)
{
	int ret;

	ret = qcom_xpcs_read(xpcs, DW_SR_MII_PCS_EEE_ABL);
	if (ret < 0)
		goto out;

	if (ret & KREEE_SUPPORT) {
		XPCSINFO("EEE supported for 10GBase-R/KR mode\n");
	} else if (ret & KX4EEE_SUPPORT) {
		XPCSINFO("EEE supported for 10GBase-X/KX4 mode\n");
	} else {
		XPCSERR("EEE not supported for SGMII or USXGMII interfaces\n");
		return -ENODEV;
	}

	ret = qcom_xpcs_read(xpcs, DW_VR_XS_PCS_EEE_MCTRL0);
	if (ret < 0)
		goto out;

	if (enable)
		ret = DW_VR_MII_EEE_LTX_EN | DW_VR_MII_EEE_TX_EN_CTRL |
			DW_VR_MII_EEE_LRX_EN | DW_VR_MII_EEE_RX_EN_CTRL |
			mult_fact_100ns << DW_VR_MII_EEE_MULT_FACT_100NS_SHIFT;
	else
		ret &= ~(DW_VR_MII_EEE_LTX_EN | DW_VR_MII_EEE_TX_EN_CTRL |
			DW_VR_MII_EEE_LRX_EN | DW_VR_MII_EEE_RX_EN_CTRL |
			mult_fact_100ns << DW_VR_MII_EEE_MULT_FACT_100NS_SHIFT);

	ret = qcom_xpcs_write(xpcs, DW_VR_XS_PCS_EEE_MCTRL0, ret);

	ret = qcom_xpcs_read(xpcs, DW_VR_XS_PCS_EEE_MCTRL1);
	if (ret < 0)
		goto out;

	if (enable)
		return qcom_xpcs_write(xpcs, DW_VR_XS_PCS_EEE_MCTRL1, ret | DW_VR_MII_EEE_TRN_LPI);

	ret &= ~DW_VR_MII_EEE_TRN_LPI;
	return qcom_xpcs_write(xpcs, DW_VR_XS_PCS_EEE_MCTRL1, ret);
out:
	XPCSERR("Register read failed\n");
	return ret;
}
EXPORT_SYMBOL_GPL(qcom_xpcs_config_eee);

int qcom_xpcs_verify_lnk_status_usxgmii(struct dw_xpcs_qcom *xpcs)
{
	unsigned int retries = LINK_STS_RETRY_COUNT;
	int ret;

	do {
		ret = qcom_xpcs_read(xpcs, DW_SR_MII_MMD_STS);
		if (ret < 0)
			return ret;

		if (ret & DW_SR_MII_STS_LINK_STS)
			break;

		ret = qcom_xpcs_read(xpcs, DW_VR_MII_DIG_CTRL1);
		if (ret < 0)
			return ret;

		ret = qcom_xpcs_write(xpcs, DW_VR_MII_DIG_CTRL1, ret | SOFT_RST);

		usleep_range(500, 1000);
	} while (--retries);

	XPCSINFO("%s : Val = 0x%x retries = %ld\n",
		 __func__,
		ret,
		(LINK_STS_RETRY_COUNT - retries));

	return !(retries) ? -ETIMEDOUT : 0;
}
EXPORT_SYMBOL(qcom_xpcs_verify_lnk_status_usxgmii);

/* Cannot sleep in interrupt-context, increase retries and remove usleep call. */
static int qcom_xpcs_poll_reset_usxgmii(struct dw_xpcs_qcom *xpcs, unsigned int offset,
					unsigned int field)
{
	unsigned int retries = 1000;
	int ret;

	do {
		ret = qcom_xpcs_read(xpcs, offset);
		if (ret < 0)
			return ret;
	} while (ret & field && --retries);

	return (ret & field) ? -ETIMEDOUT : 0;
}

/* Reset PCS and USXGMII Rate Adaptor Logic*/
static int qcom_xpcs_reset_usxgmii(struct dw_xpcs_qcom *xpcs)
{
	int ret;

	ret = qcom_xpcs_read(xpcs, DW_VR_MII_DIG_CTRL1);
	if (ret < 0)
		return ret;

	ret = qcom_xpcs_write(xpcs, DW_VR_MII_DIG_CTRL1, ret | SOFT_RST);

	if (!xpcs->intr_en)
		ret = qcom_xpcs_poll_reset(xpcs, DW_VR_MII_DIG_CTRL1, SW_RST_BIT_STATUS);
	else
		ret = qcom_xpcs_poll_reset_usxgmii(xpcs, DW_VR_MII_DIG_CTRL1, SW_RST_BIT_STATUS);

	if (ret < 0)
		return ret;

	ret = qcom_xpcs_read(xpcs, DW_VR_MII_PCS_DIG_CTRL1);
	if (ret < 0)
		return ret;

	ret = qcom_xpcs_write(xpcs, DW_VR_MII_PCS_DIG_CTRL1, ret | DW_USXGMII_RST);

	if (!xpcs->intr_en)
		return qcom_xpcs_poll_reset(xpcs, DW_VR_MII_DIG_CTRL1, SW_RST_BIT_STATUS);

	return qcom_xpcs_poll_reset_usxgmii(xpcs, DW_VR_MII_PCS_DIG_CTRL1, USXG_RST_BIT_STATUS);
}

/* Reenable Clause 37 Autonegotiation for 10/100/1000M SGMII */
static int qcom_xpcs_unset_2p5g_sgmii(struct dw_xpcs_qcom *xpcs)
{
	int ret;

	if (!xpcs->fixed_phy_mode) {
		ret = qcom_xpcs_read(xpcs, DW_SR_MII_MMD_CTRL);
		if (ret < 0)
			return -EINVAL;

		ret |= AN_CL37_EN;
		ret = qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, ret);
	}

	ret = qcom_xpcs_read(xpcs, DW_VR_MII_PCS_DIG_CTRL1);
	if (ret < 0)
		return -EINVAL;

	ret &= ~DW_VR_MII_DIG_CTRL1_2G5_EN;
	ret = qcom_xpcs_write(xpcs, DW_VR_MII_PCS_DIG_CTRL1, ret);

	xpcs->sgmii_2p5g_en = false;

	XPCSINFO("2.5Gbps-SGMII disabled\n");
	return 0;
}

/* Clause 37 Autonegotiation not supported on SGMII+ mode */
static int qcom_xpcs_set_2p5g_sgmii(struct dw_xpcs_qcom *xpcs, int duplex)
{
	int ret;

	ret = qcom_xpcs_read(xpcs, DW_SR_MII_MMD_CTRL);
	if (ret < 0)
		goto out;

	ret &= ~DW_SGMII_SS_MASK;
	ret &= ~AN_CL37_EN;

	if (duplex == DUPLEX_FULL)
		ret |= DW_FULL_DUPLEX;
	else
		ret &= ~DW_FULL_DUPLEX;

	ret |= DW_GMII_1000;
	ret = qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, ret);

	ret = qcom_xpcs_read(xpcs, DW_VR_MII_AN_CTRL);
	if (ret < 0)
		goto out;

	ret = qcom_xpcs_write(xpcs, DW_VR_MII_AN_CTRL, ret | DW_VR_MII_CTRL);

	ret = qcom_xpcs_read(xpcs, DW_VR_MII_PCS_DIG_CTRL1);
	if (ret < 0)
		goto out;

	xpcs->sgmii_2p5g_en = true;

	XPCSINFO("2.5Gbps-SGMII enabled\n");
	return qcom_xpcs_write(xpcs, DW_VR_MII_PCS_DIG_CTRL1, ret | DW_VR_MII_DIG_CTRL1_2G5_EN);

out:
	XPCSERR("Register read failed\n");
	return ret;
}

static int qcom_xpcs_sgmii_read_intr_status(struct dw_xpcs_qcom *xpcs)
{
	int intr_stat;

	intr_stat = qcom_xpcs_read(xpcs, DW_VR_MII_AN_INTR_STS);
	if (intr_stat < 0)
		return intr_stat;

	if (intr_stat & DW_VR_MII_C37_ANSGM_SP_LNKSTS) {
		int speed, mmd_ctrl, an_ctrl;

		an_ctrl = qcom_xpcs_read(xpcs, DW_VR_MII_AN_CTRL);
		if (an_ctrl < 0)
			return an_ctrl;

		mmd_ctrl = qcom_xpcs_read(xpcs, DW_SR_MII_MMD_CTRL);
		if (mmd_ctrl < 0)
			return mmd_ctrl;

		if (!(intr_stat & DW_VR_MII_AN_STS_C37_ANSGM_FD))
			mmd_ctrl &= ~DW_FULL_DUPLEX;
		else
			mmd_ctrl |= DW_FULL_DUPLEX;

		mmd_ctrl &= ~DW_SGMII_SS_MASK;
		speed = (intr_stat & DW_VR_MII_AN_STS_C37_ANSGM_SP) >>
					DW_VR_MII_AN_STS_C37_ANSGM_SP_SHIFT;

		switch (speed) {
		case DW_VR_MII_C37_ANSGM_SP_1000:
			an_ctrl |= DW_VR_MII_CTRL;
			mmd_ctrl |= DW_GMII_1000;
			XPCSINFO("1Gbps-SGMII enabled\n");
			break;
		case DW_VR_MII_C37_ANSGM_SP_100:
			an_ctrl &= ~DW_VR_MII_CTRL;
			mmd_ctrl |= DW_GMII_100;
			XPCSINFO("100Mbps-SGMII enabled\n");
			break;
		case DW_VR_MII_C37_ANSGM_SP_10:
			an_ctrl &= ~DW_VR_MII_CTRL;
			XPCSINFO("10Mbps-SGMII enabled\n");
			break;
		default:
			XPCSERR("Invalid speed mode: %d\n", speed);
			return -EINVAL;
		}

		mmd_ctrl = qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, mmd_ctrl);
		return qcom_xpcs_write(xpcs, DW_VR_MII_AN_CTRL, an_ctrl);
	} else {
		XPCSERR("Link is down, aborting\n");
		return 0;
	}
}

static int qcom_xpcs_usxgmii_read_intr_status(struct dw_xpcs_qcom *xpcs)
{
	int intr_stat;

	intr_stat = qcom_xpcs_read(xpcs, DW_VR_MII_AN_INTR_STS);
	if (intr_stat < 0)
		return intr_stat;

	if (intr_stat & DW_VR_MII_USXG_ANSGM_SP_LNKSTS) {
		int mmd_ctrl, speed;

		mmd_ctrl = qcom_xpcs_read(xpcs, DW_SR_MII_MMD_CTRL);
		if (mmd_ctrl < 0)
			return mmd_ctrl;

		mmd_ctrl &= ~DW_USXGMII_SS_MASK;
		speed = (intr_stat & DW_VR_MII_USXG_ANSGM_SP) >> DW_VR_MII_USXG_ANSGM_SP_SHIFT;

		switch (speed) {
		case DW_VR_MII_USXG_ANSGM_SP_10G:
			mmd_ctrl |= DW_USXGMII_10000;
			XPCSINFO("10Gbps-USXGMII enabled\n");
			break;
		case DW_VR_MII_USXG_ANSGM_SP_5G:
			mmd_ctrl |= DW_USXGMII_5000;
			XPCSINFO("5Gbps-USXGMII enabled\n");
			break;
		case DW_VR_MII_USXG_ANSGM_SP_2P5G:
			mmd_ctrl |= DW_GMII_2500;
			XPCSINFO("2.5Gbps-USXGMII enabled\n");
			break;
		case DW_VR_MII_USXG_ANSGM_SP_1000:
			mmd_ctrl |= DW_GMII_1000;
			XPCSINFO("1Gbps-USXGMII enabled\n");
			break;
		case DW_VR_MII_USXG_ANSGM_SP_100:
			mmd_ctrl |= DW_GMII_100;
			XPCSINFO("100Mbps-USXGMII enabled\n");
			break;
		case DW_VR_MII_USXG_ANSGM_SP_10:
			XPCSINFO("10Mbps-USXGMII enabled\n");
			break;
		default:
			XPCSERR("Invalid speed mode: %d\n", speed);
			return -EINVAL;
		}

		mmd_ctrl = qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, mmd_ctrl);

		mmd_ctrl = qcom_xpcs_reset_usxgmii(xpcs);
		if (mmd_ctrl < 0)
			XPCSERR("Failed to reset USXGMII\n");

		return mmd_ctrl;
	} else {
		XPCSERR("Link is down, aborting\n");
		return 0;
	}
}

static void qcom_xpcs_handle_an_intr(struct dw_xpcs_qcom *xpcs,
				     phy_interface_t interface)
{
	int ret = 0;

	switch (interface) {
	case PHY_INTERFACE_MODE_USXGMII:
		ret = qcom_xpcs_usxgmii_read_intr_status(xpcs);
		if (ret < 0)
			goto out;

		XPCSINFO("Finished Autonegotiation for USXGMII\n");
		return;
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_2500BASEX:
		ret = qcom_xpcs_sgmii_read_intr_status(xpcs);
		if (ret < 0)
			goto out;

		XPCSINFO("Finished Autonegotiation for SGMII\n");
		return;
	default:
		XPCSERR("Invalid MII mode for Autonegotiation\n");
		goto out;
	}

out:
	XPCSERR("Failed to handle Autonegotiation interrupt\n");
}

int qcom_xpcs_check_aneg_ioc(struct dw_xpcs_qcom *xpcs, phy_interface_t interface)
{
	int ret;

	/* Check if interrupt was set */
	ret = qcom_xpcs_read(xpcs, DW_VR_MII_AN_INTR_STS);
	if (ret < 0) {
		XPCSERR("Register read failed\n");
		return ret;
	}

	if (!(ret & DW_VR_MII_ANCMPLT_INTR)) {
		XPCSERR("Autonegotiation is still not finished\n");
		return -ENODEV;
	}

	/* Clear the IOC status */
	ret &= ~DW_VR_MII_ANCMPLT_INTR;
	ret = qcom_xpcs_write(xpcs, DW_VR_MII_AN_INTR_STS, ret);

	qcom_xpcs_handle_an_intr(xpcs, interface);
	return 0;
}
EXPORT_SYMBOL_GPL(qcom_xpcs_check_aneg_ioc);

/* By default, enable Clause 37 autonegotiation */
static int xpcs_config_aneg_c37(struct dw_xpcs_qcom *xpcs)
{
	int ret;

	if (!xpcs->fixed_phy_mode) {
		ret = qcom_xpcs_read(xpcs, DW_SR_MII_MMD_CTRL);
		if (ret < 0)
			return -EINVAL;

		ret |= AN_CL37_EN;
		ret = qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, ret);
	}

	ret = qcom_xpcs_read(xpcs, DW_VR_MII_AN_CTRL);
	if (ret < 0)
		return -EINVAL;

	if (!xpcs->fixed_phy_mode)
		ret |= DW_VR_MII_TX_CONFIG_MASK;
	ret |= DW_VR_MII_SGMII_LINK_STS;

	return qcom_xpcs_write(xpcs, DW_VR_MII_AN_CTRL, ret);
}

static int qcom_xpcs_do_config(struct dw_xpcs_qcom *xpcs, phy_interface_t interface)
{
	const struct xpcs_compat *compat;
	int ret;

	compat = xpcs_find_compat(xpcs->id, interface);
	if (!compat)
		return -ENODEV;

	switch (compat->an_mode) {
	case DW_AN_C73:
		XPCSERR("C73 Autonegotiation is not supported!\n");
		return -EINVAL;
	case DW_AN_C37_USXGMII:
	case DW_AN_C37_SGMII:
	case DW_2500BASEX:
		ret = xpcs_config_aneg_c37(xpcs);
		if (ret < 0)
			return ret;
		break;
	default:
		XPCSERR("Incompatible Autonegotiation mode\n");
		return -EINVAL;
	}

	return 0;
}

static int qcom_xpcs_config(struct phylink_pcs *pcs, unsigned int mode, phy_interface_t interface,
			    const unsigned long *advertising,
			    bool permit_pause_to_mac)
{
	struct dw_xpcs_qcom *xpcs = phylink_pcs_to_xpcs(pcs);

	return qcom_xpcs_do_config(xpcs, interface);
}

static int xpcs_get_state_c37_usxgmii(struct dw_xpcs_qcom *xpcs,
				    struct phylink_link_state *state)
{
	int ret;

	ret = qcom_xpcs_read(xpcs, DW_VR_MII_AN_INTR_STS);
	if (ret < 0)
		return -ENODEV;

	if (ret & DW_VR_MII_USXG_ANSGM_SP_LNKSTS)
		state->link = true;
	else
		state->link = false;

	return 0;
}

/* Must check if link is up even if we are in 2.5Gbps mode */
static int xpcs_get_state_c37_sgmii(struct dw_xpcs_qcom *xpcs,
				    struct phylink_link_state *state)
{
	int ret;

	/* Reset link_state */
	state->link = false;
	state->speed = SPEED_UNKNOWN;
	state->duplex = DUPLEX_UNKNOWN;
	state->pause = 0;

	/* For C37 SGMII mode, we check DW_VR_MII_AN_INTR_STS for link
	 * status, speed and duplex.
	 */
	ret = qcom_xpcs_read(xpcs, DW_VR_MII_AN_INTR_STS);
	if (ret < 0)
		return -EINVAL;

	if (ret & DW_VR_MII_C37_ANSGM_SP_LNKSTS) {
		int speed_value;

		state->link = true;

		speed_value = (ret & DW_VR_MII_AN_STS_C37_ANSGM_SP) >>
			      DW_VR_MII_AN_STS_C37_ANSGM_SP_SHIFT;
		if (speed_value == DW_VR_MII_C37_ANSGM_SP_1000)
			state->speed = SPEED_1000;
		else if (speed_value == DW_VR_MII_C37_ANSGM_SP_100)
			state->speed = SPEED_100;
		else
			state->speed = SPEED_10;

		if (ret & DW_VR_MII_AN_STS_C37_ANSGM_FD)
			state->duplex = DUPLEX_FULL;
		else
			state->duplex = DUPLEX_HALF;
	}

	return 0;
}

static void qcom_xpcs_get_state(struct phylink_pcs *pcs,
			   struct phylink_link_state *state)
{
	struct dw_xpcs_qcom *xpcs = phylink_pcs_to_xpcs(pcs);
	const struct xpcs_compat *compat;
	int ret;

	compat = xpcs_find_compat(xpcs->id, state->interface);
	if (!compat)
		return;

	switch (compat->an_mode) {
	case DW_AN_C73:
		XPCSERR("C73 Autonegotiation is not supported!\n");
		return;
	case DW_AN_C37_USXGMII:
		ret = xpcs_get_state_c37_usxgmii(xpcs, state);
		if (ret < 0)
			XPCSERR("Failed to get USXGMII state\n");
		break;
	case DW_AN_C37_SGMII:
		ret = xpcs_get_state_c37_sgmii(xpcs, state);
		if (ret < 0)
			XPCSERR("Failed to get SGMII state\n");
		break;
	default:
		return;
	}
}

void qcom_xpcs_link_up_sgmii(struct dw_xpcs_qcom *xpcs, int speed, int duplex)
{
	int mmd_ctrl, an_ctrl;
	int ret = 0;

	if (speed == SPEED_2500) {
		ret = qcom_xpcs_set_2p5g_sgmii(xpcs, duplex);
		if (ret < 0)
			goto err;
		goto out;
	} else if (xpcs->sgmii_2p5g_en) {
		ret = qcom_xpcs_unset_2p5g_sgmii(xpcs);
		if (ret < 0)
			goto err;
	}

	an_ctrl = qcom_xpcs_read(xpcs, DW_VR_MII_AN_CTRL);
	if (an_ctrl < 0)
		goto err;

	mmd_ctrl = qcom_xpcs_read(xpcs, DW_SR_MII_MMD_CTRL);
	if (mmd_ctrl < 0)
		goto err;

	mmd_ctrl &= ~DW_SGMII_SS_MASK;

	switch (speed) {
	case SPEED_1000:
		an_ctrl |= DW_VR_MII_CTRL;
		mmd_ctrl |= DW_GMII_1000;
		XPCSINFO("1Gbps-SGMII enabled\n");
		break;
	case SPEED_100:
		an_ctrl &= ~DW_VR_MII_CTRL;
		mmd_ctrl |= DW_GMII_100;
		XPCSINFO("100Mbps-SGMII enabled\n");
		break;
	case SPEED_10:
		an_ctrl &= ~DW_VR_MII_CTRL;
		XPCSINFO("10Mbps-SGMII enabled\n");
		break;
	default:
		XPCSERR("Invalid speed mode: %d\n", speed);
		return;
	}

	if (duplex == DUPLEX_FULL)
		mmd_ctrl |= DW_FULL_DUPLEX;

	mmd_ctrl = qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, mmd_ctrl);
	an_ctrl = qcom_xpcs_write(xpcs, DW_VR_MII_AN_CTRL, an_ctrl);

out:
	if (xpcs->fixed_phy_mode)
		XPCSINFO("mac2mac mode: PCS link up speed = %d\n",
			 speed);
	XPCSINFO("SGMII link is up\n");
	return;
err:
	XPCSERR("Failed to bring up SGMII link\n");
}

void qcom_xpcs_link_up_usxgmii(struct dw_xpcs_qcom *xpcs, int speed)
{
	int mmd_ctrl;

	mmd_ctrl = qcom_xpcs_read(xpcs, DW_SR_MII_MMD_CTRL);
	if (mmd_ctrl < 0)
		goto out;

	mmd_ctrl &= ~DW_USXGMII_SS_MASK;

	switch (speed) {
	case SPEED_10000:
		mmd_ctrl |= DW_USXGMII_10000;
		XPCSINFO("10Gbps-USXGMII enabled\n");
		break;
	case SPEED_5000:
		mmd_ctrl |= DW_USXGMII_5000;
		XPCSINFO("5Gbps-USXGMII enabled\n");
		break;
	case SPEED_2500:
		mmd_ctrl |= DW_GMII_2500;
		XPCSINFO("2.5Gbps-USXGMII enabled\n");
		break;
	case SPEED_1000:
		mmd_ctrl |= DW_GMII_1000;
		XPCSINFO("1Gbps-USXGMII enabled\n");
		break;
	case SPEED_100:
		mmd_ctrl |= DW_GMII_100;
		XPCSINFO("100Mbps-USXGMII enabled\n");
		break;
	case SPEED_10:
		XPCSINFO("10Mbps-USXGMII enabled\n");
		break;
	default:
		XPCSERR("Invalid speed mode selected\n");
		return;
	}

	mmd_ctrl = qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, mmd_ctrl);

	mmd_ctrl = qcom_xpcs_reset_usxgmii(xpcs);
	if (mmd_ctrl < 0)
		goto out;

	if (xpcs->fixed_phy_mode)
		XPCSINFO("mac2mac mode: PCS link up speed = %d\n",
			 speed);

	XPCSINFO("USXGMII link is up\n");
	return;
out:
	XPCSERR("Failed to bring up USXGMII link\n");
}

/* USXGMII: Return early if interrupt was enabled.
 * Autonegotiation ISR will set speed and duplex instead.
 * SGMII: For 2.5Gbps, let ISR do NOP since SGMII+ not supported in
 * registers for CL37. qcom_xpcs_link_up_sgmii will take care of
 * setting 2.5Gbps.
 */
void qcom_xpcs_link_up(struct phylink_pcs *pcs, unsigned int mode,
		  phy_interface_t interface, int speed, int duplex)
{
	struct dw_xpcs_qcom *xpcs = phylink_pcs_to_xpcs(pcs);

	switch (interface) {
	case PHY_INTERFACE_MODE_2500BASEX:
	case PHY_INTERFACE_MODE_SGMII:
		qcom_xpcs_link_up_sgmii(xpcs, speed, duplex);
		break;
	case PHY_INTERFACE_MODE_USXGMII:
		if (xpcs->intr_en)
			return;
		qcom_xpcs_link_up_usxgmii(xpcs, speed);
		break;
	default:
		XPCSERR("Invalid MII mode: %s\n", phy_modes(interface));
		return;
	}
#ifdef CONFIG_MSM_BOOT_TIME_MARKER
	update_marker("M - Ethernet XPCS is ready -system side link up received");
#endif
	return;
}
EXPORT_SYMBOL_GPL(qcom_xpcs_link_up);

int qcom_xpcs_intr_enable(struct dw_xpcs_qcom *xpcs)
{
	int ret = 0;

	ret = qcom_xpcs_read(xpcs, DW_VR_MII_AN_CTRL);
	if (ret < 0) {
		XPCSERR("Failed to enable CL37 interrupts\n");
		return ret;
	}

	ret |= DW_VR_MII_AN_INTR_EN;
	return qcom_xpcs_write(xpcs, DW_VR_MII_AN_CTRL, ret);
}
EXPORT_SYMBOL_GPL(qcom_xpcs_intr_enable);

static u32 xpcs_get_id(struct dw_xpcs_qcom *xpcs)
{
	int ret;
	u32 id;

	/* Next, search C37 PCS using Vendor-Specific MII MMD */
	ret = qcom_xpcs_read(xpcs, DW_SR_MII_PCS_DEV_ID1);
	if (ret < 0)
		return 0xffffffff;

	id = ret << 16;

	ret = qcom_xpcs_read(xpcs, DW_SR_MII_PCS_DEV_ID2);
	if (ret < 0)
		return 0xffffffff;

	/* If Device IDs are not all zeros, we found C37 AN-type device */
	if (id | ret)
		return id | ret;

	return 0xffffffff;
}

static const struct xpcs_compat synopsys_xpcs_compat[DW_XPCS_INTERFACE_MAX] = {
	[DW_XPCS_USXGMII] = {
		.supported = xpcs_usxgmii_features,
		.interface = xpcs_usxgmii_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_usxgmii_interfaces),
		.an_mode = DW_AN_C37_USXGMII,
	},
	[DW_XPCS_SGMII] = {
		.supported = xpcs_sgmii_features,
		.interface = xpcs_sgmii_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_sgmii_interfaces),
		.an_mode = DW_AN_C37_SGMII,
	},
	[DW_XPCS_2500BASEX] = {
		.supported = xpcs_2500basex_features,
		.interface = xpcs_2500basex_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_2500basex_features),
		.an_mode = DW_2500BASEX,
	},
};

static const struct xpcs_id xpcs_id_list[] = {
	{
		.id = SYNOPSYS_XPCS_ID,
		.mask = SYNOPSYS_XPCS_MASK,
		.compat = synopsys_xpcs_compat,
	},
};

static const struct phylink_pcs_ops qcom_xpcs_phylink_ops = {
	.pcs_config = qcom_xpcs_config,
	.pcs_get_state = qcom_xpcs_get_state,
	.pcs_link_up = qcom_xpcs_link_up,
};

static int qcom_xpcs_select_mode(struct dw_xpcs_qcom *xpcs, phy_interface_t interface)
{
	int ret;

	g_interface = interface;
	if (interface == PHY_INTERFACE_MODE_SGMII) {
		ret = qcom_xpcs_read(xpcs, DW_SR_MII_PCS_CTRL2);
		if (ret < 0)
			goto out;

		ret &= ~PCS_TYPE_SEL_10GBR;
		ret = qcom_xpcs_write(xpcs, DW_SR_MII_PCS_CTRL2, ret | PCS_TYPE_SEL_10GBX);

		ret = qcom_xpcs_read(xpcs, DW_VR_MII_AN_CTRL);
		if (ret < 0)
			goto out;

		ret &= ~DW_VR_MII_PCS_MODE_MASK;
		ret |= DW_VR_MII_PCS_MODE_C37_SGMII;
		return qcom_xpcs_write(xpcs, DW_VR_MII_AN_CTRL, ret);
	} else if (interface == PHY_INTERFACE_MODE_2500BASEX) {
		ret = qcom_xpcs_read(xpcs, DW_SR_MII_PCS_CTRL2);
		if (ret < 0)
			goto out;

		ret &= ~PCS_TYPE_SEL_2500BASEX;
		ret = qcom_xpcs_write(xpcs, DW_SR_MII_PCS_CTRL2, ret | PCS_TYPE_SEL_2500BASEX);

		ret = qcom_xpcs_read(xpcs, DW_VR_MII_PCS_KR_CTRL);
		if (ret < 0)
			goto out;

		ret &= ~BIT(11);
		ret = qcom_xpcs_write(xpcs, DW_VR_MII_PCS_KR_CTRL, ret | BIT(11));

		ret = qcom_xpcs_read(xpcs, DW_VR_MII_PCS_DEBUG_CTRL);
		if (ret < 0)
			goto out;

		ret &= ~(BIT(8) | BIT(9));
		ret = qcom_xpcs_write(xpcs, DW_VR_MII_PCS_DEBUG_CTRL, ret | (BIT(8) | BIT(9)));

		ret = qcom_xpcs_read(xpcs, DW_VR_MII_AN_CTRL);
		if (ret < 0)
			goto out;

		ret &= ~DW_VR_MII_PCS_MODE_MASK;
		ret |= DW_VR_MII_PCS_MODE_C37_SGMII;
		return qcom_xpcs_write(xpcs, DW_VR_MII_AN_CTRL, ret);
	} else if (interface == PHY_INTERFACE_MODE_USXGMII) {
		ret = qcom_xpcs_read(xpcs, DW_SR_MII_PCS_CTRL2);
		if (ret < 0)
			goto out;

		ret &= ~PCS_TYPE_SEL_10GBR;
		ret = qcom_xpcs_write(xpcs, DW_SR_MII_PCS_CTRL2, ret);

		ret = qcom_xpcs_read(xpcs, DW_SR_MII_PCS_CTRL1);
		if (ret < 0)
			goto out;

		ret = qcom_xpcs_write(xpcs, DW_SR_MII_PCS_CTRL1, ret | LPM_EN);

		usleep_range(1, 20);

		ret = qcom_xpcs_read(xpcs, DW_SR_MII_PCS_CTRL1);
		if (ret < 0)
			goto out;

		ret &= ~LPM_EN;
		ret = qcom_xpcs_write(xpcs, DW_SR_MII_PCS_CTRL1, ret);

		ret = qcom_xpcs_read(xpcs, DW_VR_MII_PCS_DIG_CTRL1);
		if (ret < 0)
			goto out;

		return qcom_xpcs_write(xpcs, DW_VR_MII_PCS_DIG_CTRL1, ret | DW_USXGMII_EN);
	}

	XPCSERR("Incompatible MII interface: %d\n", interface);
	return -EINVAL;

out:
	XPCSERR("Register read failed\n");
	return -EINVAL;
}

struct dw_xpcs_qcom *qcom_xpcs_create(void __iomem *addr, phy_interface_t interface)
{
	const struct xpcs_compat *compat;
	struct dw_xpcs_qcom *xpcs;
	u32 xpcs_id;
	int i, ret = 0;

	xpcs = kzalloc(sizeof(*xpcs), GFP_KERNEL);
	if (!xpcs)
		return ERR_PTR(-ENOMEM);

	xpcs->addr = addr;

	xpcs_id = xpcs_get_id(xpcs);
	if (xpcs_id == 0xffffffff) {
		XPCSERR("Invalid XPCS Device ID\n");
		ret = -ENODEV;
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(xpcs_id_list); i++) {
		const struct xpcs_id *entry = &xpcs_id_list[i];

		if ((xpcs_id & entry->mask) != entry->id)
			continue;

		xpcs->id = entry;
		compat = xpcs_find_compat(entry, interface);
		if (!compat) {
			XPCSERR("Incompatible MII interface: %d\n", interface);
			ret = -ENODEV;
			goto out;
		}

		xpcs->pcs.ops = &qcom_xpcs_phylink_ops;
		xpcs->pcs.poll = true;

		ret = xpcs_soft_reset(xpcs, compat);
		if (ret) {
			XPCSERR("Soft reset of XPCS block failed\n");
			goto out;
		}

		/* Select PCS mode before further configuration */
		ret = qcom_xpcs_select_mode(xpcs, interface);
		if (ret < 0)
			goto out;

		return xpcs;
	}

out:
	XPCSERR("XPCS creation failed\n");
	kfree(xpcs);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(qcom_xpcs_create);

/* Power off the DWC_xpcs controller */
void qcom_xpcs_destroy(struct dw_xpcs_qcom *xpcs)
{
	int i, ret;
	u32 xpcs_id;
	const struct xpcs_compat *compat;

	xpcs_id = xpcs_get_id(xpcs);
	if (xpcs_id == 0xffffffff) {
		XPCSERR("Invalid XPCS Device ID\n");
		ret = -ENODEV;
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(xpcs_id_list); i++) {
		const struct xpcs_id *entry = &xpcs_id_list[i];

		if ((xpcs_id & entry->mask) != entry->id)
			continue;

		xpcs->id = entry;
		compat = xpcs_find_compat(entry, g_interface);
		if (!compat) {
			XPCSERR("Incompatible MII interface: %d\n", g_interface);
			ret = -ENODEV;
			goto out;
		}

		ret = xpcs_soft_reset(xpcs, compat);
		if (ret) {
			XPCSERR("Soft reset of XPCS block failed\n");
			ret = -ENODEV;
		}
		goto done;
	}

	/* Enable xpc_spdown_o signal assertion on xpcs power down, then
	 * intiate the power down sequence
	 */
	ret = qcom_xpcs_read(xpcs, DW_SR_MII_VSMMD_CTRL);
	if (ret < 0)
		goto out;

	ret = qcom_xpcs_write(xpcs, DW_SR_MII_VSMMD_CTRL, ret & ~PD_CTRL);

	ret = qcom_xpcs_read(xpcs, DW_SR_MII_PCS_CTRL1);
	if (ret < 0)
		goto out;

	ret = qcom_xpcs_write(xpcs, DW_SR_MII_PCS_CTRL1, ret | LPM_EN);
	goto done;

out:
	XPCSERR("Could not power down the XPCS\n");
done:
	kfree(xpcs);
}
EXPORT_SYMBOL_GPL(qcom_xpcs_destroy);

MODULE_LICENSE("GPL v2");

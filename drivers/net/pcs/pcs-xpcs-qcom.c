// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Synopsys, Inc. and/or its affiliates.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

static bool sgmii_2p5g_on;

static const int xpcs_usxgmii_features[] = {
	ETHTOOL_LINK_MODE_Pause_BIT,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT,
	ETHTOOL_LINK_MODE_Autoneg_BIT,
	ETHTOOL_LINK_MODE_1000baseKX_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseKR_Full_BIT,
	ETHTOOL_LINK_MODE_2500baseX_Full_BIT,
	__ETHTOOL_LINK_MODE_MASK_NBITS,
};

static const int xpcs_10gkr_features[] = {
	ETHTOOL_LINK_MODE_Pause_BIT,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT,
	ETHTOOL_LINK_MODE_10000baseKR_Full_BIT,
	__ETHTOOL_LINK_MODE_MASK_NBITS,
};

static const int xpcs_xlgmii_features[] = {
	ETHTOOL_LINK_MODE_Pause_BIT,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT,
	ETHTOOL_LINK_MODE_25000baseCR_Full_BIT,
	ETHTOOL_LINK_MODE_25000baseKR_Full_BIT,
	ETHTOOL_LINK_MODE_25000baseSR_Full_BIT,
	ETHTOOL_LINK_MODE_40000baseKR4_Full_BIT,
	ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT,
	ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT,
	ETHTOOL_LINK_MODE_40000baseLR4_Full_BIT,
	ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT,
	ETHTOOL_LINK_MODE_50000baseKR2_Full_BIT,
	ETHTOOL_LINK_MODE_50000baseSR2_Full_BIT,
	ETHTOOL_LINK_MODE_50000baseKR_Full_BIT,
	ETHTOOL_LINK_MODE_50000baseSR_Full_BIT,
	ETHTOOL_LINK_MODE_50000baseCR_Full_BIT,
	ETHTOOL_LINK_MODE_50000baseLR_ER_FR_Full_BIT,
	ETHTOOL_LINK_MODE_50000baseDR_Full_BIT,
	ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT,
	ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT,
	ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT,
	ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT,
	ETHTOOL_LINK_MODE_100000baseKR2_Full_BIT,
	ETHTOOL_LINK_MODE_100000baseSR2_Full_BIT,
	ETHTOOL_LINK_MODE_100000baseCR2_Full_BIT,
	ETHTOOL_LINK_MODE_100000baseLR2_ER2_FR2_Full_BIT,
	ETHTOOL_LINK_MODE_100000baseDR2_Full_BIT,
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

static const phy_interface_t xpcs_10gkr_interfaces[] = {
	PHY_INTERFACE_MODE_10GKR,
};

static const phy_interface_t xpcs_xlgmii_interfaces[] = {
	PHY_INTERFACE_MODE_XLGMII,
};

static const phy_interface_t xpcs_sgmii_interfaces[] = {
	PHY_INTERFACE_MODE_SGMII,
};

static const phy_interface_t xpcs_2500basex_interfaces[] = {
	PHY_INTERFACE_MODE_2500BASEX,
	PHY_INTERFACE_MODE_MAX,
};

enum {
	DW_XPCS_USXGMII,
	DW_XPCS_10GKR,
	DW_XPCS_XLGMII,
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
		if (sgmii_2p5g_on) {
			XPCSINFO("Enabling 2.5Gbps-SGMII XPCS loopback\n");
			pmbl_ctrl = qcom_xpcs_read(xpcs, DW_VR_MII_PCS_DEBUG_CTRL);
			if (pmbl_ctrl < 0)
				goto out;

			pmbl_ctrl = qcom_xpcs_write(xpcs, DW_VR_MII_PCS_DEBUG_CTRL,
						    pmbl_ctrl | TX_PMBL_CTRL);
		}
		ret |= DW_R2TLBE;
	} else {
		if (sgmii_2p5g_on) {
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

/* Stub function for now until PM effort begins. This is to prevent any side effects
 * during regular data path testing.
 */
int qcom_xpcs_config_eee(struct dw_xpcs_qcom *xpcs, int mult_fact_100ns, int enable)
{
	return 0;
}
EXPORT_SYMBOL_GPL(qcom_xpcs_config_eee);

static int xpcs_usxgmii_set_speed(struct dw_xpcs_qcom *xpcs)
{
	int intr_stat;

	intr_stat = qcom_xpcs_read(xpcs, DW_VR_MII_AN_INTR_STS);
	if (intr_stat < 0)
		return intr_stat;

	if (intr_stat & DW_VR_MII_USXG_ANSGM_SP_LNKSTS) {
		int ret, speed;

		ret = qcom_xpcs_read(xpcs, DW_SR_MII_MMD_CTRL);
		if (ret < 0)
			return ret;

		if (!(intr_stat & DW_VR_MII_USXG_ANSGM_FD))
			XPCSERR("WARNING: Duplex mismatch! USXGMII supports full duplex only\n");

		ret &= ~DW_USXGMII_SS_MASK;
		speed = (intr_stat & DW_VR_MII_USXG_ANSGM_SP) >> DW_VR_MII_USXG_ANSGM_SP_SHIFT;

		switch (speed) {
		case DW_VR_MII_USXG_ANSGM_SP_10:
			return qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, ret);
		case DW_VR_MII_USXG_ANSGM_SP_100:
			return qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, ret | DW_GMII_100);
		case DW_VR_MII_USXG_ANSGM_SP_1000:
			return qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, ret | DW_GMII_1000);
		case DW_VR_MII_USXG_ANSGM_SP_2P5G:
			return qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, ret | DW_GMII_2500);
		case DW_VR_MII_USXG_ANSGM_SP_5G:
			return qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, ret | DW_USXGMII_5000);
		case DW_VR_MII_USXG_ANSGM_SP_10G:
			return qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, ret | DW_USXGMII_10000);
		default:
			return -EINVAL;
		}
	} else {
		XPCSERR("USXGMII Link is down, aborting\n");
		return -ENODEV;
	}
}

static int xpcs_config_aneg_c37_usxgmii(struct dw_xpcs_qcom *xpcs)
{
	int ret;

	/* Switch to XPCS to BASE-R mode */
	ret = qcom_xpcs_read(xpcs, DW_SR_MII_PCS_CTRL2);
	if (ret < 0)
		goto out;

	ret = qcom_xpcs_write(xpcs, DW_SR_MII_PCS_CTRL2, ret | BIT(1));

	/* Enable USXGMII 10G-SXGMII mode */
	ret = qcom_xpcs_read(xpcs, DW_VR_MII_PCS_DIG_CTRL1);
	if (ret < 0)
		goto out;

	ret = qcom_xpcs_write(xpcs, DW_VR_MII_PCS_DIG_CTRL1, ret | DW_USXGMII_EN);

	ret = qcom_xpcs_read(xpcs, DW_VR_MII_PCS_KR_CTRL);
	if (ret < 0)
		goto out;

	ret = qcom_xpcs_write(xpcs, DW_VR_MII_PCS_KR_CTRL, ret & ~USXG_MODE_SEL);

	ret = qcom_xpcs_read(xpcs, DW_VR_MII_PCS_DIG_CTRL1);
	if (ret < 0)
		goto out;

	ret = qcom_xpcs_write(xpcs, DW_VR_MII_PCS_DIG_CTRL1, ret | SOFT_RST);

	ret = qcom_xpcs_poll_reset(xpcs, DW_VR_MII_PCS_DIG_CTRL1, SW_RST_BIT_STATUS);
	if (ret < 0)
		goto out;

	ret = qcom_xpcs_read(xpcs, DW_VR_MII_AN_CTRL);
	if (ret < 0)
		goto out;

	ret &= ~(DW_VR_MII_PCS_MODE_MASK | DW_VR_MII_TX_CONFIG_MASK);
	ret |= (DW_VR_MII_TX_CONFIG_MAC_SIDE <<
		DW_VR_MII_AN_CTRL_TX_CONFIG_SHIFT &
		DW_VR_MII_TX_CONFIG_MASK);
	ret |= DW_FULL_DUPLEX | DW_VR_MII_AN_INTR_EN;
	ret = qcom_xpcs_write(xpcs, DW_VR_MII_AN_CTRL, ret);

	ret = qcom_xpcs_read(xpcs, DW_SR_MII_MMD_CTRL);
	if (ret < 0)
		goto out;

	ret = qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, ret | AN_CL37_EN);

	ret = xpcs_usxgmii_set_speed(xpcs);
	if (ret < 0) {
		XPCSERR("Failed to set USXGMII speed\n");
		return ret;
	}

	/* Reset USXGMII Rate Adaptor Logic */
	ret = qcom_xpcs_read(xpcs, DW_VR_MII_PCS_DIG_CTRL1);
	if (ret < 0)
		goto out;

	ret = qcom_xpcs_write(xpcs, DW_VR_MII_PCS_DIG_CTRL1, ret | DW_USXGMII_RST);
	ret = qcom_xpcs_poll_reset(xpcs, DW_VR_MII_PCS_DIG_CTRL1, USXG_RST_BIT_STATUS);
	if (ret < 0) {
		XPCSERR("Failed to reset USXGMII RAL\n");
		return ret;
	}

	return ret;
out:
	XPCSERR("Register read failed\n");
	return ret;
}

static int qcom_xpcs_usxgmii_read_intr_status(struct dw_xpcs_qcom *xpcs)
{
	int intr_stat;

	intr_stat = qcom_xpcs_read(xpcs, DW_VR_MII_AN_INTR_STS);
	if (intr_stat < 0)
		return intr_stat;

	if (intr_stat & DW_VR_MII_USXG_ANSGM_SP_LNKSTS) {
		int ret, speed;

		ret = qcom_xpcs_read(xpcs, DW_SR_MII_MMD_CTRL);
		if (ret < 0)
			return ret;

		ret &= ~DW_USXGMII_SS_MASK;
		speed = (intr_stat & DW_VR_MII_USXG_ANSGM_SP) >> DW_VR_MII_USXG_ANSGM_SP_SHIFT;

		switch (speed) {
		case DW_VR_MII_USXG_ANSGM_SP_10G:
			ret |= DW_USXGMII_10000;
			break;
		case DW_VR_MII_USXG_ANSGM_SP_5G:
			ret |= DW_USXGMII_5000;
			break;
		case DW_VR_MII_USXG_ANSGM_SP_2P5G:
			ret |= DW_GMII_2500;
			break;
		case DW_VR_MII_USXG_ANSGM_SP_1000:
			ret |= DW_GMII_1000;
			break;
		case DW_VR_MII_USXG_ANSGM_SP_100:
			ret |= DW_GMII_100;
			break;
		case DW_VR_MII_USXG_ANSGM_SP_10:
			ret |= DW_GMII_10;
			break;
		default:
			XPCSERR("Invalid speed mode: %d\n", speed);
			return -EINVAL;
		}

		return qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, ret);
	} else {
		XPCSERR("Link is down, aborting\n");
		return 0;
	}
}

static int qcom_xpcs_sgmii_read_intr_status(struct dw_xpcs_qcom *xpcs)
{
	int intr_stat;

	intr_stat = qcom_xpcs_read(xpcs, DW_VR_MII_AN_INTR_STS);
	if (intr_stat < 0)
		return intr_stat;

	if (intr_stat & DW_VR_MII_C37_ANSGM_SP_LNKSTS) {
		int ret, speed, mii_ctrl;

		mii_ctrl = qcom_xpcs_read(xpcs, DW_VR_MII_AN_CTRL);
		if (mii_ctrl < 0)
			return mii_ctrl;

		ret = qcom_xpcs_read(xpcs, DW_SR_MII_MMD_CTRL);
		if (ret < 0)
			return ret;

		if (!(intr_stat & DW_VR_MII_AN_STS_C37_ANSGM_FD))
			ret &= ~DW_FULL_DUPLEX;
		else
			ret |= DW_FULL_DUPLEX;

		ret &= ~DW_SGMII_SS_MASK;
		speed = (intr_stat & DW_VR_MII_AN_STS_C37_ANSGM_SP) >> DW_VR_MII_AN_STS_C37_ANSGM_SP_SHIFT;

		switch (speed) {
		case DW_VR_MII_C37_ANSGM_SP_1000:
			mii_ctrl |= DW_VR_MII_CTRL;
			ret |= BMCR_SPEED1000;
			XPCSINFO("Enabled 1Gbps-SGMII\n");
			break;
		case DW_VR_MII_C37_ANSGM_SP_100:
			mii_ctrl &= ~DW_VR_MII_CTRL;
			ret |= BMCR_SPEED100;
			XPCSINFO("Enabled 100Mbps-SGMII\n");
			break;
		case DW_VR_MII_C37_ANSGM_SP_10:
			mii_ctrl &= ~DW_VR_MII_CTRL;
			ret |= BMCR_SPEED10;
			XPCSINFO("Enabled 10Mbps-SGMII\n");
			break;
		default:
			XPCSERR("Invalid speed mode: %d\n", speed);
			return -EINVAL;
		}

		ret = qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, ret);
		return qcom_xpcs_write(xpcs, DW_VR_MII_AN_CTRL, mii_ctrl);
	} else {
		XPCSERR("Link is down, aborting\n");
		return 0;
	}
}

static void qcom_xpcs_handle_an_intr(struct dw_xpcs_qcom *xpcs)
{
	int ret = 0;
	const struct xpcs_compat *compat;

	compat = xpcs_find_compat(xpcs->id, PHY_INTERFACE_MODE_SGMII);
	if (compat) {
		ret = qcom_xpcs_sgmii_read_intr_status(xpcs);
		if (ret < 0)
			goto out;

		XPCSINFO("Finished Autonegotiation for SGMII\n");
		return;
	}

	compat = xpcs_find_compat(xpcs->id, PHY_INTERFACE_MODE_USXGMII);
	if (compat) {
		ret = qcom_xpcs_usxgmii_read_intr_status(xpcs);
		if (ret < 0)
			goto out;

		XPCSINFO("Finished Autonegotiation for USXGMII\n");
		return;
	}
out:
	XPCSERR("Failed to handle Autonegotiation interrupt\n");
}

int qcom_xpcs_check_aneg_ioc(struct dw_xpcs_qcom *xpcs)
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

	qcom_xpcs_handle_an_intr(xpcs);
	return 0;
}
EXPORT_SYMBOL_GPL(qcom_xpcs_check_aneg_ioc);

static int xpcs_config_aneg_c37_sgmii_2p5g(struct dw_xpcs_qcom *xpcs)
{
	int ret;

	ret = qcom_xpcs_read(xpcs, DW_VR_MII_AN_CTRL);
	if (ret < 0)
		return -EINVAL;

	ret |= DW_VR_MII_TX_CONFIG_MASK;
	ret &= ~DW_VR_MII_SGMII_LINK_STS;

	ret = qcom_xpcs_write(xpcs, DW_VR_MII_AN_CTRL, ret);

	ret = qcom_xpcs_read(xpcs, DW_SR_MII_MMD_CTRL);
	if (ret < 0)
		return -EINVAL;

	ret &= ~AN_CL37_EN;
	return qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, ret);
}

static int xpcs_config_aneg_c37_sgmii(struct dw_xpcs_qcom *xpcs)
{
	int ret;

	ret = qcom_xpcs_read(xpcs, DW_SR_MII_MMD_CTRL);
	if (ret < 0)
		return -EINVAL;

	ret = qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, ret | AN_CL37_EN);

	ret = qcom_xpcs_read(xpcs, DW_VR_MII_AN_CTRL);
	if (ret < 0)
		return -EINVAL;

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
		ret = xpcs_config_aneg_c37_usxgmii(xpcs);
		if (ret < 0)
			return ret;
		break;
	case DW_AN_C37_SGMII:
		ret = xpcs_config_aneg_c37_sgmii(xpcs);
		if (ret < 0)
			return ret;
		break;
	case DW_2500BASEX:
		/* Autoneg is disabled for 2.5Gbps SGMII mode */
		ret = xpcs_config_aneg_c37_sgmii_2p5g(xpcs);
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

static int qcom_xpcs_unset_2p5g_sgmii(struct dw_xpcs_qcom *xpcs)
{
	int ret;

	ret = qcom_xpcs_read(xpcs, DW_VR_MII_PCS_DIG_CTRL1);
	if (ret < 0)
		return -EINVAL;

	ret &= ~DW_VR_MII_DIG_CTRL1_2G5_EN;
	ret = qcom_xpcs_write(xpcs, DW_VR_MII_PCS_DIG_CTRL1, ret);

	XPCSINFO("Disabled 2.5Gbps-SGMII\n");
	sgmii_2p5g_on = false;

	return 0;
}

static int qcom_xpcs_set_2p5g_sgmii(struct dw_xpcs_qcom *xpcs)
{
	int ret;

	ret = qcom_xpcs_read(xpcs, DW_SR_MII_MMD_CTRL);
	if (ret < 0)
		goto out;

	ret &= ~DW_SGMII_SS_MASK;
	ret |= (DW_FULL_DUPLEX | DW_GMII_1000);
	ret = qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, ret);

	ret = qcom_xpcs_read(xpcs, DW_VR_MII_AN_CTRL);
	if (ret < 0)
		goto out;

	ret = qcom_xpcs_write(xpcs, DW_VR_MII_AN_CTRL, ret | DW_VR_MII_CTRL);

	ret = qcom_xpcs_read(xpcs, DW_VR_MII_PCS_DIG_CTRL1);
	if (ret < 0)
		goto out;

	sgmii_2p5g_on = true;

	XPCSINFO("Enabled 2.5Gbps-SGMII\n");
	return qcom_xpcs_write(xpcs, DW_VR_MII_PCS_DIG_CTRL1, ret | DW_VR_MII_DIG_CTRL1_2G5_EN);

out:
	XPCSERR("Register read failed\n");
	return ret;
}

void qcom_xpcs_link_up_usxgmii(struct dw_xpcs_qcom *xpcs, int speed)
{
	int ret, speed_sel;

	switch (speed) {
	case SPEED_10:
		speed_sel = DW_GMII_10;
		break;
	case SPEED_100:
		speed_sel = DW_GMII_100;
		break;
	case SPEED_1000:
		speed_sel = DW_GMII_1000;
		break;
	case SPEED_2500:
		speed_sel = DW_GMII_2500;
		break;
	case SPEED_5000:
		speed_sel = DW_USXGMII_5000;
		break;
	case SPEED_10000:
		speed_sel = DW_USXGMII_10000;
		break;
	default:
		return;
	}

	ret = qcom_xpcs_read(xpcs, DW_SR_MII_MMD_CTRL);
	if (ret < 0)
		goto out;

	ret &= ~DW_USXGMII_SS_MASK;
	ret = qcom_xpcs_write(xpcs, DW_SR_MII_MMD_CTRL, ret | speed_sel);

	/* Reset USXGMII RAL */
	ret = qcom_xpcs_read(xpcs, DW_VR_MII_PCS_DIG_CTRL1);
	if (ret < 0)
		goto out;

	ret = qcom_xpcs_write(xpcs, DW_VR_MII_PCS_DIG_CTRL1, ret | DW_USXGMII_RST);
	ret = qcom_xpcs_poll_reset(xpcs, DW_VR_MII_PCS_DIG_CTRL1, USXG_RST_BIT_STATUS);

	if (ret < 0)
		XPCSERR("Failed to reset USXGMII RAL\n");

	XPCSINFO("USXGMII link is up from PCS\n");
	return;
out:
	XPCSERR("Register read failed\n");
}

/* Return early if the phylink tries link-up after PCS Autoneg interrupt was
 * already serviced, unless we are switching to 2.5Gbps-SGMII.
 */
void qcom_xpcs_link_up(struct phylink_pcs *pcs, unsigned int mode,
		  phy_interface_t interface, int speed, int duplex)
{
	int ret = 0;
	struct dw_xpcs_qcom *xpcs = phylink_pcs_to_xpcs(pcs);

	if (interface == PHY_INTERFACE_MODE_SGMII && sgmii_2p5g_on &&
	    speed < SPEED_2500) {
		ret = qcom_xpcs_unset_2p5g_sgmii(xpcs);
		if (ret)
			XPCSERR("Failed to disable 2.5Gbps-SGMII mode\n");
	} else if (interface == PHY_INTERFACE_MODE_2500BASEX) {
		ret = qcom_xpcs_set_2p5g_sgmii(xpcs);
		if (ret)
			XPCSERR("Failed to enable 2.5Gbps-SGMII mode\n");
	} else if (interface == PHY_INTERFACE_MODE_USXGMII) {
		qcom_xpcs_link_up_usxgmii(xpcs, speed);
	}
}
EXPORT_SYMBOL_GPL(qcom_xpcs_link_up);

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
	[DW_XPCS_10GKR] = {
		.supported = xpcs_10gkr_features,
		.interface = xpcs_10gkr_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_10gkr_interfaces),
		.an_mode = DW_AN_C73,
	},
	[DW_XPCS_XLGMII] = {
		.supported = xpcs_xlgmii_features,
		.interface = xpcs_xlgmii_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_xlgmii_interfaces),
		.an_mode = DW_AN_C73,
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
		ret = qcom_xpcs_poll_reset(xpcs, DW_SR_MII_PCS_CTRL1, LPM_EN);
		if (ret < 0) {
			XPCSERR("Failed to get XPCS out of reset\n");
			return -EINVAL;
		}

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

		ret = qcom_xpcs_read(xpcs, DW_VR_MII_AN_CTRL);
		if (ret < 0)
			goto out;

		ret |= DW_VR_MII_AN_INTR_EN;
		ret = qcom_xpcs_write(xpcs, DW_VR_MII_AN_CTRL, ret);

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
	int ret;

	/* Enable xpcs_pdown_o signal assertion on xpcs power down, then
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

// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/of.h>
#include <linux/pcs-xpcs-qcom.h>
#include <linux/platform_device.h>
#include "stmmac.h"
#include "dwmac-qcom-ethqos.h"

irqreturn_t ethqos_xpcs_isr(int irq, void *dev_data)
{
	int ret;
	struct stmmac_priv *priv = (struct stmmac_priv *)dev_data;

	ret = qcom_xpcs_check_aneg_ioc(priv->hw->qxpcs, priv->plat->phy_interface);
	if (ret < 0)
		return IRQ_HANDLED;

	return IRQ_HANDLED;
}

int ethqos_xpcs_intr_enable(struct net_device *ndev)
{
	int ret = 0;
	struct stmmac_priv *priv = netdev_priv(ndev);

	ret = request_irq(priv->hw->qxpcs->pcs_intr, ethqos_xpcs_isr,
			  IRQF_SHARED, "qcom-xpcs-aneg", priv);
	if (ret) {
		ETHQOSERR("Unable to register XPCS IRQ %d\n",
			  priv->hw->qxpcs->pcs_intr);
		return ret;
	}

	return ret;
}

int ethqos_xpcs_intr_config(struct net_device *ndev)
{
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct qcom_ethqos *ethqos = priv->plat->bsp_priv;

	if (of_property_read_bool(ethqos->pdev->dev.of_node, "interrupt_names") ||
	    of_property_read_bool(ethqos->pdev->dev.of_node, "interrupt-names")) {
		priv->hw->qxpcs->pcs_intr = platform_get_irq_byname(ethqos->pdev, "pcs_intr");

		if (priv->hw->qxpcs->pcs_intr < 0) {
			if (priv->hw->qxpcs->pcs_intr != -EPROBE_DEFER)
				ETHQOSINFO("XPCS IRQ configuration information not found\n");
			return -EINVAL;
		}

		return qcom_xpcs_intr_enable(priv->hw->qxpcs);
	}

	return -EINVAL;
}

int ethqos_xpcs_init(struct net_device *ndev)
{
	int ret = 0;
	struct dw_xpcs_qcom *qxpcs;
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct qcom_ethqos *ethqos = priv->plat->bsp_priv;
	void __iomem *xpcs_base;

#ifdef CONFIG_MSM_BOOT_TIME_MARKER
	update_marker("M - Ethernet xpcs init start");
#endif

	xpcs_base = devm_platform_ioremap_resource_byname(ethqos->pdev, "xpcs");
	if (IS_ERR_OR_NULL(xpcs_base)) {
		ETHQOSERR("XPCS not supported from device tree!\n");
		return -ENODEV;
	}

	qxpcs = qcom_xpcs_create(xpcs_base, priv->plat->phy_interface);
	if (!qxpcs || IS_ERR(qxpcs)) {
		ETHQOSERR("XPCS failed to be created: %d\n", PTR_ERR(qxpcs));
		return -ENODEV;
	}

	priv->hw->qxpcs = qxpcs;
	priv->hw->xpcs = NULL;

	if (priv->plat->fixed_phy_mode && priv->hw->qxpcs)
		priv->hw->qxpcs->fixed_phy_mode = true;

	ret = ethqos_xpcs_intr_config(ndev);
	if (!ret) {
		ret = ethqos_xpcs_intr_enable(ndev);
		if (ret) {
			ETHQOSINFO("Failed to enable XPCS interrupt, using non-interrupt mode\n");
			priv->hw->qxpcs->intr_en = false;
			goto out;
		}
		priv->hw->qxpcs->intr_en = true;
	} else {
		ETHQOSINFO("No DTSI entry found for XPCS interrupt, using non-interrupt mode\n");
		priv->hw->qxpcs->intr_en = false;
	}

out:
	if (!priv->plat->mac2mac_en)
		phylink_set_pcs(priv->phylink, &priv->hw->qxpcs->pcs);

#ifdef CONFIG_MSM_BOOT_TIME_MARKER
	update_marker("M - Ethernet xpcs init end");
#endif

	ETHQOSINFO("Successfully initialized XPCS\n");
	return 0;
}

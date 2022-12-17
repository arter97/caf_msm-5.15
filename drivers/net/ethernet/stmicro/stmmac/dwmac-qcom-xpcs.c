// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/pcs-xpcs-qcom.h>
#include <linux/platform_device.h>
#include "stmmac.h"
#include "dwmac-qcom-ethqos.h"

extern struct qcom_ethqos *pethqos;

irqreturn_t ethqos_xpcs_isr(int irq, void *dev_data)
{
	int ret;
	struct stmmac_priv *priv = qcom_ethqos_get_priv(pethqos);

	ret = qcom_xpcs_check_aneg_ioc(priv->hw->qxpcs);
	if (ret < 0)
		return IRQ_HANDLED;

	return IRQ_HANDLED;
}

int ethqos_xpcs_intr_enable(void)
{
	int ret = 0;
	struct stmmac_priv *priv = qcom_ethqos_get_priv(pethqos);

	ret = request_irq(priv->hw->qxpcs->pcs_intr, ethqos_xpcs_isr,
			  IRQF_SHARED, "qcom-xpcs-aneg", pethqos);
	if (ret) {
		ETHQOSERR("Unable to register PCS IRQ %d\n",
			  priv->hw->qxpcs->pcs_intr);
		return ret;
	}

	return ret;
}

int ethqos_xpcs_intr_config(phy_interface_t interface)
{
	int ret = 0;
	struct stmmac_priv *priv = qcom_ethqos_get_priv(pethqos);

	priv->hw->qxpcs->pcs_intr = platform_get_irq_byname(pethqos->pdev, "pcs_intr");

	if (priv->hw->qxpcs->pcs_intr < 0 ||
	    (interface != PHY_INTERFACE_MODE_SGMII &&
	    interface != PHY_INTERFACE_MODE_USXGMII)) {
		if (priv->hw->qxpcs->pcs_intr != -EPROBE_DEFER) {
			dev_err(&pethqos->pdev->dev,
				"PCS IRQ configuration information not found\n");
		}
		ret = 1;
	}

	return ret;
}

int ethqos_xpcs_init(phy_interface_t mode)
{
	int ret = 0;
	struct dw_xpcs_qcom *qxpcs;
	struct stmmac_priv *priv = qcom_ethqos_get_priv(pethqos);
	void __iomem *xpcs_base;

	xpcs_base = devm_platform_ioremap_resource_byname(pethqos->pdev, "xpcs");
	if (IS_ERR_OR_NULL(xpcs_base)) {
		ETHQOSINFO("XPCS not supported from device tree!\n");
		return ret;
	}

	qxpcs = qcom_xpcs_create(xpcs_base, mode);
	if (IS_ERR(qxpcs)) {
		ETHQOSERR("XPCS failed to be created: %d\n", PTR_ERR(qxpcs));
		return -ENODEV;
	}

	priv->hw->qxpcs = qxpcs;
	priv->hw->xpcs = NULL;

	ret = ethqos_xpcs_intr_config(mode);
	if (!ret) {
		ret = ethqos_xpcs_intr_enable();
		if (ret) {
			ETHQOSERR("Failed to enable XPCS interrupt\n");
			return -ENODEV;
		}
	} else {
		ETHQOSERR("Failed to configure XPCS interrupt\n");
		return -ENODEV;
	}

	phylink_set_pcs(priv->phylink, &priv->hw->qxpcs->pcs);
	ETHQOSINFO("Successfully initialized XPCS\n");

	return ret;
}

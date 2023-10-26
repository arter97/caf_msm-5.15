// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/phy.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>

#include "stmmac.h"
#include "dwmac-qcom-ethqos.h"

#define EMAC_GDSC_EMAC_NAME "gdsc_emac"
#define EMAC_VREG_RGMII_NAME "vreg_rgmii"
#define EMAC_VREG_EMAC_PHY_NAME "vreg_emac_phy"
#define EMAC_VREG_RGMII_IO_PADS_NAME "vreg_rgmii_io_pads"
#define EMAC_VREG_A_SGMII_1P2_NAME "vreg_a_sgmii_1p2"
#define EMAC_VREG_A_SGMII_0P9_NAME "vreg_a_sgmii_0p9"

#if IS_ENABLED(CONFIG_ETHQOS_QCOM_VER4)
static u32 A_SGMII_1P2_MAX_VOLT = 1200000;
static u32 A_SGMII_1P2_MIN_VOLT = 1200000;
static u32 A_SGMII_0P9_MAX_VOLT = 912000;
static u32 A_SGMII_0P9_MIN_VOLT = 880000;
static u32 A_SGMII_1P2_LOAD_CURR = 25000;
static u32 A_SGMII_0P9_LOAD_CURR = 132000;
#else
static u32 A_SGMII_1P2_MAX_VOLT = 1300000;
static u32 A_SGMII_1P2_MIN_VOLT = 1200000;
static u32 A_SGMII_0P9_MAX_VOLT = 950000;
static u32 A_SGMII_0P9_MIN_VOLT = 880000;
static u32 A_SGMII_1P2_LOAD_CURR = 15000;
static u32 A_SGMII_0P9_LOAD_CURR = 458000;
#endif

int ethqos_enable_serdes_consumers(struct qcom_ethqos *ethqos)
{
	int ret = 0;

#ifdef CONFIG_MSM_BOOT_TIME_MARKER
	update_marker("M - Ethernet enable serdes consumers start");
#endif

	ret = regulator_set_voltage(ethqos->vreg_a_sgmii_1p2, A_SGMII_1P2_MIN_VOLT,
				    A_SGMII_1P2_MAX_VOLT);
	if (ret) {
		ETHQOSERR("Failed to set voltage for %s\n", EMAC_VREG_A_SGMII_1P2_NAME);
		return ret;
	}

	ret = regulator_set_load(ethqos->vreg_a_sgmii_1p2, A_SGMII_1P2_LOAD_CURR);
	if (ret) {
		ETHQOSERR("Failed to set load for %s\n", EMAC_VREG_A_SGMII_1P2_NAME);
		return ret;
	}

	ret = regulator_enable(ethqos->vreg_a_sgmii_1p2);
	if (ret) {
		ETHQOSERR("Cannot enable <%s>\n", EMAC_VREG_A_SGMII_1P2_NAME);
		return ret;
	}

	ETHQOSDBG("Enabled <%s>\n", EMAC_VREG_A_SGMII_1P2_NAME);

	ret = regulator_set_voltage(ethqos->vreg_a_sgmii_0p9, A_SGMII_0P9_MIN_VOLT,
				    A_SGMII_0P9_MAX_VOLT);
	if (ret) {
		ETHQOSERR("Failed to set voltage for %s\n", EMAC_VREG_A_SGMII_0P9_NAME);
		return ret;
	}

	ret = regulator_set_load(ethqos->vreg_a_sgmii_0p9, A_SGMII_0P9_LOAD_CURR);
	if (ret) {
		ETHQOSERR("Failed to set load for %s\n", EMAC_VREG_A_SGMII_0P9_NAME);
		return ret;
	}

	ret = regulator_enable(ethqos->vreg_a_sgmii_0p9);
	if (ret) {
		ETHQOSERR("Cannot enable <%s>\n", EMAC_VREG_A_SGMII_0P9_NAME);
		return ret;
	}

	ETHQOSDBG("Enabled <%s>\n", EMAC_VREG_A_SGMII_0P9_NAME);

#ifdef CONFIG_MSM_BOOT_TIME_MARKER
	update_marker("M - Ethernet enable serdes consumers end");
#endif

	return ret;
}
EXPORT_SYMBOL(ethqos_enable_serdes_consumers);

int ethqos_disable_serdes_consumers(struct qcom_ethqos *ethqos)
{
	int ret = 0;

#ifdef CONFIG_MSM_BOOT_TIME_MARKER
	update_marker("M - Ethernet disable serdes consumers start");
#endif

	ret = regulator_set_voltage(ethqos->vreg_a_sgmii_0p9, 0, INT_MAX);
	if (ret < 0) {
		ETHQOSERR("Failed to remove %s voltage request: %d\n", EMAC_VREG_A_SGMII_0P9_NAME,
			  ret);
		return ret;
	}

	ret = regulator_set_load(ethqos->vreg_a_sgmii_0p9, 0);
	if (ret) {
		ETHQOSERR("Failed to set load for %s\n", EMAC_VREG_A_SGMII_0P9_NAME);
		return ret;
	}

	regulator_disable(ethqos->vreg_a_sgmii_0p9);

	ret = regulator_set_voltage(ethqos->vreg_a_sgmii_1p2, 0, INT_MAX);
	if (ret < 0) {
		ETHQOSERR("Failed to remove %s voltage request: %d\n", EMAC_VREG_A_SGMII_1P2_NAME,
			  ret);
		return ret;
	}

	ret = regulator_set_load(ethqos->vreg_a_sgmii_1p2, 0);
	if (ret) {
		ETHQOSERR("Failed to set load for %s\n", EMAC_VREG_A_SGMII_1P2_NAME);
		return ret;
	}

	regulator_disable(ethqos->vreg_a_sgmii_1p2);

#ifdef CONFIG_MSM_BOOT_TIME_MARKER
	update_marker("M - Ethernet disable serdes consumers end");
#endif

	return ret;
}
EXPORT_SYMBOL(ethqos_disable_serdes_consumers);

static int setup_gpio_input_common
	(struct device *dev, const char *name, int *gpio)
{
	int ret = 0;

	if (of_find_property(dev->of_node, name, NULL)) {
		*gpio = ret = of_get_named_gpio(dev->of_node, name, 0);
		if (ret >= 0) {
			ret = gpio_request(*gpio, name);
			if (ret) {
				ETHQOSERR("Can't get GPIO %s, ret = %d\n",
					  name, *gpio);
				*gpio = -1;
				return ret;
			}

			ret = gpio_direction_input(*gpio);
			if (ret) {
				ETHQOSERR("failed GPIO %s direction ret=%d\n",
					  name, ret);
				return ret;
			}
		} else {
			if (ret == -EPROBE_DEFER)
				ETHQOSERR("get EMAC_GPIO probe defer\n");
			else
				ETHQOSERR("can't get gpio %s ret %d\n", name,
					  ret);
			return ret;
		}
	} else {
		ETHQOSERR("can't find gpio %s\n", name);
		ret = -EINVAL;
	}

	return ret;
}

int ethqos_init_regulators(struct qcom_ethqos *ethqos)
{
	int ret = 0;

	if (of_property_read_bool(ethqos->pdev->dev.of_node,
				  "gdsc_emac-supply")) {
		ethqos->gdsc_emac =
		devm_regulator_get(&ethqos->pdev->dev, EMAC_GDSC_EMAC_NAME);
		if (IS_ERR(ethqos->gdsc_emac)) {
			ETHQOSERR("Can not get <%s>\n", EMAC_GDSC_EMAC_NAME);
			return PTR_ERR(ethqos->gdsc_emac);
		}

		ret = regulator_enable(ethqos->gdsc_emac);
		if (ret) {
			ETHQOSERR("Can not enable <%s>\n", EMAC_GDSC_EMAC_NAME);
			goto reg_error;
		}

		ETHQOSDBG("Enabled <%s>\n", EMAC_GDSC_EMAC_NAME);
	}

	if (of_property_read_bool(ethqos->pdev->dev.of_node,
				  "vreg_rgmii-supply")) {
		ethqos->reg_rgmii =
		devm_regulator_get(&ethqos->pdev->dev, EMAC_VREG_RGMII_NAME);
		if (IS_ERR(ethqos->reg_rgmii)) {
			ETHQOSERR("Can not get <%s>\n", EMAC_VREG_RGMII_NAME);
			return PTR_ERR(ethqos->reg_rgmii);
		}

		ret = regulator_enable(ethqos->reg_rgmii);
		if (ret) {
			ETHQOSERR("Can not enable <%s>\n",
				  EMAC_VREG_RGMII_NAME);
			goto reg_error;
		}

		ETHQOSDBG("Enabled <%s>\n", EMAC_VREG_RGMII_NAME);
	}

	if (of_property_read_bool(ethqos->pdev->dev.of_node,
				  "vreg_emac_phy-supply")) {
		ethqos->reg_emac_phy =
		devm_regulator_get(&ethqos->pdev->dev, EMAC_VREG_EMAC_PHY_NAME);
		if (IS_ERR(ethqos->reg_emac_phy)) {
			ETHQOSERR("Can not get <%s>\n",
				  EMAC_VREG_EMAC_PHY_NAME);
			return PTR_ERR(ethqos->reg_emac_phy);
		}

		ret = regulator_enable(ethqos->reg_emac_phy);
		if (ret) {
			ETHQOSERR("Can not enable <%s>\n",
				  EMAC_VREG_EMAC_PHY_NAME);
			goto reg_error;
		}

		ETHQOSDBG("Enabled <%s>\n", EMAC_VREG_EMAC_PHY_NAME);
	}

	if (of_property_read_bool(ethqos->pdev->dev.of_node,
				  "vreg_rgmii_io_pads-supply")) {
		ethqos->reg_rgmii_io_pads = devm_regulator_get
		(&ethqos->pdev->dev, EMAC_VREG_RGMII_IO_PADS_NAME);
		if (IS_ERR(ethqos->reg_rgmii_io_pads)) {
			ETHQOSERR("Can not get <%s>\n",
				  EMAC_VREG_RGMII_IO_PADS_NAME);
			return PTR_ERR(ethqos->reg_rgmii_io_pads);
		}

		ret = regulator_enable(ethqos->reg_rgmii_io_pads);
		if (ret) {
			ETHQOSERR("Can not enable <%s>\n",
				  EMAC_VREG_RGMII_IO_PADS_NAME);
			goto reg_error;
		}

		ETHQOSDBG("Enabled <%s>\n", EMAC_VREG_RGMII_IO_PADS_NAME);
	}

	return ret;

reg_error:
	ETHQOSERR("%s failed\n", __func__);
	ethqos_disable_regulators(ethqos);
	return ret;
}
EXPORT_SYMBOL(ethqos_init_regulators);

int ethqos_init_sgmii_regulators(struct qcom_ethqos *ethqos)
{
	/* Both power supplies are required to be present together for SGMII/USXGMII mode */
	if (of_property_read_bool(ethqos->pdev->dev.of_node, "vreg_a_sgmii_1p2-supply") &&
	    of_property_read_bool(ethqos->pdev->dev.of_node, "vreg_a_sgmii_0p9-supply")) {
		ethqos->vreg_a_sgmii_1p2 = devm_regulator_get(&ethqos->pdev->dev,
							      EMAC_VREG_A_SGMII_1P2_NAME);
		if (IS_ERR(ethqos->vreg_a_sgmii_1p2)) {
			ETHQOSERR("Can not get <%s>\n", EMAC_VREG_A_SGMII_1P2_NAME);
			return PTR_ERR(ethqos->vreg_a_sgmii_1p2);
		}

		ethqos->vreg_a_sgmii_0p9 = devm_regulator_get(&ethqos->pdev->dev,
							      EMAC_VREG_A_SGMII_0P9_NAME);
		if (IS_ERR(ethqos->vreg_a_sgmii_0p9)) {
			ETHQOSERR("Can not get <%s>\n", EMAC_VREG_A_SGMII_0P9_NAME);
			return PTR_ERR(ethqos->vreg_a_sgmii_0p9);
		}

		return ethqos_enable_serdes_consumers(ethqos);
	}

	return -ENODEV;
}
EXPORT_SYMBOL(ethqos_init_sgmii_regulators);

void ethqos_disable_regulators(struct qcom_ethqos *ethqos)
{
	int ret = 0;

	if (ethqos->reg_rgmii) {
		regulator_disable(ethqos->reg_rgmii);
		devm_regulator_put(ethqos->reg_rgmii);
		ethqos->reg_rgmii = NULL;
	}

	if (ethqos->reg_emac_phy) {
		regulator_disable(ethqos->reg_emac_phy);
		devm_regulator_put(ethqos->reg_emac_phy);
		ethqos->reg_emac_phy = NULL;
	}

	if (ethqos->reg_rgmii_io_pads) {
		regulator_disable(ethqos->reg_rgmii_io_pads);
		devm_regulator_put(ethqos->reg_rgmii_io_pads);
		ethqos->reg_rgmii_io_pads = NULL;
	}

	if (ethqos->gdsc_emac) {
		regulator_disable(ethqos->gdsc_emac);
		devm_regulator_put(ethqos->gdsc_emac);
		ethqos->gdsc_emac = NULL;
	}

	if (ethqos->vreg_a_sgmii_1p2 && ethqos->vreg_a_sgmii_0p9) {
		if (ethqos->power_state) {
			ret = ethqos_disable_serdes_consumers(ethqos);
			if (ret < 0)
				ETHQOSERR("Failed to disable SerDes consumers\n");
		}
		devm_regulator_put(ethqos->vreg_a_sgmii_1p2);
		ethqos->vreg_a_sgmii_1p2 = NULL;
		devm_regulator_put(ethqos->vreg_a_sgmii_0p9);
		ethqos->vreg_a_sgmii_0p9 = NULL;
	}
}
EXPORT_SYMBOL(ethqos_disable_regulators);

void ethqos_trigger_phylink(struct qcom_ethqos *ethqos, bool status)
{
	struct stmmac_priv *priv = qcom_ethqos_get_priv(ethqos);
	struct phy_device *phydev = NULL;
	struct device_node *node;
	int ret = 0;

	if(!priv->phydev){
		ETHQOSERR("phydev is NULL\n");
		return;
	}

	if (priv->phydev && !priv->phydev->autoneg)
		linkmode_copy(priv->adv_old, priv->phydev->advertising);

	if (!status) {
		if (priv->phylink_disconnected)
			return;

		if (priv->phy_irq_enabled)
			priv->plat->phy_irq_disable(priv);

		rtnl_lock();
		phylink_stop(priv->phylink);
		phylink_disconnect_phy(priv->phylink);
		rtnl_unlock();

		priv->phylink_disconnected = true;
	} else {
		if (!priv->phylink_disconnected)
			return;

		/* reset the phy so that it's ready */
		if (priv->mii) {
			ETHQOSERR("do mdio reset\n");
			priv->mii->reset(priv->mii);
		}

		phydev = priv->phydev;
		node = priv->plat->phylink_node;

		if (priv->phydev && !priv->plat->fixed_phy_mode &&
		    priv->plat->early_eth)
			stmmac_set_speed100(priv);

		if (node)
			ret = phylink_of_phy_connect(priv->phylink, node, 0);

		rtnl_lock();
		phylink_connect_phy(priv->phylink, priv->phydev);
		rtnl_unlock();

			/*Enable phy interrupt*/
		if (priv->plat->phy_intr_en_extn_stm && phydev) {
			ETHQOSDBG("PHY interrupt Mode enabled\n");
			phydev->irq = PHY_MAC_INTERRUPT;
			phydev->interrupts =  PHY_INTERRUPT_ENABLED;

			if (phydev && phydev->drv && phydev->drv->config_intr &&
			    !phydev->drv->config_intr(phydev)) {
				ETHQOSERR("config_phy_intr successful after phy on\n");
			}
		} else if (!priv->plat->phy_intr_en_extn_stm && phydev) {
			phydev->irq = PHY_POLL;
			ETHQOSDBG("PHY Polling Mode enabled\n");
		} else {
			ETHQOSERR("phydev is null , intr value=%d\n",
				  priv->plat->phy_intr_en_extn_stm);
		}

		if (!priv->phy_irq_enabled)
			priv->plat->phy_irq_enable(priv);

		/*Give some time for the phy to config interrupt*/
		usleep_range(10000, 20000);

		rtnl_lock();
		phylink_start(priv->phylink);
		phylink_speed_up(priv->phylink);
		rtnl_unlock();

		priv->phylink_disconnected = false;
	}
}

int ethqos_phy_power_on(struct qcom_ethqos *ethqos)
{
	int ret = 0;

	if (ethqos->reg_emac_phy) {
		ret = regulator_enable(ethqos->reg_emac_phy);
		if (ret) {
			ETHQOSERR("Can not enable <%s>\n",
				  EMAC_VREG_EMAC_PHY_NAME);
			return ret;
		}
		ethqos->phy_state = PHY_IS_ON;
	} else {
		ETHQOSERR("reg_emac_phy is NULL\n");
	}
	return ret;
}

void  ethqos_phy_power_off(struct qcom_ethqos *ethqos)
{
	if (ethqos->reg_emac_phy) {
		if (regulator_is_enabled(ethqos->reg_emac_phy)) {
			regulator_disable(ethqos->reg_emac_phy);
			ethqos->phy_state = PHY_IS_OFF;
		}
	} else {
		ETHQOSERR("reg_emac_phy is NULL\n");
	}
}

void ethqos_free_gpios(struct qcom_ethqos *ethqos)
{
	if (gpio_is_valid(ethqos->gpio_phy_intr_redirect))
		gpio_free(ethqos->gpio_phy_intr_redirect);
	ethqos->gpio_phy_intr_redirect = -1;
}
EXPORT_SYMBOL(ethqos_free_gpios);

int ethqos_init_pinctrl(struct device *dev)
{
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_state;
	int i = 0;
	int num_names;
	const char *name;
	int ret = 0;

	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		ETHQOSERR("Failed to get pinctrl, err = %d\n", ret);
		return ret;
	}

	num_names = of_property_count_strings(dev->of_node, "pinctrl-names");
	if (num_names < 0) {
		dev_err(dev, "Cannot parse pinctrl-names: %d\n",
			num_names);
		return num_names;
	}

	for (i = 0; i < num_names; i++) {
		ret = of_property_read_string_index(dev->of_node,
						    "pinctrl-names",
						    i, &name);

		if (!strcmp(name, PINCTRL_STATE_DEFAULT))
			continue;

		pinctrl_state = pinctrl_lookup_state(pinctrl, name);
		if (IS_ERR_OR_NULL(pinctrl_state)) {
			ret = PTR_ERR(pinctrl_state);
			ETHQOSERR("lookup_state %s failed %d\n", name, ret);
			return ret;
		}

		ETHQOSDBG("pinctrl_lookup_state %s succeeded\n", name);

		ret = pinctrl_select_state(pinctrl, pinctrl_state);
		if (ret) {
			ETHQOSERR("select_state %s failed %d\n", name, ret);
			return ret;
		}

		ETHQOSDBG("pinctrl_select_state %s succeeded\n", name);
	}

	return ret;
}
EXPORT_SYMBOL(ethqos_init_pinctrl);

int ethqos_init_gpio(struct qcom_ethqos *ethqos)
{
	int ret = 0;

	ethqos->gpio_phy_intr_redirect = -1;

	ret = ethqos_init_pinctrl(&ethqos->pdev->dev);
	if (ret) {
		ETHQOSERR("ethqos_init_pinctrl failed");
		return ret;
	}

	if (of_property_read_bool(ethqos->pdev->dev.of_node, "qcom,phy-intr-redirect")) {
		ret = setup_gpio_input_common(&ethqos->pdev->dev,
					      "qcom,phy-intr-redirect",
				&ethqos->gpio_phy_intr_redirect);

		if (ret) {
			ETHQOSERR("Failed to setup <%s> gpio\n",
				  "qcom,phy-intr-redirect");
			goto gpio_error;
		}
	}

	return ret;

gpio_error:
	ethqos_free_gpios(ethqos);
	return ret;
}
EXPORT_SYMBOL(ethqos_init_gpio);

MODULE_LICENSE("GPL v2");

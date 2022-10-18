// SPDX-License-Identifier: GPL-2.0-only

// Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>

#include <linux/phy.h>
#include <linux/io.h>

#include <linux/iopoll.h>
#include <linux/mii.h>
#include <linux/of_mdio.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/debugfs.h>
#include <linux/dma-iommu.h>
#include <linux/iommu.h>
#include <linux/micrel_phy.h>
#include <linux/tcp.h>
#include <linux/ip.h>

#include <linux/rtnetlink.h>
#include <asm-generic/io.h>
#include <linux/kthread.h>
#include <linux/io-64-nonatomic-hi-lo.h>

#include "dwmac-qcom-serdes.h"

int configure_serdes_dt(struct qcom_ethqos *ethqos)
{
	struct resource *res = NULL;
	struct platform_device *pdev = ethqos->pdev;
	int ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "serdes");
	ethqos->sgmii_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ethqos->sgmii_base)) {
		dev_err(&pdev->dev, "Can't get sgmii base\n");
		ret = PTR_ERR(ethqos->sgmii_base);
		goto err_mem;
	}

	ethqos->sgmiref_clk = devm_clk_get(&pdev->dev, "sgmi_ref");
	if (IS_ERR(ethqos->sgmiref_clk)) {
		ret = PTR_ERR(ethqos->sgmiref_clk);
		goto err_mem;
	}
	ethqos->phyaux_clk = devm_clk_get(&pdev->dev, "phyaux");
	if (IS_ERR(ethqos->phyaux_clk)) {
		ret = PTR_ERR(ethqos->phyaux_clk);
		goto err_mem;
	}

	ret = clk_prepare_enable(ethqos->sgmiref_clk);
	if (ret)
		goto err_mem;

	ret = clk_prepare_enable(ethqos->phyaux_clk);
	if (ret)
		 goto err_mem;

err_mem:
	return -1;

}

void qcom_ethqos_serdes_init(struct qcom_ethqos *ethqos, int speed)
{
	int retry = 500;
	unsigned int val;

	/****************MODULE: SGMII_PHY_SGMII_PCS**********************************/
	writel_relaxed(0x01, ethqos->sgmii_base + QSERDES_PCS_SW_RESET);
	writel_relaxed(0x01, ethqos->sgmii_base + QSERDES_PCS_POWER_DOWN_CONTROL);

	/***************** MODULE: QSERDES_COM_SGMII_QMP_PLL*********/
	writel_relaxed(0x0F, ethqos->sgmii_base + QSERDES_COM_PLL_IVCO);
	writel_relaxed(0x06, ethqos->sgmii_base + QSERDES_COM_CP_CTRL_MODE0);
	writel_relaxed(0x16, ethqos->sgmii_base + QSERDES_COM_PLL_RCTRL_MODE0);
	writel_relaxed(0x36, ethqos->sgmii_base + QSERDES_COM_PLL_CCTRL_MODE0);
	writel_relaxed(0x1A, ethqos->sgmii_base + QSERDES_COM_SYSCLK_EN_SEL);
	writel_relaxed(0x0A, ethqos->sgmii_base + QSERDES_COM_LOCK_CMP1_MODE0);
	writel_relaxed(0x1A, ethqos->sgmii_base + QSERDES_COM_LOCK_CMP2_MODE0);
	writel_relaxed(0x82, ethqos->sgmii_base + QSERDES_COM_DEC_START_MODE0);
	writel_relaxed(0x55, ethqos->sgmii_base + QSERDES_COM_DIV_FRAC_START1_MODE0);
	writel_relaxed(0x55, ethqos->sgmii_base + QSERDES_COM_DIV_FRAC_START2_MODE0);
	writel_relaxed(0x03, ethqos->sgmii_base + QSERDES_COM_DIV_FRAC_START3_MODE0);
	writel_relaxed(0x24, ethqos->sgmii_base + QSERDES_COM_VCO_TUNE1_MODE0);

	writel_relaxed(0x02, ethqos->sgmii_base + QSERDES_COM_VCO_TUNE2_MODE0);
	writel_relaxed(0x00, ethqos->sgmii_base + QSERDES_COM_VCO_TUNE_INITVAL2);
	writel_relaxed(0x04, ethqos->sgmii_base + QSERDES_COM_HSCLK_SEL);
	writel_relaxed(0x00, ethqos->sgmii_base + QSERDES_COM_HSCLK_HS_SWITCH_SEL);
	writel_relaxed(0x0A, ethqos->sgmii_base + QSERDES_COM_CORECLK_DIV_MODE0);
	writel_relaxed(0x00, ethqos->sgmii_base + QSERDES_COM_CORE_CLK_EN);
	writel_relaxed(0xB9, ethqos->sgmii_base + QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE0);
	writel_relaxed(0x1E, ethqos->sgmii_base + QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE0);
	writel_relaxed(0x11, ethqos->sgmii_base + QSERDES_COM_BIN_VCOCAL_HSCLK_SEL);

	/******************MODULE: QSERDES_TX0_SGMII_QMP_TX***********************/
	writel_relaxed(0x05, ethqos->sgmii_base + QSERDES_TX_TX_BAND);
	writel_relaxed(0x0A, ethqos->sgmii_base + QSERDES_TX_SLEW_CNTL);
	writel_relaxed(0x09, ethqos->sgmii_base + QSERDES_TX_RES_CODE_LANE_OFFSET_TX);
	writel_relaxed(0x09, ethqos->sgmii_base + QSERDES_TX_RES_CODE_LANE_OFFSET_RX);
	writel_relaxed(0x05, ethqos->sgmii_base + QSERDES_TX_LANE_MODE_1);
	writel_relaxed(0x00, ethqos->sgmii_base + QSERDES_TX_LANE_MODE_3);
	writel_relaxed(0x12, ethqos->sgmii_base + QSERDES_TX_RCV_DETECT_LVL_2);
	writel_relaxed(0x0C, ethqos->sgmii_base + QSERDES_TX_TRAN_DRVR_EMP_EN);

	/*****************MODULE: QSERDES_RX0_SGMII_QMP_RX*******************/
	writel_relaxed(0x0A, ethqos->sgmii_base + QSERDES_RX_UCDR_FO_GAIN);
	writel_relaxed(0x06, ethqos->sgmii_base + QSERDES_RX_UCDR_SO_GAIN);
	writel_relaxed(0x0A, ethqos->sgmii_base + QSERDES_RX_UCDR_FASTLOCK_FO_GAIN);
	writel_relaxed(0x7F, ethqos->sgmii_base + QSERDES_RX_UCDR_SO_SATURATION_AND_ENABLE);
	writel_relaxed(0x00, ethqos->sgmii_base + QSERDES_RX_UCDR_FASTLOCK_COUNT_LOW);
	writel_relaxed(0x01, ethqos->sgmii_base + QSERDES_RX_UCDR_FASTLOCK_COUNT_HIGH);
	writel_relaxed(0x81, ethqos->sgmii_base + QSERDES_RX_UCDR_PI_CONTROLS);
	writel_relaxed(0x80, ethqos->sgmii_base + QSERDES_RX_UCDR_PI_CTRL2);
	writel_relaxed(0x04, ethqos->sgmii_base + QSERDES_RX_RX_TERM_BW);
	writel_relaxed(0x08, ethqos->sgmii_base + QSERDES_RX_VGA_CAL_CNTRL2);
	writel_relaxed(0x0F, ethqos->sgmii_base + QSERDES_RX_GM_CAL);
	writel_relaxed(0x04, ethqos->sgmii_base + QSERDES_RX_RX_EQU_ADAPTOR_CNTRL1);
	writel_relaxed(0x00, ethqos->sgmii_base + QSERDES_RX_RX_EQU_ADAPTOR_CNTRL2);
	writel_relaxed(0x4A, ethqos->sgmii_base + QSERDES_RX_RX_EQU_ADAPTOR_CNTRL3);
	writel_relaxed(0x0A, ethqos->sgmii_base + QSERDES_RX_RX_EQU_ADAPTOR_CNTRL4);
	writel_relaxed(0x80, ethqos->sgmii_base + QSERDES_RX_RX_IDAC_TSETTLE_LOW);
	writel_relaxed(0x01, ethqos->sgmii_base + QSERDES_RX_RX_IDAC_TSETTLE_HIGH);
	writel_relaxed(0x20, ethqos->sgmii_base + QSERDES_RX_RX_IDAC_MEASURE_TIME);
	writel_relaxed(0x17, ethqos->sgmii_base + QSERDES_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL1);
	writel_relaxed(0x00, ethqos->sgmii_base + QSERDES_RX_RX_OFFSET_ADAPTOR_CNTRL2);
	writel_relaxed(0x0F, ethqos->sgmii_base + QSERDES_RX_SIGDET_CNTRL);
	writel_relaxed(0x1E, ethqos->sgmii_base + QSERDES_RX_SIGDET_DEGLITCH_CNTRL);
	writel_relaxed(0x05, ethqos->sgmii_base + QSERDES_RX_RX_BAND);
	writel_relaxed(0xE0, ethqos->sgmii_base + QSERDES_RX_RX_MODE_00_LOW);
	writel_relaxed(0xC8, ethqos->sgmii_base + QSERDES_RX_RX_MODE_00_HIGH);
	writel_relaxed(0xC8, ethqos->sgmii_base + QSERDES_RX_RX_MODE_00_HIGH2);
	writel_relaxed(0x09, ethqos->sgmii_base + QSERDES_RX_RX_MODE_00_HIGH3);
	writel_relaxed(0xB1, ethqos->sgmii_base + QSERDES_RX_RX_MODE_00_HIGH4);
	writel_relaxed(0xE0, ethqos->sgmii_base + QSERDES_RX_RX_MODE_01_LOW);
	writel_relaxed(0xC8, ethqos->sgmii_base + QSERDES_RX_RX_MODE_01_HIGH);
	writel_relaxed(0xC8, ethqos->sgmii_base + QSERDES_RX_RX_MODE_01_HIGH2);
	writel_relaxed(0x09, ethqos->sgmii_base + QSERDES_RX_RX_MODE_01_HIGH3);
	writel_relaxed(0xB1, ethqos->sgmii_base + QSERDES_RX_RX_MODE_01_HIGH4);
	writel_relaxed(0xE0, ethqos->sgmii_base + QSERDES_RX_RX_MODE_10_LOW);
	writel_relaxed(0xC8, ethqos->sgmii_base + QSERDES_RX_RX_MODE_10_HIGH);
	writel_relaxed(0xC8, ethqos->sgmii_base + QSERDES_RX_RX_MODE_10_HIGH2);
	writel_relaxed(0x3B, ethqos->sgmii_base + QSERDES_RX_RX_MODE_10_HIGH3);
	writel_relaxed(0xB7, ethqos->sgmii_base + QSERDES_RX_RX_MODE_10_HIGH4);
	writel_relaxed(0x0C, ethqos->sgmii_base + QSERDES_RX_DCC_CTRL1);

	/****************MODULE: SGMII_PHY_SGMII_PCS**********************************/
	writel_relaxed(0x0C, ethqos->sgmii_base + QSERDES_PCS_LINE_RESET_TIME);
	writel_relaxed(0x1F, ethqos->sgmii_base + QSERDES_PCS_TX_LARGE_AMP_DRV_LVL);
	writel_relaxed(0x03, ethqos->sgmii_base + QSERDES_PCS_TX_SMALL_AMP_DRV_LVL);
	writel_relaxed(0x83, ethqos->sgmii_base + QSERDES_PCS_TX_MID_TERM_CTRL1);
	writel_relaxed(0x08, ethqos->sgmii_base + QSERDES_PCS_TX_MID_TERM_CTRL2);
	writel_relaxed(0x0C, ethqos->sgmii_base + QSERDES_PCS_SGMII_MISC_CTRL8);
	writel_relaxed(0x00, ethqos->sgmii_base + QSERDES_PCS_SW_RESET);

	writel_relaxed(0x01, ethqos->sgmii_base + QSERDES_PCS_PHY_START);


	do {
		val = readl_relaxed(ethqos->sgmii_base + QSERDES_COM_C_READY_STATUS);
		val &= BIT(0);
		if (val)
			break;
		usleep_range(1000, 1500);
		retry--;
	} while (retry > 0);
	if (!retry)
		ETHQOSERR("QSERDES_COM_C_READY_STATUS timedout with retry = %d\n", retry);

	retry = 500;
	do {
		val = readl_relaxed(ethqos->sgmii_base + QSERDES_PCS_PCS_READY_STATUS);
		val &= BIT(0);
		if (val)
			break;
		usleep_range(1000, 1500);
		retry--;
	} while (retry > 0);
	if (!retry)
		ETHQOSERR("PCS_READY timedout with retry = %d\n", retry);

	retry = 500;
	do {
		val = readl_relaxed(ethqos->sgmii_base + QSERDES_PCS_PCS_READY_STATUS);
		val &= BIT(7);
		if (val)
			break;
		usleep_range(1000, 1500);
		retry--;
	} while (retry > 0);
	if (!retry)
		ETHQOSERR("SGMIIPHY_READY timedout with retry = %d\n", retry);

	retry = 5000;
	do {
		val = readl_relaxed(ethqos->sgmii_base + QSERDES_COM_CMN_STATUS);
		val &= BIT(1);
		if (val)
			break;
		usleep_range(1000, 1500);
		retry--;
	} while (retry > 0);
	if (!retry)
		ETHQOSERR("PLL Lock Status timedout with retry = %d\n", retry);
}



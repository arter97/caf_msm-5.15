/* SPDX-License-Identifier: GPL-2.0-only */

/* Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved. */

#ifndef _EMAC_CTRL_HVM_MSGQ_H_
#define _EMAC_CTRL_HVM_MSGQ_H_

enum msg_type {
	MSGQ_HANDSHAKE_REQ,
	MSGQ_HANDSHAKE_RESP,
	NOTIFICATION,
	REG_EVENTS,
	UNREG_EVENTS,
	DMA_STOP_ACK,
	UNICAST_ADD,
	MULTICAST_ADD,
	VLAN_ADD,
	UNICAST_DEL,
	MULTICAST_DEL,
	VLAN_DEL,
	PRIO_ADD,
	PRIO_DEL,
};

enum eth_state {
	/* EMAC HW is not powered up*/
	EMAC_HW_DOWN = 0,
	/* EMAC HW clocks are voted and registers accesible*/
	EMAC_HW_UP = 1,
	/* EMAC PHY Link is down*/
	EMAC_LINK_DOWN = 2,
	/* EMAC PHY link is up*/
	EMAC_LINK_UP = 3,
	/* Interrupt in DMA channel, SW must read process and clear Ch Status Reg*/
	EMAC_DMA_INT_STS_AVAIL = 4,
};

typedef void (*qcom_ethmsgq_notify) (enum msg_type, void *buf, int len, void *user_data);

#if IS_ENABLED(CONFIG_ETHQOS_QCOM_HOSTVM)
int qcom_ethmsgq_init(struct device *dev);
void qcom_ethmsgq_deinit(struct device *dev);
void qcom_notify_ethstate_tosvm(enum msg_type type, enum eth_state event);
void qcom_ethmsgq_register_notify(qcom_ethmsgq_notify notify, void *user_data);
#else
static inline int qcom_ethmsgq_init(struct device *dev)
{
	return 0;
}

static inline void qcom_ethmsgq_deinit(struct device *dev)
{
	/* Not enabled */
}

static inline void qcom_notify_ethstate_tosvm(enum msg_type type, enum eth_state event)
{
	/* Not enabled */
}

static inline void qcom_ethmsgq_register_notify(qcom_ethmsgq_notify notify, void *user_data)
{
	/* Not enabled */
}
#endif /* CONFIG_ETHQOS_QCOM_HOSTVM */

#endif /* _EMAC_CTRL_HVM_MSGQ_H_ */

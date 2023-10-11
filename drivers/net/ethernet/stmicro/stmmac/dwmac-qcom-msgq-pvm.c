// SPDX-License-Identifier: GPL-2.0-only

// Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.

#include <linux/compiler.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/scatterlist.h>
#include <linux/workqueue.h>
#include <soc/qcom/boot_stats.h>
#include <linux/gunyah/gh_msgq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/pm.h>
#include <linux/suspend.h>
#include "dwmac-qcom-msgq-pvm.h"

#define EMAC_MAX_VLAN_ID          4094
#define EMAC_MIN_VLAN_ID             1

/**
 * struct msg_format - Message format for Ethernet control communication.
 * @type:    Message type.
 * @event:   Event type for Notification message types.
 * @data:    Data Payload.
 *
 * Messages format used for control communication between ethernet
 * drivers on Primary Virtual machine and Secondary virtual machine.
 */
struct msg_format {
	u8  type;
	u8  event;
	u8  data[12];
} __packed;

struct emac_msgq_priv {
	struct device *dev;
	void *msgq_hdl;
	struct wakeup_source *wakeup_source;
	bool notify_hw_events;
	struct workqueue_struct *wq;
	struct work_struct emac_work;
	struct list_head events_q;
	enum eth_state last_state;
	spinlock_t xmit_lock; /* To serialize tranmits on msgq */
	struct task_struct *recv_thread;
	qcom_ethmsgq_notify command_cb;
	void *user_data;
};

struct emac_be_ev {
	struct list_head list;
	struct msg_format msg;
};

static struct emac_msgq_priv *msgq_priv;

static int emac_msgq_xmit(struct msg_format *msg)
{
	int ret = 0;

	ret = gh_msgq_send(msgq_priv->msgq_hdl, msg, sizeof(*msg), 0);
	if (ret)
		dev_err(msgq_priv->dev, "send msgq failed %d\n", ret);

	dev_info(msgq_priv->dev, "%s ethqos sent msgq type = %d event %d\n",
		 __func__, msg->type, msg->event);

	return ret;
}

static void emac_post_events(struct work_struct *work)
{
	struct emac_be_ev *emac_ev;

	pr_info("%d %s posting notifications to SVM\n", __LINE__, __func__);
	do {
		spin_lock(&msgq_priv->xmit_lock);
		emac_ev = list_first_entry_or_null(&msgq_priv->events_q, struct emac_be_ev, list);
		if (emac_ev)
			list_del(&emac_ev->list);
		spin_unlock(&msgq_priv->xmit_lock);

		if (!emac_ev)
			break;

		emac_msgq_xmit(&emac_ev->msg);
		kfree(emac_ev);
	} while (1);
}

static int qcom_notify_msgq(enum msg_type type, enum eth_state event)
{
	struct emac_be_ev *emac_ev;

	if (type == NOTIFICATION && !msgq_priv->notify_hw_events) {
		dev_info(msgq_priv->dev, "Ethqos SVM not up, Drop event:%d last_state = %d\n",
			 event, msgq_priv->last_state);
		return -ENOTCONN;
	}

	emac_ev = kzalloc(sizeof(*emac_ev), GFP_ATOMIC);
	if (!emac_ev)
		return -ENOMEM;

	emac_ev->msg.type = type;
	emac_ev->msg.event = event;

	spin_lock(&msgq_priv->xmit_lock);
	list_add_tail(&emac_ev->list, &msgq_priv->events_q);
	spin_unlock(&msgq_priv->xmit_lock);

	queue_work(msgq_priv->wq, &msgq_priv->emac_work);

	return 0;
}

void qcom_notify_ethstate_tosvm(enum msg_type type, enum eth_state event)
{
	msgq_priv->last_state = event;
	qcom_notify_msgq(type, event);
}

void qcom_ethmsgq_register_notify(qcom_ethmsgq_notify notify, void *user_data)
{
	msgq_priv->command_cb = notify;
	msgq_priv->user_data = user_data;
}

static int recv_thread(void *data)
{
	struct msg_format rx_msg;
	size_t recv_size;
	int ret;

	while (!kthread_should_stop()) {
		ret = gh_msgq_recv(msgq_priv->msgq_hdl, &rx_msg, sizeof(rx_msg),
				   &recv_size, 0);
		if (ret) {
			pr_err("recv msgq failed ret = %d\n", ret);
			continue;
		}
		pr_info("EMAC recd msg: type 0x%x Event = 0x%x link last_state = %d\n",
			rx_msg.type, rx_msg.event, msgq_priv->last_state);

		switch (rx_msg.type) {
		case MSGQ_HANDSHAKE_REQ:
			qcom_notify_msgq(MSGQ_HANDSHAKE_RESP, 0);
			break;

		case REG_EVENTS:
			msgq_priv->notify_hw_events = true;

			/* Report last link state, if link is already up,
			 * post both events HW up and link up, If HW is down
			 * post both link down and HW down events. This will
			 * ensure the SVM does needed handling at each transistion
			 */
			if (msgq_priv->last_state == EMAC_LINK_UP ||
			    msgq_priv->last_state == EMAC_LINK_DOWN)
				qcom_notify_msgq(NOTIFICATION, EMAC_HW_UP);
			else if (msgq_priv->last_state == EMAC_HW_DOWN)
				qcom_notify_msgq(NOTIFICATION, EMAC_LINK_DOWN);

			qcom_notify_msgq(NOTIFICATION, msgq_priv->last_state);
			break;

		case UNREG_EVENTS:
			msgq_priv->notify_hw_events = false;
			break;

		case UNICAST_ADD:
		case MULTICAST_ADD:
		case UNICAST_DEL:
		case MULTICAST_DEL:
			break;

		case VLAN_ADD:
		case VLAN_DEL:
			if (msgq_priv->command_cb)
				msgq_priv->command_cb(rx_msg.type,
						      &rx_msg.data[0],
						      2,
						      msgq_priv->user_data);
			break;

		case PRIO_ADD:
		case PRIO_DEL:
			if (msgq_priv->command_cb)
				msgq_priv->command_cb(rx_msg.type,
						      &rx_msg.data[0],
						      1,
						      msgq_priv->user_data);
			break;

		default:
			break;
		}
	}
	return 0;
}

int qcom_ethmsgq_init(struct device *dev)
{
	int ret = 0;

	msgq_priv = devm_kzalloc(dev, sizeof(*msgq_priv), GFP_KERNEL);
	if (!msgq_priv)
		return -ENOMEM;

	msgq_priv->dev = dev;

	msgq_priv->msgq_hdl = gh_msgq_register(GH_MSGQ_LABEL_ETH);
	if (IS_ERR_OR_NULL(msgq_priv->msgq_hdl)) {
		ret = PTR_ERR(msgq_priv->msgq_hdl);
		dev_err(dev, "failed to get gunyah msgq %d\n", ret);
		return ret;
	}

	msgq_priv->wq = create_singlethread_workqueue("ethqos_wq");
	if (!msgq_priv->wq) {
		dev_err(dev, "Failed to create workqueue\n");
		return -ENOMEM;
	}

	spin_lock_init(&msgq_priv->xmit_lock);
	INIT_WORK(&msgq_priv->emac_work, emac_post_events);
	INIT_LIST_HEAD(&msgq_priv->events_q);

	msgq_priv->recv_thread = kthread_run(recv_thread, msgq_priv, dev_name(dev));

	return ret;
}

void qcom_ethmsgq_deinit(struct device *dev)
{
	if (msgq_priv->msgq_hdl)
		gh_msgq_unregister(msgq_priv->msgq_hdl);

	cancel_work_sync(&msgq_priv->emac_work);
	destroy_workqueue(msgq_priv->wq);
}

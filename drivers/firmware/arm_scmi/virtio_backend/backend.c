// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "SCMI Virtio BE: " fmt

#include <linux/device.h>
#include <linux/export.h>
#include <linux/idr.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/scmi_protocol.h>
#include <linux/scmi_virtio_backend.h>
#include <linux/slab.h>
#include <linux/bitfield.h>

#include "common.h"
#include "backend.h"
#include "client_handle.h"
#include "../common.h"
#include "backend_protocol.h"

#define MSG_HDR_SZ		4
#define MSG_STATUS_SZ		4

/**
 * scmi_virtio_client_priv - Structure respreseting client specific
 * private data maintained by backend.
 *
 * @backend_protocol_map:     IDR used by Virtio backend, to maintain per protocol
 *              private data. This map is initialized with protocol data,
 *              on scmi_virtio_be_open() call during client channel setup.
 *              The per protocol pointer is initialized by each registered
 *              backend protocol, in its open() call, and can be used to
 *              reference client specific bookkeeping information, which
 *              is maintained by that protocol. This can be used to release
 *              client specific protocol resources during close call for
 *              it.
 */
struct scmi_virtio_client_priv {
	struct idr backend_protocol_map;
};

struct scmi_virtio_be_info {
	struct device *dev;
	struct idr registered_protocols;
	struct idr active_protocols;
	struct rw_semaphore active_protocols_rwsem;
};

static struct scmi_virtio_be_info scmi_virtio_be_info;

void *scmi_virtio_get_protocol_priv(
	const struct scmi_virtio_protocol_h *__maybe_unused prot_h,
	const struct scmi_virtio_client_h *client_h)
{
	return scmi_virtio_get_client_priv(client_h);
}

void scmi_virtio_set_protocol_priv(
	const struct scmi_virtio_protocol_h *__maybe_unused prot_h,
	const struct scmi_virtio_client_h *client_h,
	void *priv)
{
	scmi_virtio_set_client_priv(client_h, priv);
}

int scmi_virtio_protocol_register(const struct scmi_virtio_protocol *proto)
{
	int ret;

	if (!proto) {
		pr_err("Invalid protocol\n");
		return -EINVAL;
	}

	if (!proto->prot_init_fn || !proto->prot_exit_fn) {
		pr_err("Missing protocol init/exit fn %#x\n", proto->id);
		return -EINVAL;
	}

	ret = idr_alloc(&scmi_virtio_be_info.registered_protocols, (void *)proto,
			proto->id, proto->id + 1, GFP_ATOMIC);
	if (ret != proto->id) {
		pr_err(
		  "Idr allocation failed for %#x - err %d\n",
		  proto->id, ret);
		return ret;
	}

	pr_debug("Registered Protocol %#x\n", proto->id);

	return 0;
}

void scmi_virtio_protocol_unregister(const struct scmi_virtio_protocol *proto)
{
	if (proto) {
		idr_remove(&scmi_virtio_be_info.registered_protocols, proto->id);
		pr_debug("Unregistered Protocol %#x\n", proto->id);
	} else {
		pr_err("%s called with NULL proto\n", __func__);
	}
}

int scmi_virtio_implemented_protocols(
	const struct scmi_virtio_protocol_h *__maybe_unused prot_h,
	const struct scmi_virtio_client_h *__maybe_unused client_h,
	u8 *imp_protocols, int *imp_proto_num)
{
	const struct scmi_virtio_protocol_h *protocol_h;
	const struct scmi_virtio_protocol_h *base_protocol_h;
	int active_protocols_num = 0;
	int proto_num = *imp_proto_num;
	unsigned int id = 0;
	int ret = 0;

	base_protocol_h = idr_find(&scmi_virtio_be_info.active_protocols,
					SCMI_PROTOCOL_BASE);
	if (prot_h != base_protocol_h)
		return -EINVAL;

	/*
	 * SCMI agents, identified by client_h, may not have access to
	 * all protocols?
	 */
	idr_for_each_entry(&scmi_virtio_be_info.active_protocols, protocol_h, id) {
		if (id == SCMI_PROTOCOL_BASE)
			continue;
		if (active_protocols_num >= proto_num) {
			pr_err("Number of active protocols %d > max num: %d\n",
				(active_protocols_num + 1), proto_num);
			ret = -ENOMEM;
			goto imp_protocols_exit;
		}
		imp_protocols[active_protocols_num] = id;
		active_protocols_num++;
	}
imp_protocols_exit:
	*imp_proto_num = active_protocols_num;

	return ret;
}

static int scmi_virtio_protocol_init(struct device *dev, u8 protocol_id)
{
	int ret = 0, ret2 = 0;
	struct scmi_virtio_protocol *protocol_info;
	struct scmi_virtio_protocol_h *protocol_h;

	down_write(&scmi_virtio_be_info.active_protocols_rwsem);
	protocol_info = idr_find(&scmi_virtio_be_info.registered_protocols, protocol_id);
	if (!protocol_info) {
		pr_err("Protocol %#x not registered; protocol init failed\n",
		       protocol_id);
		ret = -ENOENT;
		goto out_protocol_release_rwsem;
	}

	protocol_h = kzalloc(sizeof(*protocol_h), GFP_KERNEL);
	if (!protocol_h) {
		ret = -ENOMEM;
		goto out_protocol_release_rwsem;
	}

	protocol_h->dev = dev;
	ret = protocol_info->prot_init_fn(protocol_h);
	if (ret) {
		pr_err("Protocol %#x init function returned: %d\n",
			protocol_id, ret);
		kfree(protocol_h);
		protocol_h = NULL;
		goto out_protocol_release_rwsem;
	}

	/* Register this protocol in active protocols list */
	ret = idr_alloc(&scmi_virtio_be_info.active_protocols,
			(void *)protocol_h,
			protocol_id, protocol_id + 1, GFP_ATOMIC);
	if (ret != protocol_id) {
		pr_err(
		  "Allocation failed for active protocol %#x - err %d\n",
		  protocol_id, ret);
		ret2 = protocol_info->prot_exit_fn(protocol_h);
		if (ret2)
			pr_err("Protocol %#x exit function returned: %d\n",
				protocol_id, ret2);
		kfree(protocol_h);
		protocol_h = NULL;
		goto out_protocol_release_rwsem;
	}

	ret = 0;
out_protocol_release_rwsem:
	up_write(&scmi_virtio_be_info.active_protocols_rwsem);

	return ret;
}

static int scmi_virtio_protocol_exit(struct device *dev)
{
	int ret = 0;
	unsigned int id = 0;
	struct scmi_virtio_protocol *protocol_info;
	const struct scmi_virtio_protocol_h *protocol_h;

	down_write(&scmi_virtio_be_info.active_protocols_rwsem);
	idr_for_each_entry(&scmi_virtio_be_info.active_protocols, protocol_h, id) {
		protocol_info = idr_find(&scmi_virtio_be_info.registered_protocols, id);
		ret = protocol_info->prot_exit_fn(protocol_h);
		if (ret) {
			pr_err("Protocol %#x exit function returned: %d\n",
				id, ret);
		}
		kfree(protocol_h);
		idr_remove(&scmi_virtio_be_info.active_protocols, id);
	}
	up_write(&scmi_virtio_be_info.active_protocols_rwsem);

	return ret;
}

int scmi_virtio_be_open(const struct scmi_virtio_client_h *client_h)
{
	int tmp_id, ret = 0, close_ret = 0;
	const struct scmi_virtio_protocol_h *protocol_h;
	struct scmi_virtio_protocol *virtio_be_protocol;
	unsigned int id = 0;
	struct scmi_virtio_client_h *proto_client_h;
	struct scmi_virtio_client_priv *client_priv;
	u8 open_protocols[MAX_PROTOCOLS_IMP];
	u8 open_protocols_num = 0;

	client_priv = kzalloc(sizeof(*client_priv), GFP_KERNEL);

	if (!client_priv)
		return -ENOMEM;

	idr_init(&client_priv->backend_protocol_map);

	down_read(&scmi_virtio_be_info.active_protocols_rwsem);
	idr_for_each_entry(&scmi_virtio_be_info.active_protocols,
			   protocol_h, id) {
		virtio_be_protocol = idr_find(&scmi_virtio_be_info.registered_protocols, id);
		proto_client_h = scmi_virtio_get_client_h(client_h);
		if (!proto_client_h) {
			ret = -ENOMEM;
			goto virtio_proto_open_fail;
		}
		ret = virtio_be_protocol->prot_ops->open(proto_client_h);
		if (ret) {
			pr_err("->open() failed for id: %d ret: %d\n",
				id, ret);
			goto virtio_proto_open_fail;
		}
		tmp_id = idr_alloc(&client_priv->backend_protocol_map,
			(void *)proto_client_h,
			id, id + 1, GFP_ATOMIC);
		if (tmp_id != id) {
			pr_err("Failed to allocate client handle for %#x\n",
				id);
			close_ret = virtio_be_protocol->prot_ops->close(
						proto_client_h);
			if (close_ret) {
				pr_err("->close() failed for id: %d ret: %d\n",
					id, close_ret);
			}
			scmi_virtio_put_client_h(proto_client_h);
			ret = -EINVAL;
			goto virtio_proto_open_fail;
		}
		open_protocols[open_protocols_num++] = (u8)id;
	}
	up_read(&scmi_virtio_be_info.active_protocols_rwsem);
	scmi_virtio_set_client_priv(client_h, client_priv);

	return 0;

virtio_proto_open_fail:
	for ( ; open_protocols_num > 0; open_protocols_num--) {
		id = open_protocols[open_protocols_num-1];
		virtio_be_protocol = idr_find(&scmi_virtio_be_info.registered_protocols, id);
		proto_client_h = idr_find(&client_priv->backend_protocol_map, id);
		close_ret = virtio_be_protocol->prot_ops->close(proto_client_h);
		if (close_ret)
			pr_err("->close() failed for id: %d ret: %d\n", id, close_ret);
		scmi_virtio_put_client_h(proto_client_h);
	}

	up_read(&scmi_virtio_be_info.active_protocols_rwsem);
	kfree(client_priv);

	return ret;
}
EXPORT_SYMBOL(scmi_virtio_be_open);

int scmi_virtio_be_close(const struct scmi_virtio_client_h *client_h)
{
	const struct scmi_virtio_protocol_h *protocol_h;
	struct scmi_virtio_protocol *virtio_be_protocol;
	unsigned int id = 0;
	struct scmi_virtio_client_priv *client_priv =
		scmi_virtio_get_client_priv(client_h);
	struct scmi_virtio_client_h *proto_client_h;
	int ret = 0, close_ret = 0;

	if (!client_priv) {
		pr_err("->close() failed: priv data not available\n");
		return -EINVAL;
	}

	down_read(&scmi_virtio_be_info.active_protocols_rwsem);
	idr_for_each_entry(&scmi_virtio_be_info.active_protocols,
			   protocol_h, id) {
		virtio_be_protocol = idr_find(&scmi_virtio_be_info.registered_protocols, id);
		proto_client_h = idr_find(&client_priv->backend_protocol_map, id);
		/* We might have failed to alloc idr, in scmi_virtio_be_open() */
		if (!proto_client_h)
			continue;
		close_ret = virtio_be_protocol->prot_ops->close(proto_client_h);
		if (close_ret)
			pr_err("->close() failed for id: %d ret: %d\n", id, close_ret);
		/* Return first failure return code */
		ret = ret ? : close_ret;
		scmi_virtio_put_client_h(proto_client_h);
	}
	up_read(&scmi_virtio_be_info.active_protocols_rwsem);

	idr_destroy(&client_priv->backend_protocol_map);
	kfree(client_priv);
	scmi_virtio_set_client_priv(client_h, NULL);

	return ret;
}
EXPORT_SYMBOL(scmi_virtio_be_close);

int scmi_virtio_be_request(const struct scmi_virtio_client_h *client_h,
			const struct scmi_virtio_be_msg *req,
			struct scmi_virtio_be_msg *resp)
{	int ret = 0;
	struct scmi_virtio_be_msg_hdr msg_header;
	u32 msg;
	struct scmi_virtio_be_payload_buf req_payload, resp_payload;
	struct scmi_virtio_protocol *virtio_be_protocol;
	const struct scmi_virtio_protocol_h *protocol_h;
	struct scmi_virtio_client_priv *client_priv =
		scmi_virtio_get_client_priv(client_h);
	struct scmi_virtio_client_h *proto_client_h;
	int scmi_error = 0;

	/* Unpack message header */
	if (!req || req->msg_sz < MSG_HDR_SZ ||
	    !resp || resp->msg_sz <
			(MSG_HDR_SZ + MSG_STATUS_SZ)) {
		pr_err("Invalid equest/response message size\n");
		return -EINVAL;
	}

	down_read(&scmi_virtio_be_info.active_protocols_rwsem);
	msg = le32_to_cpu(*(__le32 *)req->msg_payld);
	msg_header.protocol_id = MSG_XTRACT_PROT_ID(msg);
	msg_header.msg_id = MSG_XTRACT_ID(msg);
	msg_header.type = MSG_XTRACT_TYPE(msg);
	msg_header.seq = MSG_XTRACT_TOKEN(msg);

	*(__le32 *)resp->msg_payld = cpu_to_le32(msg);

	resp_payload.payload =
		(void *)((unsigned long)(uintptr_t)resp->msg_payld
			+ MSG_HDR_SZ);

	if (!client_priv) {
		pr_err("Private map not initialized\n");
		scmi_error = SCMI_VIRTIO_BE_NOT_FOUND;
		goto protocol_find_fail;
	}

	protocol_h = idr_find(&scmi_virtio_be_info.active_protocols,
				   msg_header.protocol_id);
	if (unlikely(!protocol_h)) {
		pr_err("Invalid protocol id in request header :%#x\n",
			msg_header.protocol_id);
		scmi_error = SCMI_VIRTIO_BE_NOT_FOUND;
		goto protocol_find_fail;
	}

	proto_client_h = idr_find(&client_priv->backend_protocol_map,
				   msg_header.protocol_id);
	if (!proto_client_h) {
		pr_err("Frontend handle not present for protocol : %#x\n",
			msg_header.protocol_id);
		scmi_error = SCMI_VIRTIO_BE_NOT_FOUND;
		goto protocol_find_fail;
	}

	virtio_be_protocol = idr_find(&scmi_virtio_be_info.registered_protocols,
					msg_header.protocol_id);
	if (!virtio_be_protocol) {
		pr_err("Protocol %#x is not registered\n",
			msg_header.protocol_id);
		scmi_error = SCMI_VIRTIO_BE_NOT_FOUND;
		goto protocol_find_fail;
	}

	req_payload.payload =
		(void *)((unsigned long)(uintptr_t)req->msg_payld +
		MSG_HDR_SZ);
	req_payload.payload_size = req->msg_sz - MSG_HDR_SZ;

	resp_payload.payload_size = resp->msg_sz - MSG_HDR_SZ;

	ret = virtio_be_protocol->prot_ops->msg_handle(
		proto_client_h, &msg_header, &req_payload, &resp_payload);
	if (ret) {
		pr_err("Protocol %#x ->msg_handle() failed err: %d\n",
			msg_header.protocol_id, ret);
		scmi_error = scmi_virtio_linux_err_remap(ret);
		goto protocol_find_fail;
	}

	up_read(&scmi_virtio_be_info.active_protocols_rwsem);

	resp->msg_sz = MSG_HDR_SZ + resp_payload.payload_size;

	return 0;

protocol_find_fail:
	up_read(&scmi_virtio_be_info.active_protocols_rwsem);
	*((__le32 *)(resp_payload.payload)) = cpu_to_le32(scmi_error);
	resp->msg_sz = MSG_HDR_SZ + MSG_STATUS_SZ;

	return 0;
}
EXPORT_SYMBOL(scmi_virtio_be_request);

static int scmi_virtio_be_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct device_node *child, *np = dev->of_node;

	scmi_virtio_be_info.dev = dev;

	platform_set_drvdata(pdev, &scmi_virtio_be_info);

	/*
	 * Base protocol is always implemented. So init it.
	 */
	ret = scmi_virtio_protocol_init(dev, SCMI_PROTOCOL_BASE);
	if (ret == -ENOENT)
		return -EPROBE_DEFER;
	else if (ret)
		return ret;

	for_each_available_child_of_node(np, child) {
		u32 prot_id;

		if (of_property_read_u32(child, "reg", &prot_id))
			continue;

		if (!FIELD_FIT(MSG_PROTOCOL_ID_MASK, prot_id)) {
			dev_err(dev, "Out of range protocol %d\n", prot_id);
			continue;
		}

		ret = scmi_virtio_protocol_init(dev, prot_id);
		if (ret == -EPROBE_DEFER)
			goto virtio_be_protocol_exit;
	}

	return 0;

virtio_be_protocol_exit:
	scmi_virtio_protocol_exit(dev);

	return ret;
}

static int scmi_virtio_be_remove(struct platform_device *pdev)
{
	scmi_virtio_protocol_exit(&pdev->dev);

	return 0;
}

static const struct of_device_id scmi_virtio_be_of_match[] = {
	{ .compatible = "arm,scmi-virtio-backend" },
	{ },
};

MODULE_DEVICE_TABLE(of, scmi_virtio_be_of_match);

static struct platform_driver scmi_virtio_be_driver = {
	.driver = {
		   .name = "arm-scmi-virtio-backend",
		   .of_match_table = scmi_virtio_be_of_match,
		   },
	.probe = scmi_virtio_be_probe,
	.remove = scmi_virtio_be_remove,
};

static int __init scmi_virtio_be_init(void)
{
	idr_init(&scmi_virtio_be_info.registered_protocols);
	idr_init(&scmi_virtio_be_info.active_protocols);
	init_rwsem(&scmi_virtio_be_info.active_protocols_rwsem);
	scmi_virtio_base_register();

#ifdef CONFIG_ARM_SCMI_VIRTIO_CLK
	scmi_virtio_clock_register();
#endif

	return platform_driver_register(&scmi_virtio_be_driver);
}
module_init(scmi_virtio_be_init);

static void __exit scmi_virtio_be_exit(void)
{
#ifdef CONFIG_ARM_SCMI_VIRTIO_CLK
	scmi_virtio_clock_unregister();
#endif
	scmi_virtio_base_unregister();
	idr_destroy(&scmi_virtio_be_info.active_protocols);
	idr_destroy(&scmi_virtio_be_info.registered_protocols);
	platform_driver_unregister(&scmi_virtio_be_driver);
}
module_exit(scmi_virtio_be_exit);

MODULE_DESCRIPTION("ARM SCMI protocol Virtio Backend driver");
MODULE_LICENSE("GPL");

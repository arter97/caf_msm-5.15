/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _SCMI_VIRTIO_BE_COMMON_H
#define _SCMI_VIRTIO_BE_COMMON_H

struct scmi_virtio_protocol_h {
	struct device *dev;
};

struct scmi_virtio_be_payload_buf {
	void *payload;
	size_t payload_size;
};

struct scmi_virtio_be_msg_hdr {
	u8 msg_id;
	u8 protocol_id;
	u8 type;
	u16 seq;
};

/**
 * struct scmi_virtio_protocol_ops - operations
 *      supported by SCMI Virtio backend protocol drivers.
 *
 * @open: Notify protocol driver, about new client.
 * @close: Notify protocol driver, about exiting client.
 * @msg_handle: Unparcel a request and parcel response, to be sent over
 *              client channels.
 */
struct scmi_virtio_protocol_ops {
	int (*open)(const struct scmi_virtio_client_h *client_h);
	int (*msg_handle)(const struct scmi_virtio_client_h *client_h,
			  const struct scmi_virtio_be_msg_hdr *msg_hdr,
			  const struct scmi_virtio_be_payload_buf *req_payload_buf,
			  struct scmi_virtio_be_payload_buf *resp_payload_buf);
	int (*close)(const struct scmi_virtio_client_h *client_h);
};

typedef int (*scmi_virtio_prot_init_fn_t)(const struct scmi_virtio_protocol_h *);
typedef int (*scmi_virtio_prot_exit_fn_t)(const struct scmi_virtio_protocol_h *);

struct scmi_virtio_protocol {
	const u8 id;
	const scmi_virtio_prot_init_fn_t prot_init_fn;
	const scmi_virtio_prot_exit_fn_t prot_exit_fn;
	const struct scmi_virtio_protocol_ops *prot_ops;
};

#endif /* _SCMI_VIRTIO_BE_COMMON_H */

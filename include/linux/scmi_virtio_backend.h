/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _LINUX_SCMI_VIRTIO_BACKEND_H
#define _LINUX_SCMI_VIRTIO_BACKEND_H

#include <linux/idr.h>

/**
 * scmi_virtio_client_h : Structure encapsulating a unique
 * handle, identifying the client connection to SCMI
 * Virtio backend.
 */
struct scmi_virtio_client_h {
	const void *handle;
};

struct scmi_virtio_be_msg {
	struct scmi_msg_payld *msg_payld;
	size_t msg_sz;
};

int scmi_virtio_be_open(const struct scmi_virtio_client_h *client_h);
int scmi_virtio_be_close(const struct scmi_virtio_client_h *client_h);
int scmi_virtio_be_request(const struct scmi_virtio_client_h *client_h,
			const struct scmi_virtio_be_msg *req,
			struct scmi_virtio_be_msg *resp);

#endif /* _LINUX_SCMI_VIRTIO_BACKEND_H */

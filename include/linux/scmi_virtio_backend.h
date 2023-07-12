/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _LINUX_SCMI_VIRTIO_BACKEND_H
#define _LINUX_SCMI_VIRTIO_BACKEND_H

#include <linux/idr.h>

/* Maximum overhead of message w.r.t. struct scmi_desc.max_msg_size */
#define SCMI_MSG_MAX_PROT_OVERHEAD (2 * sizeof(__le32))

#define VIRTIO_SCMI_MAX_MSG_SIZE 128 /* Value may be increased. */
#define VIRTIO_SCMI_MAX_PDU_SIZE \
	(VIRTIO_SCMI_MAX_MSG_SIZE + SCMI_MSG_MAX_PROT_OVERHEAD)

/*
 * struct scmi_msg_payld - Transport SDU layout
 *
 * The SCMI specification requires all parameters, message headers, return
 * arguments or any protocol data to be expressed in little endian format only.
 */
struct scmi_msg_payld {
	__le32 msg_header;
	__le32 msg_payload[];
};

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

/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _SCMI_VIRTIO_BE_PROTOCOL_H
#define _SCMI_VIRTIO_BE_PROTOCOL_H

#include <linux/bitfield.h>

#define SCMI_VIRTIO_BE_SUCCESS	0
#define SCMI_VIRTIO_BE_NOT_SUPPORTED	-1
#define SCMI_VIRTIO_BE_INVALID_PARAMETERS	-2
#define SCMI_VIRTIO_BE_DENIED	-3
#define SCMI_VIRTIO_BE_NOT_FOUND	-4
#define SCMI_VIRTIO_BE_OUT_OF_RANGE	-5
#define SCMI_VIRTIO_BE_BUSY	-6
#define SCMI_VIRTIO_BE_COMMS_ERROR	-7
#define SCMI_VIRTIO_BE_GENERIC_ERROR	-8
#define SCMI_VIRTIO_BE_HARDWARE_ERROR	-9
#define SCMI_VIRTIO_BE_PROTOCOL_ERROR	-10

#define SCMI_VIRTIO_BE_VER_MINOR_MASK	GENMASK(15, 0)
#define SCMI_VIRTIO_BE_VER_MAJOR_MASK	GENMASK(31, 16)

#define SCMI_VIRTIO_BE_PROTO_MAJOR_VERSION(val) \
	((u32)FIELD_PREP(SCMI_VIRTIO_BE_VER_MAJOR_MASK, (val)))
#define SCMI_VIRTIO_BE_PROTO_MINOR_VERSION(val) \
	((u32)FIELD_PREP(SCMI_VIRTIO_BE_VER_MINOR_MASK, (val)))

#define SCMI_VIRTIO_BE_PROTO_VERSION(major, minor) \
	(SCMI_VIRTIO_BE_PROTO_MAJOR_VERSION((major)) | \
	 SCMI_VIRTIO_BE_PROTO_MINOR_VERSION((minor)))

static int __maybe_unused scmi_virtio_linux_err_remap(const int errno)
{
	switch (errno) {
	case 0:
		return SCMI_VIRTIO_BE_SUCCESS;
	case -EOPNOTSUPP:
		return SCMI_VIRTIO_BE_NOT_SUPPORTED;
	case -EINVAL:
		return SCMI_VIRTIO_BE_INVALID_PARAMETERS;
	case -EACCES:
		return SCMI_VIRTIO_BE_DENIED;
	case -ENOENT:
		return SCMI_VIRTIO_BE_NOT_FOUND;
	case -ERANGE:
		return SCMI_VIRTIO_BE_OUT_OF_RANGE;
	case -EBUSY:
		return SCMI_VIRTIO_BE_BUSY;
	case -ECOMM:
		return SCMI_VIRTIO_BE_COMMS_ERROR;
	case -EIO:
		return SCMI_VIRTIO_BE_GENERIC_ERROR;
	case -EREMOTEIO:
		return SCMI_VIRTIO_BE_HARDWARE_ERROR;
	case -EPROTO:
		return SCMI_VIRTIO_BE_PROTOCOL_ERROR;
	default:
		return SCMI_VIRTIO_BE_GENERIC_ERROR;
	}
}

int scmi_virtio_protocol_register(const struct scmi_virtio_protocol *proto);
void scmi_virtio_protocol_unregister(const struct scmi_virtio_protocol *proto);

void *scmi_virtio_get_protocol_priv(
	const struct scmi_virtio_protocol_h *__maybe_unused prot_handle,
	const struct scmi_virtio_client_h *client_h);
void scmi_virtio_set_protocol_priv(
	const struct scmi_virtio_protocol_h *__maybe_unused prot_handle,
	const struct scmi_virtio_client_h *client_h,
	void *priv);

int scmi_virtio_implemented_protocols(
	const struct scmi_virtio_protocol_h *__maybe_unused prot_handle,
	const struct scmi_virtio_client_h *__maybe_unused client_h,
	u8 *imp_protocols, int *imp_proto_num);

#define DEFINE_SCMI_VIRTIO_PROT_REG_UNREG(name, proto)	\
static const struct scmi_virtio_protocol *__this_proto = &(proto);	\
									\
int __init scmi_virtio_##name##_register(void)			\
{								\
	return scmi_virtio_protocol_register(__this_proto);	\
}								\
								\
void __exit scmi_virtio_##name##_unregister(void)		\
{								\
	scmi_virtio_protocol_unregister(__this_proto);		\
}
#endif /* _SCMI_VIRTIO_BE_PROTOCOL_H */

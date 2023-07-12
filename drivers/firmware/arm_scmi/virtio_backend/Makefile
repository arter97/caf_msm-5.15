# SPDX-License-Identifier: GPL-2.0-only
scmi-virtio-backend-y = backend.o client_handle.o
scmi-virtio-backend-protocols-y = base.o
scmi-virtio-backend-objs := $(scmi-virtio-backend-y) $(scmi-virtio-backend-protocols-y)
obj-$(CONFIG_ARM_SCMI_VIRTIO_BACKEND) += scmi-virtio-backend.o

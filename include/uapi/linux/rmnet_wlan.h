/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __RMNET_WLAN_GENL_H__
#define __RMNET_WLAN_GENL_H__

#include <linux/types.h>

#define RMNET_WLAN_GENL_VERSION 1
#define RMNET_WLAN_GENL_FAMILY_NAME "RMNET_WLAN"

/* These parameters can't exceed UINT8_MAX */
#define RMNET_WLAN_GENL_CMD_UNSPEC 0
#define RMNET_WLAN_GENL_CMD_ADD_TUPLES 1
#define RMNET_WLAN_GENL_CMD_DEL_TUPLES 2
#define RMNET_WLAN_GENL_CMD_SET_DEV 3
#define RMNET_WLAN_GENL_CMD_UNSET_DEV 4
#define RMNET_WLAN_GENL_CMD_ADD_FWD_INFO 5
#define RMNET_WLAN_GENL_CMD_DEL_FWD_INFO 6
#define RMNET_WLAN_GENL_CMD_SET_ENCAP_PORT 7
#define RMNET_WLAN_GENL_CMD_UNSET_ENCAP_PORT 8
#define RMNET_WLAN_GENL_CMD_RESET 9
#define RMNET_WLAN_GENL_CMD_ENCAP_PORT_ACT_PASS_THROUGH 10
#define RMNET_WLAN_GENL_CMD_ENCAP_PORT_ACT_DROP 11
#define RMNET_WLAN_GENL_CMD_LL_ADDR_ADD 12
#define RMNET_WLAN_GENL_CMD_LL_ADDR_DEL 13
#define RMNET_WLAN_GENL_CMD_GET_TUPLES 14

/* Update RMNET_WLAN_GENL_ATTR_MAX with the maximum value if a new entry is added */
#define RMNET_WLAN_GENL_ATTR_UNSPEC 0
#define RMNET_WLAN_GENL_ATTR_TUPLES 1
#define RMNET_WLAN_GENL_ATTR_DEV 2
#define RMNET_WLAN_GENL_ATTR_FWD_ADDR 3
#define RMNET_WLAN_GENL_ATTR_FWD_DEV 4
#define RMNET_WLAN_GENL_ATTR_ENCAP_PORT 5
#define RMNET_WLAN_GENL_ATTR_NET_TYPE 6
#define RMNET_WLAN_GENL_ATTR_LL_SRC_ADDR 7
#define RMNET_WLAN_GENL_ATTR_LL_DST_ADDR 8
#define RMNET_WLAN_GENL_ATTR_LL_SRC_PORT 9
#define RMNET_WLAN_GENL_ATTR_LL_DST_PORT 10

/* Update RMNET_WLAN_GENL_TUPLE_ATTR_MAX with the maximum value if a new entry is added */
#define RMNET_WLAN_GENL_TUPLE_ATTR_UNSPEC 0
#define RMNET_WLAN_GENL_TUPLE_ATTR_TUPLE 1

/* These parameters can't exceed UINT8_MAX */
#define DATA_PATH_PROXY_NET_WLAN 0
#define DATA_PATH_PROXY_NET_WWAN 1
#define DATA_PATH_PROXY_NET_LBO 2
#define DATA_PATH_PROXY_NET_EIWLAN_PROXY 3
#define DATA_PATH_PROXY_NET_EIWLAN 4
#define __DATA_PATH_PROXY_NET_MAX 255

struct rmnet_wlan_tuple {
	union {
		__be16 port;
		__be32 spi_val;
	};
	__u8 ip_proto;
	__u8 trans_proto;
};

#endif

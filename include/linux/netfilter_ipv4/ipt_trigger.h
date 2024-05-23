/* Kernel module to match the port-ranges, trigger related port-ranges,
 * and alters the destination to a local IP address.
 *
 * Copyright (C) 2003, CyberTAN Corporation
 * All Rights Reserved.
 *
 * Description:
 *   This is kernel module for port-triggering.
 *
 *   The module follows the Netfilter framework, called extended packet
 *   matching modules.
 */

#ifndef _IPT_TRIGGER_H_target
#define _IPT_TRIGGER_H_target
#define IPT_TRIGGER_BOTH_PROTOCOLS 253  /* unassigned number */

enum ipt_trigger_type {
	IPT_TRIGGER_DNAT = 1,
	IPT_TRIGGER_IN = 2,
	IPT_TRIGGER_OUT = 3
};

struct ipt_trigger_ports {
	u_int16_t mport[2];	/* Related destination port range */
	u_int16_t rport[2];	/* Port range to map related destination port range to */
};

struct ipt_trigger_info {
	enum ipt_trigger_type type;
	u_int16_t proto;	/* Related protocol */
	struct ipt_trigger_ports ports;
};

#endif /*_IPT_TRIGGER_H_target*/

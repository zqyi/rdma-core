/*
 * Copyright (c) 2008 Lawrence Livermore National Security
 *
 * Produced at Lawrence Livermore National Laboratory.
 * Written by Ira Weiny <weiny2@llnl.gov>.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define _GNU_SOURCE
#include <getopt.h>

#include <infiniband/mad.h>
#include <iba/ib_types.h>

#include "ibdiag_common.h"

struct ibmad_port *srcport;
/* for local link integrity */
int error_port = 1;

static int send_144_node_desc_update(void)
{
	ib_portid_t sm_port;
	ib_portid_t selfportid;
	int selfport;
	ib_rpc_t trap_rpc;
	ib_mad_notice_attr_t notice;

	if (ib_resolve_self_via(&selfportid, &selfport, NULL, srcport))
		IBERROR("can't resolve self");

	if (ib_resolve_smlid_via(&sm_port, 0, srcport))
		IBERROR("can't resolve SM destination port");

	memset(&trap_rpc, 0, sizeof(trap_rpc));
	trap_rpc.mgtclass = IB_SMI_CLASS;
	trap_rpc.method = IB_MAD_METHOD_TRAP;
	trap_rpc.trid = mad_trid();
	trap_rpc.attr.id = NOTICE;
	trap_rpc.datasz = IB_SMP_DATA_SIZE;
	trap_rpc.dataoffs = IB_SMP_DATA_OFFS;

	memset(&notice, 0, sizeof(notice));
	notice.generic_type = 0x80 | IB_NOTICE_TYPE_INFO;
	notice.g_or_v.generic.prod_type_lsb = cl_hton16(IB_NODE_TYPE_CA);
	notice.g_or_v.generic.trap_num = cl_hton16(144);
	notice.issuer_lid = cl_hton16((uint16_t) selfportid.lid);
	notice.data_details.ntc_144.lid = cl_hton16((uint16_t) selfportid.lid);
	notice.data_details.ntc_144.local_changes =
	    TRAP_144_MASK_OTHER_LOCAL_CHANGES;
	notice.data_details.ntc_144.change_flgs =
	    TRAP_144_MASK_NODE_DESCRIPTION_CHANGE;

	return (mad_send_via(&trap_rpc, &sm_port, NULL, &notice, srcport));
}

static int send_129_local_link_integrity(void)
{
	ib_portid_t sm_port;
	ib_portid_t selfportid;
	int selfport;
	ib_rpc_t trap_rpc;
	ib_mad_notice_attr_t notice;

	if (ib_resolve_self_via(&selfportid, &selfport, NULL, srcport))
		IBERROR("can't resolve self");

	if (ib_resolve_smlid_via(&sm_port, 0, srcport))
		IBERROR("can't resolve SM destination port");

	memset(&trap_rpc, 0, sizeof(trap_rpc));
	trap_rpc.mgtclass = IB_SMI_CLASS;
	trap_rpc.method = IB_MAD_METHOD_TRAP;
	trap_rpc.trid = mad_trid();
	trap_rpc.attr.id = NOTICE;
	trap_rpc.datasz = IB_SMP_DATA_SIZE;
	trap_rpc.dataoffs = IB_SMP_DATA_OFFS;

	memset(&notice, 0, sizeof(notice));
	notice.generic_type = 0x80 | IB_NOTICE_TYPE_INFO;
	notice.g_or_v.generic.prod_type_lsb = cl_hton16(IB_NODE_TYPE_CA);
	notice.g_or_v.generic.trap_num = cl_hton16(129);
	notice.issuer_lid = cl_hton16((uint16_t) selfportid.lid);
	notice.data_details.ntc_129_131.lid = cl_hton16((uint16_t) selfportid.lid);
	notice.data_details.ntc_129_131.pad = 0;
	notice.data_details.ntc_129_131.port_num = error_port;

	return (mad_send_via(&trap_rpc, &sm_port, NULL, &notice, srcport));
}

typedef struct _trap_def {
	char *trap_name;
	int (*send_func) (void);
} trap_def_t;

trap_def_t traps[3] = {
	{"node_desc_change", send_144_node_desc_update},
	{"local_link_integrity", send_129_local_link_integrity},
	{NULL, NULL}
};

int send_trap(char *trap_name)
{
	int i;

	for (i = 0; traps[i].trap_name; i++) {
		if (strcmp(traps[i].trap_name, trap_name) == 0) {
			return (traps[i].send_func());
		}
	}
	ibdiag_show_usage();
	return(1);
}

int main(int argc, char **argv)
{
	char usage_args[1024];
	int mgmt_classes[2] = { IB_SMI_CLASS, IB_SMI_DIRECT_CLASS };
	char *trap_name = NULL;
	int i, n, rc;

	n = sprintf(usage_args, "[<trap_name>] [<error_port>]\n"
		    "\nArgument <trap_name> can be one of the following:\n");
	for (i = 0; traps[i].trap_name; i++) {
		n += snprintf(usage_args + n, sizeof(usage_args) - n,
			      "  %s\n", traps[i].trap_name);
		if (n >= sizeof(usage_args))
			exit(-1);
	}
	snprintf(usage_args + n, sizeof(usage_args) - n,
		 "\n  default behavior is to send \"%s\"", traps[0].trap_name);

	ibdiag_process_opts(argc, argv, NULL, "DLG", NULL, NULL,
			    usage_args, NULL);

	argc -= optind;
	argv += optind;

	if (!argv[0]) {
		trap_name = traps[0].trap_name;
	} else {
		trap_name = argv[0];
	}

	if (argc > 1) {
		error_port = atoi(argv[1]);
	}

	madrpc_show_errors(1);

	srcport = mad_rpc_open_port(ibd_ca, ibd_ca_port, mgmt_classes, 2);
	if (!srcport)
		IBERROR("Failed to open '%s' port '%d'", ibd_ca, ibd_ca_port);

	rc = send_trap(trap_name);
	mad_rpc_close_port(srcport);
	return (rc);
}

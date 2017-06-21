#pragma once
//=====================================================================//
/*!	@file
	@brief	TCP/UDP config @n
			Copyright 2017 Kunihito Hiramatsu
	@author	平松邦仁 (hira@rvf-rc45.net)
*/
//=====================================================================//
#include "r_t4_itcpip.h"

#define MAX_TCP_CREP  8
#define MAX_TCP_CCEP  8
#define MAX_UDP_CCEP  4
#define TOTAL_BSD_SOCKET    (MAX_TCP_CCEP + MAX_UDP_CCEP)

extern const uint8_t _t4_channel_num;

extern T_TCP_CREP tcp_crep[MAX_TCP_CREP];

extern const uint16_t __tcprepn;

extern T_TCP_CCEP tcp_ccep[MAX_TCP_CCEP];

extern const uint16_t __tcpcepn;

extern const uint16_t _tcp_mss[2];

extern uint16_t _tcp_initial_seqno[2];

extern const uint16_t _tcp_2msl[2];

extern const uint16_t _tcp_2msl[2];

extern const uint16_t _tcp_rt_tmo_rst[2];

extern uint8_t _tcp_dack[2];

extern T_UDP_CCEP udp_ccep[MAX_UDP_CCEP];

extern const uint16_t __udpcepn;

extern const uint8_t __multi_TTL[2];

extern const uint8_t _udp_enable_zerochecksum[2];

extern const uint16_t _ip_tblcnt[2];

extern TCPUDP_ENV tcpudp_env[2];

extern uint8_t _myethaddr[2][6];

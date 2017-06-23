#include <ntddk.h>
#include <ntstrsafe.h>
#include <stdio.h>
#include <string.h>
#include "tdi_fw_lib.h"

#include "adfilter.h"


/*
	this file implement the code for filter dns message 
*/





#if DBG
char dump_buffer[2048 * 3];
void dump_hex(PUCHAR data, int len)
{
	memset(dump_buffer, 0, 2048 * 3);

	for (int j = 0; j <= len; j++)
		RtlStringCbPrintfA(dump_buffer + j * 3, 2048 * 3 - j * 3, "%02x ", data[j]);

	DbgPrint(dump_buffer);
}

void dump_char(PUCHAR data, int len)
{
	memset(dump_buffer, 0, 2048 * 3);
	for (int i = 0; i < len; i++)
	{
		if (isprint(data[i]))
			dump_buffer[i] = data[i];
		else
			dump_buffer[i] = '.';
	}
	DbgPrint(dump_buffer);
}

void dump_ip_and_port(SockAddr * from, SockAddr *to)
{
	DbgPrint("%d.%d.%d.%d:%d -> %d.%d.%d.%d:%d\n",
		from->sin_addr.S_un.S_un_b.s_b1,
		from->sin_addr.S_un.S_un_b.s_b2,
		from->sin_addr.S_un.S_un_b.s_b3,
		from->sin_addr.S_un.S_un_b.s_b4,
		ntohs(from->sin_port),
		to->sin_addr.S_un.S_un_b.s_b1,
		to->sin_addr.S_un.S_un_b.s_b2,
		to->sin_addr.S_un.S_un_b.s_b3,
		to->sin_addr.S_un.S_un_b.s_b4,
		ntohs(to->sin_port));

}
#else
#define dump_hex(d,l)
#define dump_char(d,l)
#define dump_ip_and_port(f,t)
#endif




// format a dns message to normal host string
// and return the pointer
char  dns_buffer[1024];
char* fmt_dns_msg(unsigned char *data, int len)
{
	memset(dns_buffer, 0, 1024);
	for (int i = 0; i < len; i++)
	{
		char c = data[i + 13];
		if (c == 0) break;
		if (c < '0') c = '.';

		dns_buffer[i] = c;
	}

	return dns_buffer;
}


// filter callback
// return true(1) for found , so packet should deny
// return false(0) then no operate
int filter(unsigned char *data, int len)
{
	char * dns = fmt_dns_msg(data, len);

	//dump_char(dns, strlen(dns));

	for (PSLIST_ENTRY h = g_adf.AdHost.list.Next.Next; // get the first entry
		h; // is the last entry
		h = h->Next) // go to next entry
	{
		char * name = CONTAINING_RECORD(h, HostList, list)->name;
		if (memcmp(dns, name, strlen(name)) == 0)
		{
			KdPrint(("[adf] ad host found : %s", name));
			return true;
		}
	}

	return false;
}




// only check dns message
static int g_need_check;

int tdifw_filter(struct flt_request *request)
{
	struct sockaddr_in* from = (struct sockaddr_in*)&request->addr.from;
	struct sockaddr_in* to = (struct sockaddr_in*)&request->addr.to;

	if (request->proto == IPPROTO_UDP)
	{
		//dump_ip_and_port(from, to);
		if (ntohs(to->sin_port) == 53) // is a dns packet
			g_need_check = 1;
		else
			g_need_check = 0;
	}

	return FILTER_ALLOW;
}



int tdifw_udp_send(PUCHAR data, int len)
{
	if (!g_need_check) return FILTER_ALLOW;

	if (g_adf.paused)  return FILTER_ALLOW;

	len--;
	//dump_hex(data, len);
	//dump_char(data, len);
	return filter(data, len) + 1;
}
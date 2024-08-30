#include "net_structures.h"
#include <IBinTools.h>
using namespace SourceMod;

void *g_pNET_SendToImpl = NULL;
ICallWrapper *g_callNET_SendToImpl = NULL;

int NET_SendToImpl( SOCKET s, const char * buf, int len, const struct sockaddr * to, int tolen, int iGameDataLength )
{
	struct {
		SOCKET s;
		const char *buf;
		int len;
		const struct sockaddr *to;
		int tolen;
		int iGameDataLength;
	} stack{s, buf, len, to, tolen, iGameDataLength};

	int ret;
	g_callNET_SendToImpl->Execute(&stack, &ret);
	return ret;
}

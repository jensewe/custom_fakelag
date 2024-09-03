#pragma once

#ifndef _WRAPPERS_H
#define _WRAPPERS_H

#include "net_structures.h"
#include <IBinTools.h>
using namespace SourceMod;

extern void *g_pNET_SendToImpl;
extern ICallWrapper *g_callNET_SendToImpl;
extern int NET_SendToImpl(SOCKET s, const char *buf, int len, const struct sockaddr *to, int tolen, int iGameDataLength);

#endif // _WRAPPERS_H
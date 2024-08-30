#include "extension.h"
#include "NET_LagPacket_Detour.h"
#include "net_ws_queued_packet_sender.h"
#include "PlayerLagManager.h"
#include "LagSystem.h"

// IForward* g_fwdLagPacket = NULL;
CDetour* DLagPacket = NULL;
CDetour* DSendTo = NULL;
CDetour* DQueuePacketForSend = NULL;
CDetour* DClearQueuedPacketsForChannel = NULL;

static const PlayerLagManager* s_LagManager;
static LagSystem* s_LagSystem;

float getLagPacketMs(const dumb_netadr_s & adr)
{
	return s_LagManager->GetPlayerLag(adr);
}

DETOUR_DECL_STATIC2(NET_LagPacket, bool, bool, newdata, _netpacket_t*, packet)
{

	// If "newdata" is true, packet points to a new packet of real data (which we could potentially lag)
	// If "newdata" is false, packet points to an empty, but initialized packet struct.
	// Returning true means we assert that "packet" is full of a consumable network packet.
	// Returning false means the consumer should ignore packet, there is no packet to consume yet.
	if (newdata) {
		const float lagTime = getLagPacketMs(packet->from);
		if (lagTime > 0.0) {
			// g_pSM->LogError(myself, "Lagging packet on socket %d for %fms", packet->source, lagTime / 2.0);
			s_LagSystem->LagPacket(packet, lagTime / 2.0);
		}
		else
		{
			// TODO: Flush lagged packets if the lag time is reduced or removed?
			return true;
		}
	}

	return s_LagSystem->GetNextPacket(packet->source, packet);
}

void SockAddrToNetAdr(const struct sockaddr *s, dumb_netadr_s *a);

#if defined __linux__
DETOUR_DECL_STATIC3(NET_SendTo, int, const sockaddr *, to, int, tolen, int, iGameDataLength)
#else
DETOUR_DECL_STATIC7(NET_SendTo, int, bool, verbose, SOCKET, soc, const char *, buf, int, len, const sockaddr *, to, int, tolen, int, iGameDataLength)
#endif
{
#if defined __linux__
	SOCKET soc;
	const char *buf;
	int len;
	asm volatile(
		"movl %%eax, %0;"
		"movl %%edx, %1;"
		"movl %%ecx, %2;"
		: "=m"(soc),
		  "=m"(buf),
		  "=m"(len)
	);
#endif

	if (g_pLagPackedSender->IsRunning())
	{
		dumb_netadr_s adr;
		SockAddrToNetAdr(to, &adr);

		const float lagTime = getLagPacketMs(adr);
		if (lagTime > 0.0)
		{
			// Warning( "NET_SendTo: Lagging packet on socket %d for %fms (%d bytes)\n", soc, lagTime, len);
			g_pLagPackedSender->QueuePacket(/*g_pLagChan*/NULL, soc, buf, len, to, sizeof(sockaddr), (uint32)(lagTime / 2.0));
			return len;
		}
	}

#if defined __linux__
	asm volatile(
		"movl %0, %%eax;"
		"movl %1, %%edx;"
		"movl %2, %%ecx;"
		: : "m"(soc),
			"m"(buf),
			"m"(len)
		: "%eax", "%edx", "%ecx");
	return NET_SendTo_Actual(to, tolen, iGameDataLength);
#else
	return NET_SendTo_Actual(verbose, soc, buf, len, to, tolen, iGameDataLength);
#endif
}

#if defined __linux__
DETOUR_DECL_STATIC3(NET_QueuePacketForSend, int, int, len, const sockaddr *, to, uint32, msecDelay)
#else
DETOUR_DECL_STATIC8(NET_QueuePacketForSend, int, CNetChan *, chan, bool, verbose, SOCKET, soc, const char *, buf, int, len, const sockaddr *, to, int, tolen, uint32, msecDelay)
#endif
{
#if defined __linux__
	CNetChan *chan;
	SOCKET soc;
	const char *buf;
	asm volatile(
		"movl %%eax, %0;"
		"movl %%edx, %1;"
		"movl %%ecx, %2;"
		: "=m"(chan),
		  "=m"(soc),
		  "=m"(buf)
	);
#endif

	// if (g_pLagPackedSender->IsRunning())
	// {
	// 	dumb_netadr_s adr;
	// 	SockAddrToNetAdr(to, &adr);

	// 	const float lagTime = getLagPacketMs(adr);
	// 	if (lagTime > 0.0)
	// 	{
	// 		// Warning( "NET_QueuePacketForSend: Lagging packet on socket %d for %fms (%d bytes)\n", soc, lagTime, len);
	// 		g_pLagPackedSender->QueuePacket(/*g_pLagChan*/NULL, soc, buf, len, to, sizeof(sockaddr), (uint32)(lagTime / 2.0));
	// 		return len;
	// 	}
	// }

	dumb_netadr_s adr;
	SockAddrToNetAdr(to, &adr);

	const float lagTime = getLagPacketMs(adr);
	if (lagTime > 0.0)
	{
		// Warning( "NET_QueuePacketForSend: Lagging packet on socket %d for %fms (%d bytes)\n", soc, lagTime, len);
		msecDelay += (uint32)(lagTime / 2.0);
	}

#if defined __linux__
	asm volatile(
		"movl %0, %%eax;"
		"movl %1, %%edx;"
		"movl %2, %%ecx;"
		: : "m"(chan),
			"m"(soc),
			"m"(buf)
		: "%eax", "%edx", "%ecx");
	return NET_QueuePacketForSend_Actual(len, to, msecDelay);
#else
	return NET_QueuePacketForSend_Actual(chan, verbose, soc, buf, len, to, tolen, msecDelay);
#endif
}

DETOUR_DECL_STATIC1(NET_ClearQueuedPacketsForChannel, void, INetChannel *, chan)
{
	g_pLagPackedSender->ClearQueuedPacketsForChannel(chan);
	return NET_ClearQueuedPacketsForChannel_Actual(chan);
}

bool CreateNetLagPacketDetour()
{
	DLagPacket = DETOUR_CREATE_STATIC(NET_LagPacket, "NET_LagPacket");
	if (DLagPacket == NULL)
	{
		g_pSM->LogError(myself, "NET_LagPacket detour could not be initialized - FeelsBadMan.");
		return false;
	}
	DLagPacket->EnableDetour();

	DSendTo = DETOUR_CREATE_STATIC(NET_SendTo, "NET_SendTo");
	if (DSendTo == NULL)
	{
		g_pSM->LogError(myself, "NET_SendTo detour could not be initialized - FeelsBadMan.");
		return false;
	}
	DSendTo->EnableDetour();

	DQueuePacketForSend = DETOUR_CREATE_STATIC(NET_QueuePacketForSend, "NET_QueuePacketForSend");
	if (DQueuePacketForSend == NULL)
	{
		g_pSM->LogError(myself, "NET_QueuePacketForSend detour could not be initialized - FeelsBadMan.");
		return false;
	}
	DQueuePacketForSend->EnableDetour();

	DClearQueuedPacketsForChannel = DETOUR_CREATE_STATIC(NET_ClearQueuedPacketsForChannel, "NET_ClearQueuedPacketsForChannel");
	if (DClearQueuedPacketsForChannel == NULL)
	{
		g_pSM->LogError(myself, "NET_ClearQueuedPacketsForChannel detour could not be initialized - FeelsBadMan.");
		return false;
	}
	DClearQueuedPacketsForChannel->EnableDetour();

	return true;
}

void RemoveNetLagPacketDetour()
{
	if (DLagPacket != NULL)
	{
		DLagPacket->Destroy();
		DLagPacket = NULL;
	}
}


// lagManager: A Lag Manager instance to look up player lag times
// pNetTime: Pointer to the engine "net_time" variable
bool LagDetour_Init(const PlayerLagManager* lagManager, const double* pNetTime)
{
	s_LagManager = lagManager;
	s_LagSystem = new LagSystem(pNetTime);
	bool detoured = CreateNetLagPacketDetour();
	if (!detoured) {
		LagDetour_Shutdown();
	}
	return detoured;
}

void LagDetour_Shutdown() {
	RemoveNetLagPacketDetour();
	if (s_LagSystem != NULL)
	{
		delete s_LagSystem;
	}
	s_LagManager = NULL;
}

void SockAddrToNetAdr( const struct sockaddr *s, dumb_netadr_s *a )
{
	if (s->sa_family == AF_INET)
	{
		a->type = NA_IP;
		*(int *)&a->ip = ((struct sockaddr_in *)s)->sin_addr.s_addr;
		a->port = ((struct sockaddr_in *)s)->sin_port;
	}
}


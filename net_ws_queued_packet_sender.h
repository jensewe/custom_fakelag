//====== Copyright � 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef NET_WS_QUEUED_PACKET_SENDER_H
#define NET_WS_QUEUED_PACKET_SENDER_H
#ifdef _WIN32
#pragma once
#endif

// Used to match against certain debug values of cvars.
#define NET_QUEUED_PACKET_THREAD_DEBUG_VALUE 581304 
#define NET_QUEUED_PACKET_THREAD_SEND_PACKET 0xFEEDBEEF

class INetChannel;

class IQueuedPacketSender
{
public:
	virtual bool Setup() = 0;
	virtual void Shutdown() = 0;
	virtual bool IsRunning() = 0;
	virtual void ClearQueuedPacketsForChannel( INetChannel *pChan ) =  0;
	virtual void QueuePacket( INetChannel *pChan, SOCKET s, const char *buf, int len, const struct sockaddr * to, int tolen, uint32 msecDelay ) = 0;
};

extern IQueuedPacketSender *g_pLagPackedSender;

#endif // NET_WS_QUEUED_PACKET_SENDER_H

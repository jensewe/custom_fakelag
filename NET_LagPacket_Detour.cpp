#include "extension.h"
#include "NET_LagPacket_Detour.h"
#include "PlayerLagManager.h"
#include "LagSystem.h"

// IForward* g_fwdLagPacket = NULL;
CDetour* DLagPacket = NULL;
CDetour* DSendPacket = NULL;

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
		const float lagTime = getLagPacketMs(&packet->from);
		if (lagTime > 0.0) {
			// g_pSM->LogError(myself, "Lagging packet on socket %d for %fms", packet->source, lagTime);
			s_LagSystem->LagPacket(packet, lagTime);
		}
		else
		{
			// TODO: Flush lagged packets if the lag time is reduced or removed?
			return true;
		}
	}

	return s_LagSystem->GetNextPacket(packet->source, packet);
}

DETOUR_DECL_STATIC8(NET_SendPacket, int, INetChannel *, chan, int, sock,  const dumb_netadr_s &, to, const unsigned char *, data, int, length, bf_write *, pVoicePayload /* = NULL */, bool, bUseCompression /*=false*/, uint32, unMillisecondsDelay /*=0u*/)
{
	const float lagTime = getLagPacketMs(to);
	if (lagTime > 0.0) {
		// g_pSM->LogError(myself, "Lagging packet on socket %d for %fms", sock, lagTime);
		unMillisecondsDelay += (uint32)lagTime;
		return NET_SendPacket_Actual(chan, sock, to, data, length, pVoicePayload, bUseCompression, unMillisecondsDelay);
	}
	return NET_SendPacket_Actual(chan, sock, to, data, length, pVoicePayload, bUseCompression, unMillisecondsDelay);
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

	DSendPacket = DETOUR_CREATE_STATIC(NET_SendPacket, "NET_SendPacket");
	if (DSendPacket == NULL)
	{
		g_pSM->LogError(myself, "NET_SendPacket detour could not be initialized - FeelsBadMan.");
		return false;
	}
	DSendPacket->EnableDetour();

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
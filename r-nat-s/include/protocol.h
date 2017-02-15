#pragma once

#include <stdint.h>

#pragma pack(push,1)

namespace R_NAT
{
	struct  base_header
	{
		uint32_t cbsize; // the packet size do not include itself
	};

	// agent to relay
	namespace A2R
	{
		enum
		{
			BIND,
			UNBIND,
			DATA,
			DISCONNECT,
			SESSION, // this is used by a session connection
		};
		// ask the connection server to listen at certain port
		struct bind
		{
			uint8_t cmd; // command
			uint16_t port; // a port number wanted
		};
		// ask the connection server to remove a listen port(auto remove when the agent disconnects from the server)
		struct unbind
		{
			uint8_t cmd; // command
			uint16_t port;
		};
		// carry a server's data
		struct data
		{
			uint8_t cmd; // command
			uint16_t port;
			uint32_t id; // the client id, it's based on the sequence on the connection server
		};
		// ask the connection server to disconnect a client
		struct disconnect
		{
			uint8_t cmd; // command
			uint16_t port;
			uint32_t id; // the client id, it's based on the sequence on the connection server
		};
		struct session
		{
			uint8_t cmd; // command
			uint16_t port;
			uint32_t id; // the client id, it's based on the sequence on the connection server
		};
	}
	// relay to agent
	namespace R2A
	{
		enum
		{
			BIND,
			UNBIND,
			DATA,
			CONNECT,
			DISCONNECT,
		};
		struct bind
		{
			uint8_t cmd; // command
			uint16_t port; // it is the same as the number wanted
			uint32_t error_code; // error code
		};
		struct unbind
		{
			uint8_t cmd; // command
			uint16_t port;
			uint32_t error_code; // error code
		};
		// carry a client's data
		struct data
		{
			uint8_t cmd; // command
			uint16_t port;
			uint32_t id; // the client id, it's based on the sequence on the connection server
			// data follows after the base header
		};
		// notify a client connected
		struct connect
		{
			uint8_t cmd; // command
			uint16_t port;
			uint32_t id; // the client id, it's based on the sequence on the connection server
			uint32_t usr_ip;
			uint16_t usr_port;
		};
		// notify a client disconnected
		struct disconnect
		{
			uint8_t cmd; // command
			uint16_t port;
			uint32_t id; // the client id, it's based on the sequence on the connection server
		};
	}
}

#pragma pack(pop)

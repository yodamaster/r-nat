// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <stdint.h>
#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#ifndef WIN32
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

#include <functional>
#include <memory>
#include <thread>
#include <mutex>
using namespace std::placeholders;

#define ASIO_STANDALONE
#define ASIO_HAS_STD_CHRONO
#include <asio.hpp>
#include <asio/steady_timer.hpp>

typedef std::string path_t;

#include <common/singleton.h>
#include <liblogger/liblogger.h>

#define HAS_RAPIDXML_HEADERS
#include <common/xmlserializer.h>

#include <protocol.h>

#define QUEUE_LIMIT 10


inline unsigned int fnv_hash_int(int v)
{
	static const unsigned int InitialFNV = 2166136261U;
	static const unsigned int FNVMultiple = 16777619;
	unsigned int hash = InitialFNV;
	for(char *p = (char*)&v, *end = (char*)&v + sizeof(int); p != end; p++)
	{
		hash = hash ^ (*p) * FNVMultiple;
	}
	return hash;
}

template<typename T>
T trim(const T& str)
{
	auto first = str.find_first_not_of(' ');
	auto last = str.find_last_not_of(' ');
	return str.substr(first, (last - first + 1));
}
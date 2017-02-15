#include "stdafx.h"
#include "config.h"

bool ConfigManager::init(const path_t& configfile)
{
	if (!FromFile(configfile.c_str()))
	{
		std::cerr << "unable to load config file" << std::endl;
		return false;
	}
	if (local_.locals_.size() == 0)
	{
		std::cerr << "missing local node configuration" << std::endl;
		return false;
	}
	return true;
}
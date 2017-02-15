#pragma once

struct AddressInfo 
{
	std::string address_;
	std::string port_;
	std::string service_port_; // used by service node only
	AddressInfo& operator=(const AddressInfo& src)
	{
		address_ = src.address_;
		port_ = src.port_;
		service_port_ = src.service_port_;
		return *this;
	}
	AddressInfo(const AddressInfo& src)
	{
		*this = src;
	}
	AddressInfo()
	{

	}
	XML_CONFIG_BEGIN();
		XML_CONFIG_ITEM("address",address_);
		XML_CONFIG_ITEM("port",port_);
		XML_CONFIG_ITEM("service_port", service_port_);
	XML_CONFIG_END();
};

struct Config
{
	struct  
	{
		std::vector<AddressInfo> locals_;
		XML_CONFIG_BEGIN();
			XML_CONFIG_NODE_GROUP("node",locals_);
		XML_CONFIG_END();
	}local_;
	struct  
	{
		std::vector<AddressInfo> remotes_;
		uint32_t concurrency_ = 1; // the concurrent connection to the logic servers
		std::string loadpolicy_;
		XML_CONFIG_BEGIN();
			XML_CONFIG_ITEM("concurrency",concurrency_);
			XML_CONFIG_NODE_GROUP("node",remotes_);
		XML_CONFIG_END();
	}remote_;
	struct  
	{
		uint32_t size_max_ = 1024 * 110;
		uint32_t recv_buf_size_ = 4096; // use page size
		XML_CONFIG_BEGIN();
			XML_CONFIG_ITEM("max",size_max_);
			XML_CONFIG_ITEM("recv_buf_size",recv_buf_size_);
		XML_CONFIG_END();
	}packet_;
	XML_CONFIG_ROOT_BEGIN("config");
		XML_CONFIG_NODE("service",local_);
		XML_CONFIG_NODE("remote",remote_);
		XML_CONFIG_NODE("packet",packet_);
	XML_CONFIG_ROOT_END();
};

class ConfigManager
	: public Config
	, public CSingleTon_t<ConfigManager>
{
public:
	bool init(const path_t& configfile);
};

#define getConfig() ConfigManager::GetInstance()
#pragma once

struct AddressInfo
{
	std::string address_;
	std::string port_;
	std::string service_port_; // used by service node only
	uint32_t concurrency_ = 1; // used by remote node only
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
	XML_CONFIG_ITEM("address", address_);
	XML_CONFIG_ITEM("port", port_);
	XML_CONFIG_ITEM("service_port", service_port_);
	XML_CONFIG_ITEM("concurrency", concurrency_);
	XML_CONFIG_END();
};

struct Config
{
	enum CONFIG_LOAD_POLICY
	{
		CONFIG_LOAD_POLICY_IP,
		CONFIG_LOAD_POLICY_SEQ
	};
	struct
	{
		std::string load_policy_  = "IP"; // can be IP or seq
		uint32_t recv_buf_size_{ 4096 }; // use page size
		uint32_t max_packet_size_{ 1024 * 110 };
		uint32_t queue_limit_{ 1 };
		uint32_t tcp_send_no_delay_{ 1 };
		uint32_t tcp_connect_timeout_{ 30 };
		XML_CONFIG_BEGIN();
		XML_CONFIG_ITEM("load_policy", load_policy_);
		XML_CONFIG_ITEM("max_packet_size", max_packet_size_);
		XML_CONFIG_ITEM("recv_buf_size", recv_buf_size_);
		XML_CONFIG_ITEM("queue_limit", queue_limit_);
		XML_CONFIG_ITEM("tcp_send_no_delay", tcp_send_no_delay_);
		XML_CONFIG_ITEM("tcp_connect_timeout", tcp_connect_timeout_);
		XML_CONFIG_END();
		auto LoadPolicy() -> int
		{
			if (!_stricmp(load_policy_.c_str(), "SEQ"))
			{
				return CONFIG_LOAD_POLICY_SEQ;
			}
			return CONFIG_LOAD_POLICY_IP;
		}
	}system_;
	struct
	{
		std::vector<AddressInfo> locals_;
		XML_CONFIG_BEGIN();
		XML_CONFIG_NODE_GROUP("node", locals_);
		XML_CONFIG_END();
	}local_;
	struct
	{
		std::vector<AddressInfo> remotes_;
		XML_CONFIG_BEGIN();
		XML_CONFIG_NODE_GROUP("node", remotes_);
		XML_CONFIG_END();
	}remote_;
	XML_CONFIG_ROOT_BEGIN("config");
	XML_CONFIG_NODE("system", system_);
	XML_CONFIG_NODE("service", local_);
	XML_CONFIG_NODE("remote", remote_);
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
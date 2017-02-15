#include "stdafx.h"
#include "app1.h"

#include "../server.h"

#include "../config.h"

AppLogic1::AppLogic1(Server* server):server_(server)
{
}

AppLogic1::~AppLogic1()
{
}

bool AppLogic1::Init()
{
	asio::error_code ignored_ec;
	asio::ip::tcp::resolver resolver(server_->main_ioservice_);

	// process listen addresses
	{
		auto&& locals = getConfig()->local_.locals_;
		for (auto&& local: locals)
		{
			auto address = trim(local.address_);
			auto port = local.port_;

			asio::ip::tcp::resolver::query query(address, port);
			for (auto iter=resolver.resolve(query,ignored_ec);
				iter!=asio::ip::tcp::resolver::iterator();iter++)
			{
				asio::ip::tcp::endpoint endpoint = *iter;
				std::cout << "Local: " << address << ":" << port << " -> " << endpoint.address().to_string() << ":" << port << std::endl; 
				listenep_.push_back(endpoint);
			}
		}
		if (listenep_.size()==0) return false;
	}
	
	auto loadpolicy = getConfig()->remote_.loadpolicy_;
	if (!stricmp(loadpolicy.c_str(),"IP"))
	{
		load_policy_ = POLICY_IP;
	}
	else if (!stricmp(loadpolicy.c_str(),"SEQ"))
	{
		load_policy_ = POLICY_SEQ;
	}
	std::cout << "LoadPolicy: " << load_policy_ << std::endl; 

	return true;
}

void AppLogic1::Start()
{
	agent_tcpserver_ = std::make_shared<TcpServer>(server_->main_ioservice_);
	agent_tcpserver_->SetMaxPacketLength(getConfig()->packet_.size_max_);
//	tcpserver_->SetNoDelay(true);
	agent_tcpserver_->SetRecvbufSize(getConfig()->packet_.recv_buf_size_);
	agent_tcpserver_->on_recv = std::bind(&AppLogic1::__OnAgentRecv, this, _1, _2);
	agent_tcpserver_->on_connect = std::bind(&AppLogic1::__OnAgentConnect, this, _1, _2);
	agent_tcpserver_->on_disconnect = std::bind(&AppLogic1::__OnAgentDisconnect, this, _1, _2);

	agent_tcpserver_->Start(listenep_);
}

void AppLogic1::Stop()
{
	agent_tcpserver_->Close();
	for (auto&& service : services_)
	{
		service.second->service_tcpserver_->Close();
	}
	services_.clear();
	agents_.clear();
}

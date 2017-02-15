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
			auto service_port = (uint16_t)atol(local.service_port_.c_str());

			asio::ip::tcp::resolver::query query(address, port);
			for (auto iter=resolver.resolve(query,ignored_ec);
				iter!=asio::ip::tcp::resolver::iterator();iter++)
			{
				asio::ip::tcp::endpoint endpoint = *iter;
				std::cout << "Local: " << address << ":" << port << " -> " << endpoint.address().to_string() << ":" << port << " service at " << service_port << std::endl; 
				service_ep_[service_port].insert(endpoint);
			}
		}
		if (service_ep_.size()==0) return false;
	}

	// process remote addresses
	auto&& remotes = getConfig()->remote_.remotes_;
	for (auto&& remote : remotes)
	{
		auto address = trim(remote.address_);
		auto port = remote.port_;

		asio::ip::tcp::resolver::query query(address, port);
		for (auto iter=resolver.resolve(query,ignored_ec);
			iter!=asio::ip::tcp::resolver::iterator();iter++)
		{
			asio::ip::tcp::endpoint endpoint = *iter;
			std::cout << "Remote: " << address << ":" << port << " -> " << endpoint.address().to_string() << ":" << port << std::endl; 
			remote_ep_.push_back(endpoint);
		}
	}
	if (remote_ep_.size()==0) return false;

	return true;
}

void AppLogic1::Start()
{
	uint32_t max_connections = getConfig()->remote_.concurrency_;
	for (auto&& ep : remote_ep_)
	{
		for (uint32_t i = 0; i < max_connections;i++)
		{
			auto remoteinfo = std::make_shared<RemoteInfo>();
			remoteinfo->ep_ = ep;
			remoteinfo->tcpclient_ = std::make_shared<TcpClient>(server_->main_ioservice_);
			remoteinfo->tcpclient_->SetMaxPacketLength(getConfig()->packet_.size_max_);
//			remoteinfo->tcpclient_->SetNoDelay(true);
			remoteinfo->tcpclient_->SetRecvbufSize(getConfig()->packet_.recv_buf_size_);
			remoteinfo->tcpclient_->on_connect = std::bind(&AppLogic1::__OnServerConnect, this, remoteinfo);
			remoteinfo->tcpclient_->on_disconnect = std::bind(&AppLogic1::__OnServerDisconnect, this, _1, remoteinfo);
			remoteinfo->tcpclient_->on_recv = std::bind(&AppLogic1::__OnServerRecv, this, _1, remoteinfo);
			relays_.push_back(remoteinfo);
		}
	}

	for (auto&& remoteinfo : relays_)
	{
		remoteinfo->tcpclient_->Start(remoteinfo->ep_);
	}
}

void AppLogic1::Stop()
{
	for (auto&& reply : relays_)
	{
		reply->tcpclient_->Close();
	}
	relays_.clear();
}

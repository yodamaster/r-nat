#include "stdafx.h"
#include "app1.h"

#include "../server.h"

#include "../config.h"

AppLogic1::AppLogic1(Server* server)
	: server_(server)
	, strand_(server->main_ioservice_)
{
}

AppLogic1::~AppLogic1()
{
}

bool AppLogic1::Init()
{
	load_policy_ = getConfig()->system_.LoadPolicy();
	queue_limit_ = getConfig()->system_.queue_limit_;

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
				std::cout << "Local: " << address << ":" << port << " -> " << endpoint.address().to_string() << ":" << port << ", service port " << service_port << std::endl; 
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
		auto concurrency = remote.concurrency_;

		asio::ip::tcp::resolver::query query(address, port);
		for (auto iter=resolver.resolve(query,ignored_ec);
			iter!=asio::ip::tcp::resolver::iterator();iter++)
		{
			asio::ip::tcp::endpoint endpoint = *iter;
			std::cout << "Remote: " << address << ":" << port << " -> " << endpoint.address().to_string() << ":" << port << std::endl; 
			for (int i = 0; i < concurrency;i++)
			{
				remote_ep_.push_back(endpoint);
			}
		}
	}
	if (remote_ep_.size()==0) return false;

	return true;
}

void AppLogic1::Start()
{
	enable_buffer_pooling_ = true;
	for (auto&& ep : remote_ep_)
	{
		auto remoteinfo = std::make_shared<RemoteInfo>();
		remoteinfo->ep_ = ep;
		remoteinfo->tcpclient_ = __CreateTunnelConn(remoteinfo);

		relays_.push_back(remoteinfo);
	}

	for (auto&& remoteinfo : relays_)
	{
		remoteinfo->tcpclient_->Start(remoteinfo->ep_);
	}
}

void AppLogic1::Stop()
{
	enable_buffer_pooling_ = false;
	for (auto&& reply : relays_)
	{
		reply->tcpclient_->Close();
	}
	relays_.clear();
}

auto AppLogic1::__CreateTcpClient()->TcpClient_ptr
{
	auto tcpclient = std::make_shared<TcpClient>(server_->GetIoService());
	tcpclient->SetMaxPacketLength(getConfig()->system_.max_packet_size_);
	tcpclient->SetNoDelay(getConfig()->system_.tcp_send_no_delay_);
	tcpclient->SetRecvbufSize(getConfig()->system_.recv_buf_size_);
	tcpclient->SetConnectTimeout(getConfig()->system_.tcp_connect_timeout_);
	tcpclient->on_allocbuf = std::bind(&AppLogic1::__CreateIoBuffer, this);
	return tcpclient;
}

auto AppLogic1::__CreateRawTcpClient()->RawTcpClient_ptr
{
	auto tcpclient = std::make_shared<RawTcpClient>(server_->GetIoService());
	tcpclient->SetMaxPacketLength(getConfig()->system_.max_packet_size_);
	tcpclient->SetNoDelay(getConfig()->system_.tcp_send_no_delay_);
	tcpclient->SetRecvbufSize(getConfig()->system_.recv_buf_size_);
	tcpclient->SetConnectTimeout(getConfig()->system_.tcp_connect_timeout_);
	tcpclient->on_allocbuf = std::bind(&AppLogic1::__CreateIoBuffer, this);
	return tcpclient;
}

auto AppLogic1::__CreateTunnelConn(RemoteInfo_ptr remoteinfo)->TcpClient_ptr
{
	auto tcpclient = __CreateTcpClient();
	tcpclient->on_connect = strand_.wrap(std::bind(&AppLogic1::__OnServerConnect, this, remoteinfo));
	tcpclient->on_disconnect = strand_.wrap(std::bind(&AppLogic1::__OnServerDisconnect, this, _1, remoteinfo));
	tcpclient->on_recv = strand_.wrap(std::bind(&AppLogic1::__OnServerRecv, this, _1, remoteinfo));
	return tcpclient;
}

auto AppLogic1::__CreateSessionTunnelConn(RemoteInfo_ptr remoteinfo, Session_ptr session, User usr)->TcpClient_ptr
{
	auto tcpclient = __CreateTcpClient();
	tcpclient->on_connect = strand_.wrap(std::bind(&AppLogic1::__OnSessionConnect, this, remoteinfo, session, usr));
	tcpclient->on_disconnect = strand_.wrap(std::bind(&AppLogic1::__OnSessionDisconnect, this, _1, remoteinfo, session, usr));
	tcpclient->on_recv = strand_.wrap(std::bind(&AppLogic1::__OnSessionRecv, this, _1, remoteinfo, session, usr));
	return tcpclient;
}

auto AppLogic1::__CreateUserConn(RemoteInfo_ptr remoteinfo, Session_ptr session, User usr, const asio::ip::tcp::endpoint& service_ep)->RawTcpClient_ptr
{
	auto tcpclient = __CreateRawTcpClient();
	tcpclient->on_connect = strand_.wrap(std::bind(&AppLogic1::__OnUserConnect, this, remoteinfo, session, usr, service_ep));
	tcpclient->on_disconnect = strand_.wrap(std::bind(&AppLogic1::__OnUserDisconnect, this, _1, remoteinfo, session, usr, service_ep));
	tcpclient->on_recv = strand_.wrap(std::bind(&AppLogic1::__OnUserRecv, this, _1, remoteinfo, session, usr));
	return tcpclient;
}

auto AppLogic1::__CreateIoBuffer()->std::shared_ptr<asio::streambuf>
{
	std::lock_guard<decltype(buff_pool_lock_)> l(buff_pool_lock_);
	asio::streambuf* p = nullptr;
	if (buffer_pool_.size())
	{
		p = buffer_pool_.front();
		buffer_pool_.pop_front();
	}
	else
	{
		p = new asio::streambuf;
	}
	return std::shared_ptr<asio::streambuf>(p, [this](asio::streambuf* p){
		std::lock_guard<decltype(buff_pool_lock_)> l(buff_pool_lock_);
		if (enable_buffer_pooling_)
		{
			p->consume(p->size());
			buffer_pool_.push_back(p);
		}
		else
		{
			delete p;
		}
	});
}

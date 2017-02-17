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

	return true;
}

void AppLogic1::Start()
{
	enable_buffer_pooling_ = true;
	agent_tcpserver_ = std::make_shared<TcpServer>(server_->main_ioservice_,server_->ioservices_);
	agent_tcpserver_->SetMaxPacketLength(getConfig()->system_.max_packet_size_);
	agent_tcpserver_->SetNoDelay(getConfig()->system_.tcp_send_no_delay_);
	agent_tcpserver_->SetRecvbufSize(getConfig()->system_.recv_buf_size_);
	agent_tcpserver_->on_allocbuf = std::bind(&AppLogic1::__CreateIoBuffer,this);
	agent_tcpserver_->on_recv = strand_.wrap(std::bind(&AppLogic1::__OnAgentRecv, this, _1, _2));
	agent_tcpserver_->on_connect = strand_.wrap(std::bind(&AppLogic1::__OnAgentConnect, this, _1, _2));
	agent_tcpserver_->on_disconnect = strand_.wrap(std::bind(&AppLogic1::__OnAgentDisconnect, this, _1, _2));

	agent_tcpserver_->Start(listenep_);
}

void AppLogic1::Stop()
{
	enable_buffer_pooling_ = false;
	agent_tcpserver_->Close();
	for (auto&& service : services_)
	{
		service.second->service_tcpserver_->Close();
	}
	services_.clear();
	agents_.clear();
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

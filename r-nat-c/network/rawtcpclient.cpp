#include "stdafx.h"
#include "rawtcpclient.hpp"

RawTcpClient::~RawTcpClient()
{
	if (impl_)
	{
		impl_->Close();
	}

	impl_ = nullptr;
}

auto RawTcpClient::Start(asio::ip::tcp::endpoint ep) -> void
{
	impl_ = std::make_shared<RawClientTCPConnection>(io_service_);
	impl_->on_allocbuf = [this]
	{
		if (on_allocbuf)
			return on_allocbuf();
		return std::make_shared<asio::streambuf>();
	};
	impl_->on_connect = [this]
	{
		if (on_connect)
			on_connect(); // notify a new connection
	};
	impl_->on_disconnect = [this](const asio::error_code& e)
	{
		if (on_disconnect)
			on_disconnect(e); // notify a connection gone
	};
	impl_->on_recv = [this](std::shared_ptr<asio::streambuf> buf)
	{
		if (on_recv)
			on_recv(buf);
	};

	impl_->SetMaxPacketLength(max_packet_length_);
	impl_->SetRecvbufSize(recv_buf_length_);
	impl_->SetConnectTimeout(connect_timeout_);

	impl_->Start(ep);
}

void RawTcpClient::Start(std::string domain, uint16_t port)
{
	Start(domain,std::to_string(port));
}

void RawTcpClient::Start(std::string domain, std::string port)
{
	std::vector<asio::ip::tcp::endpoint> endpoints;
	asio::error_code e;
	asio::ip::tcp::resolver resolver(io_service_);
	asio::ip::tcp::resolver::query query(domain, port);
	for (asio::ip::tcp::resolver::iterator it=resolver.resolve(query,e);
		it!=asio::ip::tcp::resolver::iterator();it++)
	{
		endpoints.push_back(*it);
	}
	if (endpoints.size())
	{
		srand((unsigned int)time(0));
		Start(endpoints[rand() % endpoints.size()]);
	}
	else
	{
		// post a failure
		io_service_.post([this]{
			if (on_disconnect)
				on_disconnect(asio::error::no_such_device);
		});
	}
}

void RawTcpClient::Start(uint32_t ip, uint16_t port)
{
	Start(asio::ip::tcp::endpoint(asio::ip::address_v4(ip), port));
}

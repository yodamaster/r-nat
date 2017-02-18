#pragma once

#include "tcpconnection.hpp"

class ClientTCPConnection
	: public TCPConnection<asio::ip::tcp::socket>
{
	using BaseConnection = TCPConnection<asio::ip::tcp::socket>;
protected:
	std::shared_ptr<asio::steady_timer> timer_;

public:
	std::function<void()> on_connect = nullptr;

	enum
	{
		DEFAULT_CONNECT_TIMEOUT = 30, // unit: s
	};
	uint32_t connect_timeout_{ DEFAULT_CONNECT_TIMEOUT };

public:
	ClientTCPConnection(asio::io_service& io_service)
		: BaseConnection(io_service, std::make_shared<asio::ip::tcp::socket>(io_service))
	{
	}

	auto SetConnectTimeout(uint32_t l) -> void
	{
		if (l)
			connect_timeout_ = l;
		else
			connect_timeout_ = DEFAULT_CONNECT_TIMEOUT;
	}

	auto Start(asio::ip::tcp::endpoint ep) -> void
	{
		sending_pkt_ = on_allocbuf();
		
		// create timer first because async_connect may happen quickly
		timer_ = std::make_shared<asio::steady_timer>(io_service_, std::chrono::seconds(connect_timeout_));
		timer_->async_wait(
			std::bind(&ClientTCPConnection::__OnConnectTimeout, 
			std::dynamic_pointer_cast<ClientTCPConnection>(this->shared_from_this()),
			_1));

		socket_->async_connect(ep,
			std::bind(&ClientTCPConnection::__OnConnect, 
			std::dynamic_pointer_cast<ClientTCPConnection>(this->shared_from_this()),
			_1));
	}

protected:
	auto __OnConnect(const asio::error_code& e) -> void
	{
		if (nullptr != timer_)
		{
			asio::error_code ignored_ec;
			timer_->cancel(ignored_ec);
			timer_ = nullptr;
		}

		if (e)
		{
			__Shutdown(e);
			return;
		}
		sending_pkt_ = nullptr; // it's used for connect flag

		// set option
		__SetOpt();
		__DoRecv();
		__DoSend();

		if (on_connect) on_connect();
	}

	auto __OnConnectTimeout(const asio::error_code& e) -> void
	{
		if (e == asio::error::operation_aborted)
			return; // cancelled
		__Shutdown(e);
	}
};


class TcpClient
	: public std::enable_shared_from_this<TcpClient>
{
public:
	std::function<std::shared_ptr<asio::streambuf>(void)> on_allocbuf = nullptr;
	std::function<void()> on_connect = nullptr;
	std::function<void(const asio::error_code& e)> on_disconnect = nullptr;
	std::function<void(std::shared_ptr<asio::streambuf> /*buf*/)> on_recv = nullptr;

public:
	TcpClient(asio::io_service& io_service) : io_service_(io_service){}
	~TcpClient();

	auto Start(asio::ip::tcp::endpoint ep) -> void;
	auto Start(std::string domain, std::string port) -> void;
	auto Start(std::string domain, uint16_t port) -> void;
	auto Start(uint32_t ip, uint16_t port) -> void;
	auto Send(std::shared_ptr<asio::streambuf> data, std::function<void(void)> pfn = nullptr) -> void
	{
		impl_->Send(data, pfn);
	}
	auto SendV(std::shared_ptr<std::vector<std::shared_ptr<asio::streambuf>>> data, std::function<void(void)> pfn = nullptr) -> void
	{
		impl_->SendV(data, pfn);
	}
	auto SetMaxPacketLength(uint32_t l) -> void
	{
		max_packet_length_ = l;
		if (impl_)
			impl_->SetMaxPacketLength(l);
	}
	auto SetNoDelay(int nodelay) -> void
	{
		nodelay_ = nodelay;
		if (impl_)
			impl_->SetNoDelay(nodelay);
	}

	auto SetDefragment(int defragment) -> void
	{
		defragment_ = defragment;
		if (impl_)
			impl_->SetDefragment(defragment);
	}

	auto SetRecvbufSize(size_t l) -> void
	{
		recv_buf_length_ = l;
		if (impl_)
			impl_->SetRecvbufSize(l);
	}

	auto SetConnectTimeout(uint32_t l) -> void
	{
		connect_timeout_ = l;
		if (impl_)
			impl_->SetConnectTimeout(l);
	}
	auto Close()-> void
	{
		if (impl_)
		{
			impl_->Close();
		}
	}
	auto BlockRecv(bool b) -> void
	{
		if (impl_)
			impl_->BlockRecv(b);
	}

protected:
	bool inited_{ false };
	asio::io_service& io_service_;

	std::shared_ptr<ClientTCPConnection> impl_;
	uint32_t max_packet_length_{ 0 };// 0 = default, using setting from base class
	size_t recv_buf_length_{ 0 }; // 0 = default, using setting from base class
	uint32_t connect_timeout_{ 0 };// 0 = default, using setting from base class
	int nodelay_{ 0 };
	int defragment_{ 0 };
};

typedef std::shared_ptr<TcpClient> TcpClient_ptr;
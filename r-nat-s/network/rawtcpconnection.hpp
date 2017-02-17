#pragma once

#include <memory>
#include <functional>
#include <deque>
#include <vector>

#ifdef WIN32
#include <MSTcpIP.h>
#endif


template <typename SocketType>
class RawTCPConnection
	: public std::enable_shared_from_this<RawTCPConnection<SocketType>>
{
protected:
	asio::io_service& io_service_;
	std::shared_ptr<SocketType> socket_;

	std::deque<std::pair<std::shared_ptr<asio::streambuf>, std::function<void(void)>>> outgoing_queue_;
	std::shared_ptr<asio::streambuf> sending_pkt_;
	std::shared_ptr<asio::streambuf> recving_pkt_;

	enum
	{
		DEFAULT_BUFFER_LENGTH = 4096, // page size, mtu is 1500, so it can contain 2 packets
	};

	uint32_t max_packet_length_{ 0 };
	bool nodelay_{ false };
	size_t recv_buf_length_{ DEFAULT_BUFFER_LENGTH };
	bool blocking_recv_{ false };

public:
	std::function<std::shared_ptr<asio::streambuf>(void)> on_allocbuf;
	std::function<void(const asio::error_code& e)> on_disconnect;
	std::function<void(std::shared_ptr<asio::streambuf> /*buf*/)> on_recv;

public:
	RawTCPConnection(asio::io_service& ioservice, std::shared_ptr<SocketType> socket)
		: io_service_(ioservice)
		, socket_(socket)
	{

	}
	virtual ~RawTCPConnection()
	{

	}

	virtual SocketType& Socket()
	{
		return *socket_;
	}

	auto Send(std::shared_ptr<asio::streambuf> data,std::function<void(void)> pfn = nullptr) -> void
	{
		io_service_.post(std::bind(&RawTCPConnection<SocketType>::__Send, this->shared_from_this(), data, pfn));
	}
	auto SendV(std::shared_ptr<std::vector<std::shared_ptr<asio::streambuf>>> data, std::function<void(void)> pfn = nullptr) -> void
	{
		io_service_.post(std::bind(&RawTCPConnection<SocketType>::__SendV, this->shared_from_this(), data, pfn));
	}

	auto Close() -> void
	{
		on_recv = nullptr;
		on_disconnect = nullptr;

		if (socket_ &&
			socket_->is_open())
		{
			asio::error_code ignored_ec;
			socket_->shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
			socket_->close(ignored_ec);
		}
//		socket_ = nullptr; // do this later
	}

	auto SetMaxPacketLength(uint32_t l) -> void
	{
		max_packet_length_ = l;
	}

	auto SetNoDelay(bool nodelay) -> void
	{
		nodelay_ = nodelay;
	}

	auto SetRecvbufSize(size_t l) -> void
	{
		if (l > DEFAULT_BUFFER_LENGTH)
			recv_buf_length_ = l;
		else
			recv_buf_length_ = DEFAULT_BUFFER_LENGTH;
	}

	auto BlockRecv(bool b) -> void
	{
		blocking_recv_ = b;
		if (!b)
		{
			io_service_.post(std::bind(&RawTCPConnection<SocketType>::__DoRecv, this->shared_from_this()));
		}
	}

protected:
	auto __SetOpt() -> void
	{
#ifdef WIN32
		tcp_keepalive alive;
		alive.onoff = TRUE;
		alive.keepalivetime = 60000;
		alive.keepaliveinterval = 1000;
		DWORD bytes_ret = 0;
		int ret = WSAIoctl(socket_->native_handle(), SIO_KEEPALIVE_VALS, &alive, sizeof(alive), NULL, 0,
			&bytes_ret, NULL, NULL);
		ret;
#else
		asio::error_code ignored_ec;
		asio::socket_base::keep_alive option_keep_alive(true);
		socket_->set_option(option_keep_alive, ignored_ec);
#endif
		if (nodelay_)
		{
			socket_->set_option(asio::ip::tcp::no_delay(true));
		}
	}
	auto __Send(std::shared_ptr<asio::streambuf> data, std::function<void(void)> pfn) -> void
	{
		// push the real data
		outgoing_queue_.push_back(std::make_pair(data, pfn));
		__DoSend();
	}

	auto __SendV(std::shared_ptr<std::vector<std::shared_ptr<asio::streambuf>>> datas, std::function<void(void)> pfn) -> void
	{
		// push the real data
		for (size_t i=0, count = datas->size();i<count;i++)
		{
			if (i + 1 == count)
				outgoing_queue_.push_back(std::make_pair(datas->at(i), pfn));
			else
				outgoing_queue_.push_back(std::make_pair(datas->at(i), std::function<void(void)>()));
		}
		__DoSend();
	}

	auto __DoSend() -> void
	{
		if (nullptr != sending_pkt_)
			return;
		if (!outgoing_queue_.size())
			return;
		if (nullptr == socket_)
			return;

		auto pkg = outgoing_queue_.front();
		outgoing_queue_.pop_front();

		sending_pkt_ = pkg.first;

		if (pkg.second)
		{
			io_service_.post(pkg.second);
		}

		asio::async_write(*socket_, *sending_pkt_,
			std::bind(&RawTCPConnection<SocketType>::__OnSendCallback, this->shared_from_this(),
			std::placeholders::_1,
			std::placeholders::_2));
	}
	auto __OnSendCallback(const asio::error_code& e, std::size_t /*bytes_transferred*/) -> void
	{
		if (e)
		{
			__Shutdown(e);
			if (on_disconnect)
			{
				on_disconnect(e);
				on_disconnect = nullptr;
			}
			// has been freed at this point
			return;
		}

//		sending_pkt_->consume(bytes_transferred); // no need, we don't reuse it
		sending_pkt_ = nullptr;
		__DoSend();
	}
	auto __DoRecv() -> void
	{
		if (nullptr == socket_)
			return;

		if (recving_pkt_)
			return;

		if (blocking_recv_)
			return;

		recving_pkt_ = on_allocbuf();
		socket_->async_read_some(recving_pkt_->prepare(recv_buf_length_),
			std::bind(&RawTCPConnection<SocketType>::__OnRecvCallback, this->shared_from_this(),
			std::placeholders::_1,
			std::placeholders::_2));
	}

	auto __OnRecvCallback(const asio::error_code& e, std::size_t bytes_transferred) -> void
	{
		if (e)
		{
			__Shutdown(e);
			// has been freed at this point
			return;
		}
		// commit the data
		recving_pkt_->commit(bytes_transferred);
		// let application know
		if (on_recv)
		{
			on_recv(recving_pkt_);
		}
		recving_pkt_ = nullptr;

		// prepare for next recv
		__DoRecv();
	}
	auto __Shutdown(const asio::error_code& e) -> void
	{
		asio::error_code ignored_ec;
		if (socket_)
			socket_->shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
		if (on_disconnect)
		{
			on_disconnect(e);
			on_disconnect = nullptr;
		}
	}
};
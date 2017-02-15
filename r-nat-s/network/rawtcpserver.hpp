#pragma once

#include <thread>
#include <map>
#include <mutex>
#include <vector>

#include "rawtcpconnection.hpp"
class RawServerTCPConnection
	: public RawTCPConnection<asio::ip::tcp::socket>
{
	using BaseConnection = RawTCPConnection<asio::ip::tcp::socket>;
public:
	RawServerTCPConnection(asio::io_service& io_service)
		: BaseConnection(io_service, std::make_shared<asio::ip::tcp::socket>(io_service))
	{
	}

	auto Start() -> void
	{
		io_service_.post(std::bind(&RawServerTCPConnection::__Start, 
			std::dynamic_pointer_cast<RawServerTCPConnection>(this->shared_from_this())));
	}

protected:
	auto __Start() -> void
	{
		__SetOpt();
		__DoRecv();
		__DoSend();
	}
};

class RawTcpServer
	: public std::enable_shared_from_this<RawTcpServer>
{
public:
	std::function<void(uint32_t /*seq*/, asio::ip::tcp::endpoint /*ep*/)> on_connect;
	std::function<void(uint32_t /*seq*/, const asio::error_code&)> on_disconnect;
	std::function<void(uint32_t /*seq*/, std::shared_ptr<asio::streambuf> /*buf*/)> on_recv;

public:
	RawTcpServer(asio::io_service& io_service) : io_service_(io_service){}
	~RawTcpServer() = default;

	void SetMaxPacketLength(uint32_t l)
	{
		max_packet_length_ = l;
	}
	auto SetNoDelay(bool nodelay) -> void
	{
		nodelay_ = nodelay;
	}

	auto SetRecvbufSize(size_t l) -> void
	{
		recv_buf_length_ = l;
	}

	auto Start(asio::ip::tcp::endpoint ep) -> bool
	{
		std::vector<asio::ip::tcp::endpoint> eps{ ep };
		return Start(eps);
	}
	auto Start(std::vector<asio::ip::tcp::endpoint> ep) -> bool;
	auto Close() -> void;

	// query listen port if port was 0 when call 'Start'
	auto GetPort() -> uint16_t;

	// data operation for individual connections
	auto Send(uint32_t seq, std::shared_ptr<asio::streambuf> data, std::function<void(void)> pfn = nullptr) -> void
	{
		io_service_.post(std::bind(&RawTcpServer::__Send, this->shared_from_this(), seq, data, pfn));
	}
	auto SendV(uint32_t seq, std::vector<std::shared_ptr<asio::streambuf>> datas, std::function<void(void)> pfn = nullptr) -> void
	{
		io_service_.post(std::bind(&RawTcpServer::__SendV, this->shared_from_this(), seq, datas, pfn));
	}
	auto BlockRecv(uint32_t seq, bool b) -> void
	{
		io_service_.post(std::bind(&RawTcpServer::__BlockRecv, this->shared_from_this(), seq, b));
	}
	auto Close(uint32_t seq) -> void
	{
		io_service_.post(std::bind(&RawTcpServer::__Close, this->shared_from_this(), seq));
	}

protected:
	bool inited_{ false };
	uint32_t next_seq_{ 0 };
	asio::io_service& io_service_;
	uint32_t max_packet_length_{ 0 }; // 0 means default
	size_t recv_buf_length_{ 0 }; // 0 means default
	bool nodelay_{ false };

	uint16_t port_{ 0 }; // the first listen port

	std::vector<std::shared_ptr<asio::ip::tcp::acceptor>> acceptors_;

	std::map<uint32_t,std::shared_ptr<RawServerTCPConnection>> conns_;

	auto __StartNextConnection(std::shared_ptr<asio::ip::tcp::acceptor> acceptor) -> void;
	auto __OnAccept(uint32_t seq, std::shared_ptr<RawServerTCPConnection> conn,
		std::shared_ptr<asio::ip::tcp::acceptor> acceptor, const asio::error_code& e) -> void;
	auto __GetNextSeq() -> uint32_t;
	auto __Send(uint32_t seq, std::shared_ptr<asio::streambuf> data, std::function<void(void)> pfn) -> void;
	auto __SendV(uint32_t seq, std::vector<std::shared_ptr<asio::streambuf>> datas, std::function<void(void)> pfn) -> void;
	auto __BlockRecv(uint32_t seq, bool b) -> void;
	auto __Close(uint32_t seq) -> void;
};

typedef std::shared_ptr<RawTcpServer> RawTcpServer_ptr;


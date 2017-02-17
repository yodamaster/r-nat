#include "stdafx.h"
#include "tcpserver.hpp"

auto TcpServer::Start(std::vector<asio::ip::tcp::endpoint> eps) -> bool
{
	// init acceptor
	for (auto&& ep : eps)
	{
		auto acceptor = std::make_shared<asio::ip::tcp::acceptor>(io_service_);
		acceptor->open(ep.protocol());
		acceptor->set_option(asio::ip::tcp::acceptor::reuse_address(true));
		acceptor->bind(ep);
		acceptor->listen();

		acceptors_.push_back(acceptor);
	}
	
	if (acceptors_.size())
	{
		// store local endpoint
		asio::error_code ignored_ec;
		auto ep = acceptors_.front()->local_endpoint(ignored_ec);
		port_ = ep.port();

		inited_ = true;
	}

	// start listen
	for (auto&& acceptor: acceptors_)
	{
		__StartNextConnection(acceptor);
	}
	return inited_;
}

auto TcpServer::__StartNextConnection(std::shared_ptr<asio::ip::tcp::acceptor> acceptor) -> void
{
	auto seq = __GetNextSeq();
	// create connection & set callbacks
	auto conn = std::make_shared<ServerTCPConnection>(*ioservices_[seq % ioservices_.size()]);
	conn->SetMaxPacketLength(max_packet_length_);
	conn->SetNoDelay(nodelay_);
	conn->SetRecvbufSize(recv_buf_length_);
	conn->on_allocbuf = [this]
	{
		if (on_allocbuf)
			return on_allocbuf();
		return std::make_shared<asio::streambuf>();
	};
	conn->on_disconnect = strand_.wrap([this, seq](const asio::error_code& e)
	{
		on_disconnect(seq,e); // notify a connection gone
		conns_.erase(seq);
	});
	conn->on_recv = strand_.wrap([this, seq](std::shared_ptr<asio::streambuf> buf)
	{
		on_recv(seq, buf);
	});

	conns_[seq] = conn;

	acceptor->async_accept(conn->Socket(),
		std::bind(&TcpServer::__OnAccept, this->shared_from_this(), seq, conn, acceptor, std::placeholders::_1));
}

auto TcpServer::__OnAccept(uint32_t seq, std::shared_ptr<ServerTCPConnection> conn, 
	std::shared_ptr<asio::ip::tcp::acceptor> acceptor, const asio::error_code& e) -> void
{
	if (e == asio::error::operation_aborted)
		return; // cancelled

	if (!inited_)
		return;

// 	auto msg = e.message();
// 	std::cout << msg << std::endl;

	if (!e)
	{
		asio::error_code ignored_ec;
		asio::ip::tcp::endpoint remote_ep = conn->Socket().remote_endpoint(ignored_ec);
		on_connect(seq, remote_ep); // notify a new connection
		conn->Start();
	}

	if (!inited_)
		return;
	__StartNextConnection(acceptor);
}

auto TcpServer::__GetNextSeq() -> uint32_t
{
	uint32_t seq;
	do
	{
		seq = ++next_seq_;
	} while (conns_.find(seq) != conns_.end());
	return seq;
}

auto TcpServer::Close() -> void
{
	inited_ = false;
	on_connect = nullptr;
	on_disconnect = nullptr;
	on_recv = nullptr;

	asio::error_code ignored_ec;
	for (auto&& acceptor : acceptors_)
	{
		acceptor->close(ignored_ec);
	}

	for (auto&& conn : conns_)
	{
		conn.second->Close();
	}
	conns_.clear();

	acceptors_.clear();
}

auto TcpServer::GetPort() -> uint16_t
{
	return port_;
}

auto TcpServer::__Send(uint32_t seq, std::shared_ptr<asio::streambuf> data, std::function<void(void)> pfn) -> void
{
	auto&& iter = conns_.find(seq);
	if (iter != conns_.end())
	{
		iter->second->Send(data, pfn);
	}
}

auto TcpServer::__SendV(uint32_t seq, std::shared_ptr<std::vector<std::shared_ptr<asio::streambuf>>> data, std::function<void(void)> pfn) -> void
{
	auto&& iter = conns_.find(seq);
	if (iter != conns_.end())
	{
		iter->second->SendV(data, pfn);
	}
}

auto TcpServer::__BlockRecv(uint32_t seq, bool b) -> void
{
	auto&& iter = conns_.find(seq);
	if (iter != conns_.end())
	{
		iter->second->BlockRecv(b);
	}
}

auto TcpServer::__Close(uint32_t seq) -> void
{
	auto&& iter = conns_.find(seq);
	if (iter != conns_.end())
	{
		iter->second->Close();
	}
}



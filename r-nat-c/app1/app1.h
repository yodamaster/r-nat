#pragma once

#include "../network/tcpclient.hpp"
#include "../network/rawtcpclient.hpp"

#include <set>
#include <map>
#include <atomic>
#include <deque>

class Server;

class AppLogic1
{
	Server* server_;
	asio::strand strand_;
public:
	AppLogic1(Server* server);
	~AppLogic1();

	bool Init();
	void Start();
	void Stop();

protected:
	// buffer pool
	bool enable_buffer_pooling_{ true };
	std::deque<asio::streambuf*> buffer_pool_;
	std::mutex buff_pool_lock_;

	// config
	uint32_t queue_limit_;
	uint32_t load_policy_;
	std::map<uint16_t, std::set<asio::ip::tcp::endpoint>> service_ep_;
	std::vector<asio::ip::tcp::endpoint> remote_ep_;

	using User = std::pair<uint16_t, uint32_t>;
	// running info
	struct Session
	{
		RawTcpClient_ptr user_conn_; // to target server
		TcpClient_ptr session_conn_; // to relay
		bool session_conn_ready_{ false };
		std::atomic<uint32_t> user_traffic_{ 0 };
		std::atomic<uint32_t> tunnel_traffic_{ 0 };
		bool user_corked_{ false };
		bool tunnel_corked_{ false };
		int delete_pending_{ 0 };
	};
	typedef std::shared_ptr<Session> Session_ptr;
	struct RemoteInfo 
	{
		asio::ip::tcp::endpoint ep_;
		TcpClient_ptr tcpclient_;
		std::map<User, Session_ptr> users_; // port + user seq to client socket
	};
	typedef std::shared_ptr<RemoteInfo> RemoteInfo_ptr;
	std::vector<RemoteInfo_ptr> relays_;

	auto __CreateTcpClient()->TcpClient_ptr;
	auto __CreateRawTcpClient()->RawTcpClient_ptr;
	auto __CreateTunnelConn(RemoteInfo_ptr remoteinfo)->TcpClient_ptr;
	auto __CreateSessionTunnelConn(RemoteInfo_ptr remoteinfo, Session_ptr session, User usr)->TcpClient_ptr;
	auto __CreateUserConn(RemoteInfo_ptr remoteinfo, Session_ptr session, User usr, const asio::ip::tcp::endpoint& service_ep)->RawTcpClient_ptr;
	auto __CreateIoBuffer()->std::shared_ptr<asio::streambuf>;

	// remote service(from agent to relay server)
	void __OnServerConnect(RemoteInfo_ptr remoteinfo);
	void __OnServerDisconnect(const asio::error_code& e,RemoteInfo_ptr remoteinfo);
	void __OnServerRecv(std::shared_ptr<asio::streambuf> data, RemoteInfo_ptr remoteinfo);

	// remote service(from agent to relay server, for session only)
	void __OnSessionConnect(RemoteInfo_ptr remoteinfo, Session_ptr session, User usr);
	void __OnSessionDisconnect(const asio::error_code& e, RemoteInfo_ptr remoteinfo, Session_ptr session, User usr);
	void __OnSessionRecv(std::shared_ptr<asio::streambuf> data, RemoteInfo_ptr remoteinfo, Session_ptr session, User usr);

	// proxy connection(from agent to real server)
	void __OnUserConnect(RemoteInfo_ptr remoteinfo, Session_ptr session, User usr, const asio::ip::tcp::endpoint& ep);
	void __OnUserDisconnect(const asio::error_code& e, RemoteInfo_ptr remoteinfo, Session_ptr session, User usr, const asio::ip::tcp::endpoint& ep);
	void __OnUserRecv(std::shared_ptr<asio::streambuf> data, RemoteInfo_ptr remoteinfo, Session_ptr session, User usr);
};
#pragma once

#include "../network/tcpserver.hpp"
#include "../network/rawtcpserver.hpp"

#include <set>
#include <map>
#include <atomic>

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
	std::vector<asio::ip::tcp::endpoint> listenep_;

	// running info
	struct Session 
	{
		uint32_t seq_{ 0 };
		uint32_t agent_id_{0};
		bool using_session_conn_{ false };
		std::atomic<uint32_t> user_traffic_{ 0 };
		std::atomic<uint32_t> tunnel_traffic_{ 0 };
		bool user_corked_{ false };
		bool tunnel_corked_{ false };
		bool user_disconnected_{ false };
		bool tunnel_disconnected_{ false };
		bool tunnel_hard_disconnected_{ false };
	};
	typedef std::shared_ptr<Session> Session_ptr;

	struct ServiceInfo
	{
		uint16_t port_;
		std::shared_ptr<RawTcpServer> service_tcpserver_; // in charge of individual service
		std::set<uint32_t> agents_;

		std::map<uint32_t, Session_ptr> sessions_;
	};
	typedef std::shared_ptr<ServiceInfo> Service_ptr;
	std::map<uint16_t, Service_ptr> services_; // port to listen service
	struct AgentInfo
	{
		uint32_t seq_;
		std::set<uint16_t> ports_; // port
		std::map<uint32_t, Session_ptr> sessions_;
	};
	typedef std::shared_ptr<AgentInfo> AgentInfo_ptr;
	std::map<uint32_t, AgentInfo_ptr> agents_; // seq to agent

	std::shared_ptr<TcpServer> agent_tcpserver_; // for agent

	auto __CreateIoBuffer()->std::shared_ptr<asio::streambuf>;

	// listen service(from agent)
	void __OnAgentConnect(uint32_t seq, asio::ip::tcp::endpoint ep);
	void __OnAgentDisconnect(uint32_t seq, const asio::error_code& e);
	void __OnAgentRecv(uint32_t seq,std::shared_ptr<asio::streambuf> buf);

	// ondemand listen service(from user)
	void __OnUserConnect(Service_ptr service, uint32_t seq, asio::ip::tcp::endpoint ep);
	void __OnUserDisconnect(Service_ptr service, uint32_t seq, const asio::error_code& e);
	void __OnUserRecv(Service_ptr service, uint32_t seq, std::shared_ptr<asio::streambuf> buf);

	// cleanup
	void __CleanSession(Service_ptr service, Session_ptr session);
};
#include "stdafx.h"
#include "app1.h"

#include "../server.h"

#include "../config.h"

void AppLogic1::__OnAgentConnect(uint32_t seq, asio::ip::tcp::endpoint ep)
{
	auto agent = std::make_shared<AgentInfo>();
	agents_[seq] = agent;
	LOG_DEBUG("new agent %d",seq);
}

void AppLogic1::__OnAgentDisconnect(uint32_t seq, const asio::error_code& e)
{
	LOG_DEBUG("agent %d disconnected", seq);
	auto agent_iter = agents_.find(seq);
	if (agent_iter == agents_.end())
		return;
	auto agent = agent_iter->second;
	agents_.erase(agent_iter);

	// deduct from services
	for (auto&& port : agent->ports_)
	{
		auto service_iter = services_.find(port);
		if (service_iter != services_.end())
		{
			auto&& service = service_iter->second;

			for (auto ses_in_agent_iter : agent->sessions_)
			{
				service->service_tcpserver_->Close(ses_in_agent_iter.first);
			}

			service->agents_.erase(seq);
			LOG_DEBUG("remove agent %d from service %d", seq, port);

			// check whether we have removed all agent
			if (service->agents_.size() == 0)
			{
				LOG_DEBUG("going to shutdown service %d  due to no agent", port);
				// turn off tcp server
				service->service_tcpserver_->Close();

				services_.erase(service_iter);
			}
		}
	}
	agent->ports_.clear();
	agent->sessions_.clear();
}

void AppLogic1::__OnAgentRecv(uint32_t seq, std::shared_ptr<asio::streambuf> data)
{
	if (data->size() < sizeof(uint8_t)) 
		return;
	// the first byte is the command id
	auto p = (uint8_t*)asio::buffer_cast<const uint8_t*>(data->data());
	switch (*p)
	{
	case R_NAT::A2R::BIND:
	{
		LOG_DEBUG("BIND");
		if (data->size() < sizeof(R_NAT::A2R::bind))
			break;
		auto req = reinterpret_cast<R_NAT::A2R::bind*>(p);
		// find the agent
		auto agent_iter = agents_.find(seq);
		if (agent_iter == agents_.end())
			break;
		auto agent = agent_iter->second;

		auto send_ack = [&](uint16_t port, uint32_t error_code)
		{
			auto rep_buf = __CreateIoBuffer();
			auto rep = (R_NAT::R2A::bind*)asio::buffer_cast<char*>(rep_buf->prepare(sizeof(R_NAT::R2A::bind)));
			rep->cmd = R_NAT::R2A::BIND;
			rep->port = port;
			rep->error_code = error_code;
			rep_buf->commit(sizeof(R_NAT::R2A::bind));
			agent_tcpserver_->Send(seq, rep_buf);
		};

		// find the service
		bool handled = false;
		auto service_iter = services_.find(req->port);
		if (service_iter == services_.end())
		{
			// we need to add a service
			auto service = std::make_shared<ServiceInfo>();
			service->port_ = req->port;
			service->service_tcpserver_ = std::make_shared<RawTcpServer>(server_->main_ioservice_,server_->ioservices_);
			service->service_tcpserver_->SetMaxPacketLength(getConfig()->system_.max_packet_size_);
			service->service_tcpserver_->SetNoDelay(getConfig()->system_.tcp_send_no_delay_);
			service->service_tcpserver_->SetRecvbufSize(getConfig()->system_.recv_buf_size_);
			service->service_tcpserver_->on_recv = strand_.wrap(std::bind(&AppLogic1::__OnUserRecv, this, service, _1, _2));
			service->service_tcpserver_->on_connect = strand_.wrap(std::bind(&AppLogic1::__OnUserConnect, this, service, _1, _2));
			service->service_tcpserver_->on_disconnect = strand_.wrap(std::bind(&AppLogic1::__OnUserDisconnect, this, service, _1, _2));

			service->agents_.insert(seq);
			if (service->service_tcpserver_->Start(asio::ip::tcp::endpoint(asio::ip::address_v4::any(), req->port)))
			{
				LOG_DEBUG("started listening at %d", req->port);
				
				// store in service list
				services_[req->port] = service;
				agent->ports_.insert(req->port); // add to port list
				send_ack(req->port, 0);
			}
			else
			{
				LOG_DEBUG("failed to start listening at %d",req->port);
				// failed
				send_ack(req->port, -1/*TODO*/);
			}
		}
		else
		{
			LOG_DEBUG("adding agent %d to existing service %d", seq, req->port);

			auto service = service_iter->second;
			service->agents_.insert(seq);
			agent->ports_.insert(req->port); // add to port list
			send_ack(req->port, 0);
		}

		break;
	}
	case R_NAT::A2R::UNBIND:
	{
		LOG_DEBUG("UNBIND");
		if (data->size() < sizeof(R_NAT::A2R::unbind))
			break;
		auto req = reinterpret_cast<R_NAT::A2R::unbind*>(p);
		// find the agent
		auto agent_iter = agents_.find(seq);
		if (agent_iter == agents_.end())
			break;
		auto agent = agent_iter->second;

		auto send_ack = [&](uint16_t port, uint32_t error_code)
		{
			auto rep_buf = __CreateIoBuffer();
			using MSG = R_NAT::R2A::unbind;
			auto rep = (MSG*)asio::buffer_cast<char*>(rep_buf->prepare(sizeof(MSG)));
			rep->cmd = R_NAT::R2A::UNBIND;
			rep->port = port;
			rep->error_code = error_code;
			rep_buf->commit(sizeof(MSG));
			agent_tcpserver_->Send(seq, rep_buf);
		};

		agent->ports_.erase(req->port); // remove from port list

		// find the service
		auto service_iter = services_.find(req->port);
		if (service_iter != services_.end())
		{
			LOG_DEBUG("remove agent %d from service %d", seq, req->port);

			auto service = service_iter->second;
			service->agents_.erase(seq);

			// check whether we have removed all agents
			if (service->agents_.size() == 0)
			{
				LOG_DEBUG("going to shutdown service %d  due to no agent", req->port);

				// turn off tcp server
				service->service_tcpserver_->Close();

				services_.erase(service_iter);
			}
		}

		send_ack(req->port, 0);
		break;
	}
	case R_NAT::A2R::DATA:
	{
//		LOG_DEBUG("DATA");
		if (data->size() < sizeof(R_NAT::A2R::data))
			break;
		auto req = reinterpret_cast<R_NAT::A2R::data*>(p);
		// find the agent
		auto agent_iter = agents_.find(seq);
		if (agent_iter == agents_.end())
			break;
		auto agent = agent_iter->second;

		// find the service
		auto service_iter = services_.find(req->port);
		if (service_iter != services_.end())
		{
			auto service = service_iter->second;
			// store the user id before we remove header from buffer
			auto user_id = req->id;
			// remove the extra header
			data->consume(sizeof(R_NAT::A2R::data));

			auto session_iter = service->sessions_.find(user_id);
			if (session_iter == service->sessions_.end())
				break;
			auto session = session_iter->second;

			LOG_DEBUG("relay traffic from agent %d to service %d, user %d", seq, req->port, user_id);

			session->tunnel_traffic_++;
			auto queue_limit = queue_limit_;
			if (queue_limit &&
				(!session->tunnel_corked_) &&
				session->tunnel_traffic_ > queue_limit)
			{
				// we have receive too many from relay
				session->tunnel_corked_ = true;
				agent_tcpserver_->BlockRecv(seq, true);
				LOG_DEBUG("stop recv from agent %d due to longer queue size", seq, session->tunnel_traffic_);
			}

			service->service_tcpserver_->Send(user_id, data, [this, session, seq, queue_limit]{
				session->tunnel_traffic_--;
				if (queue_limit &&
					(session->tunnel_corked_) &&
					session->tunnel_traffic_ <= queue_limit / 2)
				{
					session->tunnel_corked_ = false;
					agent_tcpserver_->BlockRecv(seq, false);
					LOG_DEBUG("resume recv from agent %d", seq, session->tunnel_traffic_);
				}
			});
		}
		break;
	}
	case R_NAT::A2R::DISCONNECT:
	{
		LOG_DEBUG("DISCONNECT");
		if (data->size() < sizeof(R_NAT::A2R::disconnect))
			break;
		auto req = reinterpret_cast<R_NAT::A2R::disconnect*>(p);
		// find the agent
		auto agent_iter = agents_.find(seq);
		if (agent_iter == agents_.end())
			break;
		auto agent = agent_iter->second;
		agent->sessions_.erase(req->id);

		// find the service
		auto service_iter = services_.find(req->port);
		if (service_iter != services_.end())
		{
			auto service = service_iter->second;

			// close the client connection
			service->service_tcpserver_->Close(req->id);

			service->sessions_.erase(req->id);

			LOG_DEBUG("the agent %d has been disconnected from final server, remote the user %d", seq, req->id);
		}
		break;
	}
	case R_NAT::A2R::SESSION:
	{
		LOG_TAG("TASK");
		if (data->size() < sizeof(R_NAT::A2R::session))
			break;
		auto req = reinterpret_cast<R_NAT::A2R::session*>(p);
		// find the agent
		auto agent_iter = agents_.find(seq);
		if (agent_iter == agents_.end())
			break;
		auto agent = agent_iter->second;
		agent->ports_.insert(req->port);

		auto service_iter = services_.find(req->port);
		if (service_iter == services_.end())
			break;
		auto service = service_iter->second;

		// switch task connection
		// the new agent is agent
		// the old agent is agent_[session->agent_id_]
		auto session_iter = service->sessions_.find(seq);
		if (session_iter == service->sessions_.end())
		{
			// usually this should happen
			auto session = std::make_shared<Session>();
			session->agent_id_ = seq;
			session->using_session_conn_ = true;
			service->sessions_[req->id] = session;
			agent->sessions_[req->id] = session;
		}
		else
		{
			auto session = session_iter->second;

			auto old_agent_iter = agents_.find(session->agent_id_);
			if (old_agent_iter != agents_.end())
			{
				auto old_agent = old_agent_iter->second;
				old_agent->sessions_.erase(req->id);
			}

			session->agent_id_ = seq;
			session->using_session_conn_ = true;
			agent->sessions_[req->id] = session;
		}

		LOG_DEBUG("the agent %d has declared to be a task connection, assigned to user %d", seq, req->id);
		break;
	}
	}
}

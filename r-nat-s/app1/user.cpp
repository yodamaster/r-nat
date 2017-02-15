#include "stdafx.h"
#include "app1.h"

#include "../server.h"

#include "../config.h"

void AppLogic1::__OnUserConnect(Service_ptr service, uint32_t seq, asio::ip::tcp::endpoint ep)
{
	LOG_FUNCTION();
	if (service->agents_.size() == 0)
		return; // no available agent

	auto cli_index = seq;
	if (load_policy_ == POLICY_IP)
	{
		cli_index = fnv_hash_int(ep.address().to_v4().to_ulong());
	}
	auto agent_seq_iter = service->agents_.begin();
	std::advance(agent_seq_iter, cli_index % service->agents_.size());
	auto agent_seq = *agent_seq_iter;

	auto agent_iter = agents_.find(agent_seq);
	if (agent_iter == agents_.end())
		return;
	auto agent = agent_iter->second;

	auto session = std::make_shared<Session>();
	session->agent_id_ = agent_seq;
	service->sessions_[seq] = session;
	agent->sessions_[seq] = session;

	auto rep_buf = std::make_shared<asio::streambuf>();
	auto rep = (R_NAT::R2A::connect*)asio::buffer_cast<char*>(rep_buf->prepare(sizeof(R_NAT::R2A::connect)));
	rep->cmd = R_NAT::R2A::CONNECT;
	rep->port = service->port_;
	rep->id = seq;
	rep->usr_ip = ep.address().to_v4().to_ulong();
	rep->usr_port = ep.port();
	rep_buf->commit(sizeof(R_NAT::R2A::connect));
	agent_tcpserver_->Send(agent_seq, rep_buf);
}

void AppLogic1::__OnUserDisconnect(Service_ptr service, uint32_t seq, const asio::error_code& e)
{
	LOG_FUNCTION();
	do 
	{
		if (service->agents_.size() == 0)
		{
			break; // no available agent
		}

		auto session_iter = service->sessions_.find(seq);
		if (session_iter == service->sessions_.end())
		{
			break;
		}

		auto session = session_iter->second;

		auto agent_iter = agents_.find(session->agent_id_);
		if (agent_iter == agents_.end())
		{
			break;
		}
		auto agent = agent_iter->second;
		agent->sessions_.erase(seq);

		auto rep_buf = std::make_shared<asio::streambuf>();
		auto rep = (R_NAT::R2A::disconnect*)asio::buffer_cast<char*>(rep_buf->prepare(sizeof(R_NAT::R2A::disconnect)));
		rep->cmd = R_NAT::R2A::DISCONNECT;
		rep->port = service->port_;
		rep->id = seq;
		rep_buf->commit(sizeof(R_NAT::R2A::disconnect));
		agent_tcpserver_->Send(session->agent_id_, rep_buf);
	} while (0);
	service->sessions_.erase(seq); // remove mapping: user <--> agent
	service->service_tcpserver_->Close(seq); // remove the connection
}

void AppLogic1::__OnUserRecv(Service_ptr service, uint32_t seq, std::shared_ptr<asio::streambuf> buf)
{
	LOG_FUNCTION();
	if (service->agents_.size() == 0)
		return; // no available agent

	auto session_iter = service->sessions_.find(seq);
	if (session_iter == service->sessions_.end())
	{
		return;
	}

	auto session = session_iter->second;

	auto agent_iter = agents_.find(session->agent_id_);
	if (agent_iter == agents_.end())
		return;
	//	auto agent = agent_iter->second;

	auto rep_buf = std::make_shared<asio::streambuf>();
	auto rep = (R_NAT::R2A::data*)asio::buffer_cast<char*>(rep_buf->prepare(sizeof(R_NAT::R2A::data)));
	rep->cmd = R_NAT::R2A::DATA;
	rep->port = service->port_;
	rep->id = seq;
	rep_buf->commit(sizeof(R_NAT::R2A::data));

	session->user_traffic_++;
	if (QUEUE_LIMIT &&
		(!session->user_corked_) &&
		session->user_traffic_ > QUEUE_LIMIT)
	{
		// we have receive too many from relay
		session->user_corked_ = true;
		service->service_tcpserver_->BlockRecv(seq,true);
	}

	std::vector<std::shared_ptr<asio::streambuf>> datas{ rep_buf, buf };
	agent_tcpserver_->SendV(session->agent_id_, datas, [service, session, seq]{
		session->user_traffic_--;
		if (QUEUE_LIMIT &&
			(session->user_corked_) &&
			session->tunnel_traffic_ <= QUEUE_LIMIT / 2)
		{
			session->user_corked_ = false;
			service->service_tcpserver_->BlockRecv(seq, false);
		}
	});
}

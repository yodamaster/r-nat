#include "stdafx.h"
#include "app1.h"

#include "../server.h"

#include "../config.h"

void AppLogic1::__OnServerConnect(RemoteInfo_ptr remoteinfo)
{
	// announce our service to the relay
	for (auto&& ep: getConfig()->local_.locals_)
	{
		auto rep_buf = __CreateIoBuffer();
		using MSG = R_NAT::A2R::bind;
		auto rep = (MSG*)asio::buffer_cast<char*>(rep_buf->prepare(sizeof(MSG)));
		rep->cmd = R_NAT::A2R::BIND;
		rep->port = (uint16_t)atol(ep.service_port_.c_str());
		rep_buf->commit(sizeof(MSG));
		remoteinfo->tcpclient_->Send(rep_buf);
	}
}

void AppLogic1::__OnServerDisconnect(const asio::error_code& e, RemoteInfo_ptr remoteinfo)
{
	// close all connections to target
	for (auto&& user : remoteinfo->users_)
	{
		auto session = user.second;
		session->user_conn_->Close();
		session->session_conn_->Close();
	}
	remoteinfo->users_.clear(); // drop all connections
	strand_.post([this,remoteinfo]
	{
		remoteinfo->tcpclient_ = __CreateTunnelConn(remoteinfo);
		remoteinfo->tcpclient_->Start(remoteinfo->ep_);
	});
}

void AppLogic1::__OnServerRecv(std::shared_ptr<asio::streambuf> data, RemoteInfo_ptr remoteinfo)
{
#define SERVER_INFO "[" << remoteinfo->ep_.address().to_string() << ":" << remoteinfo->ep_.port() << "] "
	if (data->size() < sizeof(uint8_t))
		return;
	// the first byte is the command id
	auto p = (uint8_t*)asio::buffer_cast<const uint8_t*>(data->data());
	switch (*p)
	{
	case R_NAT::R2A::BIND:
	{
//		LOG_TAG("BIND");
		if (data->size() < sizeof(R_NAT::R2A::bind))
			return;
		auto req = reinterpret_cast<R_NAT::R2A::bind*>(p);
		if (req->error_code)
		{
			std::cerr << SERVER_INFO << "failed to listen at " << req->port << std::endl;
		}
		else
		{
			std::cout << SERVER_INFO << "succeeded to listen at " << req->port << std::endl;
		}
		break;
	}
	case R_NAT::R2A::UNBIND:
	{
//		LOG_TAG("UNBIND");
		if (data->size() < sizeof(R_NAT::R2A::unbind))
			return;
		auto req = reinterpret_cast<R_NAT::R2A::unbind*>(p);
		if (req->error_code)
		{
			std::cerr << SERVER_INFO << "failed to remove port " << req->port << std::endl;
		}
		else
		{
			std::cout << SERVER_INFO << "succeeded to remove port " << req->port << std::endl;
		}
		break;
	}
	case R_NAT::R2A::DATA:
	{
//		LOG_TAG("DATA");
		if (data->size() < sizeof(R_NAT::R2A::data))
			return;
		auto req = reinterpret_cast<R_NAT::R2A::data*>(p);
		// find the user
		auto usr = std::make_pair(req->port, req->id);
		auto user_iter = remoteinfo->users_.find(usr);
		if (user_iter == remoteinfo->users_.end())
			return;

		LOG_DEBUG("data from service %d, user %d", req->port, req->id);

		auto session = user_iter->second;
		// remove the extra header
		data->consume(sizeof(R_NAT::R2A::data));

		session->tunnel_traffic_++;
		if (queue_limit_ &&
			(!session->tunnel_corked_) &&
			session->tunnel_traffic_ > queue_limit_)
		{
			// we have received too much from relay
			session->tunnel_corked_ = true;
			session->session_conn_->BlockRecv(true);
		}

		// delivery to the client
		session->user_conn_->Send(data, [this, session, remoteinfo, usr]{
			session->tunnel_traffic_--;
			if (queue_limit_ &&
				(session->tunnel_corked_) &&
				session->tunnel_traffic_ <= queue_limit_ / 2)
			{
				session->tunnel_corked_ = false;
				session->session_conn_->BlockRecv(false);
			}

			__CleanSession(remoteinfo, session, usr);
		});
		break;
	}
	case R_NAT::R2A::CONNECT:
	{
//		LOG_TAG("CONNECT");
		if (data->size() < sizeof(R_NAT::R2A::connect))
			return;
		auto req = reinterpret_cast<R_NAT::R2A::connect*>(p);
		// find the user
		auto usr = std::make_pair(req->port, req->id);
		auto user_iter = remoteinfo->users_.find(usr);
		if (user_iter != remoteinfo->users_.end())
			return;

		// find a server according service port
		auto service_iter = service_ep_.find(req->port);
		if (service_iter == service_ep_.end() ||
			service_iter->second.size() == 0)
			return; // no such service

		auto service_ep_iter = service_iter->second.begin();

		auto cli_index = req->id;
		if (load_policy_ == Config::CONFIG_LOAD_POLICY_IP)
		{
			cli_index = fnv_hash_int(req->usr_ip);
		}
		std::advance(service_ep_iter, cli_index % service_iter->second.size());
		auto service_ep = *service_ep_iter;

		std::cout << SERVER_INFO << "server: " << service_ep.address().to_string() << " : " << service_ep.port() << std::endl;
		std::cout << "\tnew user, service port : " << usr.first << ", user seq : " << usr.second << std::endl;
		std::cout << "\tsource ip: " << asio::ip::address_v4(req->usr_ip).to_string() << " port:" << req->usr_port << std::endl;

		auto session = std::make_shared<Session>();

		session->session_conn_ = __CreateSessionTunnelConn(remoteinfo,session,usr);
		session->user_conn_ = __CreateUserConn(remoteinfo, session, usr,service_ep);

		// store the user
		remoteinfo->users_[usr] = session;

		// connect to target program
		session->session_conn_->Start(remoteinfo->ep_);
		session->user_conn_->Start(service_ep);
		break;
	}
	case R_NAT::R2A::DISCONNECT:
	{
//		LOG_TAG("DISCONNECT");
		if (data->size() < sizeof(R_NAT::R2A::disconnect))
			return;
		auto req = reinterpret_cast<R_NAT::R2A::disconnect*>(p);
		// find the user
		auto usr = std::make_pair(req->port, req->id);
		auto user_iter = remoteinfo->users_.find(usr);
		if (user_iter == remoteinfo->users_.end())
			return;

		// close socket
		auto session = user_iter->second;
		session->session_conn_->Close();
		session->tunnel_disconnected_ = true;

		__CleanSession(remoteinfo, session, usr);
		break;
	}
	}
}

#include "stdafx.h"
#include "app1.h"

#include "../server.h"

#include "../config.h"

void AppLogic1::__OnServerConnect(RemoteInfo_ptr remoteinfo)
{
	// announce our service to the relay
	for (auto&& ep: getConfig()->local_.locals_)
	{
		auto rep_buf = std::make_shared<asio::streambuf>();
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
	server_->main_ioservice_.post([this,remoteinfo]
	{
		remoteinfo->tcpclient_ = std::make_shared<TcpClient>(server_->main_ioservice_);
		remoteinfo->tcpclient_->SetMaxPacketLength(getConfig()->packet_.size_max_);
//		remoteinfo->tcpclient_->SetNoDelay(true);
		remoteinfo->tcpclient_->SetRecvbufSize(getConfig()->packet_.recv_buf_size_);
		remoteinfo->tcpclient_->on_connect = std::bind(&AppLogic1::__OnServerConnect, this, remoteinfo);
		remoteinfo->tcpclient_->on_disconnect = std::bind(&AppLogic1::__OnServerDisconnect, this, _1, remoteinfo);
		remoteinfo->tcpclient_->on_recv = std::bind(&AppLogic1::__OnServerRecv, this, _1, remoteinfo);
		remoteinfo->tcpclient_->Start(remoteinfo->ep_);
	});
}

void AppLogic1::__OnServerRecv(std::shared_ptr<asio::streambuf> data, RemoteInfo_ptr remoteinfo)
{
	if (data->size() < sizeof(uint8_t))
		return;
	// the first byte is the command id
	auto p = (uint8_t*)asio::buffer_cast<const uint8_t*>(data->data());
	switch (*p)
	{
	case R_NAT::R2A::BIND:
	{
		LOG_TAG("BIND");
		if (data->size() < sizeof(R_NAT::R2A::bind))
			return;
		auto req = reinterpret_cast<R_NAT::R2A::bind*>(p);
		if (req->error_code)
		{
			std::cerr << "relay: failed to listen at " << req->port << std::endl;
		}
		else
		{
			std::cerr << "relay: succeeded to listen at " << req->port << std::endl;
		}
		break;
	}
	case R_NAT::R2A::UNBIND:
	{
		LOG_TAG("UNBIND");
		if (data->size() < sizeof(R_NAT::R2A::unbind))
			return;
		auto req = reinterpret_cast<R_NAT::R2A::unbind*>(p);
		if (req->error_code)
		{
			std::cerr << "relay: failed to remove port " << req->port << std::endl;
		}
		else
		{
			std::cerr << "relay: succeeded to remove port " << req->port << std::endl;
		}
		break;
	}
	case R_NAT::R2A::DATA:
	{
		LOG_TAG("DATA");
		if (data->size() < sizeof(R_NAT::R2A::data))
			return;
		auto req = reinterpret_cast<R_NAT::R2A::data*>(p);
		// find the user
		auto uid = std::make_pair(req->port, req->id);
		auto user_iter = remoteinfo->users_.find(uid);
		if (user_iter == remoteinfo->users_.end())
			return;

		LOG_DEBUG("data from service %d, user %d", req->port, req->id);

		auto session = user_iter->second;
		// remove the extra header
		data->consume(sizeof(R_NAT::R2A::data));

		session->tunnel_traffic_++;
		if (QUEUE_LIMIT &&
			(!session->tunnel_corked_) &&
			session->tunnel_traffic_ > QUEUE_LIMIT)
		{
			// we have receive too many from relay
			session->tunnel_corked_ = true;
			session->session_conn_->BlockRecv(true);
		}

		// delivery to the client
		session->user_conn_->Send(data, [this, session, remoteinfo, uid]{
			session->tunnel_traffic_--;
			if (QUEUE_LIMIT &&
				(session->tunnel_corked_) &&
				session->tunnel_traffic_ <= QUEUE_LIMIT / 2)
			{
				session->tunnel_corked_ = false;
				session->session_conn_->BlockRecv(false);
			}
			if (session->tunnel_traffic_ ==0 &&
				session->delete_pending_)
			{
//				std::cout << "no more pending data, inside usr conn" << std::endl;
				session->user_conn_->Close();
				remoteinfo->users_.erase(uid);
			}
		});
		break;
	}
	case R_NAT::R2A::CONNECT:
	{
		LOG_TAG("CONNECT");
		if (data->size() < sizeof(R_NAT::R2A::connect))
			return;
		auto req = reinterpret_cast<R_NAT::R2A::connect*>(p);
		// find the user
		auto uid = std::make_pair(req->port, req->id);
		auto user_iter = remoteinfo->users_.find(uid);
		if (user_iter != remoteinfo->users_.end())
			return;

		// find a server according service port
		auto service_iter = service_ep_.find(req->port);
		if (service_iter == service_ep_.end() ||
			service_iter->second.size() == 0)
			return; // no such service

		auto service_ep_iter = service_iter->second.begin();
		std::advance(service_ep_iter, fnv_hash_int(req->id) % service_iter->second.size());
		auto service_ep = *service_ep_iter;

		std::cout << "new user from service port: " << req->port << " user seq:" << req->id << std::endl;
		std::cout << "source ip: " << req->usr_ip << " port:" << req->usr_port << std::endl;
		std::cout << "selected server: " << service_ep.address().to_string() << ":" << service_ep.port() << std::endl;

		auto session = std::make_shared<Session>();

		session->session_conn_ = std::make_shared<TcpClient>(server_->main_ioservice_);
		session->session_conn_->SetMaxPacketLength(getConfig()->packet_.size_max_);
//		session->session_conn_ ->SetNoDelay(true);
		session->session_conn_->SetRecvbufSize(getConfig()->packet_.recv_buf_size_);
		session->session_conn_->on_connect = std::bind(&AppLogic1::__OnSessionConnect, this, remoteinfo, session, uid);
		session->session_conn_->on_disconnect = std::bind(&AppLogic1::__OnSessionDisconnect, this, _1, remoteinfo, session, uid);
		session->session_conn_->on_recv = std::bind(&AppLogic1::__OnSessionRecv, this, _1, remoteinfo, session, uid);

		session->user_conn_ = std::make_shared<RawTcpClient>(server_->main_ioservice_);
		session->user_conn_->SetMaxPacketLength(getConfig()->packet_.size_max_);
//		session->proxy_conn_ ->SetNoDelay(true);
		session->user_conn_->SetRecvbufSize(getConfig()->packet_.recv_buf_size_);
		session->user_conn_->on_connect = std::bind(&AppLogic1::__OnUserConnect, this, remoteinfo, session, uid, service_ep);
		session->user_conn_->on_disconnect = std::bind(&AppLogic1::__OnUserDisconnect, this, _1, remoteinfo, session, uid, service_ep);
		session->user_conn_->on_recv = std::bind(&AppLogic1::__OnUserRecv, this, _1, remoteinfo, session, uid);

		// store the user
		remoteinfo->users_[uid] = session;

		// connect to target program
		session->session_conn_->Start(remoteinfo->ep_);
		session->user_conn_->Start(service_ep);
		break;
	}
	case R_NAT::R2A::DISCONNECT:
	{
		LOG_TAG("DISCONNECT");
		if (data->size() < sizeof(R_NAT::R2A::disconnect))
			return;
		auto req = reinterpret_cast<R_NAT::R2A::disconnect*>(p);
		// find the user
		auto uid = std::make_pair(req->port, req->id);
		auto user_iter = remoteinfo->users_.find(uid);
		if (user_iter == remoteinfo->users_.end())
			return;

		// close socket
		auto session = user_iter->second;
		session->session_conn_->Close();
		session->user_conn_->Close();

		// remove the user
		remoteinfo->users_.erase(user_iter);
		break;
	}
	}
}

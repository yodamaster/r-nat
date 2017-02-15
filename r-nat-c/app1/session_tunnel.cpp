#include "stdafx.h"
#include "app1.h"

#include "../server.h"

#include "../config.h"

void AppLogic1::__OnSessionConnect(RemoteInfo_ptr remoteinfo, Session_ptr session, User uid)
{
	auto rep_buf = std::make_shared<asio::streambuf>();
	using MSG = R_NAT::A2R::session;
	auto rep = (MSG*)asio::buffer_cast<char*>(rep_buf->prepare(sizeof(MSG)));
	rep->cmd = R_NAT::A2R::SESSION;
	rep->port = uid.first;
	rep->id = uid.second;
	rep_buf->commit(sizeof(MSG));
	session->session_conn_->Send(rep_buf);

	// mark it ready
	session->session_conn_ready_ = true;
}

void AppLogic1::__OnSessionDisconnect(const asio::error_code& e, RemoteInfo_ptr remoteinfo, Session_ptr session, User uid)
{
	// close socket
	session->session_conn_->Close();
	if (session->tunnel_traffic_ &&
		session->user_traffic_ == 0 &&
		session->delete_pending_ == 0)
	{
		// we still have data from relay to proxy
		session->delete_pending_++;
//		std::cout << "enter delay delete, session conn broken" << std::endl;
	}
	else
	{
		// either still have data to relay
		// or, no more data from relay
		session->user_conn_->Close();
		// remove the user
		remoteinfo->users_.erase(uid);
	}
}

void AppLogic1::__OnSessionRecv(std::shared_ptr<asio::streambuf> data, RemoteInfo_ptr remoteinfo, Session_ptr session, User usr)
{
	if (data->size() < sizeof(uint8_t))
		return;
	// the first byte is the command id
	auto p = (uint8_t*)asio::buffer_cast<const uint8_t*>(data->data());
	switch (*p)
	{
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

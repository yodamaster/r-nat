#include "stdafx.h"
#include "app1.h"

#include "../server.h"

#include "../config.h"

void AppLogic1::__OnUserConnect(RemoteInfo_ptr remoteinfo, Session_ptr session, User usr, const asio::ip::tcp::endpoint& ep)
{
	// nothing to do, just connected
	std::cout << "connected. from service port: " << usr.first << ", user seq: " << usr.second << std::endl;
	std::cout << "server: " << ep.address().to_string() << ":" << ep.port() << std::endl;
}

void AppLogic1::__OnUserDisconnect(const asio::error_code& e, RemoteInfo_ptr remoteinfo, Session_ptr session, User usr, const asio::ip::tcp::endpoint& ep)
{
	std::cout << "disconnected. from service port: " << usr.first << ", user seq: " << usr.second << std::endl;
	std::cout << "server: " << ep.address().to_string() << ":" << ep.port() << std::endl;

	session->user_conn_->Close();

	if (session->user_traffic_ &&
		session->tunnel_traffic_ == 0 &&
		session->delete_pending_ ==0)
	{
		// we still have data from proxy to relay
		session->delete_pending_++;
//		std::cout << "enter delay delete, usr conn broken" << std::endl;
	}
	else
	{
		// either still have data to relay
		// or, no more data from relay
		session->session_conn_->Close();
		remoteinfo->users_.erase(usr);
	}
}

void AppLogic1::__OnUserRecv(std::shared_ptr<asio::streambuf> data, RemoteInfo_ptr remoteinfo, Session_ptr session, User usr)
{
	auto rep_buf = std::make_shared<asio::streambuf>();
	using MSG = R_NAT::A2R::data;
	auto rep = (MSG*)asio::buffer_cast<char*>(rep_buf->prepare(sizeof(MSG)));
	rep->cmd = R_NAT::A2R::DATA;
	rep->port = usr.first;
	rep->id = usr.second;
	rep_buf->commit(sizeof(MSG));

	session->user_traffic_++;

	if (QUEUE_LIMIT &&
		(!session->user_corked_) &&
		session->user_traffic_ > QUEUE_LIMIT)
	{
		// we have receive too many from relay
		session->user_corked_ = true;
		session->user_conn_->BlockRecv(true);
	}

	std::vector<std::shared_ptr<asio::streambuf>> datas{ rep_buf, data };

	auto tcpclient = session->session_conn_ready_ ? session->session_conn_: remoteinfo->tcpclient_;
	tcpclient->SendV(datas, [this, session, remoteinfo, usr]{  // use session connection to deliver data
		session->user_traffic_--;
		if (QUEUE_LIMIT &&
			(session->user_corked_) &&
			session->user_traffic_ <= QUEUE_LIMIT / 2)
		{
			session->user_corked_ = false;
			session->user_conn_->BlockRecv(false);
		}
		if (session->user_traffic_ == 0 &&
			session->delete_pending_)
		{
//			std::cout << "no more pending task, inside session conn" << std::endl;
			session->session_conn_->Close();
			remoteinfo->users_.erase(usr);
		}
	});
}

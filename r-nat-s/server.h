#pragma once

#include "app1/app1.h"

class Server
{
public:
	explicit Server(asio::io_service& main_ioservice, std::vector<std::shared_ptr<asio::io_service>> ioservices);
	~Server();

	bool Init();
	void Start()
	{
		main_ioservice_.post(std::bind(&Server::__Start,this));
	}
	void Stop()
	{
		main_ioservice_.post(std::bind(&Server::__Stop, this));
	}
	asio::io_service& GetIoService()
	{
		auto next = next_ioservice_++;
		return *(ioservices_[next % ioservices_.size()]);
	}

protected:
	asio::io_service& main_ioservice_;
	size_t next_ioservice_{ 0 };
	std::vector<std::shared_ptr<asio::io_service>> ioservices_;

	friend class AppLogic1;
	std::shared_ptr<AppLogic1> app1_;

	void __Start();
	void __Stop();
};
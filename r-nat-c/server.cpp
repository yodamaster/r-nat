#include "stdafx.h"
#include "server.h"

#include "config.h"

Server::Server(asio::io_service& main_ioservice, std::vector<std::shared_ptr<asio::io_service>> ioservices)
	: main_ioservice_(main_ioservice)
	, ioservices_ (ioservices)
{
}

Server::~Server()
{

}

bool Server::Init()
{
	app1_= std::make_shared<AppLogic1>(this);
	
	return app1_->Init();
}

void Server::__Start()
{
	app1_->Start();
}

void Server::__Stop()
{
	app1_->Stop();
}

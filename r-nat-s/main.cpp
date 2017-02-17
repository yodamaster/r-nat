// Defines the entry point for the console application.
//

#include "stdafx.h"
#include "server.h"

#include "config.h"

class MyService
{
public:
	MyService() = default;
	void OnRuning()
	{
		LOG_FUNCTION();

		ioservice_works_.push_back(std::make_shared<asio::io_service::work>(main_ioservice_));

		std::size_t pool_size = std::thread::hardware_concurrency();
		for (std::size_t i = 0; i < pool_size; i++)
		{
			auto ioservice = std::make_shared<asio::io_service>();
			ioservices_.push_back(ioservice);
			ioservice_works_.push_back(std::make_shared<asio::io_service::work>(*ioservice));
		}

		try
		{
			server_ = std::make_shared<Server>(main_ioservice_, ioservices_);

			if (!server_->Init())
				throw "unable to init server";

			server_->Start();

			for (auto&& io_work : ioservice_works_)
			{
				auto service = &io_work->get_io_service();
				threads_.push_back(std::make_shared<std::thread>(
					[service]()
				{
					asio::error_code ignored_ec;
					service->run(ignored_ec);
				}));
			}

			for (auto&& thread : threads_)
			{
				thread->join();
			}
		}
		catch(std::exception e)
		{
			LOG_ERROR("exception: %s",e.what());
			std::cerr << "exception: " << e.what() << "\n";
			return;
		}
	}
	void OnStop()
	{
		LOG_FUNCTION();
		ioservice_works_.clear();
// 		for (std::size_t i = 0; i < ioservices_.size(); ++i)
// 		{
// 			ioservices_[i]->stop();
// 		}
// 		main_ioservice_.stop();
// 		for (std::size_t i = 0; i < threads_.size(); ++i)
// 		{
// 			threads_[i]->join();
// 		}
		server_->Stop();
	}

protected:
	asio::io_service main_ioservice_;
	std::vector<std::shared_ptr<asio::io_service>> ioservices_;
	std::vector<std::shared_ptr<asio::io_service::work>> ioservice_works_;
	std::vector<std::shared_ptr<std::thread>> threads_;
	std::shared_ptr<Server> server_;
};

int main(int argc, char* argv[])
{
	path_t configfile;
	if (argc>=2)
	{
		configfile = argv[1];
		if (access(configfile.c_str(),0))
		{
			std::cerr << L"Unable to open config file: " << configfile << std::endl;
			return -1;
		}
	}
	else
	{
		configfile = "config.xml";
		if (access(configfile.c_str(),0))
		{
			std::cerr << "Usage:" << argv[0] << " [config.xml]" << std::endl; 
			std::cerr << "Unable to open config file: " << configfile << std::endl;
			return -1;
		}
	}

	if (!ConfigManager::GetInstance()->init(configfile))
	{
		std::cerr << "Unable to init configuration from config file: " << configfile << std::endl;
		return -1;
	}

	auto&& locals = getConfig()->local_.locals_;
	for (auto&& local : locals)
	{
		std::cout << "Local: " << local.address_ << ":" << local.port_  << std::endl; 
	}

	MyService app;
	app.OnRuning();
	return 0;
}
#include <iostream>
#include <wiringPi.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <list>
#include <csignal>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma ide diagnostic ignored "EndlessLoop"

namespace domo
{
	struct client
	{
		client(int _socket, std::thread _thread)
		{
			socket = _socket;
			tReading = std::move(_thread);
		}

		int socket;
		std::thread tReading;
	};

	bool _server;
	int _port;

	std::list<client> _clientList;
	int _serverSocket;

	char _buffer[1024] = {0};
	std::list<std::string> msgBuffer;

	std::thread tAcceptClient;
	std::thread tReading;

	std::mutex m;

	char _led;
	char _button;

	bool _oldButtonStatus;
	bool _stateLED;

	sockaddr *sockAddrCast(sockaddr_in &_sockIn)
	{
		return static_cast<sockaddr *>(static_cast<void *>(&_sockIn));
	}

	__sighandler_t SignalInterrupt(int signum, char test)
	{
		std::cout << "\nCaught signal " << signum << std::endl;

		if(_server)
		{
			for (auto &_client : _clientList)
			{
				shutdown(_client.socket, 2);
				_client.tReading.join();
			}
			tAcceptClient.join();
		}
		else
		{
			shutdown(_serverSocket, 2);
			tReading.join();
		}
		exit(signum);
	}

	void Initialisation(int argc, char *argv[])
	{
		signal(SIGINT, reinterpret_cast<__sighandler_t>(SignalInterrupt));

		_server = false;
		_port = 9669;

		if (argc > 1)
		{
			for (int idxArg = 1; idxArg < argc; ++idxArg)
			{
				std::string tmp = argv[idxArg];
				size_t line = tmp.find_first_of('-');

				bool keepLoop = line != std::string::npos;

				while (keepLoop)
				{
					size_t idxEnd = tmp.find_first_of('-', line + 1);
					if (idxEnd == std::string::npos)
					{
						keepLoop = false;
						idxEnd = tmp.size() + 1;
					}

					std::string arg = tmp.substr(line, (idxEnd - 1) - line);
					if (arg == "-server")
					{
						_server = true;
					}

					line = idxEnd;
				}
			}
		}

		wiringPiSetup();

		_led = 8;
		_button = 9;

		/*if(server)
		{
			pinMode (_led, OUTPUT);
			digitalWrite(_led, LOW);

		}
		else
		{
			pinMode (_button, INPUT);
			oldButtonStatus = digitalRead(_button);
		}*/
	}

	void TCPHost()
	{
		sockaddr_in _sin{};
		int _socket;

		if ((_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			std::cerr << "cant initialize socket error " << errno << std::endl;
			shutdown(_socket, 2);
			exit(errno);
		}

		_sin.sin_family = AF_INET;
		_sin.sin_addr.s_addr = htonl(INADDR_ANY);
		_sin.sin_port = htons(_port);

		if (bind(_socket, sockAddrCast(_sin), sizeof(_sin)) < 0)
		{
			std::cerr << "cant bind socket" << std::endl;
			shutdown(_socket, 2);
			exit(errno);
		}

		listen(_socket, 0);

		std::cout << "waiting for client to connect..." << std::endl;

		tAcceptClient = std::thread([_sin, _socket]()
			{
				socklen_t _sinLength = sizeof(_sin);
				int clientSocket;
				while (true)
				{
					if ((clientSocket = accept(_socket,	(sockaddr *) &_sin, &_sinLength)) <	0)
					{

						std::lock_guard<std::mutex> l(m);
						std::cerr << "accept" << std::endl;
						shutdown(_socket, 2);
						exit(EXIT_FAILURE);
					}
					else
					{
						std::lock_guard<std::mutex> l(m);
						client newClient = client(clientSocket,
								std::thread([clientSocket]()
								{
									while (true)
									{
										std::lock_guard<std::mutex> l(m);
										read(clientSocket, _buffer, sizeof(_buffer));
										std::cout << _buffer << std::endl;

										std::string msg(_buffer);
										if(msg == "Hello from client")
										{
											msg = "Hello from server";
											send(clientSocket, msg.c_str(), msg.size(), 0);
											std::cout << "Hello message send" << std::endl;
										}
									}
								}));
						_clientList.push_back(std::move(newClient));
						std::cout << "client connected" << std::endl;
					}
				}
			});
	}

	void TCPClient()
	{
		sockaddr_in _sin{};

		if ((_serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			printf("cant initialize socket error %d\n", errno);
			shutdown(_serverSocket, 2);
			exit(EXIT_FAILURE);
		}

		_sin.sin_family = AF_INET;
		_sin.sin_addr.s_addr = inet_addr("192.168.0.33");
		_sin.sin_port = htons(_port);

		if (connect(_serverSocket, (struct sockaddr *) &_sin, sizeof(_sin)) < 0)
		{
			std::cerr << "Connection Failed" << std::endl;
			shutdown(_serverSocket, 2);
			exit(EXIT_FAILURE);
		}

		std::string hello = "Hello from client";
		send(_serverSocket, hello.c_str(), hello.size(), 0);

		std::cout << "Hello message send" << std::endl;

		tReading = std::thread([]()
			{
				while (true)
				{
					std::lock_guard<std::mutex> l(m);
					read(_serverSocket, _buffer, sizeof(_buffer));
					std::cout << _buffer << std::endl;
				}
			});
		std::cout << _buffer << std::endl;
	}
}

int main(int argc, char *argv[])
{
	domo::Initialisation(argc, argv);

	if (domo::_server)
	{
		domo::TCPHost();
	} else
	{
		domo::TCPClient();
	}

	while(true)
	{
//		if(_server)
//		{
//			recv(_socket, buffer, sizeof(buffer), 0);
//			_stateLED = buffer[0];
//			cout << "msg receive : " << _stateLED << endl;
//
//			digitalWrite(_led, _stateLED);
//		}
//		else
//		{
//
//			if(digitalRead(_button) == 0 && _oldButtonStatus == 1)
//			{
//				_stateLED = !_stateLED;
//				send(_socket, &_stateLED, sizeof(_stateLED), 0);
//				cout << "msg send : " << _stateLED << endl;
//			}
//
//			_oldButtonStatus = digitalRead(_button);
//		}
	}

	return 0;
}
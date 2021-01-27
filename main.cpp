#include <iostream>
#include <fstream>
#include <wiringPi.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma ide diagnostic ignored "EndlessLoop"

#define PORT 9669

#define LED 8
#define BUTTON 9

using namespace std;

enum eHostname
{
	HOST,
	CLIENT
}hostname;

int sock;
char buffer[1024] = {0};

bool oldButtonStatus;
bool stateLED;

void init()
{
	//Define if we are on the host or client raspberry
	ifstream hostFile;
	hostFile.open("/etc/hostname");

	if(hostFile.is_open())
	{
		string line;
		getline(hostFile, line);
		cout << line << endl;

		if(line == "domohost")
		{
			hostname = HOST;
		}
		else if(line == "domoclient")
		{
			hostname = CLIENT;
		}

		hostFile.close();
	}
	else cout << "WARN : Error open hostname file !" << endl;

	wiringPiSetup();

	if(hostname == HOST)
	{
		pinMode (LED, OUTPUT);
		digitalWrite(LED, LOW);

	}
	else if(hostname == CLIENT)
	{
		pinMode (BUTTON, INPUT);
		oldButtonStatus = digitalRead(BUTTON);
	}
}

void TCPHost()
{
	int server_fd;
	struct sockaddr_in address{};
	int opt = 1;
	int addrlen = sizeof(address);

	// Creating socket file descriptor
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	// Forcefully attaching socket to the port 8080
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
				   &opt, sizeof(opt)))
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( PORT );

	// Forcefully attaching socket to the port 8080
	if (bind(server_fd, (struct sockaddr *)&address,
			 sizeof(address))<0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	if (listen(server_fd, 3) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}
	if ((sock = accept(server_fd, (struct sockaddr *)&address,
							 (socklen_t*)&addrlen))<0)
	{
		perror("accept");
		exit(EXIT_FAILURE);
	}

	string hello = "Hello from server";

	read( sock , buffer, sizeof(buffer));
	printf("%s\n",buffer );

	send(sock , hello.c_str() , hello.size() , 0 );
	printf("Hello message sent\n");
}

void TCPClient()
{
	sockaddr_in serv_addr{};

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error \n");
		exit(EXIT_FAILURE);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if(inet_pton(AF_INET, "192.168.0.40", &serv_addr.sin_addr)<=0)
	{
		printf("\nInvalid address/ Address not supported \n");
		exit(EXIT_FAILURE);
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\nConnection Failed \n");
		exit(EXIT_FAILURE);
	}

	string hello = "Hello from client";

	send(sock , hello.c_str(), hello.size() , 0 );
	printf("Hello message sent\n");

	read(sock , buffer, sizeof(buffer));
	printf("%s\n", buffer);
}

int main() //int argc, char const *argv[])
{
	init();

	if(hostname == HOST)
	{
		TCPHost();

		stateLED = digitalRead(LED);
		cout << stateLED << endl;
		send(sock, &stateLED, sizeof(bool), 0);
	}
	else if (hostname == CLIENT)
	{
		TCPClient();

		recv(sock, buffer, sizeof(buffer), 0);
		stateLED = buffer[0];

		cout << stateLED << endl;
	}

	while(true)
	{
		if(hostname == HOST)
		{
			recv(sock, buffer, sizeof(buffer), 0);
			stateLED = buffer[0];

			digitalWrite(LED, stateLED);
		}
		else if(hostname == CLIENT)
		{

			if(digitalRead(BUTTON) == 0 && oldButtonStatus == 1)
			{
				stateLED = !stateLED;
				send(sock, &stateLED, sizeof(bool), 0);
			}

			oldButtonStatus = digitalRead(BUTTON);
		}
	}
}

#pragma clang diagnostic pop
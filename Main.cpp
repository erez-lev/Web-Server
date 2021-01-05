#include "WebServer.h"

void main()
{
	// Initialize Winsock (Windows Sockets).
	WSAData wsaData;
	CheckErrorsForWinsock(&wsaData);

	// Create a SOCKET object called listenSocket. 
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	CheckErrorsForSocket(listenSocket);

	// Create a sockaddr_in object called serverService. 
	sockaddr_in serverService;
	CreateSockAddressForServer(&serverService);

	// Bind the socket for client's requests.
	int bytesSent = bind(listenSocket, (SOCKADDR*)&serverService, sizeof(serverService));
	CheckSocketFunctionError(bytesSent, &listenSocket);

	// Listen on the Socket for incoming connections.
	bytesSent = listen(listenSocket, 5);
	CheckSocketFunctionError(bytesSent, &listenSocket);

	//SocketState sockets[MAX_SOCKETS] = { 0 };
	//int socketsCount = 0;
	SocketCollection sockList;
	initSockCollection(&sockList);
	addSocket(listenSocket, LISTEN, &sockList);

	// Accept connections and handles them one by one.
	while (true)
	{
		fd_set waitRecv;
		AddSocketsToRecvSet(&waitRecv, &sockList);

		fd_set waitSend;
		AddSocketsToSendSet(&waitSend, &sockList);

		int nfd;
		nfd = select(0, &waitRecv, &waitSend, NULL, NULL);
		CheckSocketFunctionError(nfd, &listenSocket);

		RecvStatusOperation(&waitRecv, &sockList, &nfd);
		TimeoutOperation(&sockList);
		SendStatusOperation(&waitSend, &sockList, &nfd);
	}

	// Need to close all new .html files!!!!!!!!
	// Closing connections and Winsock.
	ClosingSocket(&listenSocket);
}

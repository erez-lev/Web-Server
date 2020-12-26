#include "WebServer.h"

void CheckErrorsForWinsock(WSAData* wsaData)
{
	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), wsaData))
	{
		printf("Error at WSAStartup()\n");
		exit(0);
	}
}

void CheckErrorsForSocket(SOCKET connSocket)
{
	if (INVALID_SOCKET == connSocket)
	{
		printf("Error at socket(): %d\n", WSAGetLastError());
		WSACleanup();
		exit(0);
	}
}

void CreateSockAddressForServer(sockaddr_in* address)
{
	address->sin_family = AF_INET;
	address->sin_addr.s_addr = inet_addr("127.0.0.1");
	address->sin_port = htons(SERVER_PORT);
}

void CheckSocketFunctionError(int bytesSent, SOCKET* connSocket)
{
	if (SOCKET_ERROR == bytesSent)
	{
		printf("Error at socket function! - : %d\n", WSAGetLastError());
		closesocket(*connSocket);
		WSACleanup();
		exit(0);
	}
}

void AddSocketsToRecvSet(fd_set* waitRecv, SocketCollection* sockets)
{
	FD_ZERO(waitRecv);
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if ((sockets->sockets[i].recvStatus == LISTEN) || (sockets->sockets[i].recvStatus == RECEIVE))
			FD_SET(sockets->sockets[i].id, waitRecv);
	}
}

void AddSocketsToSendSet(fd_set* waitSend, SocketCollection* sockets)
{
	FD_ZERO(waitSend);
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets->sockets[i].sendStatus == SEND)
			FD_SET(sockets->sockets[i].id, waitSend);
	}
}

void RecvStatusOperation(fd_set* waitRecv, SocketCollection* sockets, int* nfd)
{
	int i;
	for (i = 0; i < MAX_SOCKETS && (*nfd) > 0; i++)
	{
		if (FD_ISSET(sockets->sockets[i].id, waitRecv))
		{
			(*nfd)--;
			switch (sockets->sockets[i].recvStatus)
			{
			case LISTEN:
				acceptConnection(i, sockets);
				break;

			case RECEIVE:
				receiveMessage(i, sockets);
				break;
			}
		}
	}
}

void SendStatusOperation(fd_set* waitSend, SocketCollection* sockets, int* nfd)
{
	int i;
	time_t currentTime;;
	for (i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets->sockets[i].sendStatus == IDLE)
		{
			currentTime = time(0);
			if (currentTime - sockets->sockets[i].lastActivityTimer > 180)
				removeSocket(i, sockets);
		}
	}

	for (i = 0; i < MAX_SOCKETS && (*nfd) > 0; i++)
	{
		if (FD_ISSET(sockets->sockets[i].id, waitSend))
		{
			(*nfd)--;
			switch (sockets->sockets[i].sendStatus)
			{
			case SEND:
				sendMessage(i, sockets);
				break;
			}
		}
	}
}

void ClosingSocket(SOCKET* listenSocket)
{
	printf("Time Server: Closing Connection.\n");
	closesocket(*listenSocket);
	WSACleanup();
}

void initSockCollection(SocketCollection* sockList)
{
	int i;
	sockList->sockets = (SocketState*)malloc(sizeof(SocketState) * MAX_SOCKETS);
	sockList->socketsCount = 0;
	for (i = 0; i < MAX_SOCKETS; i++)
		sockList->sockets[i].recvStatus = EMPTY;
}

bool addSocket(SOCKET id, int what, SocketCollection* sockets)
{
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets->sockets[i].recvStatus == EMPTY)
		{
			sockets->sockets[i].id = id;
			sockets->sockets[i].recvStatus = what;
			sockets->sockets[i].sendStatus = IDLE;
			sockets->sockets[i].lenOfMsg = 0;
			sockets->sockets[i].lastActivityTimer = time(0);
			sockets->socketsCount++;
			return (true);
		}
	}
	return (false);
}

void removeSocket(int index, SocketCollection* sockets)
{
	sockets->sockets[index].recvStatus = EMPTY;
	sockets->sockets[index].sendStatus = EMPTY;
	sockets->sockets[index].lastActivityTimer = 0;
	sockets->socketsCount--;
}

void acceptConnection(int index, SocketCollection* sockets)
{
	SOCKET id = sockets->sockets[index].id;
	sockets->sockets[index].lastActivityTimer = time(0);
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Time Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Time Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	// Set the socket to be in non-blocking mode.
	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
		cout << "Time Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;

	if (addSocket(msgSocket, RECEIVE, sockets) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}
	return;
}

void updateSocket(int index, SocketCollection* sockets, int send, int msg, int bufferLen)
{
	sockets->sockets[index].sendStatus = send;
	sockets->sockets[index].msgType = msg;
	memcpy(sockets->sockets[index].msgBuffer, &sockets->sockets[index].msgBuffer[bufferLen], sockets->sockets[index].lenOfMsg - bufferLen);
	sockets->sockets[index].lenOfMsg -= bufferLen;
}

void receiveMessage(int index, SocketCollection* sockets)
{
	SOCKET msgSocket = sockets->sockets[index].id;

	int len = sockets->sockets[index].lenOfMsg;
	int bytesRecv = recv(msgSocket, &sockets->sockets[index].msgBuffer[len], sizeof(sockets->sockets[index].msgBuffer) - len, 0);


	if (SOCKET_ERROR == bytesRecv)
	{// Check for an error.
		cout << "Time Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index, sockets);
		return;
	}
	if (bytesRecv == 0)
	{// Check if there's empty message - which means we need to close the socket
		closesocket(msgSocket);
		removeSocket(index, sockets);
		return;
	}
	else
	{
		sockets->sockets[index].lastActivityTimer = time(0);
		sockets->sockets[index].msgBuffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		cout << "Time Server: Recieved: " << bytesRecv << " bytes of \"" << &sockets->sockets[index].msgBuffer[len] << "\" message.\n";

		sockets->sockets[index].lenOfMsg += bytesRecv;

		if (sockets->sockets[index].lenOfMsg > 0)
		{
			if (strncmp(sockets->sockets[index].msgBuffer, HEAD_STR, HEAD_LEN) == 0)
			{
				updateSocket(index, sockets, SEND, HEAD, HEAD_LEN);
				return;
			}
			else if (strncmp(sockets->sockets[index].msgBuffer, GET_STR, GET_LEN) == 0)
			{
				updateSocket(index, sockets, SEND, GET, GET_LEN);
				return;
			}
			else if (strncmp(sockets->sockets[index].msgBuffer, PUT_STR, PUT_LEN) == 0)
			{
				updateSocket(index, sockets, SEND, PUT, PUT_LEN);
				return;
			}
			else if (strncmp(sockets->sockets[index].msgBuffer, DELETE_STR, DELETE_LEN) == 0)
			{
				updateSocket(index, sockets, SEND, _DELETE, DELETE_LEN);
				return;
			}
			else if (strncmp(sockets->sockets[index].msgBuffer, TRACE_STR, TRACE_LEN) == 0)
			{
				updateSocket(index, sockets, SEND, TRACE, TRACE_LEN);
				return;
			}
			else if (strncmp(sockets->sockets[index].msgBuffer, OPTIONS_STR, OPTIONS_LEN) == 0)
			{
				updateSocket(index, sockets, SEND, OPTIONS, OPTIONS_LEN);
				return;
			}
			else if (strncmp(sockets->sockets[index].msgBuffer, POST_STR, POST_LEN) == 0)
			{
				updateSocket(index, sockets, SEND, POST, POST_LEN);
				return;
			}
		}
	}
}

void sendMessage(int index, SocketCollection* sockets)
{
	int bytesSent = 0;
	char sendBuff[MAX_STR_LEN];
	sockets->sockets[index].lastActivityTimer = time(0);
	strcpy(sendBuff, HTTP_STR);		// Insert to "HTTP/1.1" to status code line.
	SOCKET msgSocket = sockets->sockets[index].id;
	handleClientRequest(sendBuff, sockets->sockets + index);


	bytesSent = send(msgSocket, sendBuff, (int)strlen(sendBuff), 0);
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "Time Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}

	cout << "Time Server: Sent: " << bytesSent << "\\" << strlen(sendBuff) << " bytes of \"" << sendBuff << "\" message.\n";

	sockets->sockets[index].sendStatus = IDLE;
}

void handleClientRequest(char* sendBuff, SocketState* socket)
{
	eClientRequest requestType = (eClientRequest)socket->msgType;

	switch (requestType)
	{
	case GET:
		handleGetRequest(sendBuff, socket);		// DONE
		break;
	case HEAD:
		handleHeadRequest(sendBuff, socket);	// DONE	
		break;
	case PUT:
		handlePutRequest(sendBuff, socket);
		break;
	case _DELETE:
		handleDeleteRequest(sendBuff, socket);
		break;
	case TRACE:
		handleTraceRequest(sendBuff, socket);
		break;
	case OPTIONS:
		handleOptionsRequest(sendBuff, socket);
		break;
	case POST:
		handlePostRequest(sendBuff, socket);
		break;
	}
	strcpy(socket->msgBuffer, sendBuff);
}

// Request Handlers Methods:
void handleGetRequest(char* sendBuff, SocketState* socket)
{
	handleGetAndHeadRequests(sendBuff, socket, GET); // , fileFromUrl, statusCode, responseBody);
}

void handleHeadRequest(char* sendBuff, SocketState* socket)
{
	handleGetAndHeadRequests(sendBuff, socket, HEAD);
}

void handlePutRequest(char* sendBuff, SocketState* socket)
{
	char fileFromUrl[STANDART_LEN];
	strcpy(fileFromUrl, getQueryFromUrl(socket->msgBuffer));
	char statusCode[STANDART_LEN], responseBody[MAX_STR_LEN] = "";

	if (isFileExists(fileFromUrl))
	{
		strcpy(statusCode, NO_CONTENT);
		strcat(statusCode, CRLF);
	}
	else
	{
		FILE* file = fopen(fileFromUrl, "w+");
		char fileBody[BUF_SIZE] = "";
		char delimiter[10];

		strcpy(delimiter, makeCRLFDelimiter());
		strcpy(fileBody, getAfterSpecificStrLocation(socket->msgBuffer, delimiter));
		fputs(fileBody, file);

		strcpy(statusCode, CREATED);
		strcat(statusCode, CRLF);
		fclose(file);
	}

	strcat(sendBuff, statusCode);
	strcat(sendBuff, "Content-Location: ");
	strcat(sendBuff, strcat(fileFromUrl, CRLF));
	strcat(sendBuff, createAndGetHeader(NO_RESPONSE_BODY, PUT));
}

void handleDeleteRequest(char* sendBuff, SocketState* socket)
{
	char fileFromUrl[STANDART_LEN];
	strcpy(fileFromUrl, getQueryFromUrl(socket->msgBuffer));
	char statusCode[STANDART_LEN], responseBody[MAX_STR_LEN] = "";

	if (!isFileExists(fileFromUrl))
	{
		// Make status code line ready.
		strcpy(statusCode, NO_CONTENT);
		strcat(statusCode, CRLF);
	}
	else
	{
		// Delete specific .html file from dir.
		remove(fileFromUrl);
		// Prepare body delete (response) message.
		FILE* file = fopen("delete.html", "r");
		char line[100];

		while (fgets(line, 100, file) != 0)
			strcat(responseBody, line);

		// Make status code line ready.
		strcpy(statusCode, OK_STATUS);
		strcat(statusCode, CRLF);
	}

	strcat(sendBuff, statusCode);
	strcat(sendBuff, createAndGetHeader(strlen(responseBody), _DELETE));
	strcat(sendBuff, responseBody);
}

void handleTraceRequest(char* sendBuff, SocketState* socket)
{
	strcat(sendBuff, OK_STATUS);
	strcat(sendBuff, createAndGetHeader(strlen(socket->msgBuffer), TRACE));
	strcat(sendBuff, socket->msgBuffer);
}

void handleOptionsRequest(char* sendBuff, SocketState* socket)
{
	strcat(sendBuff, NO_CONTENT);
	strcat(sendBuff, createAndGetHeader(strlen(socket->msgBuffer), OPTIONS));
	strcat(sendBuff, socket->msgBuffer);
}

void handlePostRequest(char* sendBuff, SocketState* socket)
{
	char fileFromUrl[STANDART_LEN];
	strcpy(fileFromUrl, getQueryFromUrl(socket->msgBuffer));
	char statusCode[STANDART_LEN], responseBody[MAX_STR_LEN] = "";

	if (!isFileExists(fileFromUrl))
	{
		strcpy(statusCode, NO_CONTENT);
		strcat(statusCode, CRLF);
	}
	else
	{
		FILE* file = fopen(fileFromUrl, "a");
		char fileBody[BUF_SIZE] = "";
		char delimiter[10];

		strcpy(delimiter, makeCRLFDelimiter());
		strcpy(fileBody, getAfterSpecificStrLocation(socket->msgBuffer, delimiter));
		fputs(fileBody, file);

		strcpy(statusCode, OK_STATUS);
		strcat(statusCode, CRLF);
		fclose(file);
	}

	strcat(sendBuff, statusCode);
	strcat(sendBuff, "Content-Location: ");
	strcat(sendBuff, strcat(fileFromUrl, CRLF));
	strcat(sendBuff, createAndGetHeader(NO_RESPONSE_BODY, POST));
}

char* createAndGetHeader(int contentLength, eClientRequest typeOfRequest)
{
	char header[MAX_STR_LEN] = "";
	char currentDate[STANDART_LEN];
	char contentLengthStr[STANDART_LEN];
	getTime(currentDate);
	_itoa(contentLength, contentLengthStr, 10);
	makeHeader(header, currentDate, contentLengthStr, typeOfRequest);

	return header;
}

// Helper Methods:
void handleGetAndHeadRequests(char* sendBuff, SocketState* socket, eClientRequest typeOfRequest)
{
	char fileFromUrl[STANDART_LEN];
	strcpy(fileFromUrl, getQueryFromUrl(socket->msgBuffer));
	char statusCode[STANDART_LEN], responseBody[MAX_STR_LEN] = "";

	if (!isFileExists(fileFromUrl))
	{
		strcpy(statusCode, NOT_FOUND);
		strcat(statusCode, CRLF);
	}
	else
	{
		FILE* file = fopen(fileFromUrl, "r");
		char line[100];

		while (fgets(line, 100, file) != 0)
			strcat(responseBody, line);

		strcpy(statusCode, OK_STATUS);
		strcat(statusCode, CRLF);

		fclose(file);
	}

	if (typeOfRequest == GET)
	{
		strcat(sendBuff, statusCode);
		strcat(sendBuff, createAndGetHeader(strlen(responseBody), GET));
		strcat(sendBuff, responseBody);
	}
	else if (typeOfRequest == HEAD)
	{
		strcat(sendBuff, statusCode);
		strcat(sendBuff, createAndGetHeader(strlen(responseBody), HEAD));
	}
}

char* getQueryFromUrl(char* msgBuff)
{
	msgBuff += 2;
	char resultOfQuery[STANDART_LEN] = "";

	for (int i = 0; i < strlen(msgBuff); i++)
	{
		if (msgBuff[i] != ' ')
			resultOfQuery[i] = msgBuff[i];
		else
			break;
	}

	strcat(resultOfQuery, ".html");
	return resultOfQuery;
}

Bool isFileExists(char* fileName)
{
	struct stat buffer;
	return (stat(fileName, &buffer) == 0);
}

void getTime(const char* iSendBuff)
// This method gets a pointer to 'const char*'.
{
	// Answer client's request by the current time.

	// Get the current time.
	time_t timer;
	time(&timer);
	// Parse the current time to printable string.
	strcpy((char*)iSendBuff, ctime(&timer));
}

void makeHeader(char* header, char* currentDate, char* contentLengthStr, eClientRequest typeOfRequest)
{
	strcpy(header, "Date: ");
	strcat(header, currentDate);

	if (typeOfRequest == OPTIONS)
		strcat(header, "Allowed: GET, HEAD, PUT, DELETE, TRACE, OPTIONS, POST\r\n");

	strcat(header, "Server: Lior's and Erez's server:)\r\n");
	strcat(header, "Content-Length: ");
	strcat(header, strcat(contentLengthStr, CRLF));

	if (typeOfRequest != TRACE)
		strcat(header, "Content-type: text/html");
	else
		strcat(header, "Content-type: message/http");

	strcat(header, CRLF);
	strcat(header, CRLF);
}

const char* getAfterSpecificStrLocation(char* msgBuff, char* delimiter)
{
	char result[BUF_SIZE];

	while (strncmp(msgBuff, delimiter, 4) != 0)
		msgBuff++;

	msgBuff += strlen(delimiter);
	strcpy(result, msgBuff);
	return result;
}

char* makeCRLFDelimiter()
{
	char delimiter[10];

	strcpy(delimiter, CRLF);
	strcat(delimiter, CRLF);

	return delimiter;
}

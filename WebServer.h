#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
using namespace std;
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <string.h>
#include <time.h>
#define TIME_PORT 27015
#define TRUE 1
#define FALSE 0
typedef int Bool;

#define MY_WEB		"mywebsite.html"
#define BUF_SIZE	8000
#define CRLF		"\r\n"

#define MAX_STR_LEN			1000
#define STANDART_LEN		50
#define NO_RESPONSE_BODY	0

#define HTTP_STR	"HTTP/1.1 "
#define OK_STATUS	"200 OK "
#define NOT_FOUND	"404 Not Found "
#define CREATED		"201 Created "
#define NO_CONTENT	"204 No Content "
#define NOT_ALLOWED  "405 Method Not Allowed "

#define SERVER_PORT 80	// Internet port.
#define MAX_SOCKETS 60  
#define EMPTY 0
#define LISTEN 1
#define RECEIVE 2
#define IDLE 3
#define SEND 4
#define SEND_TIME 1
#define SEND_SECONDS 2

#define HEAD_STR	"HEAD"
#define GET_STR		"GET"
#define PUT_STR		"PUT"
#define DELETE_STR	"DELETE"
#define TRACE_STR	"TRACE"
#define OPTIONS_STR	"OPTIONS"
#define POST_STR	"POST"

#define HEAD_LEN	4
#define GET_LEN		3
#define PUT_LEN		3
#define DELETE_LEN	6
#define TRACE_LEN	5
#define OPTIONS_LEN	7
#define POST_LEN	4



typedef enum eClientRequest {
	GET = 1,
	HEAD,
	PUT,
	_DELETE,
	TRACE,
	OPTIONS,
	POST
};

typedef struct socketState
{
	SOCKET id;				// Socket handle
	int	recvStatus;			// Receiving?
	int	sendStatus;			// Sending?
	int msgType;			// Sending sub-type
	char msgBuffer[BUF_SIZE];
	int lenOfMsg;
	time_t lastActivityTimer;
}SocketState;

typedef struct socketCollection
{
	SocketState* sockets;
	int socketsCount;
}SocketCollection;

void CheckErrorsForWinsock(WSAData* wsaData);
void CheckErrorsForSocket(SOCKET connSocket);
void CreateSockAddressForServer(sockaddr_in* address);
void CheckSocketFunctionError(int bytesSent, SOCKET* connSocket);

void initSockCollection(SocketCollection* sockList);
void AddSocketsToRecvSet(fd_set* waitRecv, SocketCollection* sockets);
void AddSocketsToSendSet(fd_set* waitSend, SocketCollection* sockets);
void RecvStatusOperation(fd_set* waitSend, SocketCollection* sockets, int* nfd);
void SendStatusOperation(fd_set* waitSend, SocketCollection* sockets, int* nfd);
void ClosingSocket(SOCKET* listenSocket);

bool addSocket(SOCKET id, int what, SocketCollection* sockets);
void removeSocket(int index, SocketCollection* sockets);
void acceptConnection(int index, SocketCollection* socket);
void receiveMessage(int index, SocketCollection* socket);
void sendMessage(int index, SocketCollection* socket);

// Request Handlers Methods:
void handleClientRequest(char* sendBuff, SocketState* socket);
void handleGetRequest(char* sendBuff, SocketState* socket);
void handleHeadRequest(char* sendBuff, SocketState* socket);
void handlePutRequest(char* sendBuff, SocketState* socket);
void handleDeleteRequest(char* sendBuff, SocketState* socket);
void handleTraceRequest(char* sendBuff, SocketState* socket);
void handleOptionsRequest(char* sendBuff, SocketState* socket);
void handlePostRequest(char* sendBuff, SocketState* socket);

// Helper Methods:
char* createAndGetHeader(int contentLength, eClientRequest typeOfRequest);
void getTime(const char* iSendBuff);
void handleGetAndHeadRequests(char* sendBuff, SocketState* socket, eClientRequest typeOfRequest);
char* getQueryFromUrl(char* msgBuff);
Bool isFileExists(char* fileName);
void makeHeader(char* header, char* currentDate, char* contentLengthStr, eClientRequest typeOfRequest);
const char* getAfterSpecificStrLocation(char* msgBuff, char* delimiter);
char* makeCRLFDelimiter();

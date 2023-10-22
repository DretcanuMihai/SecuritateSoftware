#pragma once
#include "connection.h"
#include "logger.h"

#define DEFAULT_PORT "25565"
#define DEFAULT_ADDRESS "localhost"

int startWSA(WSADATA* wsaData) {
	log("Starting WSA\n");
	int wsaStartupSuccess = WSAStartup(MAKEWORD(2, 2), wsaData);
	log("WSA StartUp Code:%d\n", wsaStartupSuccess);
	return wsaStartupSuccess;
}

void prepareHints(struct addrinfo* hints) {
	log("Preparing hints\n");
	ZeroMemory(hints, sizeof(struct addrinfo));
	hints->ai_family = AF_INET;
	hints->ai_socktype = SOCK_STREAM;
	hints->ai_protocol = IPPROTO_TCP;
}

int getAddrInfo(struct addrinfo* hints, struct addrinfo** result) {
	log("Getting addr info\n");
	int status = getaddrinfo(DEFAULT_ADDRESS, DEFAULT_PORT, hints, result);
	log("Result:%d\n", status);

	if (status != 0) {
		WSACleanup();
	}
	return status;
}

SOCKET createSocket(struct addrinfo* result) {
	struct addrinfo* ptr = result;
	SOCKET mySocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
	if (mySocket == INVALID_SOCKET) {
		WSACleanup();
	}
	int iResult = connect(mySocket, ptr->ai_addr, (int)ptr->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		closesocket(mySocket);
		WSACleanup();
		mySocket = INVALID_SOCKET;
	}
	return mySocket;
}

int communicate(SOCKET socket, HANDLE srcFile, char destPath[MAX_PATH]) {
	char myByte;
	char sendBuffer[256] = { 0 };
	DWORD readBytes;

	myByte = strlen(destPath);
	log("Sending size:%d\n", myByte);
	if (send(socket, &myByte, 1, 0) == SOCKET_ERROR) { return 4; }
	log("Sending destPath:%s\n", destPath);
	if (send(socket, destPath, strlen(destPath), 0) == SOCKET_ERROR) { return 4; }
	if (recv(socket, &myByte, 1, 0) <= 0) {
		log("Error when receiving response\n");
		return 4;
	}
	//if myByte is a valid error value, it will preserve when converted to int
	//if it isn't a valid protocol error value, I don't care that much what it is,
	//so I decided to just let it be casted to int
	if (myByte != 0) { return myByte; }

	while (1) {
		log("\n");
		BOOL readFileSuccess = ReadFile(srcFile, sendBuffer, 255, &readBytes, NULL);
		log("Read File Success:%d\n", readFileSuccess);
		log("Bytes read:%d\n", readBytes);
		if ((readFileSuccess == FALSE) || (readBytes > 255)) {
			return 4;
		}
		//readBytes is guaranteed to be representable on a byte
		//when converting, it's just trunciated, so I won't lose info
		myByte = readBytes;
		log("Sending Chunk size:%d\n", myByte);
		if (send(socket, &myByte, 1, 0) == SOCKET_ERROR) { return 4; }
		if (myByte != 0) {
			log("Sending Chunk:%s\n", sendBuffer);
			if (send(socket, sendBuffer, readBytes, 0) == SOCKET_ERROR) { return 4; }
		}

		if (recv(socket, &myByte, 1, 0) <= 0) {
			log("Error when receiving response\n");
			return 4;
		}
		log("Received byte:%d\n", myByte);
		//if myByte is a valid error value, it will preserve when converted to int
		//if it isn't a valid protocol error value, I don't care that much what it is,
		//so I decided to just let it be casted to int
		if (myByte != 0) { return myByte; }
		else if (readBytes == 0) { return 0; }
	}
}

int communicateWithServer(HANDLE srcFile, char destPath[MAX_PATH]) {
	WSADATA wsaData;
	struct addrinfo* result = NULL, * ptr = NULL, hints;

	if (startWSA(&wsaData) != 0) {
		return 4;
	}
	prepareHints(&hints);

	if (getAddrInfo(&hints, &result) != 0) {
		return 4;
	}

	SOCKET socket = createSocket(result);
	if (socket == INVALID_SOCKET) {
		return 4;
	}

	freeaddrinfo(result);

	int communicationResult = communicate(socket, srcFile, destPath);
	closesocket(socket);
	WSACleanup();
	return communicationResult;
}
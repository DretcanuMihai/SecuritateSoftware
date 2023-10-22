#pragma once
#include"console.h"

#include "filesystem.h"
#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

void resetSTDIN() {
	log("Flushing stdin\n");
	fseek(stdin, 0, SEEK_END);
}

int readPath(char path[MAX_PATH]) {
	int readSuccess = scanf_s("%s", path, sizeof(char)*MAX_PATH); // guaranteed null terminated
	resetSTDIN();

	log("Path:%s\n", path);
	log("Read Success:%d\n", readSuccess);

	return readSuccess;
}

void treatUser() {
	//on Windows, processes run by default with the rights of the user that started it, so no need to drop privilegies, but also
	//might spawn trouble with creating sockets??
	char srcPath[MAX_PATH] = { 0 };
	char destPath[MAX_PATH] = { 0 };

	printf("Enter source path:");
	if (!readPath(srcPath)) {
		printf("Path is invalid\n");
		return;
	}

	printf("Enter destination path:");
	if ((!readPath(destPath))||(strlen(destPath)>255)) {
		printf("Path is invalid\n");
		return;
	}

	HANDLE srcFile;
	char nrmSrcPath[MAX_PATH] = { 0 };

	int openSrcFileSuccess = openFile(&srcFile, srcPath, nrmSrcPath, FILE_ATTRIBUTE_NORMAL, 0);
	if (openSrcFileSuccess == 1) {
		printf("Couldn't open specified file\n");
		return;
	}
	if (openSrcFileSuccess == 2) {
		printf("Internal error\n");
		return;
	}


	HANDLE dirFile;
	char nrmDirPath[MAX_PATH] = { 0 };

	int openDirSuccess = openFile(&dirFile, ".", nrmDirPath, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, FILE_SHARE_READ | FILE_SHARE_WRITE);
	if (openDirSuccess != 0) {
		printf("Internal error\n");
		closeHandle(srcFile);
		return;
	}

	//we only needed current directory for the path
	closeHandle(dirFile);

	//if file is symlink, the normalized path is the absolute path to the actual file
	if (strstr(nrmSrcPath, nrmDirPath) != nrmSrcPath) {
		closeHandle(srcFile);
		printf("You can't use this file\n");
		return;
	}

	int communicationSuccess = communicateWithServer(srcFile, destPath);
	log("Communication Success:%d\n", communicationSuccess);
	if (communicationSuccess == 0) {
		printf("File transfer succeeded\n");
	}
	else {
		printf("File transfer failed\n");
		if (communicationSuccess == 1) {
			printf("Invalid path\n");
		}
		else if (communicationSuccess == 2) {
			printf("File already exists\n");
		}
		else if (communicationSuccess == 3) {
			printf("Operation aborted\n");
		}
		else {
			printf("Unknown reason\n");
		}
	}
	closeHandle(srcFile);
	return;
}

void runConsole() {
	printf("Client started\n");
	char command[5] = { 0 };
	while (1) {
		printf("Enter 'send' to send file or 'exit' to exit\n>>");
		//the upperbound is exclusive, so I have to give exactly sizeof(command) - any less and it won't
		//actually read 'send'/'exit' if I write them
		int success = scanf_s("%s", command, sizeof(command));
		resetSTDIN();
		log("User input:%s\n", command);
		log("Success:%d\n", success);
		if (strcmp(command, "send") == 0) {
			treatUser();
		}
		else if (strcmp(command, "exit") == 0) {
			break;
		}
		else {
			printf("Unrecognized command\n");
		}
	}
	printf("Client stopped\n");
}
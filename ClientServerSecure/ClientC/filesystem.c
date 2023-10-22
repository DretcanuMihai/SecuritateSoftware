#pragma once
#include "filesystem.h"
#include "logger.h"

int openFile(HANDLE* fileHandlePtr, char path[MAX_PATH], char normalizedPath[MAX_PATH], DWORD flags, DWORD shareMode) {

	log("Entered openFile for:%s\n", path);

	HANDLE fileHandle = CreateFileA(path, GENERIC_READ, shareMode, NULL, OPEN_EXISTING, flags, NULL);

	if (fileHandle == INVALID_HANDLE_VALUE) {
		log("Error %d while opening file\n", GetLastError());
		return 1;
	}
	log("File opened\n");

	DWORD normalizationResult = GetFinalPathNameByHandleA(fileHandle, normalizedPath, sizeof(char) * MAX_PATH, FILE_NAME_NORMALIZED);
	log("Normalization Result: %d\n", normalizationResult);

	if (normalizationResult > MAX_PATH) {
		log("Couldn't normalize path\n");
		closeHandle(fileHandle);
		return 2;
	}
	log("Normalized path:%s\n", normalizedPath);
	*fileHandlePtr = fileHandle;
	return 0;
}

void closeHandle(HANDLE fileHandle) {
	BOOL closeHandleSuccess = CloseHandle(fileHandle);
	log("Close handle result:%d\n", closeHandleSuccess);
}
#pragma once
#include <windows.h>


int openFile(HANDLE* fileHandlePtr, char path[MAX_PATH], char normalizedPath[MAX_PATH], DWORD flags, DWORD shareMode);

void closeHandle(HANDLE fileHandle);
#pragma once
#include <ws2tcpip.h>
#include <windows.h>

int communicateWithServer(HANDLE srcFile,char destPath[MAX_PATH]);

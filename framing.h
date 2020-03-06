#ifndef __FRAMING_H_INCLUDED__
#define __FRAMING_H_INCLUDED__

#include <BLEServer.h>

#define ATT_MTU 20

#define UPDATE_SUCCESS 0
#define UPDATE_ERROR 1
#define UPDATE_FRAMING 2

enum processingStatus {Idle, IsProcessing, CmdReady, ResultReady};

extern std::string response;

uint8_t update(std::string value);
void processCMD();

#endif

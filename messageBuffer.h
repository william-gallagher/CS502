#include "os_globals.h"
#include "global.h"


int CreateMessageQueue();

void osSendMessage(long TargetPID, char* MessageBuffer, long MessageLength, long* ReturnError);

void osReceiveMessage(long SourcePID, char* MessageBuffer,
		      long MessRecLength, long* MessSendLength,
		      long* SenderPID, long* ReturnError);

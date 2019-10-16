/*
messageBuffer.h

This file is the include file for everything to do with the Message
Buffer.
*/

#ifndef MESSAGE_BUFFER_H
#define MESSAGE_BUFFER_H


void CreateMessageBuffer();
void osSendMessage(long TargetPID, char *MessageBuffer,
		   long MessageLength, long *ReturnError);
void osReceiveMessage(long SourcePID, char *MessageBuffer,
		      long MessRecLength, long *MessSendLength,
		      long *SenderPID, long *ReturnError);

#endif //MESSAGE_BUFFER_H

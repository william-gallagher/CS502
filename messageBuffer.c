#include "timerQueue.h"
#include "readyQueue.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "protos.h"
#include "process.h"
#include "os_globals.h"
#include <unistd.h>



//======================================================================
/*

Message Buffer Functions

*/



int CreateMessageQueue(){
  message_queue_id = QCreate("MQueue");
  return message_queue_id;
}





/*
Counts the number of messages in the Message Buffer
*/
long CountMessagesInBuffer(){

  long MessageCount = 0;
  long Index = 0;
  MQ_ELEMENT *mqe;
  
  
  mqe = QWalk(message_queue_id, Index);
  
  while((long)mqe != -1){
    Index++;
    MessageCount++;
    mqe = QWalk(message_queue_id, Index);
  }

  return MessageCount;
}


void AddToMessageBuffer(MQ_ELEMENT* mqe){

  LockLocation(MESSAGE_LOCK);

  QInsertOnTail(message_queue_id, mqe);

  UnlockLocation(MESSAGE_LOCK);
}

MQ_ELEMENT* GetMessageFromBuffer(long SourcePID, long CurrentPID){

  aprintf("In GetMessage %ld is the SenderPID that I am looking for\n", SourcePID);
  
  MQ_ELEMENT* mqe;
  int Index = 0;

  LockLocation(MESSAGE_LOCK);
  
  QPrint(message_queue_id);
  
  mqe = QWalk(message_queue_id, Index);
  
  while((long)mqe != -1){
    aprintf("Here is the source pid %ld\nhere is the target pid %ld\n\n", mqe->sender_pid,  mqe->target_pid);
    if((mqe->target_pid == CurrentPID && mqe->sender_pid == SourcePID)||
       ((mqe->target_pid == -1 || mqe->target_pid == CurrentPID)  && SourcePID == -1)){

      mqe = QRemoveItem(message_queue_id, (void *)mqe);
      aprintf("Found a message! %ld is the PID \n\n", mqe->target_pid);
      break;
    }
    Index++;
    mqe = QWalk(message_queue_id, Index);
  }

  UnlockLocation(MESSAGE_LOCK);
  
  return mqe;

}
  


void osSendMessage(long TargetPID, char* MessageBuffer, long MessageLength, long* ReturnError){

  PROCESS_CONTROL_BLOCK *process;
  
    //check to see if process exists
  if(TargetPID != -1){
    process = GetPCB(TargetPID);
    if(process == NULL){
      aprintf("Cannot send message to process of a PID that DNE! PID %ld\n\n", TargetPID);
      (*ReturnError) = 1;
      return;
    }
  }

  //Make sure MessageLength is in the right range
  if(MessageLength < 0 || MessageLength > MAX_MESSAGE_LENGTH){
    aprintf("Message Length out of range!\n\n");
    (*ReturnError) = 1;
    return;
  }

  //Check to make sure there is room in the Message Buffer
  if(CountMessagesInBuffer() >= MAX_MESSAGES_IN_BUFFER){
    aprintf("Message Buffer Full!\n\n");
    (*ReturnError) = 1;
    return;
  }

  MQ_ELEMENT *mqe = malloc(sizeof(MQ_ELEMENT));
  mqe->target_pid = TargetPID;
  strcpy(mqe->message, MessageBuffer);
  mqe->message_length = MessageLength;
  mqe->sender_pid = GetCurrentPID();
  AddToMessageBuffer(mqe);

  //check to see what the state of the target process is
  if(TargetPID != -1){
    INT32 State;
    long ReturnErrorResume;
  
    GetProcessState(TargetPID, &State);
    if(State == SUSPENDED_WAITING_FOR_MESSAGE){
      osResumeProcess(TargetPID, &ReturnErrorResume);
      if(ReturnErrorResume == 1){
	aprintf("Failure to Resume Process in osSendMessage\n\n");
      }
      ChangeProcessState(TargetPID, READY);
    }

    //set flag in PCB to let process know it has a message waiting
    process->waiting_for_message = TRUE;
  }

  //return success
  (*ReturnError) = 0;
}

void osReceiveMessage(long SourcePID, char* MessageBuffer,
		      long MessRecLength, long* MessSendLength,
		      long* SenderPID, long* ReturnError){

  long CurrentPID = GetCurrentPID();
  
  //check to see if process exists. -1 always allowed.
  if(SourcePID != -1){
    
    PROCESS_CONTROL_BLOCK* process = GetPCB(SourcePID);;

    if(process == NULL){
      aprintf("Cannot receive message from process that DNE!\n\n");
      (*ReturnError) = 1;
      return;
    }
  }

  //Make sure MessageLength is in the right range
  if(MessRecLength < 0 || MessRecLength > MAX_MESSAGE_LENGTH){
    aprintf("Can't Receive Message Length out of range!\n\n");
    (*ReturnError) = 1;
    return;
  }

  //check Message Buffer for message. If it there is none suspend process

  MQ_ELEMENT *mqe = GetMessageFromBuffer(SourcePID, CurrentPID);
  
  if((long)mqe == -1){
    ChangeProcessState(CurrentPID, SUSPENDED_WAITING_FOR_MESSAGE);
    dispatcher();
    mqe = GetMessageFromBuffer(SourcePID, CurrentPID);
  }

  strcpy(MessageBuffer, mqe->message);
  (*MessSendLength) = mqe->message_length;
  (*SenderPID) = mqe->sender_pid;
  (*ReturnError) = 0;
}

/*
messageBuffer.c

This file holds the functions that deal with the Message Buffer and the
Send and Receive Message functionality. Note that the Message Buffer 
utilizes the functions found in the Queue Manager. The Message Buffer 
uses the same functions as the other queues. However, there is no 
priority in the Message Buffer. The whole buffer must be searched for a 
message.
*/

#include "timerQueue.h"
#include "readyQueue.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "protos.h"
#include "process.h"
#include "osGlobals.h"
#include <unistd.h>
#include "osSchedulePrinter.h"

/*
This function creates the Message Buffer using the functions found in the
Queue Manager. 
*/
void CreateMessageBuffer(){
  message_buffer_id = QCreate("MBuffer");
  if(message_buffer_id == -1){
    aprintf("\n\nError: Unable to create Message Buffer!\n\n");
  }
}

/*
This function counts the number of messages in the Message Buffer and 
returns the answer.
*/
long CountMessagesInBuffer(){

  long MessageCount = 0;
  long Index = 0;
  MQ_ELEMENT *mqe;

  //Step through the Message Buffer
  mqe = QWalk(message_buffer_id, Index);
  
  while((long)mqe != -1){
    Index++;
    MessageCount++;
    mqe = QWalk(message_buffer_id, Index);
  }

  return MessageCount;
}

/*
This function adds to the Message Buffer. Messages are always put on the
tail of the buffer.
*/
void AddToMessageBuffer(MQ_ELEMENT* mqe){

  LockLocation(MESSAGE_LOCK);
  QInsertOnTail(message_buffer_id, mqe);
  UnlockLocation(MESSAGE_LOCK);
}

/*
This function looks for a message in the Message Buffer. It walks
through the Message Buffer and looks for a matching message. There are 
a couple of cases:
The message TargetPID equals the Current PID and the message SenderPID 
equals ...

*/
MQ_ELEMENT* GetMessageFromBuffer(long SourcePID, long CurrentPID){
  
  MQ_ELEMENT *mqe;
  int Index = 0;

  LockLocation(MESSAGE_LOCK);

  mqe = QWalk(message_buffer_id, Index);
  
  while((long)mqe != -1){
   
    if((mqe->target_pid == CurrentPID && mqe->sender_pid == SourcePID)||
       ((mqe->target_pid == -1 || mqe->target_pid == CurrentPID)
	&& SourcePID == -1)){

      mqe = QRemoveItem(message_buffer_id, (void *)mqe);
      //aprintf("Found a message! %ld is the PID \n\n", mqe->target_pid);
      break;
    }
    Index++;
    mqe = QWalk(message_buffer_id, Index);
  }

  UnlockLocation(MESSAGE_LOCK);
  
  return mqe;
}
  
/*
Handle the Send Message System Call. 
*/
void osSendMessage(long TargetPID, char *MessageBuffer,
		   long MessageLength, long *ReturnError){

  PROCESS_CONTROL_BLOCK *pcb;
  
  //check to see if process exists
  if(TargetPID != -1){
    pcb = GetPCB(TargetPID);
    if(pcb == NULL){
      //aprintf("\n\nError: Cannot send message to process of a PID that DNE! PID %ld\n\n", TargetPID);
      (*ReturnError) = ERR_BAD_PARAM;
      return;
    }
  }

  //Do some error checking:
  //Make sure MessageLength is in the right range
  if(MessageLength < 0 || MessageLength > MAX_MESSAGE_LENGTH){
    //aprintf("Message Length out of range!\n\n");
    (*ReturnError) = ERR_BAD_PARAM;
    return;
  }

  //Check to make sure there is room in the Message Buffer
  if(CountMessagesInBuffer() >= MAX_MESSAGES_IN_BUFFER){
    //aprintf("Message Buffer Full!\n\n");
    (*ReturnError) = ERR_BAD_PARAM;
    return;
  }

  //Create the struct to hold all the message data.
  MQ_ELEMENT *mqe = malloc(sizeof(MQ_ELEMENT));
  mqe->target_pid = TargetPID;
  strcpy(mqe->message, MessageBuffer);
  mqe->message_length = MessageLength;
  mqe->sender_pid = GetCurrentPID();
  AddToMessageBuffer(mqe);

  //check to see what the state of the target process is.
  if(TargetPID != -1){
    INT32 State;
    long ReturnErrorResume;
  
    GetProcessState(TargetPID, &State);
    if(State == SUSPENDED_WAITING_FOR_MESSAGE){
      osResumeProcess(TargetPID, &ReturnErrorResume);
      if(ReturnErrorResume == 1){
	aprintf("\n\nError: Failure to Resume Process\n\n");
      }
      ChangeProcessState(TargetPID, READY);
    }
    //set flag in PCB to let process know it has a message waiting
    pcb->waiting_for_message = TRUE;
  }
  //return success
  (*ReturnError) = ERR_SUCCESS;
}

/*
This function handles the System Call for receive message. It checks
some parameters and returns an error if these are not up to snuff.
Then if looks through the Message Buffer for a message that matches the
criterion. If no message is present the process suspends itself.
*/
void osReceiveMessage(long SourcePID, char *MessageBuffer,
		      long MessRecLength, long *MessSendLength,
		      long *SenderPID, long *ReturnError){

  long CurrentPID = GetCurrentPID();
  
  //check to see if process exists. -1 always allowed.
  if(SourcePID != -1){
    
    PROCESS_CONTROL_BLOCK* process = GetPCB(SourcePID);;

    if(process == NULL){
      //aprintf("Cannot receive message from process that DNE!\n\n");
      (*ReturnError) = ERR_BAD_PARAM;
      return;
    }
  }

  //Make sure MessageLength is in the right range
  if(MessRecLength < 0 || MessRecLength > MAX_MESSAGE_LENGTH){
    //aprintf("Can't Receive Message Length out of range!\n\n");
    (*ReturnError) = ERR_BAD_PARAM;
    return;
  }

  //check Message Buffer for message.
  //If it there is none suspend process.
  MQ_ELEMENT *mqe = GetMessageFromBuffer(SourcePID, CurrentPID);
  
  if((long)mqe == -1){
    ChangeProcessState(CurrentPID, SUSPENDED_WAITING_FOR_MESSAGE);
    osPrintState("SUS MES", CurrentPID, CurrentPID);
    dispatcher();
    mqe = GetMessageFromBuffer(SourcePID, CurrentPID);
  }
  //Must check to make sure there is room in the Receiving Buffer for the
  //the message. If not return an error
  if(mqe->message_length > MessRecLength){
    (*ReturnError) = 1;
  }
  else{
    strcpy(MessageBuffer, mqe->message);
    (*MessSendLength) = mqe->message_length;
    (*SenderPID) = mqe->sender_pid;
    (*ReturnError) = 0;
  }
}

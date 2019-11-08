/*



*/

#include <string.h>
#include <stdlib.h>
#include "protos.h"
#include "process.h"
#include "readyQueue.h"
#include "timerQueue.h" 
#include "osGlobals.h"
//#include "osSchedulePrinter.h"

void InitializeFrameManager(){

  for(INT16 i=0; i<16; i++){
    FrameManager[i] = 0;
  }
}

void GetAvailablePhysicalFrame(INT16 *Frame){

  for(INT16 i=0; i<NUMBER_PHYSICAL_PAGES; i++){

    if(FrameManager[i] == 0){
      FrameManager[i] = 1; //signify in use
      (*Frame) = i;
      return;
    }
  }

  //no physical frames left. Set return to -1
  (*Frame) = -1;
}

void SetValidBit(INT16 *PageEntry){

  (*PageEntry) |= PTBL_VALID_BIT;
}

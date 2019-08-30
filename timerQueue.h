//Include file for everything to do with the timer queue


//The struct that holds the data for processes in the timer queue
typedef struct{
	long context;
	long wakeup_time;
}TQ_ELEMENT;

int timer_queue_id;

//use functions in QueueManager.c to create a timer for storing 
int CreateTimerQueue(){
  timer_queue_id = QCreate("TQueue");
  return timer_queue_id;
}


  

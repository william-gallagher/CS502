//Include file for everything to do with processes

//Should there be a max number of processes????
#define MAXPROCESSES 15



//name has limit of 100 for now. Seems like a hack
//should priority be an INT32 or long??????
typedef struct{
  INT32 in_use;
  INT32 parent;
  long idnum;
  long context;
  char name[100];
  INT32 priority;
  INT32 suspended;
} PROCESS_CONTROL_BLOCK;

typedef struct{
  INT32 nextid;
  INT32 current;
  PROCESS_CONTROL_BLOCK PCB[MAXPROCESSES];
} PROCESSES_INFORMATION;

//make this outside a function for now to allow everyone to use
PROCESSES_INFORMATION* PRO_INFO;

//Test function that prints the states of all the processes
void ProcessesState();


//need to remember to free at end somehow
  void CreatePRO_INFO();
  
long osCreateProcess(char* name, long context, INT32 parent);

void osSuspendProcess(long PID, long* return_error);

void osResumeProcess(long PID, long* return_error);

void getProcessID(SYSTEM_CALL_DATA* scd);

int CheckProcessName(char* name);

/*This function checks to see if MAX_PROCESSES has been reached. If the OS has reached its limit 0 is returned. Otherwise 1 is returned. The function steps through the PRO_INFO to see if there is an open slot.
 */
int CheckProcessCount();

//need a getcontext function

void CreateProcess(char* name, long start_address, long priority, INT32 parent, long* process_id, long* return_error);

void TerminateProcess(long process_id);


void TerminateChildren(long process_id);
  
int CheckProcessName(char* name);

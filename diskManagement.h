

unsigned int InodeArray[32];



void osFormatDisk(long DiskID, long *ReturnError);
void osOpenDirectory(long DiskID, char *FileName, long *ReturnError);
void osCreateFile(char *FilenName, long *ReturnError, INT32 FileOrDir);
void InitializeInodes();
void GetInode(unsigned char *NewInode);

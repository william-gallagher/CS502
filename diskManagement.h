

#ifndef DISK_MANAGE_H
#define DISK_MANAGE_H

#define FILE 0
#define DIR 1

unsigned int InodeArray[MAX_NUMBER_INODES];


void osFormatDisk(long DiskID, long *ReturnError);
void osOpenDirectory(long DiskID, char *FileName, long *ReturnError);
DISK_BLOCK* osCreateFile(char *FilenName, long *ReturnError, INT32 FileOrDir);
void osOpenFile(char *FileName, long *Inode, long *ReturnError);
void osWriteFile(long Inode, long Index, char *WriteBuffer, long *ReturnError);
void osReadFile(long Inode, long Index, char *WriteBuffer, long *ReturnError);
void osCloseFile(long Inode, long *ReturnError);
void osPrintCurrentDirContents(long *ReturnError);
void InitializeInodes();
void GetInode(unsigned char *NewInode);
DISK_CACHE* CreateDiskCache();

#endif //DISK_MANAGE_H

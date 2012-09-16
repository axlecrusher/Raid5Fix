#include "FileHandle.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <list>

FileHandle Log;
FileHandle Drives[100];
FileHandle Fout;

char* NextLine(FILE* f);
bool IsFailedBlock(char x);
bool ParseNumbers(const char* line, uint64_t& position, uint64_t& length);
uint64_t Hex64(char* s, char* end);
void OpenLogFile(const char* fn);
void OpenDrives(const char* fn, const char* fn_out);
void OpenFileHandle(const char* fn, const char* mode, FileHandle& fh);
void RepairSectors(uint64_t p, uint64_t);
int BufferedRead(void* buffer, unsigned int bSize, uint64_t length, FileHandle& f);
int ComputeParityChunk(char* buffer, uint64_t l);
int WriteChunk(char* buffer, int l);
void SeekDrive(uint64_t pos,FileHandle& f);

int main(int argv, char** argc)
{

	if (argv != 4)
	{
		printf("Use: raidfix DeviceFileList ddrescueLog brokenDevice\n");
		return 0;
	}

	OpenLogFile(argc[2]);
	OpenDrives(argc[1],argc[3]);

	for (char* line = NextLine(Log.File); line != NULL; line = NextLine(Log.File))
	{
		size_t length = strlen(line);
		if (length == 0) continue;

		printf("Processing %s", line);

		if ( IsFailedBlock(line[length-1]) )
		{
			printf(" Repairing...");
			uint64_t p,l;
			if ( !ParseNumbers(line,p,l) )
			{
				fprintf(stderr,"\nError parsing hex numbers from log:\"%s\"\n",line);
				exit(1);
			}
			RepairSectors(p,l);
		}
		else
		{
			printf(" OK!");			
		}

		printf("\n");
		free(line);
	}
}

void OpenLogFile(const char* fn)
{
	Log.Name = fn;
	Log.File = fopen(fn, "r");
	if ( !Log.IsOpen() )
	{
		fprintf(stderr,"Failed to open log file \"%s\"\n", fn);
		exit(1);
	}
	char* trash = NextLine(Log.File); //trash first line
	free(trash);
}

void OpenDrives(const char* fn, const char* fn_out)
{
	int i = 0;
	FileHandle dlist;
	dlist.Name = fn;
	dlist.File = fopen(fn, "r");

	for (char* line = NextLine(dlist.File); line != NULL; line = NextLine(dlist.File))
	{
		if (strcmp(line,fn_out)==0) continue;
		FileHandle drive;
		OpenFileHandle(line,"rb", drive);
		Drives[i++] = drive;
		drive.File = NULL;
	}

	OpenFileHandle(fn_out,"wb",Fout); //uncomment after testing
//	OpenFileHandle("testout.img","wb",Fout);
}

void OpenFileHandle(const char* fn, const char* mode, FileHandle& fh)
{
	fh.Name = fn;
	fh.File = fopen(fn,mode);
	if ( !fh.IsOpen() )
	{
		fprintf(stderr,"Failed to open device \"%s\"\n", fn);
		exit(1);
	}
	printf("Opened %s\n",fn);
}

char* NextLine(FILE* f)
{
	char* line = NULL;
	size_t blength;
	ssize_t read;

	do
	{
		read = getline(&line, &blength, f); //allocates space

		//remove \r\n
		char* c = strstr(line,"\r\n");
		if (c != NULL) *c = '\0';

		//remove \n
		c = strchr(line, '\n');
		if (c != NULL) *c = '\0';

	} while ((read>-1) && line[0]=='#'); //skip over comment lines

	if (read <= -1)
	{
		free(line);
		line = NULL;
	}

	return line;
}

bool IsFailedBlock(char x)
{
	switch (x)
	{
		case '*':
		case '/':
		case '-':
			return true;
		default:
			return false;
	}
	return false;
}

bool ParseNumbers(const char* line, uint64_t& position, uint64_t& length)
{
	int r = sscanf(line, "%Lx  %Lx", &position, &length);
	return r == 2;
}

void RepairSectors(uint64_t p, uint64_t l)
{
	char buffer[4096];

	for (int i = 0; (i < 100) && Drives[i].IsOpen(); ++i)
		SeekDrive(p,Drives[i]);

	SeekDrive(p,Fout);

	for (uint64_t read = 0; read<l;)
	{
		int r = ComputeParityChunk(buffer,l-read);
		if (r<0)
		{
			r*=-1; //make positive tor skipping
		}
		else
		{
			WriteChunk(buffer,r);
		}
		read += r;
	}
}

void SeekDrive(uint64_t pos,FileHandle& f)
{
	int r = fseeko(f.File,pos,SEEK_SET);
	if (r != 0)
	{
		fprintf(stderr,"\nCould not seek to %Lx on device %s\n", pos, f.Name.c_str());
		exit(1);
	}
}

int ComputeParityChunk(char* buffer, uint64_t l)
{
	char a[4096];

	int r = BufferedRead(buffer,4096,l,Drives[0]);

	if (r < 0)
	{
		fprintf(stderr,"\nSkipping\n");
		return r;
	}

	for (int i = 1; (i < 100) && Drives[i].IsOpen(); ++i)
	{
		int r = BufferedRead(a,4096,l,Drives[i]);
		if (r < 0)
		{
			fprintf(stderr,"\nSkipping\n");
			return r;
		}
		for (int y = 0; y < 4096; ++y)
			buffer[y] ^= a[y];
	}

	return r;
}

int WriteChunk(char* buffer, int l)
{
	int r = fwrite (buffer,l, 1, Fout.File );
	if (r != 1)
	{
		fprintf(stderr,"\nFailed to write to device\n");
		exit(1);
	}
	return l;
}

int BufferedRead(void* buffer, unsigned int bSize, uint64_t length, FileHandle& f)
{
	int l = length>bSize?bSize:length;
	int r = fread(buffer, l, 1, f.File);
	if (r != 1)
	{
		uint64_t at = ftello(f.File);
		fprintf(stderr,"\nFailed to read from device %s at 0x%Lx for %d bytes\n", f.Name.c_str(), at, l);
		return l * -1; //negative value is failed read of that many bytes
	}
	return l;
}


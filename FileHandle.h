#ifndef FILEHANDLE_H
#define FILEHANDLE_h

#define _FILE_OFFSET_BITS 64
#define _LARGEFILE64_SOURCE /* See feature_test_macros(7) */
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

#include <string>

class FileHandle
{
	public:
		FileHandle()
			:File(NULL)
		{}

		~FileHandle()
		{
			if (File!=NULL) fclose(File);
			File=NULL;
		}

		bool inline IsOpen() { return File!=NULL; }
		FILE* File;
		std::string Name;
};

#endif

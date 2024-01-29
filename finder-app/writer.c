/*
    This program takes two command line arguments: a file path and a string. 
    It opens the specified file, writes the provided string to it, and logs the operation using syslog.
*/

#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>

//main 
int main( int argc, char *argv[])
{
	// Check if the correct number of arguments is provided
	if(argc!=3)
	{
		syslog(LOG_USER | LOG_ERR, "Arguments missing, first should be file path and second should be string to be written in the file");
		return 1;
	}
	else
	{
		//open the file
		FILE *file = fopen(argv[1], "w");

		//check if file opened without errors
		if(file==NULL)
		{
			syslog( LOG_USER | LOG_ERR, "File does not exists: %m");
			return 1;
		}
		else
		{

			//write the string in the file
			int n_bytes = fprintf(file, "%s", argv[2]);

			//check if string written successfully
			if(n_bytes<=0)
			{
				syslog(LOG_USER|LOG_ERR, "File write operation unsuccessful");
			}

			//close the file
			fclose(file);

			//log the message
			openlog("writer", 0, LOG_USER);
			syslog(LOG_USER | LOG_DEBUG, "Writing '%s' to '%s'", argv[2], argv[1]);
			closelog();
		}
	}
	return 0;
}

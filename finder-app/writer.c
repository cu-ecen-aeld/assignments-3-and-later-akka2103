/*

*/
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>

//main 
int main( int argc, char *argv[])
{
	if(argc!=2)
	{
		syslog(LOG_USER | LOG_ERR, "Arguments missing, first should be file path and second should be string to be written in the file");
	}
	else
	{
		//open the file
		FILE *file = fopen(argv[1], "w");

		//check if file opened without errors
		if(file==NULL)
		{
			syslog( LOG_USER | LOG_ERR, "File does not exists: %m");
			exit(1);
		}
		else
		{
			//write thr string in the file
			fprintf(file, "%s", argv[2]);

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

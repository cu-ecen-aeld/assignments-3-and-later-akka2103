#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LOG_FILE_LOC "/var/tmp/aesdsocketdata"
#define PORT "9000"
#define MAX_BUFFER_SIZE 1024

int sockfd = -1;

void cleanup_and_exit(int status)
{
    syslog(LOG_INFO, "Closing aesdsocket application");
    closelog();
    close(sockfd);
    remove(LOG_FILE_LOC);
    exit(status);
}

void signal_handler(int signo)
{
    syslog(LOG_USER, "Caught signal, exiting");
    cleanup_and_exit(EXIT_SUCCESS);
}

void setup_signal_handler()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = signal_handler;

    if (sigaction(SIGTERM, &sa, NULL) != 0 || sigaction(SIGINT, &sa, NULL) != 0)
    {
        syslog(LOG_ERR, "Error setting up signal handler");
        cleanup_and_exit(EXIT_FAILURE);
    }
}

void handle_client_connection(int client_fd)
{
    FILE *fp = fopen(LOG_FILE_LOC, "a+");
    bool flag=0;
   int index=0; 
    char *bptr=(char*)malloc(sizeof(char)*MAX_BUFFER_SIZE);
   // int byte_count=0;
    while (1)
    {
        ssize_t bytes_recv = recv(client_fd,bptr+ index, sizeof(char)*(MAX_BUFFER_SIZE), 0);
        if (bytes_recv <= 0)
       	{
            break;
        }
        index+=bytes_recv;
      //  byte_count=bytes_recv;
        if(index>=MAX_BUFFER_SIZE)
        {
            //realloc
            char *newBptr = (char *)realloc(bptr,sizeof(char)*(index+MAX_BUFFER_SIZE+index));

            if (newBptr != NULL)
            {
            // realloc successful
                bptr = newBptr;
            } else
            {
                //realloc failed
                break; 
            }
        }
        
        
        //fwrite(buffer, bytes_recv, 1, fp);

        if (memchr(bptr, '\n', index) != NULL)
       {
            flag=1;
            break;
        }
    }
   //printf("%d=%s\n",index,bptr);
    if(flag==1)
    {
        flag=0;
        fwrite(bptr, index,1,fp);
        fclose(fp);
        free(bptr);
    
fp = fopen(LOG_FILE_LOC, "rb");
if (fp != NULL)
{
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Allocate space for the entire content plus null terminator
    char *rptr = (char *)malloc(sizeof(char) * (file_size + 1));

    if (rptr != NULL)
    {
        // Read the entire content into rptr
        size_t bytes_read = fread(rptr, 1, file_size, fp);

        // Null-terminate the string
        //rptr[bytes_read] = '\0';

        //printf("%s", rptr);

        // Send the content to the client
        send(client_fd, rptr, bytes_read, 0);

        // Free allocated memory
        free(rptr);
    }

    // Close the file
    fclose(fp);
}

    close(client_fd);
    }
}

void daemonize()
{
    pid_t pid = fork();

    if (pid < 0)
    {
        syslog(LOG_ERR, "Error forking: %m");
        cleanup_and_exit(EXIT_FAILURE);
    } else if (pid > 0)
    {
        exit(EXIT_SUCCESS); // Parent exits
    }

    if (setsid() == -1)
    {
        syslog(LOG_ERR, "Error creating new session: %m");
        cleanup_and_exit(EXIT_FAILURE);
    }

    chdir("/"); // Change working directory to root

    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Redirect stdin/out/err to /dev/null
    int dev_null = open("/dev/null", O_RDWR);
    if (dev_null == -1)
    {
        syslog(LOG_ERR, "Error opening /dev/null: %m");
        cleanup_and_exit(EXIT_FAILURE);
    }

    dup2(dev_null, STDIN_FILENO);
    dup2(dev_null, STDOUT_FILENO);
    dup2(dev_null, STDERR_FILENO);

    close(dev_null);
}

int main(int argc, char* argv[])
{
    openlog("AESD Socket", LOG_PID | LOG_NDELAY, LOG_USER);

    bool daemon = false;

    if (argc > 1 && strcmp(argv[1], "-d") == 0)
    {
        daemon = true;
    }

    setup_signal_handler();

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(PORT));
    server_addr.sin_addr.s_addr = INADDR_ANY;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        syslog(LOG_ERR, "Error creating socket: %m");
        cleanup_and_exit(EXIT_FAILURE);
    }

    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != 0)
    {
        syslog(LOG_ERR, "Error setting socket options: %m");
        cleanup_and_exit(EXIT_FAILURE);
    }

    if (daemon)
    {
        daemonize();
    }

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0)
    {
        syslog(LOG_ERR, "Error binding: %m");
        cleanup_and_exit(EXIT_FAILURE);
    }

    if (listen(sockfd, 5) != 0)
    {
        syslog(LOG_ERR, "Error listening: %m");
        cleanup_and_exit(EXIT_FAILURE);
    }

    syslog(LOG_INFO, "Listening on port %s", PORT);

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_addr_size = sizeof(client_addr);
        int client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_size);

        if (client_fd == -1)
       	{
            syslog(LOG_ERR, "Issue accepting connection: %m");
            continue;
        }

        char ip_addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_addr, sizeof(ip_addr));
        syslog(LOG_USER, "Accepted connection from %s", ip_addr);

        handle_client_connection(client_fd);

        syslog(LOG_USER, "Closed connection from %s", ip_addr);
    }

    cleanup_and_exit(EXIT_SUCCESS);
}



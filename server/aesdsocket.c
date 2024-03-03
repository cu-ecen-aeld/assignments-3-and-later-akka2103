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
#include <sys/queue.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#define LOG_FILE_LOC "/var/tmp/aesdsocketdata"
#define PORT "9000"
#define MAX_BUFFER_SIZE 1024
#define TIMESTAMP_INTERVAL 10 // seconds

pthread_mutex_t aesdsock_mutex;
pthread_mutex_t thread_list_mutex;
pthread_mutex_t timer_mutex;
pthread_t timer_thread;
time_t last_timestamp;

struct thread_data_s
{
    pthread_t id;
    int cfd;
    char peer_ip[INET6_ADDRSTRLEN];
    bool thread_complete_flag;
    SLIST_ENTRY(thread_data_s) next;
};

int sockfd = -1;
SLIST_HEAD(thread_list, thread_data_s) thread_head = SLIST_HEAD_INITIALIZER(thread_head);

// Function declarations
void cleanup_and_exit(int status);
void signal_handler(int signo);
void setup_signal_handler();
void *thread_function(void *arg);
void handle_client_connection(int client_fd);
void daemonize();
void add_thread(int client_fd, const char *ip_addr);
void setup_timer();
void timer_handler(int signo);

void cleanup_and_exit(int status)
{
    syslog(LOG_INFO, "Closing aesdsocket application");
    closelog();
    close(sockfd);
    remove(LOG_FILE_LOC);

    // Lock the mutex before deallocating memory
    pthread_mutex_lock(&thread_list_mutex);

    // Deallocate memory and join threads
    struct thread_data_s *entry;
    SLIST_FOREACH(entry, &thread_head, next)
    {
        pthread_join(entry->id, NULL);
        free(entry);
    }

    // Unlock the mutex after deallocating memory
    pthread_mutex_unlock(&thread_list_mutex);

    // Cleanup timer resources
    pthread_cancel(timer_thread);
    pthread_join(timer_thread, NULL);
    pthread_mutex_destroy(&timer_mutex);

    pthread_mutex_destroy(&aesdsock_mutex);
    pthread_mutex_destroy(&thread_list_mutex);

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

void *thread_function(void *arg)
{
    struct thread_data_s *thread_data = (struct thread_data_s *)arg;
    handle_client_connection(thread_data->cfd);
    thread_data->thread_complete_flag = true;
    pthread_exit(NULL);
}

void handle_client_connection(int client_fd)
{
    FILE *fp = fopen(LOG_FILE_LOC, "a+");
    bool flag = 0;
    int index = 0;
    char *bptr = (char *)malloc(sizeof(char) * MAX_BUFFER_SIZE);

    while (1)
    {
        ssize_t bytes_recv = recv(client_fd, bptr + index, sizeof(char) * (MAX_BUFFER_SIZE), 0);
        if (bytes_recv <= 0)
        {
            break;
        }
        index += bytes_recv;
        if (index >= MAX_BUFFER_SIZE)
        {
            char *newBptr = (char *)realloc(bptr, sizeof(char) * (index + MAX_BUFFER_SIZE + index));

            if (newBptr != NULL)
            {
                bptr = newBptr;
            }
            else
            {
                break;
            }
        }

        if (memchr(bptr, '\n', index) != NULL)
        {
            flag = 1;
            break;
        }
    }

    if (flag == 1)
    {
        flag = 0;

        // Lock the mutex before writing to the file
        pthread_mutex_lock(&aesdsock_mutex);

        fwrite(bptr, index, 1, fp);
        fclose(fp);
        free(bptr);

        // Unlock the mutex after writing to the file
        pthread_mutex_unlock(&aesdsock_mutex);

        fp = fopen(LOG_FILE_LOC, "rb");
        if (fp != NULL)
        {
            fseek(fp, 0, SEEK_END);
            long file_size = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            char *rptr = (char *)malloc(sizeof(char) * (file_size + 1));

            if (rptr != NULL)
            {
                size_t bytes_read = fread(rptr, 1, file_size, fp);

                fclose(fp);

                // Send the content to the client
                send(client_fd, rptr, bytes_read, 0);

                // Free allocated memory
                free(rptr);
            }
        }
    }

    close(client_fd);
}

void daemonize()
{
    pid_t pid = fork();

    if (pid < 0)
    {
        syslog(LOG_ERR, "Error forking: %m");
        cleanup_and_exit(EXIT_FAILURE);
    }
    else if (pid > 0)
    {
        exit(EXIT_SUCCESS); // Parent exits
    }

    if (setsid() == -1)
    {
        syslog(LOG_ERR, "Error creating new session: %m");
        cleanup_and_exit(EXIT_FAILURE);
    }

    chdir("/"); // Change working directory to root

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

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

void setup_timer()
{
    struct itimerval timer;
    struct sigaction sa;

    // Initialize the timer mutex
    pthread_mutex_init(&timer_mutex, NULL);

    // Setup the signal handler for the timer
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = timer_handler;
    sigaction(SIGALRM, &sa, NULL);

    // Configure the timer to expire every TIMESTAMP_INTERVAL seconds
    timer.it_interval.tv_sec = TIMESTAMP_INTERVAL;
    timer.it_interval.tv_usec = 0;
    timer.it_value = timer.it_interval;

    // Set the timer
    setitimer(ITIMER_REAL, &timer, NULL);
    last_timestamp = time(NULL);
}

void timer_handler(int signo)
{
    time_t current_time = time(NULL);
    char timestamp[50];

    // Lock the mutex before updating last_timestamp
    pthread_mutex_lock(&timer_mutex);

    // Append timestamp every TIMESTAMP_INTERVAL seconds
    if (current_time - last_timestamp >= TIMESTAMP_INTERVAL)
    {
        // Update last_timestamp
        last_timestamp = current_time;

        // Unlock the mutex after updating last_timestamp
        pthread_mutex_unlock(&timer_mutex);

        // Get the formatted timestamp
        struct tm *timeinfo;
        timeinfo = localtime(&current_time);
        strftime(timestamp, sizeof(timestamp), "timestamp:%a, %d %b %Y %H:%M:%S %z", timeinfo);

        // Lock the mutex before writing to the file
        pthread_mutex_lock(&aesdsock_mutex);

        // Open the file in append mode
        FILE *fp = fopen(LOG_FILE_LOC, "a");
        if (fp != NULL)
        {
            fprintf(fp, "%s\n", timestamp);
            fclose(fp);
        }

        // Unlock the mutex after writing to the file
        pthread_mutex_unlock(&aesdsock_mutex);
    }
    else
    {
        // Unlock the mutex if the timestamp is not appended
        pthread_mutex_unlock(&timer_mutex);
    }
}

int main(int argc, char *argv[])
{
    openlog("AESD Socket", LOG_PID | LOG_NDELAY, LOG_USER);

    bool daemon = false;

    if (argc > 1 && strcmp(argv[1], "-d") == 0)
    {
        daemon = true;
    }

    setup_signal_handler();
    pthread_mutex_init(&aesdsock_mutex, NULL);
    pthread_mutex_init(&thread_list_mutex, NULL);

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

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
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

    // Setup the timer thread
    setup_timer();

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_addr_size = sizeof(client_addr);
        int client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_size);

        if (client_fd == -1)
        {
            syslog(LOG_ERR, "Issue accepting connection: %m");
            continue;
        }

        char ip_addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_addr, sizeof(ip_addr));
        syslog(LOG_USER, "Accepted connection from %s", ip_addr);

        struct thread_data_s *thread_data = (struct thread_data_s *)malloc(sizeof(struct thread_data_s));
        if (thread_data == NULL)
        {
            syslog(LOG_ERR, "Error allocating memory for thread data");
            close(client_fd);
            continue;
        }

        thread_data->cfd = client_fd;
        strcpy(thread_data->peer_ip, ip_addr);
        thread_data->thread_complete_flag = false;

        // Lock the mutex before modifying the linked list
        pthread_mutex_lock(&thread_list_mutex);

        SLIST_INSERT_HEAD(&thread_head, thread_data, next);

        // Unlock the mutex after modifying the linked list
        pthread_mutex_unlock(&thread_list_mutex);

        // Create a new thread to handle the connection
        if (pthread_create(&thread_data->id, NULL, thread_function, (void *)thread_data) != 0)
        {
            syslog(LOG_ERR, "Error creating thread");
            close(client_fd);

            // Lock the mutex before modifying the linked list
            pthread_mutex_lock(&thread_list_mutex);

            SLIST_REMOVE(&thread_head, thread_data, thread_data_s, next);

            // Unlock the mutex after modifying the linked list
            pthread_mutex_unlock(&thread_list_mutex);

            free(thread_data);
        }

        // Check for completion and join threads
        struct thread_data_s *entry;
        SLIST_FOREACH(entry, &thread_head, next)
        {
            if (entry->thread_complete_flag)
            {
                pthread_join(entry->id, NULL);

                // Lock the mutex before deallocating memory
                pthread_mutex_lock(&thread_list_mutex);

                SLIST_REMOVE(&thread_head, entry, thread_data_s, next);

                // Unlock the mutex after deallocating memory
                pthread_mutex_unlock(&thread_list_mutex);

                free(entry);
            }
        }
    }

    cleanup_and_exit(EXIT_SUCCESS);
}


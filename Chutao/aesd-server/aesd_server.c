/*
 ============================================================================
 Name        : aesdsocket.c
 Author      : Chutao Wei
 Version     : 1.01
 Copyright   : MIT
 Description : AESD Socket program in C
 ============================================================================
 */

/********************* Include *********************/
// std related
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// Error related
#include <errno.h>
#include <syslog.h>

// File related
#include <fcntl.h>
#include <unistd.h>

// Thread related
#include <pthread.h>

// Network related
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include "queue.h"

// Time related
#include <time.h>
#include <unistd.h>

// Signal/Exception related
#include <signal.h>

/********************* Define *********************/

//#define WRITE_ERROR_TO_FILE
#define BUF_SIZE 512
#define CHUTAO_IP_ADDR "71.205.27.171"
#define PORT "9000"
#define SAM_IP_ADDR "71.205.27.171"
/********************* Error Checking Define *********************/
// Just to make life easy, too much error checking
#define ERROR_CHECK_NULL(pointer) \
if (pointer == NULL)\
	{\
		printf("%s%d\n",__func__,__LINE__);\
		perror("");\
		exit(1);\
	}\

#define ERROR_CHECK_LT_ZERO(return_val) \
if (return_val < 0)\
	{\
		printf("%s%d\n",__func__,__LINE__);\
		perror("");\
		exit(1);\
	}\

#define ERROR_CHECK_NE_ZERO(return_val) \
if (return_val != 0)\
	{\
		printf("%s%d\n",__func__,__LINE__);\
		perror("");\
		exit(1);\
	}\

//#define ERROR_CHECK_NULL(pointer)
//#define ERROR_CHECK_LT_ZERO(return_val)
//#define ERROR_CHECK_NE_ZERO(return_val)


/********************* Global Variables *********************/

// signal related
volatile bool caught_sigint = false;
volatile bool caught_sigterm = false;
// mutex
pthread_mutex_t lock;
/********************* Signal Handler *********************/

static void signal_handler(int signal_number)
{
	// save errno
	int errno_original = errno;

	// check which signal trigger the handler
	if(signal_number == SIGINT)
	{
		caught_sigint = true;
	}
	else if (signal_number == SIGTERM)
	{
		caught_sigterm = true;
	}
	// restore errno
	errno = errno_original;
}


/********************* Prototype *********************/

bool check_main_input_arg(int argc, char *argv[]);
int init_signal_handle(void);
int aesd_recv(int sockfd);
int aesd_send(int sockfd);

/********************* Thread *********************/
/* Singly-linked List head. */
struct entry {
        SLIST_ENTRY(entry) entries;
        pthread_t thread_id;
        int target_sockfd;
        bool complete_flag;
};

SLIST_HEAD(slisthead, entry) head =
    SLIST_HEAD_INITIALIZER(head);

// Recv-Send Thread
void *recv_send_thread(void * arg)
{
	struct entry * data = (struct entry *) arg;

	/* Start recv */
	if(data->target_sockfd)
	{
		// recv function
		aesd_recv(data->target_sockfd);
	}

	data->complete_flag = true;
	return NULL;
}
/********************* Function *********************/

bool check_main_input_arg(int argc, char *argv[])
{
	// check main function input argument
	if (argc == 2)
	{
		if (strcmp(argv[1],"-d") == 0)
		{
			// enable daemon_mode
			return true;
		}
		else
		{
			// Wrong argument, just exit
			exit(1);
		}
	}
	return false;
}

int init_signal_handle(void)
{
	struct sigaction new_action;

	memset(&new_action,0,sizeof(struct sigaction));
	new_action.sa_handler=signal_handler;
	if ((sigaction(SIGINT, &new_action, NULL)) != 0)
	{
		perror("sigaction fault for SIGINT");
		exit(1);
	}
	if ((sigaction(SIGTERM, &new_action, NULL)) != 0)
	{
		perror("sigaction fault for SIGTERM");
		exit(1);
	}

	return 0;
}



int aesd_recv(int sockfd)
{
	int error_code = 0;
	// create a buffer for receiving message
	void * local_buf = malloc(BUF_SIZE);
	ERROR_CHECK_NULL(local_buf);

	/* receive data */
	int num_receive = 1;
	bool EOT_flag = false;
	ssize_t recv_size;
	ssize_t total_recv_size; // assume ssize_t never overflow
	while(EOT_flag==false)
	{
		// receive
		recv_size = recv(sockfd,(local_buf+(num_receive-1)*BUF_SIZE),BUF_SIZE,0);
		ERROR_CHECK_LT_ZERO(recv_size);
		total_recv_size = ((num_receive-1)*BUF_SIZE)+recv_size;
		// check if receive '\n'
		if (((char *)local_buf)[total_recv_size-1]==0x4)
		{
			EOT_flag = true;
		}
		else
		{
			num_receive++;
			local_buf = realloc(local_buf,num_receive*BUF_SIZE);
			ERROR_CHECK_NULL(local_buf);
		}
	}
	total_recv_size = total_recv_size - 3;
	local_buf = realloc(local_buf,total_recv_size - 1);
	ERROR_CHECK_NULL(local_buf);
	/* Open /var/tmp/cap_recv.ppm */
	int fd = open("/var/tmp/cap_recv.ppm",
			O_WRONLY|O_CREAT/*|O_APPEND*/,
			S_IRWXU|S_IRWXG|S_IRWXO);
	ERROR_CHECK_LT_ZERO(fd);

	// write to /var/tmp/aesdsocketdata
	pthread_mutex_lock(&lock);

	ssize_t write_size = write(fd, local_buf, (size_t) total_recv_size);
	// Check for error
	if (write_size != total_recv_size)
	{
		// Use errno to print error
		perror("write error");
	}

	pthread_mutex_unlock(&lock);
	syslog(LOG_USER, "Image_recv saved");
	error_code = close(fd);
	ERROR_CHECK_NE_ZERO(error_code);

	// free local_buf
	free(local_buf);

	return 0;
}


/********************* Main *********************/

int main(int argc, char *argv[])
	{
	/* Check for input */
	bool daemon_mode = check_main_input_arg(argc, argv);

	/* Open syslog for this program */
	openlog(NULL,0,LOG_USER);

	/* Register Signal Handler */
	init_signal_handle();

	/* Start a Connection with host */

	// Error_code for debugging
	int error_code = 0;

	// Use getaddrinfo to get socket_addr
	struct addrinfo hints, *res;
	memset(&hints,0,sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	error_code = getaddrinfo(NULL, "9000", &hints, &res);
		// Check for error
		if (error_code != 0)
		{
			// Use errno to print error
			perror("getaddrinfo error");
			// Use gai_strerror
			printf("%s",gai_strerror(error_code));
			exit(1);
		}
	// Create a socket endpoint
	int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	// Check for error
	ERROR_CHECK_LT_ZERO(sockfd);
	/*Workaround for --address already in use-- error*/
	int yes = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	// Get address info
	error_code = bind(sockfd, res->ai_addr, res->ai_addrlen);
	ERROR_CHECK_NE_ZERO(error_code);

	/* Daemon if "-d" specified*/
	// Got from http://www.netzmafia.de/skripten/unix/linux-daemon-howto.html
	if(daemon_mode==true)
	{
	    pid_t pid, sid;
	    /* Fork off the parent process */
	    pid = fork();
	    if (pid < 0) {
			exit(EXIT_FAILURE);
	    }
	    /* If we got a good PID, then
	       we can exit the parent process. */
	    if (pid > 0) {
			exit(EXIT_SUCCESS);
	    }
	    /* Change the file mode mask */
	    umask(0);
	    /* Open any logs here */
	    /* Create a new SID for the child process */
	    sid = setsid();
	    if (sid < 0) {
			/* Log any failures here */
				exit(EXIT_FAILURE);
	    }
	    /* Change the current working directory */
	    if ((chdir("/")) < 0) {
			/* Log any failures here */
			exit(EXIT_FAILURE);
	    }
	    /* Close out the standard file descriptors */
	    close(STDIN_FILENO);
	    close(STDOUT_FILENO);
	    close(STDERR_FILENO);

	    /* Redirect standard file descriptors to /dev/null */
	    open("/dev/null",O_RDONLY);
	    open("/dev/null",O_WRONLY);
	    open("/dev/null",O_WRONLY);


	}
    // Create a mutex
    error_code = pthread_mutex_init(&lock, NULL);
    ERROR_CHECK_NE_ZERO(error_code);


	// listen(sockfd)
	error_code = listen(sockfd,10);
	ERROR_CHECK_LT_ZERO(error_code);


	// Declare some variable used in the while loop
	int num_thread = 0;

    // Initialize the list
    SLIST_INIT(&head);

	// inifite while loop for accept recv/send
	while((caught_sigint==false)||(caught_sigterm==false))
	{
		// accept connection
		int target_sockfd = accept(sockfd, res->ai_addr,
				(socklen_t*)(&res->ai_addrlen));

		// Log open connection message to syslog
		syslog(LOG_USER,"Accepted connection from %.14s",
				res->ai_addr->sa_data);

		if(target_sockfd<0)
		{
		}
		else
		{
		    // Step 1: malloc linked list entry memory
			struct entry *t;
			t = malloc(sizeof(struct entry));
			ERROR_CHECK_NULL(t);
			// Step 2: add linked list entry
			SLIST_INSERT_HEAD(&head, t, entries);
			num_thread ++;
		    // Step 3: Set input arguments
		    t->target_sockfd = target_sockfd;
		    t->complete_flag = false;
		    // Step 4: create a thread
		    pthread_t thread_id;
		    error_code = pthread_create(&thread_id,NULL,recv_send_thread,t);
		    ERROR_CHECK_NE_ZERO(error_code);
		    t->thread_id = thread_id;
		}

	    /************************
	    ***** Thread running*****
	    ************************/

	    // Step 5: Check if Complete
	    struct entry * list_ptr = NULL;
	    struct entry * list_ptr_next = NULL;
	    SLIST_FOREACH_SAFE(list_ptr,&head,entries,list_ptr_next)
	    {
			if(list_ptr->complete_flag == true)
			{
				// Step 6: join a thread
				error_code = pthread_join(list_ptr->thread_id, NULL);
				ERROR_CHECK_NE_ZERO(error_code);
				num_thread--;
				// Step 7: delete linked list entry
				SLIST_REMOVE(&head, list_ptr, entry, entries);/* Deletion. */
				// Step 8: free linked list entry memory
				free(list_ptr);
			}
	    }
	    if((caught_sigint==true)||(caught_sigterm==true))
		{
	    	break;
		}
	}

	/* Signal caught */
	if(caught_sigint||caught_sigterm)
	{
		// Log signal message to syslog
		syslog(LOG_USER, "Caught signal, exiting");
		if(num_thread)
		{
			//Check if Complete
			printf("num_thread %d\n",num_thread);
		}
	}

	/* Clean up */
	error_code = close(sockfd);
	ERROR_CHECK_NE_ZERO(error_code);
	// Log close connection message to syslog
	syslog(LOG_USER,"Closed connection from %.14s",
			res->ai_addr->sa_data);

	// remove /var/tmp/aesdsocketdata
	error_code = remove("/var/tmp/cap_recv.ppm");
	// Check for error
	if (error_code)
	{
		printf("%s%d\n",__func__,__LINE__);
		perror("");
	}

	// freeaddrinfo so that no memory leak
	freeaddrinfo(res);

}


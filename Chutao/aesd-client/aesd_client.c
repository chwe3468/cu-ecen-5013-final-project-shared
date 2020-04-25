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
#include <arpa/inet.h>

// Time related
#include <time.h>
#include <unistd.h>

// Signal/Exception related
#include <signal.h>

// my own library
//#include "capture.hpp"
/********************* Define *********************/

//#define WRITE_ERROR_TO_FILE
//#define USE_AESD_CHAR_DEVICE 1
#define BUF_SIZE 512
#define CHUTAO_IP_ADDR "71.205.27.171"
#define PORT "9000"
#define SAM_IP_ADDR "71.205.27.171"
#define PERIOD_T 2
#define PPM_HEADER_SIZE 3
#define DEV_VIDEO_NUM 2

/********************* Typedef *********************/

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
pthread_mutex_t image_lock;
pthread_mutex_t timer_flag;
// socket file discriptor
int sockfd;

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
	// unlock all mutex so that that can terminate
	pthread_mutex_unlock(&timer_flag);
	pthread_mutex_unlock(&image_lock);
	// restore errno
	errno = errno_original;
}


/********************* Prototype *********************/

bool check_main_input_arg(int argc, char *argv[]);
int init_signal_handle(void);
int aesd_recv(int sockfd);
int aesd_send(int sockfd);
#ifndef USE_AESD_CHAR_DEVICE
int delete_periodic_timer(timer_t *timerid);
int init_periodic_timer (timer_t *timerid, uint32_t second);

static inline void timespec_add( struct timespec *result,
                        const struct timespec *ts_1, const struct timespec *ts_2)
{
    result->tv_sec = ts_1->tv_sec + ts_2->tv_sec;
    result->tv_nsec = ts_1->tv_nsec + ts_2->tv_nsec;
    if( result->tv_nsec > 1000000000L ) {
        result->tv_nsec -= 1000000000L;
        result->tv_sec ++;
    }
}
#endif

/********************* Thread *********************/

static void timer_thread ()
{
	pthread_mutex_unlock(&timer_flag);
}


/********************* Function *********************/
// inspired by beej.us
void *get_in_addr(struct sockaddr *sa){
    if(sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int delete_periodic_timer(timer_t * timerid)
{
	// delete timer
	if (timer_delete(*timerid) != 0)
	{
		printf("Error %d (%s) deleting timer!\n",errno,strerror(errno));
		return -1;
	}
	return 0;
}

int init_periodic_timer (timer_t * timerid, uint32_t second)
{
    // set up input arguments for timer_create()
	struct sigevent sev;
	memset(&sev,0,sizeof(struct sigevent));
	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_value.sival_ptr = NULL;
	sev.sigev_notify_function = timer_thread;

	// create timer
	if ( timer_create(CLOCK_MONOTONIC,&sev,timerid) != 0 )
	{
		printf("Error %d (%s) creating timer!\n",errno,strerror(errno));
		exit(1);
	}

	// set up input arguments for timer_settime()
	struct timespec start_time;
    if ( clock_gettime(CLOCK_MONOTONIC,&start_time) != 0 ) {
        printf("Error %d (%s) getting clock %d time\n",errno,strerror(errno),CLOCK_MONOTONIC);
        exit(1);
    }
	struct itimerspec new_value;
    new_value.it_interval.tv_sec = second;
    new_value.it_interval.tv_nsec = 0;
    timespec_add(&new_value.it_value,&start_time,&new_value.it_interval);
	// set timer

    if( timer_settime(*timerid, TIMER_ABSTIME, &new_value, NULL ) != 0 ) {
    	perror("Error setting timer");
    	exit(1);
    }

    return 0;
}

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


void *capture_thread(void * arg)
{
	// inifite while loop for capturing
	while(1)
	{
		/* Obtain timer_flag */
		pthread_mutex_lock(&timer_flag);

		if((caught_sigint==true)||(caught_sigterm==true))
		{
			break;
		}

		/* Capture Image */
		//capture_write(DEV_VIDEO_NUM);
		/* open image file at /var/tmp/cap.ppm */
		int fd = open("/var/tmp/cap.ppm",
				O_RDONLY/*|O_APPEND*/,
				S_IRWXU|S_IRWXG|S_IRWXO);
		ERROR_CHECK_LT_ZERO(fd);
		// create a buffer for reading message
		void * local_buf = malloc(BUF_SIZE);
		ERROR_CHECK_NULL(local_buf);

		/* Read from /var/tmp/cap.ppm */
		int num_read = 1;
		bool EOF_flag = false;
		ssize_t read_size;
		ssize_t total_read_size;// assume ssize_t never overflow
		while(EOF_flag==false)
		{
			// receive
			read_size = read(fd, (local_buf+(num_read-1)*BUF_SIZE), (size_t) BUF_SIZE);
			// update total_size
			total_read_size = ((num_read-1)*BUF_SIZE)+read_size;
			// check if read '\n'
			if(read_size <= BUF_SIZE)
			{
				EOF_flag = true;
			}
			else
			{
				num_read++;
				local_buf = realloc(local_buf,num_read*BUF_SIZE);
				ERROR_CHECK_NULL(local_buf);
			}
		}
		//local_buf = realloc(local_buf,total_read_size);
		//ERROR_CHECK_NULL(local_buf);
		int error_code = close(fd);
		ERROR_CHECK_NE_ZERO(error_code);

		/* open image file at /var/tmp/cap_stamped.ppm */
		fd = open("/var/tmp/cap_stamped.ppm",
				O_WRONLY|O_CREAT/*|O_APPEND*/,
				S_IRWXU|S_IRWXG|S_IRWXO);
		ERROR_CHECK_LT_ZERO(fd);

		ssize_t write_size;
		// write to /var/tmp/cap_stamped.ppm
		write_size = write(fd, local_buf, (size_t) PPM_HEADER_SIZE);
		// Check for error
		if (write_size != PPM_HEADER_SIZE)
		{
			// Use errno to print error
			syslog(LOG_USER,"write_size is %ld", write_size);
			perror("PPM Header write error");
			//exit(1);
		}
		/* Add timestamp to the end of the file as a comment*/
		time_t t ;
		struct tm *tmp ;
		char MY_TIME[100];
		time( &t );
		tmp = localtime( &t );
		// using strftime to display time
		strftime(MY_TIME, sizeof(MY_TIME), "\n#timestamp:%a, %d %b %Y %T %z \n", tmp);
		size_t str_size = strlen(MY_TIME);
		write_size = write(fd, MY_TIME, (size_t) str_size);
		// Check for error
		if (write_size != str_size)
		{
			// Use errno to print error
			perror("timestamp write error");
			//exit(1);
		}
		write_size = write(fd, ((char *)local_buf)+PPM_HEADER_SIZE, (size_t) total_read_size-PPM_HEADER_SIZE);
		// Check for error
		if (write_size < total_read_size-PPM_HEADER_SIZE)
		{
			// Use errno to print error
			perror("ppm write error");
			//exit(1);
		}


		error_code = close(fd);
		ERROR_CHECK_NE_ZERO(error_code);

		syslog(LOG_USER, "Image captured");
		pthread_mutex_unlock(&image_lock);

		// free local_buf
		free(local_buf);

		if((caught_sigint==true)||(caught_sigterm==true))
		{
			break;
		}
	}

	return NULL;
}

void *send_thread(void * arg)
{
	struct addrinfo * p = (struct addrinfo *) arg;
	// inifite while loop for sending image
	while(1)
	{
		int error_code = 0;
		/* Obtain timer_flag */
		pthread_mutex_lock(&timer_flag);
		if((caught_sigint==true)||(caught_sigterm==true))
		{
			break;
		}
		/* Open image file at /var/tmp/cap_stamped.ppm */
		int fd = open("/var/tmp/cap_stamped.ppm",
				O_RDONLY,
				S_IRWXU|S_IRWXG|S_IRWXO);
		ERROR_CHECK_LT_ZERO(fd);

	    /* Read all content */
	    // create a buffer for sending message
		void * local_buf = malloc(BUF_SIZE);
		ERROR_CHECK_NULL(local_buf);

		int num_read = 1;
		bool EOF_flag = false;
		ssize_t read_size;
		ssize_t total_read_size;// assume ssize_t never overflow
		while(EOF_flag==false)
		{
			read_size = read(fd, (local_buf+(num_read-1)*BUF_SIZE), (size_t) BUF_SIZE);
			ERROR_CHECK_LT_ZERO(read_size);
			// update total_size
			total_read_size = ((num_read-1)*BUF_SIZE)+read_size;
			if(read_size <= BUF_SIZE)
			{
				EOF_flag = true;
			}
			else
			{
				num_read++;
				local_buf = realloc(local_buf,num_read*BUF_SIZE);
				ERROR_CHECK_NULL(local_buf);
			}
		}
		/* Add '\n''#''EOF' at the end of buffer */
		total_read_size = total_read_size + 3;
		local_buf = realloc(local_buf,total_read_size);
		((char *)local_buf)[total_read_size-3] = '\n';
		((char *)local_buf)[total_read_size-2] = '#';
		((char *)local_buf)[total_read_size-1] = 0x4;

		/* Close /var/tmp/cap_stamped.ppm */
		error_code = close(fd);
		ERROR_CHECK_NE_ZERO(error_code);

		/* check NULL */
	    if(p == NULL){
	        syslog(LOG_ERR, "client: client failed to connect: %s", strerror(errno));
	    }

        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
                perror("client: socket");
                continue;
        }

		/* Connect to Target ip else just dont send anything*/
        if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("client: connection failed");
        }
        else
        {
	        /* Send image to Sam over TCP */
			ssize_t send_size = send(sockfd,local_buf,total_read_size,0);
			ERROR_CHECK_LT_ZERO(send_size);
			syslog(LOG_USER, "Image sent");
        }


		// free local_buf
		free(local_buf);
		
		if((caught_sigint==true)||(caught_sigterm==true))
		{
			break;
		}
	}

	return NULL;
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

	// Error_code for debugging
	int error_code = 0;

	// Check for error
	ERROR_CHECK_LT_ZERO(sockfd);

	// Use getaddrinfo to get socket_addr
	struct addrinfo hints, *res;
	memset(&hints,0,sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	error_code = getaddrinfo(CHUTAO_IP_ADDR, PORT, &hints, &res);
		// Check for error
		if (error_code != 0)
		{
			// Use errno to print error
			perror("getaddrinfo error");
			// Use gai_strerror
			printf("%s",gai_strerror(error_code));
	#ifdef WRITE_ERROR_TO_FILE

	#endif
			exit(1);
		}

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

    // Create timer_flag mutex 
    error_code = pthread_mutex_init(&timer_flag, NULL);
    pthread_mutex_lock(&timer_flag);
    // Create image_lock mutex 
    error_code = pthread_mutex_init(&image_lock, NULL);
    pthread_mutex_lock(&image_lock);

    // Initialize the timer for period PERIOD_T sec
	timer_t timer_id = NULL;
    error_code = init_periodic_timer(&timer_id,PERIOD_T);
    ERROR_CHECK_LT_ZERO(error_code);


    /**** Work load begin ****/
    pthread_t thread_id1;
    /* Capture thread is a infinite while loop that runs only if timer_flag unlocked */
    error_code = pthread_create(&thread_id1,NULL,capture_thread,NULL);
    ERROR_CHECK_NE_ZERO(error_code);

    pthread_t thread_id2;
    /* Send thread is a infinite while loop that runs only if image_lock unlocked */
    error_code = pthread_create(&thread_id2,NULL,send_thread,res);
    ERROR_CHECK_NE_ZERO(error_code);

	error_code = pthread_join(thread_id2, NULL);
	ERROR_CHECK_NE_ZERO(error_code);
	error_code = pthread_join(thread_id1, NULL);
	ERROR_CHECK_NE_ZERO(error_code);

	/**** Work load end ****/

	/* Signal caught */
	if(caught_sigint||caught_sigterm)
	{
		// Log signal message to syslog
		syslog(LOG_USER, "Caught signal, exiting");
	}

	/* Clean up */
    // delete the timer for period 10 sec
	if (timer_id == NULL)
	{
		syslog(LOG_USER, "Cannot Delete timer");
		exit(1);
	}
	
	error_code = delete_periodic_timer(&timer_id);
	ERROR_CHECK_LT_ZERO(error_code);

	error_code = close(sockfd);
	ERROR_CHECK_NE_ZERO(error_code);

	// Log close connection message to syslog
	syslog(LOG_USER,"Closed connection from %.14s",
			res->ai_addr->sa_data);


	// remove /var/tmp/cap_stamped.ppm
	error_code = remove("/var/tmp/cap_stamped.ppm");
	// Check for error
	ERROR_CHECK_NE_ZERO(error_code);

	// freeaddrinfo so that no memory leak
	freeaddrinfo(res);

}


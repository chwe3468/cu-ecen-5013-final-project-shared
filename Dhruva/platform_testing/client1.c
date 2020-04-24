#include <sys/inotify.h>
#include <limits.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/queue.h>
#include <fcntl.h>
#include <time.h>
#include <syslog.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

// define macros for server port and IP address (public)
#define PORT "9000"
// #define HOSTNAME "127.0.0.1" // localhost
#define HOSTNAME "73.78.219.44"
#define BACKLOG 100
// define macro for inotify interface
#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

typedef struct{
    pthread_mutex_t lock;
} thread_data_t;

static void timer_thread(union sigval sigval);
static bool setup_timer(int clock_id, timer_t timerid, unsigned int timer_period, struct timespec *start_time);
static inline void timespec_add(struct timespec *result, const struct timespec *ts_1, const struct timespec *ts_2);
static void daemonize(void);
void sig_handler(int signo);
void *get_in_addr(struct sockaddr *sa);
char * get_latest_temperature(struct inotify_event *i);
int send_temperature(struct addrinfo *info);

timer_t timerid;
thread_data_t td;
bool sig_handler_exit = false;

// log file vars
const char filename[] = "/var/tmp/log/log.txt";
FILE *fd;

// inotify interface vars
static const long max_len = 5 + 1;
char fbuff[8];
char * sensorbuf;
bool is_done = false;

// socket vars
int sockfd;
char ip_addr[INET6_ADDRSTRLEN];

int main(void){
	// signal handler handle
    struct sigaction sa;
    // socket vars
    struct addrinfo hints, *servinfo;
    int rv;
    // timer thread vars
    struct timespec start_time;
    struct sigevent sev;
    memset(&td, 0, sizeof(thread_data_t));

    if(pthread_mutex_init(&td.lock, NULL) != 0){
        syslog(LOG_ERR, "client1: %d, %s failed initializing thread mutex", errno, strerror(errno));
    }

    // set up signal handler for interrupts and termination
    sa.sa_handler = sig_handler;
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGINT, &sa, NULL) == -1){
        syslog(LOG_ERR, "client1: failed to set up sigaction SIGINT, errno: %s", strerror(errno));
        exit(1);
    }
    if(sigaction(SIGTERM, &sa, NULL) == -1){
        syslog(LOG_ERR, "client1: failed to set up sigaction SIGTERM, errno: %s", strerror(errno));
        exit(1);
    }
    syslog(LOG_INFO, "Set up handler for client1\n");
    // set up socket
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if((rv = getaddrinfo(HOSTNAME, PORT, &hints, &servinfo)) != 0){
        syslog(LOG_ERR, "client1: getaddrinfo: %s", gai_strerror(rv));
        return 1;
    }

    // daemonize
    daemonize();

    int clock_id = CLOCK_MONOTONIC;
    memset(&sev, 0, sizeof(struct sigevent));
    // Setup a call to timer_thread passing in the td structure as the sigev_value argument
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_value.sival_ptr = &td;
    sev.sigev_notify_function = timer_thread;
    if(timer_create(clock_id, &sev, &timerid) != 0 ){
        syslog(LOG_ERR, "client1: %d, %s failed to create timer", errno, strerror(errno));
    }
    // set timer for inotify interface for 6 second intervals. Client will activate inotify interface to get latest sensor data
    setup_timer(clock_id, timerid, 6, &start_time);
    syslog(LOG_INFO, "Set up inotify timer\n");

    sensorbuf = NULL;
    // main loop to spin in. Check for sig_handler_exit to exit the program. Check if timer thread is done, then send latest sensor data to server
    syslog(LOG_INFO, "Entering main loop\n");
    while(1){
        if(sig_handler_exit){
            closelog();
            shutdown(sockfd, SHUT_WR);
            freeaddrinfo(servinfo);
            exit(0);
        }
        if(pthread_mutex_lock(&td.lock) != 0){
            syslog(LOG_ERR, "Error %d (%s) locking thread data!\n", errno, strerror(errno));
        }
        if(is_done){
            syslog(LOG_INFO, "Getting the temperature\n");
            syslog(LOG_INFO, "The temperature is %s 'C\n", sensorbuf);
            if(send_temperature(servinfo) != 0){
                syslog(LOG_ERR, "client1: %d, %s failed to send temperature to server", errno, strerror(errno));
            }
            close(sockfd);
            is_done = false;
        }
        if(pthread_mutex_unlock(&td.lock) != 0){
            syslog(LOG_ERR, "Error %d (%s) unlocking thread data!\n", errno, strerror(errno));
        }
    }
    return 0;
}

/* Retrieve latest sensor data from inotify_event structure watching log file for close writes
 * inspired by http://man7.org/tlpi/code/online/diff/inotify/demo_inotify.c.html
 * returns the last line of data (latest) as a string
 */
char * get_latest_temperature(struct inotify_event *i){
    char *last_newline;
    char *last_line;
	// if interface detects a close write (file closed after writing), then open file for reading and get the last line of data
    if(i->mask & IN_CLOSE_WRITE){
        syslog(LOG_INFO, "IN_CLOSE_WRITE\n");
        if((fd = fopen(filename, "r")) != NULL){
            fseek(fd, -max_len, SEEK_END); // seek to end of file and movemax_len bytes back
            fread(fbuff, (max_len - 1), 1, fd);	// read last line
            fclose(fd);
            // process buffer to be a string
            fbuff[max_len-1] = '\0';
            last_newline = strchr(fbuff, '\n');
            last_line = last_newline + 1;

            syslog(LOG_INFO, "captured: [%s]\n", last_line);
        }
    }
    return last_line;
}


/**
* A thread which runs every timer_period_s seconds
* Assumes timer_create has configured for sigval.sival_ptr to point to the
* thread data used for the timer
* inspired by https://github.com/cu-ecen-5013/aesd-lectures/blob/master/lecture9/timer_thread.c
*/
static void timer_thread(union sigval sigval){
    thread_data_t *td = (thread_data_t*) sigval.sival_ptr;
    int inotifyFd, wd;
    char buf[BUF_LEN] __attribute__ ((aligned(8)));
    ssize_t numRead;
    char *p;
    struct inotify_event *event;

    if(!sig_handler_exit){
        syslog(LOG_INFO, "In timer inotify thread\n");
        if(pthread_mutex_lock(&td->lock) != 0){
            syslog(LOG_ERR, "Error %d (%s) locking thread data!\n", errno, strerror(errno));
        } else {
            inotifyFd = inotify_init(); // create inotify instance
            if (inotifyFd == -1){
                perror("inotify_init");
                exit(-1);
            }

            // add a watch for IN_CLOSE_WRITE events
            wd = inotify_add_watch(inotifyFd, filename, IN_CLOSE_WRITE);
            if (wd == -1){
                perror("inotify_add_watch");
                exit(-1);
            }
            // read events forever
            numRead = read(inotifyFd, buf, BUF_LEN);
            // didn't read anything
            if (numRead == 0){
                perror("read() from inotify fd returned 0!");
                exit(-1);
            }
            // failed to read anything
            if (numRead == -1){
                perror("read");
                exit(-1);
            }

            syslog(LOG_INFO, "Read %ld bytes from inotify fd\n", (long)numRead);

            // process all of the events in buffer returned by read()
            for (p = buf; p < buf + numRead; ) {
                event = (struct inotify_event *) p;
                sensorbuf = get_latest_temperature(event);
                p += sizeof(struct inotify_event) + event->len;
                is_done = true;
                syslog(LOG_INFO, "Thread done is %d\n", is_done);
                if(sig_handler_exit){
                    break;
                }
            }

        }
        if(pthread_mutex_unlock(&td->lock) != 0){
            syslog(LOG_ERR, "Error %d (%s) unlocking thread data!\n", errno, strerror(errno));
        }
    }
}


/**
* Setup the timer at @param timerid (previously created with timer_create) to fire
* every @param timer_period seconds, using @param clock_id as the clock reference.
* The time now is saved in @param start_time
* @return true if the timer could be setup successfuly, false otherwise
* inspired by https://github.com/cu-ecen-5013/aesd-lectures/blob/master/lecture9/timer_thread.c
*/
static bool setup_timer(int clock_id, timer_t timerid, unsigned int timer_period, struct timespec *start_time){
    bool success = false;
    if (clock_gettime(clock_id,start_time) != 0) {
        syslog(LOG_ERR, "Error %d (%s) getting clock %d time\n", errno, strerror(errno), clock_id);
    } else{
        struct itimerspec itimerspec;
        itimerspec.it_interval.tv_sec = timer_period;
        itimerspec.it_interval.tv_nsec = 0;
        timespec_add(&itimerspec.it_value, start_time, &itimerspec.it_interval);

        if(timer_settime(timerid, TIMER_ABSTIME, &itimerspec, NULL ) != 0) {
            syslog(LOG_ERR, "Error %d (%s) setting timer\n", errno, strerror(errno));
        } else{
            success = true;
        }
    }
    return success;
}


// daemonize program
static void daemonize(void){
    pid_t pid;
    // fork and exit parent if success
    pid = fork();
    if(pid < 0){
        // Fork failed
        exit(1);
    }
    if (pid > 0){
        // Fork success
        exit(0);
    }
    // create new session id for child
    if(setsid() < 0){
        exit(1);
    }
    // catch and ignore HUP and CHLD signals
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    // change filemode mask
    umask(0);
    openlog("client-daemon", LOG_PID, LOG_DAEMON);
    // change working dir to "/"
    if((chdir("/")) < 0){
        exit(1);
    }
    syslog(LOG_NOTICE, "client daemonized");
}


// inspired by beej.us
void *get_in_addr(struct sockaddr *sa){
    if(sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


/**
* set @param result with @param ts_1 + @param ts_2
* inspired by https://github.com/cu-ecen-5013/aesd-lectures/blob/master/lecture9/time_functions_shared.h
*/
static inline void timespec_add( struct timespec *result, const struct timespec *ts_1, const struct timespec *ts_2){
    result->tv_sec = ts_1->tv_sec + ts_2->tv_sec;
    result->tv_nsec = ts_1->tv_nsec + ts_2->tv_nsec;
    if(result->tv_nsec > 1000000000L){
        result->tv_nsec -= 1000000000L;
        result->tv_sec ++;
    }
}


// signal handler to catch signals, shutdown socket 
void sig_handler(int signo){
    int saved_errno = errno;
    if(signo == SIGINT || signo == SIGTERM){
        syslog(LOG_DEBUG, "Caught signal, exiting client\n");
        pthread_mutex_lock(&td.lock);
        sig_handler_exit = true;
        pthread_mutex_unlock(&td.lock);
    }
    errno = saved_errno;
}


// connect to server and send latest sensor data
int send_temperature(struct addrinfo *info){
    struct addrinfo *p;
    int ret = 0;
    int bytes_sent = 0;

    for(p = info; p != NULL; p = p->ai_next){
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
                perror("client: socket");
                continue;
        }

        if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(sockfd);
            perror("client: connect");
            continue;
        }
        break;
    }

    if(p == NULL){
        syslog(LOG_ERR, "client1: client failed to connect: %s", strerror(errno));
        ret = 2;
        return ret;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), ip_addr, sizeof(ip_addr));
    syslog(LOG_INFO, "client: connecting to %s\n", ip_addr);
    // cat newline to end of string for easier parsing on server
    strcat(sensorbuf, "\n");
    bytes_sent = send(sockfd, sensorbuf, strlen(sensorbuf), 0);
    if(bytes_sent == -1){
        syslog(LOG_ERR, "client1: %d, %s failed to send", errno, strerror(errno));
    }
    syslog(LOG_INFO, "client1: sent %d bytes", bytes_sent);
    return ret;
}

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include <sys/time.h>
#include <pthread.h>

#include <sys/queue.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <syslog.h>
#include <errno.h>


typedef struct{
    pthread_mutex_t lock;
} thread_data_t;

static void timer_thread(union sigval sigval);
static bool setup_timer(int clock_id, timer_t timerid, unsigned int timer_period, struct timespec *start_time);
static inline void timespec_add(struct timespec *result, const struct timespec *ts_1, const struct timespec *ts_2);
// static void daemonize(void);
void sig_handler(int signo);

timer_t timerid;
thread_data_t td;
bool sig_handler_exit = false;

int main(void){

	struct sigaction sa;

	struct timespec start_time;
	struct sigevent sev;
	memset(&td, 0, sizeof(thread_data_t));

	if(pthread_mutex_init(&td.lock, NULL) != 0){
        syslog(LOG_ERR, "sensor: %d, %s failed initializing thread mutex", errno, strerror(errno));
    }

	// set up signal handler for interrupts and termination
	sa.sa_handler = sig_handler;
	sa.sa_flags = SA_RESTART;
	if(sigaction(SIGINT, &sa, NULL) == -1){
		syslog(LOG_ERR, "sensor: failed to set up sigaction SIGINT, errno: %s", strerror(errno));
		exit(1);
	}
	if(sigaction(SIGTERM, &sa, NULL) == -1){
		syslog(LOG_ERR, "sensor: failed to set up sigaction SIGTERM, errno: %s", strerror(errno));
		exit(1);
	}
	printf("Set up handler\n");

	// daemonize
    // daemonize();

    int clock_id = CLOCK_MONOTONIC;
    memset(&sev, 0, sizeof(struct sigevent));
    // Setup a call to timer_thread passing in the td structure as the sigev_value argument
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_value.sival_ptr = &td;
    sev.sigev_notify_function = timer_thread;
    if(timer_create(clock_id, &sev, &timerid) != 0 ){
        syslog(LOG_ERR, "sensor: %d, %s failed to create timer", errno, strerror(errno));
    }

	setup_timer(clock_id, timerid, 5, &start_time);
	printf("Set up timer\n");
	int i = 0;
	while(1){
		if(sig_handler_exit){
			closelog();
			exit(0);
		}
		i++;
		printf("%d\n", i);
		for(int j = 0; j < 150000000; j++){
			;
		}
	}
	return 0;
}

/**
* A thread which runs every timer_period_ms milliseconds
* Assumes timer_create has configured for sigval.sival_ptr to point to the
* thread data used for the timer
* inspired by https://github.com/cu-ecen-5013/aesd-lectures/blob/master/lecture9/timer_thread.c
*/
static void timer_thread(union sigval sigval){
    thread_data_t *td = (thread_data_t*) sigval.sival_ptr;
    int ret = 0;
	printf("In timer thread\n");
    if(pthread_mutex_lock(&td->lock) != 0){
        printf("Error %d (%s) locking thread data!\n", errno, strerror(errno));
    } else {
		// set SIGCHLD to default behaviour to allow system() to use SIGCHLD
		// if(signal(SIGCHLD, SIG_DFL) != SIG_IGN){
		// 	syslog(LOG_ERR, "sensor: failed to switch SIGCHLD behaviour, errno: %d (%s)", errno, strerror(errno));
		// 	perror("signal");
		// }
		// call shell script to get the temperature and log it to /var/tmp/log/log.txt
		ret = system("/usr/bin/gettemp.sh");
		if(WEXITSTATUS(ret)){
			syslog(LOG_ERR, "sensor: failed to get sensor temp value, errno: %d (%s), exit status: %d", errno, strerror(errno), ret);
			perror("system(getsensor.sh)");
		}
		printf("ran temperature script\n");
		// restore SIGCHLD to ignore to prevent zombie processes
		// if(signal(SIGCHLD, SIG_IGN) != SIG_DFL){
		// 	syslog(LOG_ERR, "sensor: failed to restore SIGCHLD behaviour, errno: %d (%s)", errno, strerror(errno));
		// 	perror("signal");
		// }
    }
    if(pthread_mutex_unlock(&td->lock) != 0){
		printf("Error %d (%s) unlocking thread data!\n", errno, strerror(errno));
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
        printf("Error %d (%s) getting clock %d time\n", errno, strerror(errno), clock_id);
    } else{
        struct itimerspec itimerspec;
        itimerspec.it_interval.tv_sec = timer_period;
        itimerspec.it_interval.tv_nsec = 0;
        timespec_add(&itimerspec.it_value, start_time, &itimerspec.it_interval);

        if(timer_settime(timerid, TIMER_ABSTIME, &itimerspec, NULL ) != 0) {
            printf("Error %d (%s) setting timer\n", errno, strerror(errno));
        } else{
            success = true;
        }
    }
    return success;
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


// daemonize program
// static void daemonize(void){
// 	pid_t pid;
// 	// fork and exit parent if success
// 	pid = fork();
// 	if(pid < 0){
// 		// Fork failed
// 		exit(1);
// 	}
// 	if (pid > 0){
// 		// Fork success
// 		exit(0);
// 	}
// 	// create new session id for child
// 	if(setsid() < 0){
// 		exit(1);
// 	}
// 	// catch and ignore HUP and CHLD signals
// 	signal(SIGCHLD, SIG_IGN);
// 	signal(SIGHUP, SIG_IGN);
// 	// change filemode mask
// 	umask(0);
// 	openlog("sensor-daemon", LOG_PID, LOG_DAEMON);
// 	// change working dir to "/"
// 	if((chdir("/")) < 0){
// 		exit(1);
// 	}
// 	syslog(LOG_NOTICE, "sensor daemonized");
// }


// signal handler to catch signals, shutdwn socket 
void sig_handler(int signo){
	int saved_errno = errno;
	if(signo == SIGINT || signo == SIGTERM){
		syslog(LOG_DEBUG, "Caught signal, exiting\n");
		pthread_mutex_lock(&td.lock);
		sig_handler_exit = true;
		pthread_mutex_unlock(&td.lock);
	}
	errno = saved_errno;
}

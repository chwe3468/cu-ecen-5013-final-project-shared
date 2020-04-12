/* aesdsocket.c
* Author: Sam Solondz
* Description: This file implements the socket server described in ECEN5013 assignment6
* */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <linux/fs.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <time.h>


#define RXBUFFERSIZE 2048

/*Uncomment to echo back to sender*/
//#define WRITEBACK 1

/*Uncommment to Timestamp the log*/
//#define TIMESTAMP 1


typedef struct node
{
  pthread_mutex_t nodeLock;
  pthread_t thread_id;
  struct node * next;
  bool thread_complete;
  int connection_id;
  char * IPBuffer;
} Node;

/*Global variables*/

Node * head = NULL;

const char * writefile = "/var/tmp/aesdsocketdata";
pthread_mutex_t writefileLock;

volatile bool signal_exit = false;
int socketfd;

/*
* Handles SIGNINT and SIGTERM
*
* @param signo raised signal
*/
static void signal_handler (int signo)
{
  if(signo == SIGINT || signo == SIGTERM)
  syslog(LOG_INFO, "Caught signal, exiting");
  else
  return;

  if(shutdown(socketfd, SHUT_RDWR))
  {
  	perror("Failed on shutdown()");
  	syslog(LOG_ERR, "Could not close socket FD in signal handler : %s", strerror(errno));
  }

  signal_exit = true;

  return;
}

/*
* Processes incoming packets from a TCP socket.
*
* @param args void pointer to structure of arguments
*/
void * processRX(void * args)
{

  int err = 0;
  const pthread_t self_id = pthread_self();
  syslog(LOG_INFO, "Spawned thread ID: %ld\n", self_id);

  /*Extract the needed information and free the arguments struct from heap*/
  Node * info = (Node *)args;

  pthread_mutex_lock(&info->nodeLock);

  int accepted = info->connection_id;
  char * IPBuffer = info->IPBuffer;
  info->thread_id = self_id;

  pthread_mutex_unlock(&info->nodeLock);

  /*Open the file for read/append, create if it doesn't exist*/
  FILE *fp = fopen(writefile, "a+");
  if(!fp)
  {
    err = errno;
    perror("File creation");
    syslog(LOG_ERR, "Could not create file %s, errno: %s", writefile, strerror(err));
    return 0;
  }


  /*Receive and write date*/
  while(!signal_exit)
  {
    /*If packet is greater than RXBUFFERSIZE, it will be handled on the next loop because of TCP stream socket.
    Will be queued by kernel for next loop. */

    char rxbuffer[RXBUFFERSIZE];
    int totalwritten = 0;
    int rxcount = 0;

    rxcount = recv(accepted, (void *)(rxbuffer), RXBUFFERSIZE, 0);
    if(rxcount ==  0)
    {
      syslog(LOG_ERR, "Client %s disconnected",  IPBuffer);
      break;
    }
    else
    {
      syslog(LOG_INFO, "Received %d bytes from %s", rxcount, IPBuffer);
      totalwritten += rxcount;
    }

    int to_write = rxcount;
    int written = 0;
    /*Lock the writefile mutex*/
    pthread_mutex_lock(&writefileLock);
    /*Ensure entire buffer is written*/

    while((written = fwrite(rxbuffer, sizeof(char), rxcount, fp)) != to_write)
    {
      to_write -= written;
      if(written == EOF)
      {
        pthread_mutex_unlock(&writefileLock);
        err = errno;
        perror("Write rxbuffer");
        syslog(LOG_ERR, "Could not write rxbuffer, errno: %s", strerror(err));
        break;
      }
    }
    pthread_mutex_unlock(&writefileLock);

    /*Parse the rxbuffer for newline character*/
    for(int i = 0; i < rxcount; i++)
    {
      if(rxbuffer[i] == '\n')
      {
        pthread_mutex_lock(&writefileLock);
        /*Return full content of writefile back to client*/
        fseek(fp, 0, SEEK_END);
        int filesize = ftell(fp);
        fseek(fp, 0, SEEK_SET);   /*Set back to beginning*/
        pthread_mutex_unlock(&writefileLock);

        /*Malloc enough memory to read in the file contents to transmit*/

        char * buffer = 0;

        buffer = malloc(filesize);
        if(buffer)
        {
          pthread_mutex_lock(&writefileLock);
          fread(buffer, sizeof(char), filesize, fp);
          pthread_mutex_unlock(&writefileLock);
        }
        else
        {
          syslog(LOG_ERR, "Could not malloc %d bytes", filesize);
          break;
        }

        #ifdef WRITEBACK
        int sent = 0;
        int to_send = filesize;
        /*Ensure entire file is sent back*/
        while((sent = send(accepted, (const void *)buffer, filesize, 0)) != to_send)
        {
          to_send -= sent;
          if(sent == -1)
          {
            err = errno;
            syslog(LOG_ERR, "Could not send data to client, errno: %s", strerror(err));
            free(buffer);
            break;
          }

        }
        #endif

        /*Free the send buffer*/
        free(buffer);
        break;
      }
    }
  }

  /*Close file pointer to writefile */
  fclose(fp);

  /*Close the connection*/
  shutdown(accepted, SHUT_RDWR);
  if(close(accepted))
  {
    err = errno;
    syslog(LOG_ERR, "Unable to close FD  %d, error: %s\n", accepted, strerror(err));
    fprintf(stderr, "could not close FD %d, error: %s\n", accepted, strerror(err));
  }
  //else
  //syslog(LOG_INFO, "Closed socket to from %s\n", IPBuffer);

  pthread_mutex_lock(&info->nodeLock);
  info->thread_complete = true;
  pthread_mutex_unlock(&info->nodeLock);


  //syslog(LOG_INFO, "Thread %ld complete! \n", self_id);

  return 0;
}


/*Add a timestamp to the writfile in RFC 2822-compliant format
*
*@param sigevl_val data passed from notification, unused
*/

void * timestamp(union sigval sigev_val)
{
  int err = 0;
  syslog(LOG_INFO, "Timer thread %ld entered", pthread_self());

  time_t raw;
  struct tm *curr;

  time(&raw);
  curr = localtime(&raw);
  if(curr == NULL)
  {
    err = errno;
    syslog(LOG_ERR, "error getting current time: %s", strerror(err));

    return 0;
  }
  else
  syslog(LOG_INFO, "Current local time and date: %s", asctime(curr));


  /*Open the file for read/append, create if it doesn't exist*/
  FILE *fp = fopen(writefile, "a+");
  if(!fp)
  {
    err = errno;
    syslog(LOG_ERR, "Could not create file %s, errno: %s", writefile, strerror(err));
    return 0;
  }

  char timestamp[80] = {0};
  strftime(timestamp, sizeof(timestamp),"timestamp:%a, %d %b %Y %T %z\n", curr);


  pthread_mutex_lock(&writefileLock);
  int written;
  int to_write = strlen(timestamp);
  while((written = fwrite(timestamp, sizeof(char), strlen(timestamp), fp)) != to_write)
  {
    to_write -= written;
    if(written == EOF)
    {
      pthread_mutex_unlock(&writefileLock);
      err = errno;
      perror("Write rxbuffer");
      syslog(LOG_ERR, "Could not write rxbuffer, errno: %s", strerror(err));

      return 0;
    }
  }

  pthread_mutex_unlock(&writefileLock);

  fclose(fp);

  return 0;
}

/*
* Given a filename and text string
*
* @param argc Number of commandline arguments
* @param argv[] Pointer to commandline arguments
*
*/
int main(int argc, char * argv[])
{
  syslog(LOG_INFO, "Starting mulithreaded socket server...\n");

  int err = 0;

  /*Setup logging*/
  openlog(NULL, LOG_PID, LOG_USER);

  /*Init writefile  mutex with default attributes*/
  if(pthread_mutex_init(&writefileLock, NULL))
  {
    err = errno;
    syslog(LOG_ERR, "failed to init mutex: %s\n", strerror(err));
    return -1;
  }


  /*Register the signal handlers*/
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  /*Setup addrinfo hints struct to create socket details*/
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;		/*Unspecified IPv4 or IPv6*/
  hints.ai_socktype = SOCK_STREAM; 	/*TCP stream socket*/
  hints.ai_flags = AI_PASSIVE;		/*Assign the adress of local host to socket structures*/
  const char port[] = "9000";

  struct addrinfo *serverinfo;
  if(getaddrinfo(NULL, port, &hints, &serverinfo))
  {
    err = errno;
    fprintf(stderr, "getaddrinfo error: %s\n", strerror(err));
    return -1;
  }
  //serverinfo now points to a linked list of 1 or more struct addrinfo

  /*Open a socket using struct addrinfo serverinfo*/
  socketfd = socket(serverinfo->ai_family, serverinfo->ai_socktype, serverinfo->ai_protocol);
  if(socketfd == -1)
  {
    err = errno;
    fprintf(stderr, "bind error: %s\n", strerror(err));
    return -1;
  }

  /*Workaround for --address already in use-- error*/
  int yes = 1;
  setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

  /*Bind the socket*/
  if(bind(socketfd, serverinfo->ai_addr, serverinfo->ai_addrlen))
  {
    err = errno;
    fprintf(stderr, "bind error: %s\n", strerror(err));
    return -1;
  }

  /*Handle daemon setup if specified in commandline*/
  /*as described in Linux System Programming chapter 5*/
  if(argc > 1 && memcmp(argv[1], "-d", 2) == 0) /*Short circuit logic to avoid seg fault*/
  {
    syslog(LOG_INFO, "Creating daemon...\n");
    /*Note: This could also be done with daemon()*/

    /*Fork new process*/
    pid_t pid = fork();
    if(pid == -1)
    {
      err = errno;
      syslog(LOG_ERR, "Could not fork process, error : %s", strerror(err));
      return -1;
    }
    else if (pid != 0) /*Parent exits*/
    exit(EXIT_SUCCESS);

    /*Create new session and process group*/
    if(setsid() == -1)
    {
      err = errno;
      syslog(LOG_ERR, "Could not set create new session, error : %s", strerror(err));
      return -1;
    }

    /*Set working directory to root*/
    if(chdir("/") == -1)
    {
      err = errno;
      syslog(LOG_ERR, "Could not change working dir to root, error : %s", strerror(err));
      return -1;
    }


    /*Close open files besides socketfd*/
    for(int i = 0; i < 3; i++)
    {
      if(i != socketfd)
      close(i);
    }

    /*redirect FD's 0, 1, 2, to /dev/null */
    open("/dev/null", O_RDWR);	/*stdin*/
    dup(0);				/*stdout*/
    dup(0);				/*stderror*/

  }


  /*Setup POSIX Timer and sigevent struct*/
  struct sigevent evp;
  timer_t timer;
  evp.sigev_value.sival_ptr = &timer;
  evp.sigev_notify = SIGEV_THREAD;	/*Specify behavior of timer expiration: create a new thread*/
  evp.sigev_notify_attributes = NULL;
  evp.sigev_notify_function = (void *) timestamp;

  if(timer_create(CLOCK_MONOTONIC, &evp, &timer))
  {
    err = errno;
    syslog(LOG_ERR, "error creating timer: %s", strerror(err));
    return -1;
  }

  #ifdef TIMESTAMP
  /*Set expiration for 10 seconds, rearm after expiration*/
  struct itimerspec exp;
  exp.it_interval.tv_sec = 10;
  exp.it_interval.tv_nsec = 0;
  exp.it_value.tv_sec = 10;
  exp.it_value.tv_nsec = 0;

  /*Arm the timer*/
  if(timer_settime(timer, 0, &exp, NULL))
  {
    err = errno;
    syslog(LOG_ERR, "error arming timer: %s", strerror(err));
    return -1;
  }
  #endif
  /*Listen*/
  if(listen(socketfd, 5))
  {
    err = errno;
    fprintf(stderr, "listen error: %s\n", strerror(err));
    return -1;
  }


  /*Accept, using listening socketfd and client*/
  struct sockaddr_in client_addr;
  socklen_t addrlen = sizeof(client_addr);
  memset(&client_addr, 0, addrlen);


  while(1)
  {
    int accepted = accept(socketfd, (struct sockaddr *)(&client_addr), &addrlen);
    if(accepted == -1)
    {
      /*Don't return, this will happen due to socket shutdown in sig handler*/
      syslog(LOG_ERR, "accept error: %s\n", strerror(err));
    }
    else
    {
      syslog(LOG_INFO, "Accepted connection from %s\n", inet_ntoa(client_addr.sin_addr));

      /*Create a new node with the thread*/
      Node * newNode = malloc(sizeof(Node));
      if(newNode == NULL)
      {
        err = errno;
        syslog(LOG_ERR, "Malloc failed for newNode: %s\n", strerror(err));
        return -1;
      }
      pthread_t newThread_id;
      newNode->next = NULL;
      newNode->thread_complete = false;
      newNode->connection_id = accepted;
      newNode->IPBuffer = inet_ntoa(client_addr.sin_addr);
      pthread_mutex_init(&newNode->nodeLock, NULL);

      /*Add new node to end of the list.
      *Since this is the only thead writing to the Linked list, only lock when updateing*/
      if(head == NULL)
      head = newNode;
      else
      {
        Node * traverse = head;
        while(traverse->next != NULL)
        traverse = traverse->next;

        pthread_mutex_lock(&traverse->nodeLock);
        traverse->next = newNode;
        pthread_mutex_unlock(&traverse->nodeLock);
      }

      /*Spawn new thread with default attributes, pass a void ptr to the LL node*/
      pthread_create(&newThread_id, NULL, processRX, (void *) newNode);

    }


    /*Check the linked list to see if any threads have completed. If so, join them and remove node*/
    Node * traverse = head;
    while(traverse != NULL)
    {
      syslog(LOG_INFO, "Checking for done threads in main...\n");

      pthread_mutex_lock(&traverse->nodeLock);
      /*Join the completed thread*/
      if(traverse->thread_complete == true)
      {
        /*Thread is complete, so it won't try to access the node anymore*/
        pthread_mutex_unlock(&traverse->nodeLock);

        /*Join the completed thread*/
        pthread_join(traverse->thread_id, NULL);
        syslog(LOG_INFO, "Joined thread %ld\n", traverse->thread_id);

        /*Adjust the linked list*/
        Node * temp = head;

        /*Node to be removed is last*/
        if(traverse->next == NULL)
        {
          /*Only 1 node in LL*/
          if(traverse == head)
          head = NULL;
          else
          {
            while(temp->next != traverse)
            temp = temp->next;
            pthread_mutex_lock(&temp->nodeLock);
            temp->next = NULL;
            pthread_mutex_unlock(&temp->nodeLock);
          }

          free(traverse);
          traverse = NULL;
        }
        /*Node to be removed is first*/
        else if(head == traverse)
        {
          head = traverse->next;
          free(traverse);
          traverse = head;
        }
        else
        {
          while(temp->next != traverse)
          temp = temp->next;
          pthread_mutex_lock(&temp->nodeLock);
          temp->next = traverse->next;
          pthread_mutex_unlock(&temp->nodeLock);

          free(traverse);
          traverse = temp->next;

        }

      }
      else
      {
        pthread_mutex_unlock(&traverse->nodeLock);
        traverse = traverse->next;
      }
    }

    if(signal_exit == true)
    {
      int cleaned = 0;

      syslog(LOG_INFO, "Cleaning up LL\n");

      #ifdef TIMESTAMP
      /*Destroy the timer*/
      if(timer_delete(timer))
      {
        err = errno;
        syslog(LOG_ERR, "Unable to delete timer, error %s\n", strerror(err));
      }
      #endif



      /*Kill each running thread and close all client connections in Linked List*/
      while(head != NULL)
      {

        if(head->thread_complete == false)
        {
          syslog(LOG_ERR, "Incomplete\n");
          int connection = head->connection_id;

          shutdown(connection, SHUT_RDWR);

          if(close(connection))
          {
            err = errno;
            syslog(LOG_ERR, "Unable to close FD  %d, error: %s\n", connection, strerror(err));
            fprintf(stderr, "could not close FD %d, error: %s\n", connection, strerror(err));
          }
          else
          syslog(LOG_INFO, "Closed connection from %s\n", head->IPBuffer);
        }
  syslog(LOG_ERR, "Complete\n");


        Node * temp = head;
        head = head->next;
        free(temp);
        cleaned++;
        syslog(LOG_INFO, "Freed %d nodes\n", cleaned);
      }

      /*Destroy the writefile mutex*/
      pthread_mutex_destroy(&writefileLock);

      /*Close the socket*/
      shutdown(socketfd, SHUT_RDWR);
      if(close(socketfd))
      {
        err = errno;
        fprintf(stderr, "could not close FD %d, error: %s\n", socketfd, strerror(err));
      }
      else
      syslog(LOG_INFO, "Closed socket, FD %d\n", socketfd);

      /*Free socket pointer*/
      freeaddrinfo(serverinfo);

      /*Delete /var/tmp/aesdsocketdata */
      if(remove(writefile))
      {
        err = errno;
        syslog(LOG_ERR, "Unable to remove %s, error: %s\n", writefile, strerror(err));
      }
      else
      syslog(LOG_INFO, "Removed /var/tmp/aesdsocketdata");

      return 0;
    }
  }

  return 0;
}

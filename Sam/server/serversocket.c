/* serversocket.c
* Author: Sam Solondz
* Description: This file implements a server to store data for each connection
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

/*Large buffer size to account for .ppm image data*/
#define RXBUFFERSIZE 240000


//Path to log directory
#define LOG_DIR "/var/tmp/logs/"

typedef struct node
{
  pthread_mutex_t nodeLock;
  pthread_t thread_id;
  struct node * next;
  bool thread_complete;
  int connection_id;
  char * IPBuffer;
  char logFile [50];
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



/*Add a timestamp to the logFile. Mutex for file must be obtained by caller thread.
*
*@param fp file pointer passed from processRX()
*/
void addTimestamp(FILE * fp)
{
	int err = 0;
	if(!fp)
	{
		perror("Invalid file pointer");
		syslog(LOG_ERR, "Invalid FP passed to addTimestamp");
		return;
	}
	
	time_t raw;
  	struct tm *curr;

  	time(&raw);
  	curr = localtime(&raw);
  	if(curr == NULL)
  	{
   		err = errno;
    		syslog(LOG_ERR, "error getting current time: %s", strerror(err));
    		return;
  	}


	char timestamp[80] = {0};
	strftime(timestamp, sizeof(timestamp),"%d %b %Y %T: ", curr);

	int written;
  	int to_write = strlen(timestamp);
  	while((written = fwrite(timestamp, sizeof(char), strlen(timestamp), fp)) != to_write)
  	{
    		to_write -= written;
    		if(written == EOF)
    		{
      			err = errno;
      			perror("Write rxbuffer");
      			syslog(LOG_ERR, "Could not write rxbuffer, errno: %s", strerror(err));

     			 return;
    		}
  	}

	return;

}

/*
* Processes incoming packets from a TCP socket.
*
* @param args void pointer to structure of arguments
*/
void * processRX(void * args)
{

  /*Extract the needed information and free the arguments struct from heap*/
  pthread_mutex_lock(&((Node *)args)->nodeLock);
  Node * info = (Node *)args;
  pthread_mutex_unlock(&((Node *)args)->nodeLock);
  
  int err = 0;
  const pthread_t self_id = pthread_self();
  //syslog(LOG_INFO, "Spawned thread ID: %ld\n", self_id);

  char IPBuffer[50] = {0};
  char logFile[50] = {0};

  pthread_mutex_lock(&info->nodeLock);
  int accepted = info->connection_id; 
  info->thread_id = self_id;
  
  strcpy(IPBuffer, info->IPBuffer);
  strcpy(logFile, info->logFile);
  
  pthread_mutex_unlock(&info->nodeLock);

  FILE * fp;
  /*Open the file for read/append, create if it doesn't exist*/
  if(strcmp(IPBuffer, "71.205.27.171") == 0) 
  {
	//syslog(LOG_INFO, "Image file IP");
	
	/*Overwrite the image file each time*/
	fp = fopen(logFile, "w+");
  }
  else
  {
	fp = fopen(logFile, "a+");
  }
  
  if(!fp)
  {
    err = errno;
    perror("File creation");
    syslog(LOG_ERR, "Could not create file %s, errno: %s", logFile, strerror(err));
    return 0;
  }

  
 
  int total_count = 0;
  /*Receive and write date*/
  while(!signal_exit)
  {
    /*If packet is greater than RXBUFFERSIZE, it will be handled on the next loop because of TCP stream socket.
    Will be queued by kernel for next loop. */

    char * rxbuffer = (char *) malloc(RXBUFFERSIZE);
    //memset(rxbuffer, '\0', sizeof(rxbuffer));
    int totalwritten = 0;
    int rxcount = 0;
 
    rxcount = recv(accepted, (void *)(rxbuffer), RXBUFFERSIZE, 0);
    total_count += rxcount;
   
    if(rxcount ==  0)
    {
      syslog(LOG_ERR, "Client %s disconnected",  IPBuffer);
      break;
    }
    else
    {
      //syslog(LOG_INFO, "Received %d bytes from %s", rxcount, IPBuffer);
      totalwritten += rxcount;
    
    }
    int to_write = rxcount;
    int written = 0;
    //syslog(LOG_INFO, "Total count = %d", total_count);
    
    /*Add a timestamp if this is not image data*/
   if(strcmp(IPBuffer, "71.205.27.171"))
   	addTimestamp(fp);
   
     pthread_mutex_lock(&info->nodeLock);
    /*Ensure entire buffer is written*/
    while((written = fwrite((char *)rxbuffer, sizeof(char), rxcount, fp)) < to_write)
    {
      //syslog(LOG_INFO, "wrote %d bytes", written); 
      to_write -= written;
     if(written == EOF)
      {
        pthread_mutex_unlock(&info->nodeLock);
        err = errno;
        perror("Write rxbuffer");
        syslog(LOG_ERR, "Could not write rxbuffer, errno: %s", strerror(err));
        break;
      }
    }

    //syslog(LOG_INFO, "wrote %d bytes", written); 
    pthread_mutex_unlock(&info->nodeLock);

    free(rxbuffer);
  }
	

  /*Close file pointer to writefile */
  fclose(fp);

 
  pthread_mutex_lock(&info->nodeLock);
  info->thread_complete = true;
  pthread_mutex_unlock(&info->nodeLock);


  //syslog(LOG_INFO, "Thread %ld complete! \n", self_id);

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


	/*Delete all logs*/
	char createCmd[50] = "mkdir ";
	strcat(createCmd, LOG_DIR);
	system(createCmd);

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
      pthread_mutex_init(&newNode->nodeLock, NULL);

      pthread_mutex_lock(&newNode->nodeLock);
      newNode->next = NULL;
      newNode->thread_complete = false;
      newNode->connection_id = accepted;
      newNode->IPBuffer = inet_ntoa(client_addr.sin_addr);
      pthread_mutex_unlock(&newNode->nodeLock);

      strcpy(newNode->logFile, LOG_DIR);
      strcat(newNode->logFile, newNode->IPBuffer);
      //syslog(LOG_INFO, "Log file: %s", newNode->logFile);

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
      //syslog(LOG_INFO, "Checking for done threads in main...\n");

      pthread_mutex_lock(&traverse->nodeLock);
      /*Join the completed thread*/
      if(traverse->thread_complete == true)
      {
        /*Thread is complete, so it won't try to access the node anymore*/
        pthread_mutex_unlock(&traverse->nodeLock);
	
	
  	/*Close the connection*/
  	shutdown(traverse->connection_id, SHUT_RDWR);
  	if(close(traverse->connection_id))
  	{
    		err = errno;
    		syslog(LOG_ERR, "Unable to close FD  %d, error: %s\n", traverse->connection_id, strerror(err));
    		fprintf(stderr, "could not close FD %d, error: %s\n", traverse->connection_id, strerror(err));
  	}

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
      /*Kill each running thread and close all client connections in Linked List*/
      while(head != NULL)
      {

        if(head->thread_complete == false)
        {
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
	

	/*Delete all logs*/
	char deleteCmd[50] = "exec rm -r ";
	strcat(deleteCmd, LOG_DIR);
	strcat(deleteCmd, "*");
	system(deleteCmd);

      return 0;
    }
  }

  return 0;
}

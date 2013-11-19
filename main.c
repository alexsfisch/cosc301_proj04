#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "network.h"


// global variable; can't be avoided because
// of asynchronous signal interaction
struct work_queue_item *head = NULL;
struct work_queue_item *tail = NULL;
struct thread *ThreadHead = NULL;
int queue_count = 0;
pthread_mutex_t work_mutex; //need to initalize
pthread_cond_t work_cond; //need to initialize

int still_running = TRUE;
void signal_handler(int sig) {
    still_running = FALSE;
}


struct work_queue_item * removeItem() {
	struct work_queue_item *temp = head; 

	if (head->next == NULL) { //1 item
		head = NULL;
		tail = NULL;
	}
	else if (head != NULL) { //list of any length
		head = head->next;	
	}

	return temp;

}

void addToLinkedListItem(int sock) {

	struct work_queue_item *new =  malloc(sizeof(struct work_queue_item));
	new->sock = sock;
	new->next= NULL;
	if(tail == NULL)
		printf("%s\n","Tail is NULL!");

	if (tail == NULL) {
		head = new;
	}
	else {
		tail->next = new;
	}
	tail = new;
}

void usage(const char *progname) {
    fprintf(stderr, "usage: %s [-p port] [-t numthreads]\n", progname);
    fprintf(stderr, "\tport number defaults to 3000 if not specified.\n");
    fprintf(stderr, "\tnumber of threads is 1 by default.\n");
    exit(0);
}

void runserver(int numthreads, unsigned short serverport) {
	int lock;
	int i = 0;
	int rc;
	pthread_t p;
	pthread_t threadArray[numthreads];
    //////////////////////////////////////////////////

    // create your pool of threads here

    //////////////////////////////////////////////////

	//array of pthread t's
   	//made a struct....nvm didn't do this either
	//before that, create/init condition variable and mutex
	//for loop to start some number of worker threads
	
	for (;i<numthreads;i++) {
		rc = pthread_create(&threadArray[i], NULL, worker, NULL);
	}
	//separate funtions for threads to start up in, above runserver.
	//particular signature...
	
    
    int main_socket = prepare_server_socket(serverport);
    if (main_socket < 0) {
        exit(-1);
    }
    signal(SIGINT, signal_handler);

    struct sockaddr_in client_address;
    socklen_t addr_len;

    fprintf(stderr, "Server listening on port %d.  Going into request loop.\n", serverport);
    while (still_running) {
		printf("%s\n","HERE");
        struct pollfd pfd = {main_socket, POLLIN};
        int prv = poll(&pfd, 1, 10000);

        if (prv == 0) {
            continue;
        } else if (prv < 0) {
            PRINT_ERROR("poll");
            still_running = FALSE;
            continue;
        }
        
        addr_len = sizeof(client_address);
        memset(&client_address, 0, addr_len);

        int new_sock = accept(main_socket, (struct sockaddr *)&client_address, &addr_len);
        if (new_sock > 0) {
            printf("%s\n","got a new request");
            time_t now = time(NULL);
            fprintf(stderr, "Got connection from %s:%d at %s\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port), ctime(&now));

           ////////////////////////////////////////////////////////
           /* You got a new connection.  Hand the connection off
            * to one of the threads in the pool to process the
            * request.
            *
            * Don't forget to close the socket (in the worker thread)
            * when you're done.
            */
           ////////////////////////////////////////////////////////
		


			pthread_mutex_lock(&work_mutex); //lock
	
			addToLinkedListItem(new_sock);
			printf("%i\n",head); 
			queue_count +=1;
			printf("%s","Queue Count:   ");
			printf("%i\n", queue_count);
			pthread_cond_signal(&work_cond); 			//activate worker on it

		   	 pthread_mutex_unlock(&work_mutex); //unlock (not sure if int he right place)
			
			printf("%s\n","test");
        }
    }
    fprintf(stderr, "Server shutting down.\n");
    close(main_socket);
	//wait for all threads to die
}


void worker(){
	printf("%s\n","Created Thread");
	while(still_running) {
		pthread_mutex_lock(&work_mutex);	
		while (queue_count == 0) {
			pthread_cond_wait(&work_cond, &work_mutex); //not sure about second parameter
		}


		printf("%s\n","worker thread activated");
		printf("%i\n",work_mutex);

		struct work_queue_item *temp = NULL;
	
		
		//remove item
		temp = removeItem();
		char buffer[1024];
		//getrequest(temp->sock, buffer, 1024);
		memset(buffer, 0, 1024);
		recv(temp->sock, buffer, 1024, 0);
		printf("got <%s> from remote\n", buffer);
		send(temp->sock, buffer, strlen(buffer), 0);
		printf("%i\n",temp->sock);
		queue_count -= 1;
		pthread_mutex_unlock(&work_mutex);	
		close(temp->sock);
		printf("%s\n","Removed Item");
		//respond to request on temp
	
		free(temp); //free temp
	}
	//some code to kill threads
	
}



/*void addToLinkedListThread(){
	struct thread *new = malloc(sizeof(struct thread));
	if (ThreadHead == NULL) {
		head = new;
	}
	new->next = NULL;
}*/
int main(int argc, char **argv) {
	pthread_mutex_init(&work_mutex, NULL); //correct place???
	pthread_cond_init(&work_cond, NULL); //correct place?
    unsigned short port = 3000;
    int num_threads = 1;

    int c;
    while (-1 != (c = getopt(argc, argv, "hp:t:"))) {
        switch(c) {
            case 'p':
                port = atoi(optarg);
                if (port < 1024) {
                    usage(argv[0]);
                }
                break;

            case 't':
                num_threads = atoi(optarg);
                if (num_threads < 1) {
                    usage(argv[0]);
                }
                break;
            case 'h':
            default:
                usage(argv[0]);
                break;
        }
    }

    runserver(num_threads, port);
    
    fprintf(stderr, "Server done.\n");
    exit(0);
}

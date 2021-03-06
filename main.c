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
#include <stdio.h>
#include <string.h>


//		strcat(fileFullName,directory);
		//strcat(fileFullName,filename);

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
	}

	return temp;

}

void addToLinkedListItem(int sock, struct in_addr clientID, in_port_t port, time_t time) {

	struct work_queue_item *new =  malloc(sizeof(struct work_queue_item));
	new->sock = sock;
	new->clientIP = clientID;
	new->timestamp = time;
	new->clientPort = port;
	new->next= NULL;
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
	pthread_t threadArray[numthreads];
    //////////////////////////////////////////////////

    // create your pool of threads here

    //////////////////////////////////////////////////


	
	for (;i<numthreads;i++) {
		rc = pthread_create(&threadArray[i], NULL, worker, NULL);
	}
	
    
    int main_socket = prepare_server_socket(serverport);
    if (main_socket < 0) {
        exit(-1);
    }
    signal(SIGINT, signal_handler);

    struct sockaddr_in client_address;
    socklen_t addr_len;

    fprintf(stderr, "Server listening on port %d.  Going into request loop.\n", serverport);
    while (still_running) {
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
			addToLinkedListItem(new_sock, client_address.sin_addr, client_address.sin_port, &now);
			queue_count +=1;
			printf("%s","Queue Count:   ");
			printf("%i\n", queue_count);
			pthread_cond_signal(&work_cond); //activate worker on it
		   	pthread_mutex_unlock(&work_mutex); //unlock
			
        }
    }
    fprintf(stderr, "Server shutting down.\n");
    close(main_socket);
}


void worker(){
	int fileExists = 1;
	char cwd[1024];
	getcwd(cwd,1024); //get current directory
	struct stat sb;
	char header[1024];
	char fullFileName[1024];
	FILE *file;
	printf("%s\n","Created Thread");
	
	while(still_running) {
		pthread_mutex_lock(&work_mutex);	
		while (queue_count == 0) {
			pthread_cond_wait(&work_cond, &work_mutex); 
		}

		memset(fullFileName, 0, 1024); //reset char[]

		printf("%s\n","worker thread activated");

		struct work_queue_item *temp = NULL;
	

		temp = removeItem(); //remove item
		queue_count -= 1; //update queue
		pthread_mutex_unlock(&work_mutex);	 //unlock

		//executing request
		char buffer[1024];

		getrequest(temp->sock, buffer, 1024);
		printf("%s","buffer before: ");
		printf("%s\n",buffer);

		strcat(fullFileName,cwd);
		strcat(fullFileName,buffer);

		fileExists = stat(fullFileName, &sb);
		printf("%s","File:   ");
		printf("%s\n", fullFileName);
		temp->totalRequestSize = sb.st_size;
		if (fileExists==0) {
			printf("%s\n","FILE FOUND!!!!!!");

			memset(header, 0, 1024); //reset char[]

			//send header
			sprintf(header, HTTP_200, temp->totalRequestSize);
			senddata(temp->sock,header, strlen(header));

			//open file
			file = fopen(fullFileName,"r");
			int numberBytes = 0;
			//read through file contents
			while ((numberBytes = fread(header,1, 1024,file)==1024)) {
				//printf("%s\n",header);
				senddata(temp->sock,header, numberBytes);
				memset(header, 0, 1024); //reset char[]
			}
			//printf("%s\n",header);
			senddata(temp->sock,header, numberBytes);
			fclose(file);



			file = fopen("weblog.txt","a+"); //append file
			fprintf(file, "%s:%d %s GET %s %i %zu\n", inet_ntoa(temp->clientIP), ntohs(temp->clientPort), ctime(temp->timestamp), fullFileName, 200, temp->totalRequestSize); 
			fclose(file);
		}
		else {
			printf("%s\n", "file does not exist");
			senddata(temp->sock, HTTP_404, strlen(HTTP_404));
			file = fopen("weblog.txt","a+"); //append file
			fprintf(file, "%s:%d %s GET %s %i %zu\n", inet_ntoa(temp->clientIP), ntohs(temp->clientPort), ctime(temp->timestamp), fullFileName, 404, temp->totalRequestSize);
			fclose(file);

		}
	

		close(temp->sock);
		printf("%s\n","Removed Item. Sock Closed.");
		printf("%s\n","------------------------------------------------\n");
	
		free(temp); //free temp

	}
	
}

int main(int argc, char **argv) {
	pthread_mutex_init(&work_mutex, NULL); 
	pthread_cond_init(&work_cond, NULL); 
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

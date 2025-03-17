#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <arpa/inet.h>

#define KEY "/"
#define VAL 65

#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)
struct sembuf pop, vop;

// Define a fixed-size array for tasks
#define MAX_TASKS 1000
#define MAX_RESULTS 1000
#define TIMEOUT_DURATION 30

int task_count = 0, task_number = 0;

// message that stores the directive (getting task/result)
// and a type that is analogous to "status code" to indicate the situation
typedef struct {
    int type;
    char data[50];
}message;


const char filename[20] = "tasks.txt";

typedef struct {
    int id;
    char expression[50];
}task_item;

typedef struct {
    task_item tasks[MAX_TASKS];
    int front;
    int rear;
    int count;
}task_queue;

typedef struct {
    int task_id;
    int computed_value;
}result_item;

typedef struct {
    result_item results[MAX_RESULTS];
    int count;
}result_queue;

task_queue *shared_task_q;
result_queue *shared_result_q;

int is_queue_empty() {
    return (shared_task_q->count == 0);
}

int is_queue_full() {
    return (shared_task_q->count == MAX_TASKS);
}

// add a task in the task queue
void add_task(int id, char expression[]) {
    if (is_queue_full()) {
        printf("Task queue is full\n");
        return;
    }
    
    shared_task_q->rear = (shared_task_q->rear + 1) % MAX_TASKS;
    shared_task_q->tasks[shared_task_q->rear].id = (id != -1) ? id : ++task_count;
    strcpy(shared_task_q->tasks[shared_task_q->rear].expression, expression);
    shared_task_q->count++;
}

// get the next task from the queue
task_item* get_task() {
    if (is_queue_empty()) {
        return NULL;
    }
    
    task_item* task = &shared_task_q->tasks[shared_task_q->front];
    shared_task_q->front = (shared_task_q->front + 1) % MAX_TASKS;
    shared_task_q->count--;

    return task;
}

// read the config file to populate the shared task queue
void populate_TQ() {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error opening file\n");
        exit(0);
    }

    char expression[50];
    while (fscanf(file, "%[^\n]\n", expression) != EOF) {
        add_task(-1, expression);
    }

    fclose(file);    
    return;
}

// insert the computed result in the result queue at appropriate position based on task_id
void insert_computed_result(int task_id, int computed_value) {
    if (shared_result_q->count >= MAX_RESULTS) {
        printf("Result queue is full\n");
        return;
    }
    
    int pos = shared_result_q->count;
    while (pos > 0 && shared_result_q->results[pos-1].task_id > task_id) {
        shared_result_q->results[pos] = shared_result_q->results[pos-1];
        pos--;
    }
    
    shared_result_q->results[pos].task_id = task_id;
    shared_result_q->results[pos].computed_value = computed_value;
    shared_result_q->count++;
}

void send_message(int type, char data[], int sockfd) {
    message msg;
    msg.type = type;
    strcpy(msg.data, data);
    for(int i=strlen(msg.data); i<50; i++) {
        msg.data[i] = '\0';
    }
    send(sockfd, &msg, sizeof(message), 0);
}

// get the next task to be processes and allocate it to the client with comm_sockfd
void allocate_task(int comm_sockfd, int *is_worker_processing, char task_expression_for_worker[]) {
    message msg;
    if(*is_worker_processing != -1) {
        send_message(406, "Please process previous task first", comm_sockfd);
        return;
    }

    task_item* task = get_task();
    if (task == NULL) {
        send_message(404, "No tasks available", comm_sockfd);
        // mark the task_id to be -1, indicating that the client is not processing any tasks currently
        *is_worker_processing = -1;
        bzero(task_expression_for_worker, 50);
        return;
    }
    
    // mark the task_id to be the sequential number of the task
    *is_worker_processing = ++task_number;
    strcpy(task_expression_for_worker, task->expression);
    
    send_message(200, task->expression, comm_sockfd);
}

// store the result sent by the client, in a priority queue based on task_id (so sequence is maintained)
void store_result(int comm_sockfd, char recv_buff[], int *is_worker_processing, char task_expression_for_worker[]) {  
    int computed_value = 0;

    for(int i=7; i<strlen(recv_buff); i++) {
        computed_value = computed_value*10 + (recv_buff[i] - '0');
    }

    insert_computed_result(*is_worker_processing, computed_value);
    // mark the task_id to be -1, indicating that the client is not processing any tasks currently
    *is_worker_processing = -1;
    bzero(task_expression_for_worker, 50);

    return;
}

int main() {
    // shared memory initialization
    int shmid = shmget(IPC_PRIVATE, sizeof(task_queue) + sizeof(result_queue), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget failed");
        exit(1);
    }
    
    void *shm_ptr = shmat(shmid, NULL, 0);
    if (shm_ptr == (void *) -1) {
        perror("shmat failed");
        exit(1);
    }
    
    shared_task_q = (task_queue *)shm_ptr;
    shared_result_q = (result_queue *)((char *)shm_ptr + sizeof(task_queue));
    
    shared_task_q->front = 0;
    shared_task_q->rear = -1;
    shared_task_q->count = 0;
    shared_result_q->count = 0;

    // semaphore initialization
    int q_sem = semget(ftok(KEY, VAL), 1, IPC_CREAT | 0666);
    if(q_sem < 0) {
        perror("semaphore creation failed\n");
        exit(0);
    }

    semctl(q_sem, 0, SETVAL, 1);
    
    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1; vop.sem_op = 1;

    populate_TQ();

    struct sockaddr_in server_addr, client_addr;
    int server_sockfd, comm_sockfd;
    socklen_t clilen;

    if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed\n");
        exit(0);
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(5050);
    
    if(bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("local address binding failed\n");
        exit(0);
    }
    
    int flags = fcntl(server_sockfd, F_GETFL, 0);
    fcntl(server_sockfd, F_SETFL, flags|O_NONBLOCK);

    listen(server_sockfd, 5);

    while(1) {
        usleep(1000);
        
        clilen = sizeof(client_addr);
        comm_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &clilen);
            
        if(comm_sockfd < 0) {
            if(errno != EWOULDBLOCK && errno != EAGAIN) {
                perror("accept error");
                exit(1);
            }
            // continue because we are in non-blocking mode (EWOULDBLOCK or EAGAIN)
            continue;
        }
        
        flags = fcntl(comm_sockfd, F_GETFL, 0);
        fcntl(comm_sockfd, F_SETFL, flags|O_NONBLOCK);
        
        // store client details in server
        char* client_ip = inet_ntoa(client_addr.sin_addr);
        int client_port = ntohs(client_addr.sin_port);
        
        printf("Connection established with client: %s:%d\n", client_ip, client_port);

        pid_t pid = fork();
        if(!pid) {
            time_t connection_time = time(NULL);

            // variables for tracking the client status
            int is_worker_processing = -1;
            char task_expression_for_worker[50];
            
            close(server_sockfd);

            while(1) {
                message msg;
                int temp = 0;
                
                while(1) {
                    int num_received = recv(comm_sockfd, &msg, sizeof(message), MSG_WAITALL);

                    if(num_received < 0) {
                        // if timeout has occurred
                        if(time(NULL) - connection_time > TIMEOUT_DURATION) {
                            // add the task back to queue because client timed-out while processing
                            if(is_worker_processing != -1) {
                                add_task(is_worker_processing, task_expression_for_worker);
                                printf("Client %s:%d timed out during task processing\n", client_ip, client_port);
                                printf("Task ( %s ) added back to queue\n", task_expression_for_worker);

                                is_worker_processing = -1;
                                bzero(task_expression_for_worker, 50);
                            }
                            else {
                                printf("Client %s:%d timed out after connecting.\n", client_ip, client_port);
                            }
                            
                            send_message(405, "Closing connection due to inactivity", comm_sockfd);                            

                            close(comm_sockfd);
                            temp = 1;

                            break;
                        }

                        // if non-blocking case
                        if(errno == EWOULDBLOCK || errno == EAGAIN) {
                            usleep(1000);
                            continue;
                        } 
                        else {
                            perror("receive error");
                            close(comm_sockfd);
                            exit(1);
                        }
                    }
                    // client disconnected
                    else if(num_received == 0) {
                        printf("Client %s:%d disconnected\n", client_ip, client_port);

                        // if client disconnects while processing, add the task back to queue
                        if(is_worker_processing != -1) {
                            add_task(is_worker_processing, task_expression_for_worker);
                            printf("Allocated task ( %s ) is unprocessed, adding back to queue\n", task_expression_for_worker);
                            
                            is_worker_processing = -1;
                            bzero(task_expression_for_worker, 50);
                        }

                        close(comm_sockfd);
                        temp = 1;
                    }

                    break;
                }

                if(temp) exit(1);

                printf("Client %s:%d : %s\n", client_ip, client_port, msg.data);
                int message_type = msg.type;
                
                P(q_sem);
                    if(message_type == 201) 
                        allocate_task(comm_sockfd, &is_worker_processing, task_expression_for_worker);
                    else if(message_type == 203) {
                        printf("Client %s:%d requested to exit, closing connection\n", client_ip, client_port);
                        close(comm_sockfd);
                        V(q_sem);
                        exit(0);
                    }
                    else 
                        store_result(comm_sockfd, msg.data, &is_worker_processing, task_expression_for_worker); 
                    connection_time = time(NULL);
                V(q_sem);
            }
        }
        else {
            close(comm_sockfd);
            waitpid(-1, NULL, WNOHANG);
        }
    }
    
    for(int i=0; i<shared_result_q->count; i++) {
        printf("Task ID: %d, Computed Value: %d\n", 
               shared_result_q->results[i].task_id, 
               shared_result_q->results[i].computed_value);
    }

    shmdt(shm_ptr);
    shmctl(shmid, IPC_RMID, NULL);
    
    return 0;
}
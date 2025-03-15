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

#define KEY "/"
#define VAL 65
#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

// Define a fixed-size array for tasks
#define MAX_TASKS 100
#define MAX_RESULTS 100

int task_count = 0;
int ready_for_new[1050];
int task_id_for_worker[1050];

int task_number = 0;

typedef struct {
    int type;
    char data[50];
} message;

struct sembuf pop, vop;

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

// Add a task to the queue
void add_task(char expression[]) {
    if (is_queue_full()) {
        printf("Task queue is full\n");
        return;
    }
    
    shared_task_q->rear = (shared_task_q->rear + 1) % MAX_TASKS;
    shared_task_q->tasks[shared_task_q->rear].id = ++task_count;
    strcpy(shared_task_q->tasks[shared_task_q->rear].expression, expression);
    shared_task_q->count++;
}

task_item* get_task() {
    if (is_queue_empty()) {
        return NULL;
    }
    
    task_item* task = &shared_task_q->tasks[shared_task_q->front];
    shared_task_q->front = (shared_task_q->front + 1) % MAX_TASKS;
    shared_task_q->count--;
    
    return task;
}

void populateTaskQueue() {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error opening file\n");
        exit(0);
    }

    char expression[50];
    while (fscanf(file, "%[^\n]\n", expression) != EOF) {
        add_task(expression);
    }

    fclose(file);
    
    return;
}

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

void allocate_task(int comm_sockfd) {
    message msg;
    if(ready_for_new[comm_sockfd] == 0) {
        msg.type = 406;
        sprintf(msg.data, "Please process previous task first");
        for(int i=strlen(msg.data); i<50; i++) {
            msg.data[i] = '\0';
        }

        send(comm_sockfd, &msg, sizeof(message), 0);
        return;
    }

    ready_for_new[comm_sockfd] = 0;

    task_item* task = get_task();
    if (task == NULL) {
        msg.type = 404;
        strcpy(msg.data, "No tasks available");
        for(int i=strlen(msg.data); i<50; i++) {
            msg.data[i] = '\0';
        }
        send(comm_sockfd, &msg, sizeof(message), 0);
        ready_for_new[comm_sockfd] = 1;
        return;
    }
    
    task_id_for_worker[comm_sockfd] = ++task_number;
    
    msg.type = 200;
    strcpy(msg.data, task->expression);
    for(int i=strlen(msg.data); i<50; i++) {
        msg.data[i] = '\0';
    }

    send(comm_sockfd, &msg, sizeof(message), 0);
}

void store_result(int comm_sockfd, char recv_buff[]) {  
    int computed_value = 0;

    for(int i=7; i<strlen(recv_buff); i++) {
        computed_value = computed_value*10 + (recv_buff[i] - '0');
    }

    insert_computed_result(task_id_for_worker[comm_sockfd], computed_value);
    ready_for_new[comm_sockfd] = 1;
    return;
}

int main() {
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

    int q_sem = semget(ftok(KEY, VAL), 1, IPC_CREAT | 0666);
    if(q_sem < 0) {
        perror("Error creating semaphore\n");
        exit(0);
    }

    semctl(q_sem, 0, SETVAL, 1);
    
    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1; vop.sem_op = 1;

    populateTaskQueue();

    for(int i=0; i<1050; i++) {
        ready_for_new[i] = 1;
        task_id_for_worker[i] = -1;
    }

    struct sockaddr_in server_addr, client_addr;
    int server_sockfd, comm_sockfd;
    socklen_t clilen;

    if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Unable to create socket\n");
        exit(0);
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(5050);
    
    if(bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Unable to bind local address\n");
        exit(0);
    }
    
    int flags = fcntl(server_sockfd, F_GETFL, 0);
    fcntl(server_sockfd, F_SETFL, flags|O_NONBLOCK);

    int yes = 1;
    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        perror("server: setsockopt");
        exit(1);
    }

    listen(server_sockfd, 5);

    while(1) {
        usleep(1000);

        clilen = sizeof(client_addr);
        comm_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &clilen);
        
        if(comm_sockfd < 0) {
            if(errno != EWOULDBLOCK && errno != EAGAIN) {
                perror("Accept error");
                exit(1);
            }
            continue;
        }

        flags = fcntl(comm_sockfd, F_GETFL, 0);
        fcntl(comm_sockfd, F_SETFL, flags|O_NONBLOCK);

        printf("A client connected\n");

        pid_t pid = fork();
        if(!pid) {
            close(server_sockfd);

            while(1) {
                message msg;
                int temp = 0;
                
                while(1) {
                    int num_received = recv(comm_sockfd, &msg, sizeof(message), MSG_WAITALL);

                    if(num_received < 0) {
                        if(errno == EWOULDBLOCK || errno == EAGAIN) {
                            usleep(1000);
                            continue;
                        } else {
                            perror("Receive error");
                            close(comm_sockfd);
                            exit(1);
                        }
                    }
                    else if(num_received == 0) {
                        printf("Worker client disconnected\n");
                        close(comm_sockfd);
                        temp = 1;
                    }

                    break;
                }

                if(temp) {
                    exit(1);
                }

                printf("Message received: %s, id: %d\n", msg.data, msg.type);

                int message_type = msg.type;

                P(q_sem);
                if(message_type == 201) {
                    allocate_task(comm_sockfd);
                }
                else {
                    store_result(comm_sockfd, msg.data);
                }
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
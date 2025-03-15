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
#include <string.h>

#define KEY "/"
#define VAL 65
#define P(s) semop(s, &pop, 1);
#define V(s) semop(s, &vop, 1);

int task_count = 0;
int ready_for_new[1050];
int task_id_for_worker[1050];

int task_number = 0;

typedef struct {
    int type;
    char data[50];
}message;

struct sembuf pop, vop;

const char filename[20] = "tasks.txt";

// queue of computational tasks
typedef struct task {
    int id;
    char expression[50];
    struct task *next;
}task;

task *head = NULL;

void add_task(char expression[]) {
    task_count++;

    task *new_task = (task *)malloc(sizeof(task));
    strcpy(new_task->expression, expression);
    new_task->id = task_count;
    new_task->next = NULL;

    if (head == NULL) {
        head = new_task;
    } else {
        task *current = head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_task;
    }
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

typedef struct computed_result {
    int task_id;
    int computed_value;
    struct computed_result *next;
}computed_result;

computed_result *priority_head = NULL;

void insert_computed_result(int task_id, int computed_value) {
    computed_result *new_task = (computed_result *)malloc(sizeof(computed_result));
    new_task->task_id = task_id;
    new_task->computed_value = computed_value;
    new_task->next = NULL;

    if (priority_head == NULL || priority_head->task_id > task_id) {
        new_task->next = priority_head;
        priority_head = new_task;
    } else {
        computed_result *current = priority_head;
        while (current->next != NULL && current->next->task_id <= task_id) {
            current = current->next;
        }
        new_task->next = current->next;
        current->next = new_task;
    }
}

computed_result *pop_computed_result() {
    if (priority_head == NULL) {
        return NULL;
    }
    computed_result *task_to_return = priority_head;
    priority_head = priority_head->next;
    return task_to_return;
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

    task *current = head;
    head = head->next;    
    task_id_for_worker[comm_sockfd] = ++task_number;
    
    printf("Task ID: %d, Task: %s\n", task_number, current->expression);

    msg.type = 200;
    strcpy(msg.data, current->expression);
    for(int i=strlen(msg.data); i<50; i++) {
        msg.data[i] = '\0';
    }

    send(comm_sockfd, &msg, sizeof(message), 0);

    free(current);

    return;
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
    int clilen;

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

    listen(server_sockfd, 5);

    while(1) {
        usleep(1000);

        clilen = sizeof(struct sockaddr_in);
        comm_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &clilen);
        int flags = fcntl(comm_sockfd, F_GETFL, 0);
        fcntl(comm_sockfd, F_SETFL, flags|O_NONBLOCK);

        if(comm_sockfd < 0) {
            // printf("No worker client connected, meanwhile add a task 1) Yes 2) No\n");
            // int choice; scanf("%d", &choice);
            // if(choice == 1) {
            //     int op1, op2; char op;
            //     printf("Enter first operand: ");
            //     scanf(" %d", &op1);
            //     printf("Enter operator: ");
            //     scanf(" %c", &op);
            //     printf("Enter second operand: ");
            //     scanf(" %d", &op2);

            //     char temp[50];
            //     sprintf(temp, "%d %c %d", op1, op, op2);

            //     P(q_sem);
            //     add_task(temp);
            //     V(q_sem);
            // }
            continue;
        }

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
                        continue;
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
                    // printf("--- ready: %d\n", ready_for_new[comm_sockfd]);
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
        }
    }

    // computed results
    computed_result *current = priority_head;
    while(current != NULL) {
        printf("Task ID: %d, Computed Value: %d\n", current->task_id, current->computed_value);
        current = current->next;
    }

    return 0;
}
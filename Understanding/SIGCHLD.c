// https://stackoverflow.com/questions/13792900/signal-and-sigchld-what-does-it-do

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define N 4
int val = 9;

void handler(int sig) {
   val += 3;
   return;
}

int main() {
  pid_t pid;
  int i;
  signal(SIGCHLD,handler);
  for (i=0;i<N;i++) {
    if ((pid=fork()) == 0) {
        val -= 3;
        exit(0);
    }
  }
  for (i=0;i<N;i++) {
    waitpid(-1,NULL,0);
  }
  printf("val = %d\n",val);
}
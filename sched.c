#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include <errno.h>

#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

static int pid_counter = 0;

int next_pid() {
  return pid_counter++;
}

typedef void (*process_fun)();
typedef enum { NEW, SUSPENDED, DEAD } process_state;

typedef struct {
  int pid;
  process_state state;
  process_fun init;
  ucontext_t context;
  char* stack;
  int stack_size;
} process;

#define MAX_PROCS 10
int cur_procs = 0;
static process *processes[MAX_PROCS];
static process *current_process = 0;

const int STACK_SIZE = 16384;

static ucontext_t new_context;
static char *scheduler_stack;
static ucontext_t scheduler_context;
static ucontext_t on_process_end;

process* make_process(process_fun init) {
  cur_procs++;
  process *p = (process*)malloc(sizeof(process));
  p->pid = next_pid();
  p->state = NEW;
  p->init = init;
  p->stack_size = STACK_SIZE;
  p->stack = (char*)malloc(STACK_SIZE);

  /* fill context */
  if(getcontext(&p->context) == -1) {
    printf("cant get context!!\n");
    exit(errno);
  }
  p->context.uc_stack.ss_sp = p->stack;
  p->context.uc_stack.ss_size = p->stack_size;
  p->context.uc_stack.ss_flags = 0;
  p->context.uc_link = &on_process_end;
  sigemptyset(&scheduler_context.uc_sigmask);
  makecontext(&p->context, p->init, 0);
  return p;
}

void destroy_process(process *p) {
  if(p->state == DEAD) {
    free(p->stack);
  }
}

int random_(int max) {
  return rand() % max;
}

int current_process_id() {
  int i;
  for(i = 0; i < cur_procs; i++) {
    if(processes[i] == current_process) {
      return i;
    }
  }
  printf("no process found!!\n");
  return -1;
}

void delay_signal(int sig, int nsec) {};

void scheduler() {
  printf(">> entering scheduler\n");
  int process_id;
  if(current_process) {
    current_process->context = new_context;
    process_id = random_(cur_procs-1);
    int curr_id = current_process_id();
    if(process_id >= curr_id) {
      process_id++;
    }
  } else {
    process_id = random_(cur_procs);
  }

  current_process = processes[process_id];
  if(current_process->state != DEAD) {
    printf(">> switching to pid: %d\n", current_process->pid);
    if(setcontext(&current_process->context) == -1) {
      printf("scheduler swap context failed!!");
      exit(errno);
    }
  }
  printf("scheduler exit!!\n");
  return;
}

void reinit_scheduler_context() {
  if(getcontext(&scheduler_context) == -1) {
    printf("scheduler context fail!!\n");
    exit(errno);
  }
  scheduler_context.uc_stack.ss_sp = scheduler_stack;
  scheduler_context.uc_stack.ss_size = STACK_SIZE;
  scheduler_context.uc_stack.ss_flags = 0;
  scheduler_context.uc_link = NULL;
  sigemptyset(&scheduler_context.uc_sigmask);
  makecontext(&scheduler_context, scheduler, 0);
}

void init_scheduler_context() {
  scheduler_stack = (char*)malloc(STACK_SIZE);
  reinit_scheduler_context();
}

void usr2_handler(int sig, siginfo_t *info, void *ctx) {
  /* printf("got signal\n"); */
  reinit_scheduler_context();
  /* printf("switching to scheduler\n"); */
  if(swapcontext(&new_context, &scheduler_context) == -1) {
    printf("failed to switch to scheduler!!\n");
    exit(errno);
  }
}

void fun1() {
  printf("fun1 start\n");
  while(1) {
    sleep(1);
    printf("fun1 execution\n");
    sleep(1);
  }
}

void fun2() {
  printf("fun2 start\n");
  while(1) {
    sleep(1);
    printf("fun2 execution\n");
    sleep(1);
  }
}

void fun3() {
  printf("fun3 start\n");
  while(1) {
    sleep(1);
    printf("fun3 execution\n");
    sleep(1);
  }
}

static ucontext_t main_context;

int main(int argc, char* argv[]) {
  struct sigaction action;
  action.sa_sigaction = usr2_handler;
  action.sa_flags = SA_SIGINFO;
  sigemptyset(&action.sa_mask);
    
  if(sigaction(SIGUSR2, &action, NULL) == -1) {
    printf("can't set usr2 singal handler\n");
    return errno;
  }

  processes[0] = make_process(fun1);
  processes[1] = make_process(fun2);
  processes[2] = make_process(fun3);

  init_scheduler_context();
  srand(time(NULL));

  if(fork()) {
    if(swapcontext(&main_context, &scheduler_context) == -1)  {
      printf("swap scheduler context failed!!\n");
      exit(errno);
    }
  } else {
    int ppid = getppid();
    while(1) {
      usleep(10000);
      kill(ppid, SIGUSR2);
    }
  }
  return 0;
}

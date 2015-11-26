#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include <errno.h>

#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/time.h>

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

const int STACK_SIZE = 65535;

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
    perror("make process - get context");
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

static struct itimerval scheduler_timer;

void set_timer(int usec) {
  scheduler_timer.it_value.tv_usec = usec;
  if(setitimer(ITIMER_REAL, &scheduler_timer, NULL) == -1) {
    perror("set timer - setitimer");
  }
}

void scheduler() {
  printf(">> entering scheduler\n");
  int process_id;
  if(current_process) {
    /* current_process->context = new_context; */
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
    set_timer(1000);
    if(setcontext(&current_process->context) == -1) {
      perror("scheduler - setcontext");
    }
  }
  printf("scheduler exit!!\n");
  return;
}

void reinit_scheduler_context() {
  if(getcontext(&scheduler_context) == -1) {
    printf("reinit_scheduler_context - getcontext");
  }
  scheduler_context.uc_stack.ss_sp = scheduler_stack;
  scheduler_context.uc_stack.ss_size = STACK_SIZE;
  scheduler_context.uc_stack.ss_flags = 0;
  scheduler_context.uc_link = NULL;
  sigemptyset(&scheduler_context.uc_sigmask);
  sigaddset(&scheduler_context.uc_sigmask, SIGALRM);
  makecontext(&scheduler_context, scheduler, 0);
}

void init_scheduler_context() {
  scheduler_stack = (char*)malloc(STACK_SIZE);
  reinit_scheduler_context();
}

void sigalrm_handler(int sig, siginfo_t *info, void *ctx) {
  /* printf("got signal\n"); */
  reinit_scheduler_context();
  /* printf("switching to scheduler\n"); */
  if(swapcontext(&current_process->context, &scheduler_context) == -1) {
    perror("sigalrm_handler - swapcontext");
  }
}

void fun0() {
  printf("fun0 start\n");
  while(1) {
    usleep(100);
    printf("fun0 execution\n");
  }
}

void fun1() {
  printf("fun1 start\n");
  while(1) {
    usleep(100);
    printf("fun1 execution\n");
  }
}

void fun2() {
  printf("fun2 start\n");
  while(1) {
    usleep(100);
    printf("fun2 execution\n");
  }
}

static ucontext_t main_context;

int main(int argc, char* argv[]) {
  struct sigaction action;
  action.sa_sigaction = sigalrm_handler;
  action.sa_flags = SA_RESTART | SA_SIGINFO;
  sigemptyset(&action.sa_mask);
    
  if(sigaction(SIGALRM, &action, NULL) == -1) {
    perror("main - sigaction");
    return errno;
  }

  processes[0] = make_process(fun0);
  processes[1] = make_process(fun1);
  processes[2] = make_process(fun2);

  init_scheduler_context();
  srand(time(NULL));

  scheduler_timer.it_value.tv_sec = 0;
  scheduler_timer.it_interval.tv_sec = 0;
  scheduler_timer.it_interval.tv_usec = 0;

  if(swapcontext(&main_context, &scheduler_context) == -1)  {
    perror("main - swapcontext");
    exit(errno);
  }
  return 0;
}

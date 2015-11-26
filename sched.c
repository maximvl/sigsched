#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/queue.h>
#include <sys/time.h>

static int pid_counter = 0;

int next_pid() {
  return pid_counter++;
}

typedef void (*process_fun)();
typedef enum { NEW, SUSPENDED, DEAD } process_state;

typedef struct message_s {
  void *data;
  int   size;
  TAILQ_ENTRY(message_s) next;
} message;

typedef struct {
  int pid;
  process_state state;
  process_fun init;
  ucontext_t context;
  char* stack;
  int stack_size;
  TAILQ_HEAD(,message_s) mailbox;
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

  TAILQ_INIT(&p->mailbox);

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


process* get_current_process() {
  int i = current_process_id();
  if(i < 0) {
    return NULL;
  }
  return processes[i];
}

process* get_process(int pid) {
  int i;
  for(i = 0; i < cur_procs; i++) {
    if(processes[i]->pid == pid) {
      return processes[i];
    }
  }
  printf("no process found!!\n");
  return NULL;
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
  enter_scheduler();
}
  
void enter_scheduler() {
  /* printf("got signal\n"); */
  reinit_scheduler_context();
  /* printf("switching to scheduler\n"); */
  if(swapcontext(&current_process->context, &scheduler_context) == -1) {
    perror("sigalrm_handler - swapcontext");
  }
}

void usr2_handler(int sig, siginfo_t *info, void *ctx) {
  enter_scheduler();
}

void send(int pid, void *msg, int msgsz) {
  process *dest = get_process(pid);
  message *m = malloc(sizeof(message));
  m->data = malloc(msgsz);
  m->size = msgsz;
  memcpy(m->data, msg, msgsz);
  TAILQ_INSERT_TAIL(&dest->mailbox, m, next);
}

void receive(void **msg, int *msgsz) {
  process *self = get_current_process();
  message *head;
  while((head = TAILQ_FIRST(&self->mailbox)) == NULL) {
    enter_scheduler();
  }
  *msg = head->data;
  *msgsz = head->size;
  TAILQ_REMOVE(&self->mailbox, head, next);
  free(head);
}

void self() {
  return current_process->pid;
}

void fun1() {
  char *str = "Hello, world!";
  printf("process %d start\n", self());
  while(1) {
    usleep(100);
    printf("process %d execution\n", self());
    send(1, (void*)str, strlen(str) + 1);
  }
}

void fun2() {
  char *data = NULL;
  int   size;
  printf("process %d start\n", self());
  while(1) {
    usleep(100);
    printf("process %d execution\n", self());
    receive((void**)&data, &size);
    printf("process %d got: %s (%d)\n", self(), data, size);
    free(data);
    data = NULL;
  }
}

void fun3() {
  printf("process %d start\n", self());
  while(1) {
    usleep(100);
    printf("process %d execution\n", self());
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

  processes[0] = make_process(fun1);
  processes[1] = make_process(fun2);
  processes[2] = make_process(fun3);

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

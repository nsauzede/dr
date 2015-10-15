#include <signal.h>
#include <syscall.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <unistd.h>
#include <linux/stat.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>

enum {
  USER_DR0 = 63*sizeof(void*),
  USER_DR1 = 64*sizeof(void*),
  USER_DR2 = 65*sizeof(void*),
  USER_DR3 = 66*sizeof(void*),
  USER_DR4 = 67*sizeof(void*),
  USER_DR5 = 68*sizeof(void*),
  USER_DR6 = 69*sizeof(void*),
  USER_DR7 = 70*sizeof(void*),
};

typedef enum {
  BREAK_ON_EXEC  = 0x00,
  BREAK_ON_WRITE = 0x01,
  BREAK_ON_RW    = 0x03,
} BreakFlags;

typedef enum {
  LEN_1 = 0x00,
  LEN_2 = 0x01,
  LEN_4 = 0x03,
} DataLength;
  
  
typedef struct {
  int        dr0_local:1;
  int        dr0_global:1;
  int        dr1_local:1;
  int        dr1_global:1;
  int        dr2_local:1;
  int        dr2_global:1;
  int        dr3_local:1;
  int        dr3_global:1;
  int        exact_local:1;
  int        exact_global:1;
  int        reserved:6;
  BreakFlags dr0_break:2;
  DataLength dr0_len:2;
  BreakFlags dr1_break:2;
  DataLength dr1_len:2;
  BreakFlags dr2_break:2;
  DataLength dr2_len:2;
  BreakFlags dr3_break:2;
  DataLength dr3_len:2;
} DR7;


void addwatchpoint(pid_t pid, void* address, 
                   void (*handler)(int, siginfo_t, void*))
{
  pid_t child;
  struct sigaction trap_action;

  sigaction(SIGTRAP, NULL, &trap_action);
  trap_action.sa_handler = (void (*)(int))handler;
  trap_action.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER;
  sigaction(SIGTRAP, &trap_action, NULL);

  if ((child=fork()) == 0) {
    // setup debugging registers
    uintptr_t dr0 = (uintptr_t)address;
    DR7 dr7 = {0};
    dr7.dr0_local = 1;
    dr7.dr0_break = BREAK_ON_WRITE;
    dr7.dr0_len   = LEN_4;
    
    if (ptrace(PTRACE_ATTACH, pid, NULL, NULL)) {		
      perror("attach");
      return;
    }

    while (ptrace(PTRACE_POKEUSER, pid, USER_DR0, dr0));

    printf("watchpoint for pid: %u, DR0: %x, DR7: %x\n", 
           pid, dr0, *(uintptr_t*)(&dr7));   
    if (ptrace(PTRACE_POKEUSER, pid, USER_DR7, dr7)) {
      perror("poke dr7");
      ptrace(PTRACE_DETACH, pid, NULL, NULL);
      return;
    }
    if (ptrace(PTRACE_POKEUSER, pid, USER_DR6, 0)) {
      perror("poke dr6");
      ptrace(PTRACE_DETACH, pid, NULL, NULL);
      return;
    }    
    if (ptrace(PTRACE_DETACH, pid, NULL, NULL)) {
      perror("detach");
    }
    return;
  }

  waitpid(child, NULL, 0);
}

volatile int var = 0;

void trap(int sig, siginfo_t info, void* uc) { 
  if (var == 50) {
    printf("caught: %d, run gdb - %d\n", var, getpid());
    while (1) {}
  }
}

pid_t gettid() {
  return syscall(__NR_gettid);
}


int main(int argc, char * argv[])
{
  int i;

  addwatchpoint(gettid(), (void*)&var, trap);
  
  for (i=0; i<100; i++) {
    var++;
  }

  return 0;  
}

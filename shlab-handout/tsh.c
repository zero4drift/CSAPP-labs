/* 
 * tsh - A tiny shell program with job control
 * 
 * <Put your name and login ID here>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
  pid_t pid;              /* job PID */
  int jid;                /* job ID [1, 2, ...] */
  int state;              /* UNDEF, BG, FG, or ST */
  char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
  char c;
  char cmdline[MAXLINE];
  int emit_prompt = 1; /* emit prompt (default) */

  /* Redirect stderr to stdout (so that driver will get all output
   * on the pipe connected to stdout) */
  dup2(1, 2);

  /* Parse the command line */
  while ((c = getopt(argc, argv, "hvp")) != EOF) {
    switch (c) {
    case 'h':             /* print help message */
      usage();
      break;
    case 'v':             /* emit additional diagnostic info */
      verbose = 1;
      break;
    case 'p':             /* don't print a prompt */
      emit_prompt = 0;  /* handy for automatic testing */
      break;
    default:
      usage();
    }
  }

  /* Install the signal handlers */

  /* These are the ones you will need to implement */
  Signal(SIGINT,  sigint_handler);   /* ctrl-c */
  Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
  Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

  /* This one provides a clean way to kill the shell */
  Signal(SIGQUIT, sigquit_handler); 

  /* Initialize the job list */
  initjobs(jobs);

  /* Execute the shell's read/eval loop */
  while (1) {

    /* Read command line */
    if (emit_prompt) {
      printf("%s", prompt);
      fflush(stdout);
    }
    if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
      app_error("fgets error");
    if (feof(stdin)) { /* End of file (ctrl-d) */
      fflush(stdout);
      exit(0);
    }

    /* Evaluate the command line */
    eval(cmdline);
    fflush(stdout);
    fflush(stdout);
  } 

  exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
 */
void eval(char *cmdline) 
{
  pid_t pid = 0;
  int state, bg_or_not, builtin_or_not, is_available_cmd;
  char *args[MAXARGS];
  char *first_cmd;
  sigset_t blockset;
  const char *err_msg = "%s: Command not found\n";
  /* 使用一个字符串数组来存储所有可以接受的命令 */
  /* 偷懒使用这样的硬编码方式，肯定有更好的在路径查询函数，比如查询 '/bin/echo' 是否存在 */
  /* 更新：这个函数就是C库函数 access, 这个硬编码套路留着以博一笑 :) */
  /* const char *available_cmds[5]={"./myint", "./myspin", "./mysplit", "./mystop", "/bin/echo"}; */
  /* 创建空的信号集合 */
  sigemptyset(&blockset);
  /* 添加指定信号到集合中 */
  sigaddset(&blockset, SIGCHLD);
  bg_or_not = parseline(cmdline, args);
  first_cmd = args[0];
  builtin_or_not = builtin_cmd(args);
  
  if(!builtin_or_not)
    {
      is_available_cmd = access(first_cmd, 00);
      /* 硬编码套路 */
      /* for(i=0;i<5;i++) */
      /* 	{ */
      /* 	  if(!strcmp(first_cmd, available_cmds[i])) */
      /* 	    { */
      /* 	      is_available_cmd = 1; */
      /* 	      break; */
      /* 	    } */
      /* } */
      if(is_available_cmd==-1)
	{
	  printf(err_msg, first_cmd);
	  return;
	}
      /* 阻塞对应信号，最后一个参数是 NULL 是因为不需要保存之前的集合为备份 */
      sigprocmask(SIG_BLOCK, &blockset, NULL);
      pid = fork();
      if(pid>0)
	{
	  if(bg_or_not)
	    {
	      state = BG;
	    }
	  else
	    {
	      state = FG;
	    }
	  addjob(jobs, pid, state, cmdline);
	  /* 在执行完 addjob 之后需要在父进程中取消阻塞 */
	  sigprocmask(SIG_UNBLOCK, &blockset, NULL);
	  if(state==FG)
	    {
	      /* 如果是 foreground job, 需要等待其完成 */
	      waitfg(pid);
	    }
	  else
	    {
	      printf("[%d] (%d) %s", maxjid(jobs), pid, cmdline);
	    }
	}
      if(pid==0)
	{
	  /* lab pdf 中的提示，后台子进程需要更改自己的组号 */
	  /* 因为 ./tsh 执行时其实就是一个 foreground process */
	  /* 任何本程序派生的子进程默认都在 foreground process group 里 */
	  setpgid(0, 0);
	  /* 子进程继承了父进程的阻塞状态，必须在 execve 前取消阻塞 */
	  sigprocmask(SIG_UNBLOCK, &blockset, NULL);
	  execve(args[0], args, environ);
	}
    }
  return;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
  static char array[MAXLINE]; /* holds local copy of command line */
  char *buf = array;          /* ptr that traverses command line */
  char *delim;                /* points to first space delimiter */
  int argc;                   /* number of args */
  int bg;                     /* background job? */

  strcpy(buf, cmdline);
  buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
  while (*buf && (*buf == ' ')) /* ignore leading spaces */
    buf++;

  /* Build the argv list */
  argc = 0;
  if (*buf == '\'') {
    buf++;
    delim = strchr(buf, '\'');
  }
  else {
    delim = strchr(buf, ' ');
  }

  while (delim) {
    argv[argc++] = buf;
    *delim = '\0';
    buf = delim + 1;
    while (*buf && (*buf == ' ')) /* ignore spaces */
      buf++;

    if (*buf == '\'') {
      buf++;
      delim = strchr(buf, '\'');
    }
    else {
      delim = strchr(buf, ' ');
    }
  }
  argv[argc] = NULL;
    
  if (argc == 0)  /* ignore blank line */
    return 1;

  /* should the job run in the background? */
  if ((bg = (*argv[argc-1] == '&')) != 0) {
    argv[--argc] = NULL;
  }
  return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
  char *first_cmd = argv[0];
  if(!strcmp(first_cmd, "quit"))
    {
      exit(0);
    }
  if(!strcmp(first_cmd, "jobs"))
    {
      listjobs(jobs);
      return 1;
    }
  if(!strcmp(first_cmd, "bg") || !strcmp(first_cmd, "fg"))
    {
      do_bgfg(argv);
      return 1;
    }
  return 0;     /* not a builtin command */
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv)
{
  int jid=0, pid=0;
  unsigned long i;
  const char *str_number;
  struct job_t *job;
  char *first_cmd = argv[0];
  char *second_cmd = argv[1];
  char e;
  /* 几个格式化字符串常量提示错误 */
  const char *err_msg1 = "%s command requires PID or %%jobid argument\n";
  const char *err_msg2 = "%s argument must be a PID or %%jobid\n";
  const char *err_msg3 = "%s: No such job\n";
  const char *err_msg4 = "(%d): No such process\n";
  
  if(second_cmd==NULL)
    {
      /* 如果 bg fg 没有带第二个参数 */
      printf(err_msg1, first_cmd);
      return;
    }
  if(second_cmd[0]=='%')
    {
      /* 如果第二个参数以 '%' 开头，就是一个 jobid */
      str_number = strtok(second_cmd, "%");
      /* 验证 '%' 后面的字符串是不是一个数字字符串 */
      for(i=0;i<strlen(str_number);i++)
	{
	  e = str_number[i];
	  if(e=='\0')
	    {
	      break;
	    }
	  if(e<'0' || e>'9')
	    {
	      /* 如果有数字字符以外的东西，报错 */
	      printf(err_msg2, first_cmd);
	      return;
	    }
	}
      /* 数字字符串转换为数字 */
      jid = atoi(str_number);
      job = getjobjid(jobs, jid);
      /* 如果 job 不存在的话，报错 */
      /* 其实这里也可以比较 jid 和 maxjid 执行获得的 jobs 中最大的 jid */
      if(!job)
	{
	  printf(err_msg3, second_cmd);
	  return;
	}
      else
	{
	  /* 这里需要拿到 pid, 后面会用 */
	  pid = job->pid;
	}
    }
  else
    {
      /* 同样的，对于不是以 '%' 开头的字符串，检查其是否是数字字符串 */
      for(i=0;i<strlen(second_cmd);i++)
	{
	  e = second_cmd[i];
	  if(e=='\0')
	    {
	      break;
	    }
	  if(e<'0' || e>'9')
	    {
	      printf(err_msg2, first_cmd);
	      return;
	    }
	}
      pid = atoi(second_cmd);
      job = getjobpid(jobs, pid);
      if(!job)
	{
	  /* 如果 job 不存在，报错 */
	  printf(err_msg4, pid);
	  return;
	}
    }
  /* 这也在 shlab pdf 中重点讲过 */
  /* The bg(fg) <job> command restarts <job> by sending it a SIGCONT signal */
  /* and then runs it in the background. */
  /* When you implement your signal handlers, be sure to send SIGINT and SIGSTP signals to the entire foreground process group */
  /* using "-pid" instead of "pid" in the argument to the kill function. */
  kill(-pid, SIGCONT);
  if(!strcmp(first_cmd, "bg"))
    {
      job->state = BG;
      printf("[%d] (%d) %s", jid, pid, job->cmdline);
    }
  else
    {
      job->state = FG;
      /* 当 job state 是 FG 的时候，要等待其完成 */
      waitfg(job->pid);
    }
  return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
  /* 我原来的做法是这样的，这当然可以解决问题，毕竟 forground job 结束之后会被 reap */
  /* 见 sigchld_handler, 最后执行的 deletejob 只是清理 job 的内容 */
  /* 指向这个 job 的指针仍然存在（内存空间没有被回收） */
  /* struct job_t *job = getjobpid(jobs, pid); */
  /* if(job) */
  /*   { */
  /*     while(job->state==FG) */
  /* 	{ */
  /* 	  sleep(1); */
  /* 	} */
  /*   } */
  while(pid == fgpid(jobs))
    {
      sleep(1);
    }
  return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
  /* 这个就要需要详细了解 waitpid 这个函数了，见 man waitpid */
  /* :( 其实看了文档之后还是有点找不到北，按照指导说明拼凑下面的代码，改了多次才验证成功 */
  pid_t pid;
  int status;
  struct job_t *job;
  /* waitpid 手册和网上关于它的只言片语拼凑出来的 */
  /* 为什么代码是这样的？目前的理解来说也给不出一个完全自洽的解释 */
  /* 最开始 waitpid 第二个参数是 NULL, 无法通过 trace16 的验证 */
  /* 这样的话，./tsh 无法捕捉从进程中发送的 SIGSTP 和 SIGINT */
  while((pid = waitpid(-1, &status, WUNTRACED|WNOHANG))>0)
    {
      if(WIFEXITED(status))	      /* process is exited in normal way */
	{
	  deletejob(jobs, pid);
	}
      if(WIFSIGNALED(status))	      /* process is terminated by a signal */
	{
	  printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(pid), pid, WTERMSIG(status));
	  deletejob(jobs, pid);
	}
      if(WIFSTOPPED(status))	      /* process is stop because of a signal */
	{
	  printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(pid), pid, WSTOPSIG(status));
	  job = getjobpid(jobs, pid);
	  if(job!=NULL)
	    {
	      job->state = ST;
	    }
	}
    }
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
  pid_t fpid;
  fpid = fgpid(jobs);
  if(fpid)
    {
      /* kill 这个用法在 do_bgfg 中的 kill 调用前注释讲过*/
      kill(-fpid, sig);
    }
  return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
  pid_t fpid;
  struct job_t * fjob;
  fpid = fgpid(jobs);
  if(fpid)
    {
      fjob = getjobpid(jobs, fpid);
      if(fjob->state==ST)
	{
	  return;
	}
      /* kill 这个用法在 do_bgfg 中的 kill 调用前注释讲过*/
      kill(-fpid, sig);
    }
  return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
  job->pid = 0;
  job->jid = 0;
  job->state = UNDEF;
  job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
  int i;

  for (i = 0; i < MAXJOBS; i++)
    clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
  int i, max=0;

  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].jid > max)
      max = jobs[i].jid;
  return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
  int i;
    
  if (pid < 1)
    return 0;

  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid == 0) {
      jobs[i].pid = pid;
      jobs[i].state = state;
      jobs[i].jid = nextjid++;
      if (nextjid > MAXJOBS)
	nextjid = 1;
      strcpy(jobs[i].cmdline, cmdline);
      if(verbose){
	printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
      }
      return 1;
    }
  }
  printf("Tried to create too many jobs\n");
  return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
  int i;

  if (pid < 1)
    return 0;

  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid == pid) {
      clearjob(&jobs[i]);
      nextjid = maxjid(jobs)+1;
      return 1;
    }
  }
  return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
  int i;

  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].state == FG)
      return jobs[i].pid;
  return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
  int i;

  if (pid < 1)
    return NULL;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].pid == pid)
      return &jobs[i];
  return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
  int i;

  if (jid < 1)
    return NULL;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].jid == jid)
      return &jobs[i];
  return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
  int i;

  if (pid < 1)
    return 0;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].pid == pid) {
      return jobs[i].jid;
    }
  return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
  int i;
    
  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid != 0) {
      printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
      switch (jobs[i].state) {
      case BG: 
	printf("Running ");
	break;
      case FG: 
	printf("Foreground ");
	break;
      case ST: 
	printf("Stopped ");
	break;
      default:
	printf("listjobs: Internal error: job[%d].state=%d ", 
	       i, jobs[i].state);
      }
      printf("%s", jobs[i].cmdline);
    }
  }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
  printf("Usage: shell [-hvp]\n");
  printf("   -h   print this message\n");
  printf("   -v   print additional diagnostic information\n");
  printf("   -p   do not emit a command prompt\n");
  exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
  fprintf(stdout, "%s: %s\n", msg, strerror(errno));
  exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
  fprintf(stdout, "%s\n", msg);
  exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
  struct sigaction action, old_action;

  action.sa_handler = handler;  
  sigemptyset(&action.sa_mask); /* block sigs of type being handled */
  action.sa_flags = SA_RESTART; /* restart syscalls if possible */

  if (sigaction(signum, &action, &old_action) < 0)
    unix_error("Signal error");
  return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
  printf("Terminating after receipt of SIGQUIT signal\n");
  exit(1);
}




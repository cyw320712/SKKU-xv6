#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

uint total_weight;
uint vruntime_limit = 2147483647;//21 4748 3647

int weight[40] = 
{
/*  0  */ 88761,  71755,  56483,  46273,  36291,
/*  5  */ 29154,  23254,  18705,  14949,  11916,
/*  10 */ 9548,   7620,   6100,   4904,   3906,
/*  15 */ 3121,   2501,   1991,   1586,   1277,
/*  20 */ 1024,   820,    655,    526,    423,
/*  25 */ 335,    272,    215,    172,    137,
/*  30 */ 110,    87,     70,     56,     45,
/*  35 */ 36,     29,     23,     18,     15
};

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->nice = 20;
  p->weight = 1024;
  p->start = ticks;
  p->runtime = 0;
  p->vrunIndex = 0;
  p->progress = 0;
  p->vruntime = 0;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  np->vruntime = curproc->vruntime;
  np->vrunIndex = curproc->vrunIndex;
  *np->tf = *curproc->tf;
  //cprintf("parent: %d vrun: %d child: %d vrun: %d\n", curproc->pid, curproc->vruntime[0], np->pid, np->vruntime[0]);

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }
  
  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }
  
  curproc->runtime += curproc->progress;
  int flag = 0; int remind = 0, temp;
  if(curproc->vruntime + curproc->progress * 1024 / curproc->weight < 0) flag = 1;
  if(flag){
    temp = curproc->vruntime - vruntime_limit;
    remind = temp + curproc->progress * 1024 / curproc->weight;
    curproc->vrunIndex++;
    curproc->vruntime = remind;
  }
  else curproc->vruntime += curproc->progress * 1024 / curproc->weight;
  curproc->progress = 0;
  //cprintf("%d exit vruntime %d\n", curproc->pid, curproc->vruntime[0]);
  total_weight = 0;
  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct proc *min;
  struct cpu *c = mycpu();

  c->proc = 0;

  for(;;){
    // Enable interrupts on this processor.
    sti();
    
    total_weight = 0;

    acquire(&ptable.lock);
    // Loop over process table looking for process to run.
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE) continue;
      min = p;

      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->state == RUNNABLE) total_weight += weight[p->nice];
        if(p->state == RUNNABLE && p->vrunIndex <= min->vrunIndex) {
          if(p->vruntime < min->vruntime) min = p;
        }
      }
      p = min;
      
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      
      c->proc = p;
      switchuvm(p);

      p->state = RUNNING;
      p->allocated = 10000 * p->weight / total_weight; // total timeslice: 10000 militicks

      //cprintf("weight: %d total: %d pid: %d vruntime: %d ticks: %d allocated: %d\n", p->weight, total_weight, p->pid, p->vruntime, p->progress, p->allocated);
      swtch(&(c->scheduler), p->context);
      switchkvm();
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->runtime += myproc()->progress;
  int flag = 0; int remind = 0, temp;
  if(vruntime_limit - myproc()->vruntime < myproc()->progress * 1024 / myproc()->weight) flag = 1;
  if(flag){
    temp = myproc()->vruntime - vruntime_limit;
    remind = temp + myproc()->progress * 1024 / myproc()->weight;
    myproc()->vrunIndex++;
    myproc()->vruntime = remind;
  }
  else myproc()->vruntime += myproc()->progress * 1024 / myproc()->weight;
  myproc()->progress = 0;
  //cprintf("%d yield vruntime: %d index: %d\n", myproc()->pid, myproc()->vruntime[myproc()->vrunIndex], myproc()->vrunIndex);
  total_weight = 0;
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  p->runtime += p->progress;
  int flag = 0; int remind = 0, temp;
  if(vruntime_limit - p->vruntime < p->progress * 1024 / p->weight) flag = 1;
  if(flag){
    temp = p->vruntime - vruntime_limit;
    remind = temp + p->progress * 1024 / p->weight;
    p->vrunIndex++;
    p->vruntime = remind;
  }
  else p->vruntime += p->progress * 1024 / p->weight;
  p->progress = 0;
  //cprintf("%d sleep vruntime: %d\n", p->pid, p->vruntime[0]);
  total_weight = 0;
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;
  uint min=0, minindex = 0;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == RUNNABLE) {
      minindex = p->vrunIndex;
      break;
    }
  }
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == RUNNABLE){
      if(p->vrunIndex < minindex){
        minindex = p->vrunIndex;
        min = p->vruntime;
      }
    }
  }

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == SLEEPING && p->chan == chan){
      p->state = RUNNABLE;
      p->parse = ticks;
      p->vrunIndex = minindex;
      p->vruntime = min;
    }
  }


}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

int getnice(int pid){
  struct proc *p;
  int result;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      result = p->nice;
      release(&ptable.lock);
      return result;
    }
  }
  release(&ptable.lock);
  return -1;
}

int setnice(int pid, int value){
  struct proc *p;
  if(value < 0 || value > 39) return -1;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->nice = value;
      //p->weight = 1; // for overflow testing
      p->weight = weight[value];
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

void padding1(int length, int num){
  int i, digit=1, temp;
  for(i=0, temp=num; temp >= 10; temp /= 10, digit++);
  for(i=0; i < length - digit; i++) cprintf(" ");
}

void padding3(int length, struct proc *p){
  int i = 0, digit = 10, j;
  int temp;
  int carry, cur = 1, next = 0;
  int max[10]= {2, 1, 4, 7, 4, 8, 3, 6, 4, 7};
  //cprintf("pass\n");
  while(i<10){
    carry = 0;
    if(i>=1) cur *= 10;
    for(j=0; j<p->vrunIndex; j++)
      carry += max[9-i];
    if(i<9)
      carry += (p->vruntime - ((p->vruntime / (cur * 10)) * cur * 10)) / cur;
    else if(i==9) carry += p->vruntime / 1000000000;

    carry += next;
    next = carry / 10;
    i++;
  }
  if(next>=1) digit++;
  for(i=0, temp = next; temp>=10; temp/=10, digit++);
  
  //cprintf("\ndigit: %d\n", digit);
  // for(i=0; i<=p->vrunIndex; i++){
  //   cprintf("%d\n", p->vruntime[i]);
  // }
  cur = 1, i = 0;
  
  int output[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  
  while(i<=digit){
    carry = 0;
    if(i>=1) cur *= 10;
    for(j=0; j<p->vrunIndex; j++)
      carry += max[9-i];
    if(i<9)
      carry += (p->vruntime - ((p->vruntime / (cur * 10)) * cur * 10)) / cur;
    else if(i==9) carry += p->vruntime / 1000000000;

    carry += next;
    output[i] = carry%10;
    next = carry / 10;
    i++;
  }

  //cprintf("%d\n", digit);
  for(i=21-digit; i<=20; i++){
    cprintf("%d", output[20 - i]);
  }
}

void padding2(int length, char* s){
  int i, size = 0;
  char temp;
  for(i=0; (temp = s[i])!='\0'; i++, size++);
  for(i=0; i<length - size; i++) cprintf(" ");
}

void ps(int pid){
  struct proc *p;
  static char *states[] = {
  [UNUSED]    "UNUSED",
  [EMBRYO]    "EMBRYO",
  [SLEEPING]  "SLEEPING",
  [RUNNABLE]  "RUNNABLE",
  [RUNNING]   "RUNNING",
  [ZOMBIE]    "ZOMBIE"
  };

  acquire(&ptable.lock);
  cprintf("name      pid       state      priority       runtime/weight   runtime        vruntime            tick %d\n", ticks*1000);
          //10       10         11        15              17              15            20
  if(pid == 0){
    for(p = ptable.proc; p <= &ptable.proc[NPROC]; p++){
      enum procstate pstate = p->state;
      if(pstate >=1 && pstate <= 5 && p->vrunIndex==0){
        cprintf("%s", p->name); padding2(10, p->name);
        cprintf("%d", p->pid); padding1(10, p->pid);
        cprintf("%s", states[pstate]); padding2(11, states[pstate]);
        cprintf("%d", p->nice); padding1(15, p->nice);
        cprintf("%d", p->runtime / p->weight); padding1(17, p->runtime / p->weight);
        cprintf("%d", p->runtime); padding1(15, p->runtime);
        cprintf("%d", p->vruntime); padding1(20, p->vruntime); cprintf("\n");
      }
      else if(pstate >=1 && pstate <=5 && p->vrunIndex!=0){
        cprintf("%s", p->name); padding2(10, p->name);
        cprintf("%d", p->pid); padding1(10, p->pid);
        cprintf("%s", states[pstate]); padding2(11, states[pstate]);
        cprintf("%d", p->nice); padding1(15, p->nice);
        cprintf("%d", p->runtime / p->weight); padding1(17, p->runtime / p->weight);
        cprintf("%d", p->runtime); padding1(15, p->runtime);
        padding3(20, p); cprintf("\n");
      }
    }
  }
  else{
    for(p = ptable.proc; p <= &ptable.proc[NPROC]; p++){
      enum procstate pstate = p->state;
      if(p->pid == pid){
        if(pstate >=1 && pstate <= 5 && p->vrunIndex==0){
          cprintf("%s", p->name); padding2(10, p->name);
          cprintf("%d", p->pid); padding1(10, p->pid);
          cprintf("%s", states[pstate]); padding2(11, states[pstate]);
          cprintf("%d", p->nice); padding1(15, p->nice);
          cprintf("%d", p->runtime / p->weight); padding1(17, p->runtime / p->weight);
          cprintf("%d", p->runtime); padding1(15, p->runtime);
          cprintf("%d", p->vruntime); padding1(20, p->vruntime); cprintf("\n");
        }
        else if(pstate >=1 && pstate <=5 && p->vrunIndex!=0){
          cprintf("%s", p->name); padding2(10, p->name);
          cprintf("%d", p->pid); padding1(10, p->pid);
          cprintf("%s", states[pstate]); padding2(11, states[pstate]);
          cprintf("%d", p->nice); padding1(15, p->nice);
          cprintf("%d", p->runtime / p->weight); padding1(17, p->runtime / p->weight);
          cprintf("%d", p->runtime); padding1(15, p->runtime);
          padding3(20, p); cprintf("\n");
        }
      }
    }
  }
  release(&ptable.lock);
}

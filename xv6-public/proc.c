#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "queue.h"
struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

struct stride_proc st_proc_list[NPROC+1];
struct priority_queue st_queue;
struct mlfq_proc mf_proc_list[CQ_SIZE];
struct mlf_queue mlfq;
struct circular_queue cq_tmp;
struct priority_queue pq_tmp;
int remain_ticket;
int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

struct stride_proc*
getfreest_proc()
{
  int i=0;
  for (i=0; i<NPROC; i++) {
    if(st_proc_list[i].is_free)
      return &st_proc_list[i];
  }
  panic("no free st_proc");
  return (void*)0;
}

struct mlfq_proc*
getfreemf_proc()
{
  int i=0;
  for (i=0; i<CQ_SIZE; i++) {
    if(mf_proc_list[i].is_free)
      return &mf_proc_list[i];
  }
  panic("no free mf_proc");
  return (void*)0;
}

int
getlev(void)
{
  return myproc()->level;
}

int
set_cpu_share(int share)
{
  acquire(&ptable.lock);
  uint ticket = (MAX_TICKET / 100) * share;
  if(ticket > remain_ticket) {
    int i;
    int all_ticket = 0;
    for(i =1; i<= st_queue.size; i++)
    {
      all_ticket += st_queue.p_procs[i]->tickets;
    }
    remain_ticket = MAX_TICKET - all_ticket;
    if(ticket > remain_ticket)
      return -1;
  }
  struct stride_proc* st_proc = getfreest_proc();
  st_proc->is_process = PROCESS;
  st_proc->tickets = ticket;
  remain_ticket -= ticket;
  st_proc->pass = pq_top(&st_queue)->pass;
  st_proc->p_proc = myproc();
  st_proc->pid = st_proc->p_proc->pid;
  st_proc->p_proc->is_stride = 1;
  st_proc->p_proc->st_proc = st_proc;
  pq_push(&st_queue,st_proc);
  release(&ptable.lock);
  return 0; 
}



void
setlevel(struct mlfq_proc* mf_proc, uint level)
{
  mf_proc->level = level;
  mf_proc->p_proc->level = level;
  if(level == HIGH_LV) {
    mf_proc->time_allot = HIGH_ALLOT;
    mf_proc->time_quantum = HIGH_QNT;
  }
  else if(level == MID_LV) {
    mf_proc->time_allot = MID_ALLOT;
    mf_proc->time_quantum = MID_QNT;
  }
  else {
    mf_proc->time_allot = BOOST_TICK;
    mf_proc->time_quantum = LOW_QNT;
  }
  return;
}

void
pinit(void)
{
  int i = 0;
  struct stride_proc* st_proc;
  initlock(&ptable.lock, "ptable");
  for( i =0; i< NPROC; i++)
    st_proc_list[i].is_free = FREE;
  for( i=0; i< CQ_SIZE; i++)
    mf_proc_list[i].is_free = FREE;
  pq_init(&st_queue);
  mlfq_init(&mlfq);
  st_proc = getfreest_proc();
  st_proc->is_process = MLFQ;
  st_proc->tickets =MIN_MLFQ_TICKET;
  st_proc->pass = 0;
  remain_ticket = MAX_TICKET - MIN_MLFQ_TICKET;
  pq_push(&st_queue,st_proc);

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
  {

    struct mlfq_proc* mf_proc= getfreemf_proc();
    mf_proc->p_proc = p;
    mf_proc->pid = p->pid;
    setlevel(mf_proc,HIGH_LV);

    mlfq_push(&mlfq,mf_proc);
    p->is_stride = MLFQ;
    p->mf_proc = mf_proc;
    p->tick_used = 0;
    p->is_sys_yield = 0;
  }

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
  *np->tf = *curproc->tf;

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
  {
    struct mlfq_proc* mf_proc= getfreemf_proc();
    mf_proc->p_proc = np;
    mf_proc->pid = np->pid;
    setlevel(mf_proc,HIGH_LV);
    mlfq_push(&mlfq,mf_proc);
    mlfq_push(&mlfq,curproc->mf_proc);
    np->is_stride = MLFQ;
    np->mf_proc = mf_proc;
    np->tick_used = 0;
    np->is_sys_yield = 0;
  }

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
  struct cpu *c = mycpu();
  c->proc = 0;
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    //start of stride scheduler.
    {
      int tick_used = 0;
      struct stride_proc* st_proc = 0;
      pq_init(&pq_tmp);
      while(!st_proc) {
        //cprintf("before top\t");
        st_proc = pq_top(&st_queue);
        //cprintf("after top\t");
        pq_pop(&st_queue);

        if(st_proc->is_process) {
          if(st_proc->p_proc->killed || st_proc->p_proc->pid != st_proc->pid || !st_proc->p_proc->is_stride) {
              st_proc = 0;
              break;
            }
          else if(st_proc->p_proc->state != RUNNABLE) {
            if(!st_proc->p_proc->killed&& st_proc->p_proc->state != ZOMBIE) {
              
              pq_push(&pq_tmp,st_proc);
            }
            
            if(pq_isempty(&st_queue)) {
              break;
              }
            st_proc =0;
            continue;
          }
          else {
            st_proc->is_free = INUSE;
            p = st_proc->p_proc;
            c->proc = p;
            switchuvm(p);
            p->state = RUNNING;
            swtch(&(c->scheduler), p->context);
            switchkvm();
            c->proc = 0;
            break;
          }
        }
        //start of mlfq scheduler.
        else {
          //cprintf("enter mlfq\n");
          st_proc->is_free = INUSE;
          cq_init(&cq_tmp);
          struct mlfq_proc* mf_proc = 0;
          
            
          while(!mf_proc)
          {
            mf_proc = mlfq_top(&mlfq);
            mlfq_pop(&mlfq);
            if(mf_proc->p_proc->killed || mf_proc->p_proc->pid != mf_proc->pid || mf_proc->p_proc->is_stride) {
              mf_proc = 0;
              break;
            }
            else {
                if(mf_proc->p_proc->state != RUNNABLE) {
                  if(!mf_proc->p_proc->killed&& mf_proc->p_proc->state != ZOMBIE) {
        
                      cq_push(&cq_tmp,mf_proc);
                    }
                  mf_proc = 0;
                  if(mlfq_isempty(&mlfq))
                    break;
                  continue;
                }
                else {
                  mf_proc->is_free = INUSE;
                  p = mf_proc->p_proc;
                  c->proc = p;
                  switchuvm(p);
                  p->state = RUNNING;
                  swtch(&(c->scheduler), p->context);
                  switchkvm();
                  tick_used = p->tick_used;
                  mlfq.ticks += tick_used;
                  p->tick_used = 0;
                  if(tick_used >= mf_proc->time_allot) {
                    (mf_proc->level)++;
                    if(mf_proc->level > LOW_LV)
                      mf_proc->level = LOW_LV;
                    setlevel(mf_proc,mf_proc->level);
                  }
                  else
                    mf_proc->time_allot -= tick_used;

                  mlfq_push(&mlfq,mf_proc);
                  if(mlfq.ticks >= BOOST_TICK)
                    mlfq_boosting(&mlfq);
                  // Process is done running for now.
                  // It should have changed its p->state before coming back.
                  c->proc = 0;

                  break;
                }
            }
          };
                    
          while(!cq_isempty(&cq_tmp)) {
            struct mlfq_proc* mf_proc = cq_top(&cq_tmp);
            cq_pop(&cq_tmp);
            mlfq_push(&mlfq,mf_proc);
          };
          

          break;
        }
        //end of mlfq scheduler.
        
        //end of while
        };
      
        while(!pq_isempty(&pq_tmp))
        {
          cprintf("inside tmp pq\t");
          struct stride_proc* st_proc = pq_top(&pq_tmp);
          pq_pop(&pq_tmp);
          pq_push(&pq_tmp,st_proc);
        }
        if(st_proc)
        {
          if(st_proc->is_process)
            st_proc->pass += MAX_TICKET/(st_proc->tickets);
          else if(tick_used != 0)
            st_proc->pass += (MAX_TICKET/(st_proc->tickets+remain_ticket))*tick_used;
          else
            st_proc->pass += (MAX_TICKET/(st_proc->tickets+remain_ticket));
          if((!st_proc->is_process || st_proc->p_proc->state != ZOMBIE))
            pq_push(&st_queue,st_proc);

        }
        
    }
    //end of stride scheduler.
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
  uint qnt = LOW_QNT;
  struct proc* mp = myproc();
  switch(mp->level)
  {
    case HIGH_LV:
      qnt = HIGH_QNT;
      break;
    case MID_LV:
      qnt = MID_QNT;
      break;
    default:
      break;
  }
  mp->state = RUNNABLE;
  mp->tick_used++;

  if(mp->is_sys_yield || mp->is_stride || qnt >= mp->tick_used) {
    mp->is_sys_yield = 0;
    sched();
  }
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

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
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

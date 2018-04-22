#define _queue_h
#ifndef _types_h
    #include "types.h"
#endif
#ifndef _defs_h
    #include "defs.h"
#endif
#ifndef _param_h
    #include "param.h"
#endif
#ifndef _mmu_h
    #include "mmu.h"
#endif
#ifndef _proc_h
    #include "proc.h"
#endif
#define CQ_SIZE 2*(NPROC)
#define HIGH_LV 0
#define MID_LV 1
#define LOW_LV 2
#define HIGH_QNT 1
#define MID_QNT 2
#define LOW_QNT 4
#define HIGH_ALLOT 5
#define MID_ALLOT 10
#define BOOST_TICK 100
#define FREE 1
#define INUSE 0
#define PROCESS 1
#define MLFQ 0
struct stride_proc {
    uint is_free;
    uint is_process;
    struct proc *p_proc;
    uint pid;
    uint tickets;
    uint pass;
};
struct priority_queue  {
    struct stride_proc *p_procs[NPROC];
    uint size; 
};
void pq_init(struct priority_queue *pq);
uint pq_isempty(struct priority_queue *pq);
struct stride_proc* pq_top(struct priority_queue *pq);
void pq_push(struct priority_queue *pq, struct stride_proc *st_proc);
void pq_pop(struct priority_queue *pq);

struct mlfq_proc {
    uint is_free;
    struct proc *p_proc;
    uint pid;
    uint time_quantum;
    uint time_allot;
    uint level;
};

struct circular_queue {
    struct mlfq_proc *p_procs[CQ_SIZE];
    uint front;
    uint rear;
};

void cq_init(struct circular_queue *cq);
uint cq_isempty(struct circular_queue *cq);
struct mlfq_proc* cq_top(struct circular_queue *cq);
void cq_push(struct circular_queue *cq, struct mlfq_proc* mf_proc);
void cq_pop(struct circular_queue *cq);

struct mlf_queue {
    struct circular_queue cq[3];
    uint ticks;
};

void mlfq_init(struct mlf_queue* mlfq);
uint mlfq_isempty(struct mlf_queue* mlfq);
struct mlfq_proc* mlfq_top(struct mlf_queue* mlfq);
void mlfq_push(struct mlf_queue* mlfq, struct mlfq_proc* mf_proc);
void mlfq_pop(struct mlf_queue* mlfq);
void mlfq_boosting(struct mlf_queue* mlfq);
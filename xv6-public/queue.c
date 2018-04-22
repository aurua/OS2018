#include "queue.h"
uint
stride_cmp(struct stride_proc *st1,struct stride_proc *st2) 
{
    if(st1->pass <= st2->pass)
        return 0;
    else
        return 1;
}
void 
stride_swap(struct stride_proc *st1,struct stride_proc *st2) 
{
    struct stride_proc tmp = *st1;
    *st1 = *st2;
    *st2 = tmp;
    return;
}
void 
pq_init(struct priority_queue *pq)
{
    pq->size = 0;
}
uint 
pq_isempty(struct priority_queue *pq)
{
    if(pq->size == 0)
        return 1;
    return 0;
}
struct stride_proc* 
pq_top(struct priority_queue *pq)
{
    return pq->p_procs[1];
}
void 
pq_push(struct priority_queue *pq, struct stride_proc *st_proc)
{
    uint cur;
    pq->p_procs[++pq->size] = st_proc;
    st_proc->is_free = INUSE;
    cur = pq->size;
    while(cur/2 > 0 && stride_cmp(pq->p_procs[cur/2],pq->p_procs[cur])) {
        stride_swap(pq->p_procs[cur/2],pq->p_procs[cur]);
        cur = cur/2;
    };
    return;
}
void 
pq_pop(struct priority_queue *pq)
{
    uint cur;
    pq->p_procs[1]->is_free = FREE;
    pq->p_procs[1] = pq->p_procs[pq->size--];
    cur = 1;
    while(cur * 2 <= pq->size)
    {
        uint target = cur * 2;
        if( target+1 <= pq-> size && stride_cmp(pq->p_procs[target],pq->p_procs[target+1]))
            target = target +1;
        if( stride_cmp(pq->p_procs[cur],pq->p_procs[target]))
            stride_swap(pq->p_procs[cur],pq->p_procs[target]);
        else
            break;
        cur = target;
    };
    return;
}


void 
cq_init(struct circular_queue *cq)
{
    cq->front = 0;
    cq->rear = 0;
    return;
}
uint 
cq_isempty(struct circular_queue *cq)
{
    if(cq->front == cq->rear)
        return 1;
    return 0;
}
struct mlfq_proc* 
cq_top(struct circular_queue *cq)
{
    return cq->p_procs[cq->front];
}
void 
cq_push(struct circular_queue *cq, struct mlfq_proc *mf_proc)
{
    cq->p_procs[cq->rear++] = mf_proc;
    cq->rear %= CQ_SIZE;
    mf_proc->is_free = INUSE;
    return;
}
void 
cq_pop(struct circular_queue *cq)
{
    cq->p_procs[cq->front]->is_free=FREE;
    cq->front++;
    cq->front %= CQ_SIZE;
    return;
}


void 
mlfq_init(struct mlf_queue* mlfq)
{
    int i;
    mlfq->ticks = 0;
    for(i =0; i < 3; i++ )
        cq_init(&mlfq->cq[i]);
}
uint 
mlfq_isempty(struct mlf_queue* mlfq)
{
    int i;
    for(i =HIGH_LV; i<= LOW_LV; ++i) {
        if(!cq_isempty( &(mlfq->cq[i]) ) )
            return 0;
    }
    return 1;
}

struct mlfq_proc* 
mlfq_top(struct mlf_queue* mlfq)
{
    int i;
    for(i = HIGH_LV; i<= LOW_LV; ++i) {
        if(!cq_isempty( &(mlfq->cq[i]) ) )
            return cq_top( &(mlfq->cq[i]));
    }
    //panic("empty");
    panic(mlfq->cq[0].p_procs[0]->p_proc->name);
    return (void*)0;
}

void 
mlfq_push(struct mlf_queue* mlfq, struct mlfq_proc* mf_proc)
{
    cq_push( &(mlfq->cq[mf_proc->level]),mf_proc);
    return;
}
void 
mlfq_pop(struct mlf_queue* mlfq)
{
    int i =0;
    for(i =0; i< 3; ++i) {
        if(!cq_isempty( &(mlfq->cq[i]) ) ) {
            cq_pop(&(mlfq->cq[i]));
            return;
        }
    }
    return;
}
void
mlfq_boosting(struct mlf_queue* mlfq)
{

    int i = 0; int iter_front = mlfq->cq[HIGH_LV].front;
    while(iter_front != mlfq->cq[HIGH_LV].rear) {
        struct mlfq_proc* pr= mlfq->cq[HIGH_LV].p_procs[iter_front++];
        pr->p_proc->tick_used = 0;
        pr->time_allot = HIGH_ALLOT;
        pr->time_quantum = HIGH_QNT;
        iter_front %= CQ_SIZE;
    }

    for (i =MID_LV; i<= LOW_LV ; ++i) {
        while( !cq_isempty(&mlfq->cq[i]) ) {
            struct mlfq_proc* pr= cq_top(&mlfq->cq[i]);
            pr->p_proc->level = HIGH_LV;
            pr->p_proc->tick_used = 0;
            pr->level = HIGH_LV;
            pr->time_allot = HIGH_ALLOT;
            pr->time_quantum = HIGH_QNT;
            cq_pop(&mlfq->cq[i]);
            if(pr->pid == pr->p_proc->pid && !(pr->p_proc->killed) )
                cq_push(&mlfq->cq[HIGH_LV],pr);
        }
    }
    mlfq->ticks = 0;
    return;
}

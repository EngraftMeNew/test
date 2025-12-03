#include <os/list.h>
#include <os/sched.h>
#include <type.h>

uint64_t time_elapsed = 0;
uint64_t time_base = 0;

uint64_t get_ticks()
{
    __asm__ __volatile__(
        "rdtime %0"
        : "=r"(time_elapsed));
    return time_elapsed;
}

uint64_t get_timer()
{
    return get_ticks() / time_base;
}

uint64_t get_time_base()
{
    return time_base;
}

void latency(uint64_t time)
{
    uint64_t begin_time = get_timer();

    while (get_timer() - begin_time < time)
        ;
    return;
}

void check_sleeping(void)
{
    //  Pick out tasks that should wake up from the sleep queue
    uint64_t now = get_timer();

    for (list_node_t *p = sleep_queue.next, *next; p != &sleep_queue; p = next)
    {
        next = p->next;
        pcb_t *t = get_pcb(p);

        if (t->wakeup_time > now)
        {
            continue;
        }

        do_unblock(p);
        // t->status = TASK_READY;
        // add_node_to_q(p, &ready_queue);
    }
}
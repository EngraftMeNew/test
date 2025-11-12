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
// TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
void check_sleeping(void)
{
    uint64_t now = get_timer();

    // 安全遍历：在删除/移动节点前先缓存 next
    for (list_node_t *it = sleep_queue.next, *next; it != &sleep_queue; it = next)
    {
        next = it->next;

        pcb_t *t = get_pcb_from_node(it);

        // 尚未到时间，若队列按时间升序可直接提前结束
        if (t->wakeup_time > now)
        {
            // 如果 sleep_queue **已按时间排序**，解除注释可小优化：
            // break;
            continue;
        }

        // 到点：从睡眠队列移除并转入就绪队列
        // do_unblock 通常只负责把 it 从 sleep_queue 里摘下/清状态
        do_unblock(it);

        // 统一设置状态为 READY，避免残留状态造成调度歧义
        t->status = TASK_READY;

        // 入就绪队列
        add_node_to_q(it, &ready_queue);
    }
}
#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

int FLY_SPEED_TABLE[16];
int FLY_LENGTH_TABLE[16];

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* global process id */
pid_t process_id = 1;
int fly_num = 0;
int table_p = 0;
int if_switch = 0;

void do_scheduler(void)
{
    // 唤醒到点的睡眠进程
    check_sleeping();

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/

    pcb_t *prev = current_running;

    printl("Scheduler start \n");

    // 消耗当前进程的一个tick 
    if (prev->status == TASK_RUNNING && prev->time_slice_remain > 0)
        prev->time_slice_remain--;

    int need_switch = 0;

    // 时间片用完 
    if (prev->status == TASK_RUNNING && prev->time_slice_remain == 0)
        need_switch = 1;


    if (prev->status == TASK_BLOCKED)
        need_switch = 1;

    if (!need_switch)
    {
        printl("pid[%d]: continue running, ts_remain=%d\n",
               prev->pid, prev->time_slice_remain);
        printl("[%d] switch_to skipped\n", prev->pid);
        return;
    }

    // 把当前进程放回合适队列 
    if (prev->pid != 0)
    { 
        if (prev->status == TASK_RUNNING)
        {
            // 时间片用完但仍可运行 → 进就绪队列
            prev->status = TASK_READY;
            //下次再被调度前，先重置它的时间片
            prev->time_slice_remain = prev->time_slice;
            add_node_to_q(&prev->list, &ready_queue);
        }
        else if (prev->status == TASK_BLOCKED)
        {
            // 已在别处改为 BLOCKED，这里送入阻塞队列
            add_node_to_q(&prev->list, &sleep_queue);
        }
    }

    //从就绪队列挑选下一个
    list_node_t *node = seek_ready_node();
    if (node == NULL)
    {
        // 没有就绪进程 → 运行 idle (pid 0)
        current_running = &pcb[0]; // 或你的 idle_pcb
    }
    else
    {
        current_running = get_pcb_from_node(node);
    }

    current_running->status = TASK_RUNNING;

    // 新切入的进程必须拥有“整片”的时间片
    if (current_running->time_slice_remain <= 0 ||
        current_running->time_slice_remain > current_running->time_slice)
        current_running->time_slice_remain = current_running->time_slice;

    printl("pid[%d]: is going to run, ts=%d\n",
           current_running->pid, current_running->time_slice_remain);

    switch_to(prev, current_running);

    printl("[%d] switch_to success!!!\n", current_running->pid);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    current_running->status = TASK_BLOCKED;
    // 2. set the wake up time for the blocked task
    current_running->wakeup_time = get_timer() + sleep_time;
    // 3. reschedule because the current_running is blocked.
    do_scheduler();
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    pcb_t *tmp = get_pcb_from_node(pcb_node);
    tmp->status = TASK_BLOCKED;
    add_node_to_q(pcb_node, queue);
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    delete_node_from_q(pcb_node);
    pcb_t *tmp = get_pcb_from_node(pcb_node);
    tmp->status = TASK_READY;
    add_node_to_q(pcb_node, &ready_queue);
}

list_node_t *seek_ready_node()
{
    list_node_t *p = ready_queue.next;
    // delete p from queue
    delete_node_from_q(p);
    return p;
}

void add_node_to_q(list_node_t *node, list_head *head)
{
    list_node_t *p = head->prev; // tail ptr
    p->next = node;
    node->prev = p;
    node->next = head;
    head->prev = node; // update tail ptr
}

void delete_node_from_q(list_node_t *node)
{
    list_node_t *p, *q;
    p = node->prev;
    q = node->next;
    p->next = q;
    q->prev = p;
    node->next = node->prev = NULL; // delete the node completely
}

pcb_t *get_pcb_from_node(list_node_t *node)
{
    for (int i = 0; i < NUM_MAX_TASK; i++)
    {
        if (node == &pcb[i].list)
            return &pcb[i];
    }
    return &pid0_pcb; // fail to find the task, return to kernel
}

void do_set_sche_workload(int position)
{
    if (current_running->if_fly == 0) //首次进入
    {
        current_running->if_fly = 1;
        current_running->fly_id = fly_num++;
        current_running->position_last = position; // 记录当前观测位置
        current_running->position_now = 0;         // 还未通过检查点
        return;
    }

    // 判断“位置是否变化”
    if (position != current_running->position_last)
    {
        current_running->position_last = position;
        current_running->position_now++; // 通过了一个检查点
        FLY_LENGTH_TABLE[current_running->fly_id] = current_running->position_now;

        // 只有“首次通过检查点”的飞行器才计入 table_p
        if (current_running->position_now == 1)
            table_p++;

        // 冷启动阶段：有人还没到过检查点 → 均分
        if (table_p < fly_num)
        {
            int base = 60 / (fly_num > 0 ? fly_num : 1);
            if (base < 1)
                base = 1;
            current_running->time_slice = base;
        }
        else
        {
            current_running->time_slice = calculate_time_slice(
                FLY_LENGTH_TABLE, table_p, current_running->fly_id);
        }

        printl("pid[%d]:time_slice[%d]\n",
               current_running->pid, current_running->time_slice);
    }
}

int calculate_time_slice(int *D_table, int table_p, int fly_id)
{
    if (table_p <= 0 || fly_id < 0 || fly_id >= table_p)
        return -1;

    const int T_min = 1;    // 最小时间片
    const int Total_T = 60; // 本轮预算
    const int scale = 100;  // 定点放大

    // 1) 平均进度（用64位避免溢出）
    long long sum_D = 0;
    for (int i = 0; i < table_p; i++)
        sum_D += (long long)D_table[i];
    int D_avg = (int)(sum_D / table_p);

    // 2) 两遍扫描
    long long sum_weights = 0;
    long long my_weight = 0;

    //落后者分配正权重
    for (int i = 0; i < table_p; i++)
    {
        int delta = D_avg - D_table[i];
        long long w = (delta > 0) ? ((long long)delta * scale ) : 0;
        sum_weights += w;
        if (i == fly_id)
            my_weight = w;
    }

    // 3) 时间片分配（四舍五入）
    int time_slice;
    if (sum_weights > 0)
    {
        time_slice = (int)((my_weight * Total_T + sum_weights / 2) / sum_weights);
    }
    else
    {
        // 无落后者：均分
        time_slice = (Total_T + table_p / 2) / table_p;
    }

    // 4) 动态最小时间片（领先更严）
    int dynamic_T_min = T_min;
    if (D_table[fly_id] > D_avg)
        dynamic_T_min = (T_min > 1) ? (T_min - 1) : 1;

    if (time_slice < dynamic_T_min)
        time_slice = dynamic_T_min;

    if (time_slice > Total_T)
        time_slice = Total_T;

    return time_slice;
}

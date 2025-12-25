#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <os/loader.h>
#include <os/task.h>
#include <os/string.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <os/smp.h>
#include <pgtable.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack,
    .pgdir = 0xffffffc051000000};

const ptr_t s_pid0_stack = INIT_KERNEL_STACK + 2 * PAGE_SIZE;
pcb_t s_pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)s_pid0_stack,
    .user_sp = (ptr_t)s_pid0_stack,
    .pgdir = 0xffffffc051000000};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* global process id */
pid_t process_id = 1;

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    check_sleeping();

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/

    // TODO: [p2-task1] Modify the current_running[cpu_id] pointer.
    pcb_t *prior_running;
    prior_running = current_running[cpu_id];

    if (current_running[cpu_id]->pid != 0)
    {
        // add to the ready queue
        if (current_running[cpu_id]->status == TASK_RUNNING)
        {
            current_running[cpu_id]->status = TASK_READY;
            add_node_to_q(&current_running[cpu_id]->list, &ready_queue);
        }
    }
    list_node_t *tmp = seek_ready_node();
    current_running[cpu_id] = get_pcb_from_node(tmp);
    current_running[cpu_id]->status = TASK_RUNNING;
    current_running[cpu_id]->run_cpu_id = cpu_id;

    printl("[scheduler] switch to %d\n", current_running[cpu_id]->pid);
    do_process_show_l();

    // TODO: [p2-task1] switch_to current_running[cpu_id]
    set_satp(SATP_MODE_SV39, current_running[cpu_id]->pid, kva2pa(current_running[cpu_id]->pgdir) >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();
    switch_to(prior_running, current_running[cpu_id]);
    return;
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running[cpu_id]
    do_block(&current_running[cpu_id]->list, &sleep_queue);
    // 2. set the wake up time for the blocked task
    current_running[cpu_id]->wakeup_time = get_timer() + sleep_time;
    // 3. reschedule because the current_running[cpu_id] is blocked.
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
    pcb_t *tmp;
    // delete p from queue
    while (1)
    {
        if (p == &ready_queue)
            return cpu_id ? &s_pid0_pcb.list : &pid0_pcb.list;
        tmp = get_pcb_from_node(p);
        if (tmp->cpu_mask & (cpu_id + 1))
            break;
        p = p->next;
    }
    delete_node_from_q(p);
    return p;
}

int search_free_pcb()
{ // 查找可用pcb并返回下标，若无则返回-1
    for (int i = 0; i < NUM_MAX_TASK; i++)
    {
        if (pcb[i].status == TASK_EXITED)
            return i;
    }
    return -1;
}

void pcb_release(pcb_t *p)
{

    // 将之从原队列删除
    if (current_running[0]->pid != p->pid & current_running[1]->pid != p->pid)
        delete_node_from_q(&(p->list));
    // 释放等待队列的所有进程
    free_block_list(&(p->wait_list));
    // 释放持有的所有锁
    release_all_lock(p->pid);

    free_all_pages(p);
}
void release_all_lock(pid_t pid)
{
    for (int i = 0; i < LOCK_NUM; i++)
    {
        if (mlocks[i].pid == pid)
            do_mutex_lock_release(i);
    }
}

void free_block_list(list_node_t *head)
{ // 释放被阻塞的进程
    list_node_t *p, *next;
    for (p = head->next; p != head; p = next)
    {
        next = p->next;
        do_unblock(p);
    }
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
    return cpu_id ? &s_pid0_pcb : &pid0_pcb; // fail to find the task, return to kernel
}

pid_t do_exec(char *name, int argc, char *argv[])
{ // 创建进程，不成功返回0
    char **argv_ptr;
    int index = search_free_pcb();
    if (index == -1) // 进程数已满，返回
        return 0;
    uint64_t entry_point;
    uintptr_t pgdir = allocPage(1);
    *(uint64_t *)pgdir = 0;
    clear_pgdir(pgdir);
    // 共享kernel页表
    share_pgtable(pgdir, pa2kva(PGDIR_PA));
    entry_point = map_task(name, pgdir);

    if (entry_point == 0) // 找不到相应task，返回
        return 0;
    // 创建PCB
    else
    {
        pcb[index].pid = task_num + 1; // pid 0 is for kernel
        pcb[index].pgdir = pgdir;
        pcb[index].kernel_sp = (reg_t)(allocPage(1) + PAGE_SIZE); // 分配一页
        uint64_t user_sp = alloc_page_helper(USER_STACK_ADDR, pgdir) + PAGE_SIZE;
        pcb[index].user_sp = (reg_t)(USER_STACK_ADDR + PAGE_SIZE);
        pcb[index].status = TASK_READY;
        pcb[index].cursor_x = 0;
        pcb[index].cursor_y = 0;
        pcb[index].wait_list.prev = pcb[index].wait_list.next = &pcb[index].wait_list;
        pcb[index].list.prev = pcb[index].list.next = NULL;
        pcb[index].cpu_mask = current_running[cpu_id]->cpu_mask;
        uint64_t user_sp_ori = user_sp;
        // 参数搬到用户栈
        user_sp -= sizeof(char *) * argc;
        argv_ptr = (char **)user_sp;

        for (int i = argc - 1; i >= 0; i--)
        {
            int len = strlen(argv[i]) + 1; // 要拷贝'\0'
            user_sp -= len;
            argv_ptr[i] = (char *)(pcb[index].user_sp - (user_sp_ori - user_sp));
            strcpy((char *)user_sp, argv[i]);
        }
        user_sp = (reg_t)ROUNDDOWN(user_sp, 128); // 栈指针128字节对齐

        argv_ptr = (char **)(pcb[index].user_sp - (sizeof(char *) * argc));
        pcb[index].user_sp -= (user_sp_ori - user_sp);
        // 初始化栈，改变入口地址，存储参数

        init_pcb_stack(pcb[index].kernel_sp, pcb[index].user_sp, entry_point, &pcb[index], argc, argv_ptr);
        // 加入ready队列
        add_node_to_q(&pcb[index].list, &ready_queue);
        // 进程数加一
        task_num++;
    }
    return pcb[index].pid; // 返回pid值
}

void do_exit(void)
{
    current_running[cpu_id]->status = TASK_EXITED;
    set_satp(SATP_MODE_SV39, 0, PGDIR_PA >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();
    pcb_release(current_running[cpu_id]);
    do_scheduler();
}

int do_kill(pid_t pid)
{
    for (int i = 0; i < NUM_MAX_TASK; i++)
    {
        if (pcb[i].status != TASK_EXITED && pcb[i].pid == pid)
        {
            // 修改进程状态
            pcb[i].status = TASK_EXITED;

            pcb_release(&pcb[i]);
            // 返回1，表示找到对应进程且将其kill
            return 1;
        }
    }
    return 0;
}

int do_waitpid(pid_t pid)
{
    for (int i = 0; i < NUM_MAX_TASK; i++)
    {
        if (pcb[i].pid == pid)
        {
            if (pcb[i].status != TASK_EXITED)
            {
                do_block(&(current_running[cpu_id]->list), &(pcb[i].wait_list));
                do_scheduler();
                return pid;
            }
        }
    }
    return 0;
}

void do_process_show()
{
    int i;
    static char *stat_str[3] = {
        "BLOCKED", "RUNNING", "READY"};
    screen_write("[Process table]:\n");
    for (i = 0; i < NUM_MAX_TASK; i++)
    {
        if (pcb[i].status == TASK_EXITED)
            continue;
        else if (pcb[i].status == TASK_RUNNING)
            printk("[%d] PID : %d  STATUS : %s mask: 0x%x Running on core %d\n", i, pcb[i].pid, stat_str[pcb[i].status], pcb[i].cpu_mask, pcb[i].run_cpu_id);
        else
            printk("[%d] PID : %d  STATUS : %s mask: 0x%x\n", i, pcb[i].pid, stat_str[pcb[i].status], pcb[i].cpu_mask);
    }
}

void do_process_show_l()
{
    int i;
    static char *stat_str[3] = {
        "BLOCKED", "RUNNING", "READY"};
    for (i = 0; i < NUM_MAX_TASK; i++)
    {
        if (pcb[i].status == TASK_EXITED)
            continue;
        else if (pcb[i].status == TASK_RUNNING)
            printl("[%d] PID : %d  STATUS : %s mask: 0x%x Running on core %d\n", i, pcb[i].pid, stat_str[pcb[i].status], pcb[i].cpu_mask, pcb[i].run_cpu_id);
        else
            printl("[%d] PID : %d  STATUS : %s mask: 0x%x\n", i, pcb[i].pid, stat_str[pcb[i].status], pcb[i].cpu_mask);
    }
} // debug用

pid_t do_getpid()
{
    return current_running[cpu_id]->pid;
}

pid_t do_taskset(int mode_p, int mask, void *pid_name)
{
    int pid = (int)pid_name;
    if (mode_p)
    {
        for (int i = 0; i < NUM_MAX_TASK; i++)
        {
            if (pcb[i].status != TASK_EXITED && pcb[i].pid == pid)
            {
                pcb[i].cpu_mask = mask;
                return pid;
            }
        }
        printk("Fail to find task with pid %d", pid);
        return 0;
    }
    else
    {
        char *name = (char *)pid_name;
        pid = do_exec(name, 1, &name);
        for (int i = 0; i < NUM_MAX_TASK; i++)
        {
            if (pcb[i].status != TASK_EXITED && pcb[i].pid == pid)
                pcb[i].cpu_mask = mask;
        }
        return pid;
    }
}




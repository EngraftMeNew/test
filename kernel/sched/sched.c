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

pcb_t *current_running[NR_CPUS];

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

list_node_t *seek_ready_node()
{
    list_node_t *p = ready_queue.next;
    // delete p from queue
    if (p == &ready_queue)
        return cpu_id ? &s_pid0_pcb.list : &pid0_pcb.list;
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

pcb_t *get_pcb(list_node_t *node)
{
    for (int i = 0; i < NUM_MAX_TASK; i++)
    {
        if (node == &pcb[i].list)
            return &pcb[i];
    }
    return cpu_id ? &s_pid0_pcb : &pid0_pcb; // fail to find the task, return to kernel
}

void do_scheduler(void)
{
    //  Check sleep queue to wake up PCBs
    check_sleeping();

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/
    //  Modify the current_running[cpu_id] pointer.
    // printk("this is in do_scheduler\n");
    pcb_t *prior_running;
    prior_running = current_running[cpu_id];

    if (current_running[cpu_id]->pid != 0)
    {
        if (current_running[cpu_id]->status == TASK_RUNNING)
        {
            current_running[cpu_id]->status = TASK_READY;
            add_node_to_q(&current_running[cpu_id]->list, &ready_queue);
        }
    }
    list_node_t *ready_node = seek_ready_node();
    current_running[cpu_id] = get_pcb(ready_node);
    current_running[cpu_id]->status = TASK_RUNNING;
    // printk("going to do switch_to\n");
    //   switch_to current_running[cpu_id]
    // 切换页表根
    set_satp(SATP_MODE_SV39, current_running[cpu_id]->pid, kva2pa(current_running[cpu_id]->pgdir) >> NORMAL_PAGE_SHIFT);
    // 刷TLB
    local_flush_tlb_all();
    switch_to(prior_running, current_running[cpu_id]);
}

void do_sleep(uint32_t sleep_time)
{
    //  sleep(seconds)
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
    //  block the pcb task into the block queue
    pcb_t *tmp = get_pcb(pcb_node);
    tmp->status = TASK_BLOCKED;
    add_node_to_q(pcb_node, queue);
}

void do_unblock(list_node_t *pcb_node)
{
    // unblock the `pcb` from the block queue
    delete_node_from_q(pcb_node);
    pcb_t *tmp = get_pcb(pcb_node);
    tmp->status = TASK_READY;
    add_node_to_q(pcb_node, &ready_queue);
}

void do_process_show(void)
{
    static const char *states[] = {
        "BLOCKED", "RUNNING", "READY", "EXITED"};

    screen_write("[Process table]:\n");
    for (int i = 0; i < NUM_MAX_TASK; i++)
    {
        if (pcb[i].status == TASK_EXITED)
            continue; // 不打印空槽位

        printk("[%d] PID : %d  STATUS : %s\n",
               i, pcb[i].pid, states[pcb[i].status]);
    }
}

pid_t do_exec(char *name, int argc, char *argv[])
{
    char **argv_ptr = NULL;
    int index = -1;
    uint64_t user_sp;
    pid_t ret = 0; // 默认失败返回 0

    index = search_free_pcb();
    if (index == -1)
        return 0;

    // 新建页表并共享内核映射
    uintptr_t pgdir = allocPage(1);
    clear_pgdir(pgdir);
    share_pgtable(pgdir, pa2kva(PGDIR_PA));

    // 把程序映射到用户地址空间
    uint64_t entry_point = map_task(name, pgdir);
    if (entry_point == 0)
        return 0;

    // 分配内核栈和用户栈
    pcb[index].kernel_sp = (reg_t)(allocPage(1) + PAGE_SIZE);

    uint8_t *stack_kva = (uint8_t *)alloc_page_helper(USER_STACK_ADDR, pgdir);
    uint64_t user_sp_va_top = USER_STACK_ADDR + PAGE_SIZE; // 用户态看到的栈顶VA

    // 分配 pid / 初始化通用字段
    task_num++;
    pcb[index].pid = task_num;
    pcb[index].pgdir = pgdir;
    pcb[index].status = TASK_READY;
    pcb[index].cursor_x = 0;
    pcb[index].cursor_y = 0;
    pcb[index].wait_list.prev = pcb[index].wait_list.next = &pcb[index].wait_list;
    pcb[index].list.prev = pcb[index].list.next = NULL;

    //  在“栈页KVA”里构造 argv，但写入的指针必须是“用户VA”
    //  offset 表示距离 USER_STACK_ADDR 的偏移
    uint64_t sp_off = PAGE_SIZE; // 从页顶往下推
    sp_off -= sizeof(char *) * (uint64_t)argc;

    char **argv_ptr_user = (char **)(USER_STACK_ADDR + sp_off); // 用户看到的 argv 指针数组地址（VA）
    char **argv_ptr_kva = (char **)(stack_kva + sp_off);        // 内核写入用的地址（KVA）

    for (int i = argc - 1; i >= 0; i--)
    {
        uint64_t len = (uint64_t)strlen(argv[i]) + 1;
        sp_off -= len;

        // 字符串在用户栈中的 VA
        argv_ptr_kva[i] = (char *)(USER_STACK_ADDR + sp_off);

        // 把字符串写到对应位置（KVA）
        strcpy((char *)(stack_kva + sp_off), argv[i]);
    }

    // 对齐
    sp_off = (uint64_t)ROUNDDOWN(sp_off, 128);
    pcb[index].user_sp = (reg_t)(USER_STACK_ADDR + sp_off);

    init_pcb_stack(pcb[index].kernel_sp,
                   pcb[index].user_sp,
                   entry_point,
                   &pcb[index],
                   argc,
                   argv_ptr_user);

    add_node_to_q(&pcb[index].list, &ready_queue);
    return pcb[index].pid;
}

pid_t do_getpid()
{
    return current_running[cpu_id]->pid;
}

int do_waitpid(pid_t pid)
{
    pcb_t *target = NULL;

    for (int i = 0; i < NUM_MAX_TASK; i++)
    {
        if (pcb[i].pid == pid)
        {
            target = &pcb[i];
            break;
        }
    }

    if (target == NULL)
    {
        return 0;
    }

    if (target->status == TASK_EXITED)
    {
        return pid;
    }

    do_block(&current_running[cpu_id]->list, &target->wait_list);
    do_scheduler();

    return pid;
}

void do_exit(void)
{
    //  exit the current_running[cpu_id] task.
    current_running[cpu_id]->status = TASK_EXITED;

    // unblock all the tasks in wait_list
    release_pcb(current_running[cpu_id]);

    // reschedule because the current_running[cpu_id] is exited.
    do_scheduler();
}

int do_kill(pid_t pid)
{
    for (int i = 0; i < NUM_MAX_TASK; i++)
    {
        if (pcb[i].pid == pid && pcb[i].status != TASK_EXITED)
        {
            pcb[i].status = TASK_EXITED;
            release_pcb(&pcb[i]);
            return 1; // kill 成功
        }
    }

    return 0; // 没有这个 pid
}

void release_all_lock(pid_t pid)
{
    for (int i = 0; i < LOCK_NUM; i++)
    {
        if (mlocks[i].pid == pid)
            do_mutex_lock_release(i);
    }
}

int search_free_pcb()
{
    for (int i = 0; i < NUM_MAX_TASK; i++)
    {
        if (pcb[i].status == TASK_EXITED)
            return i;
    }
    return -1;
}

void release_pcb(pcb_t *p)
{
    // 如果该 PCB 还在某个队列中（ready/sleep/其它阻塞队列），先将其摘除
    if (p->list.next != NULL && p->list.prev != NULL)
    {
        delete_node_from_q(&p->list);
    }

    // 唤醒所有在 p->wait_list 上等待它的进程（wait/waitpid）
    free_block_list(&p->wait_list);
    p->wait_list.next = &p->wait_list;
    p->wait_list.prev = &p->wait_list;

    // 释放该进程持有的所有锁
    release_all_lock(p->pid);

    // 其它资源（栈、页表）本实验可以不管，reuse 的时候会重新初始化
}

void free_block_list(list_node_t *head)
{
    list_node_t *p, *next;
    for (p = head->next; p != head; p = next)
    {
        next = p->next;
        do_unblock(p);
    }
}
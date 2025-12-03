#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>
#include <os/string.h>

barrier_t barriers[BARRIER_NUM];
mutex_lock_t mlocks[LOCK_NUM];
condition_t conditions[CONDITION_NUM];
mailbox_t mailboxes[MBOX_NUM];

int lock_used_num = 0;


void init_locks(void)
{
    /*  initialize mlocks */
    for (int i = 0; i < LOCK_NUM; i++)
    {
        spin_lock_init(&mlocks[i].lock);
        mlocks[i].block_queue.prev = mlocks[i].block_queue.next = &mlocks[i].block_queue;
        mlocks[i].key = -1;
        mlocks[i].pid = -1;
    }
}

void spin_lock_init(spin_lock_t *lock)
{
    /*  initialize spin lock */
    lock->status = UNLOCKED;
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /*  try to acquire spin lock */
    /*
    if (lock->status == UNLOCKED)
    {
        lock->status = LOCKED;
        return 1;
    }
    return 0;
    */
    // 原子上锁
    return (atomic_swap(LOCKED, (ptr_t)&lock->status) == UNLOCKED);
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /*  acquire spin lock */
    /*
    while (lock->status == UNLOCKED)
        ;
    lock->status = LOCKED;
    */
    // 原子上锁
    while (atomic_swap(LOCKED, (ptr_t)&lock->status) == LOCKED)
        ;
}

void spin_lock_release(spin_lock_t *lock)
{
    /*  release spin lock */
    lock->status = UNLOCKED;
}

int do_mutex_lock_init(int key)
{
    for (int i = 0; i < lock_used_num; i++)
    {
        if (mlocks[i].key == key)
            return i;
    }
    mlocks[lock_used_num].key = key;
    mlocks[lock_used_num].pid = -1;
    return lock_used_num++;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    mutex_lock_t *m = &mlocks[mlock_idx];

    // 先拿内部自旋锁，保护 pid 和队列
    spin_lock_acquire(&m->lock);

    if (m->pid == -1)
    {
        // 没有人持有这把 mutex，直接占有
        m->pid = current_running->pid;
        spin_lock_release(&m->lock);
        return;
    }

    // 已经有人持有，把自己加入阻塞队列
    do_block(&current_running->list, &m->block_queue);

    // 加到队列后，释放自旋锁
    spin_lock_release(&m->lock);

    // 交出 CPU，等待别人 do_mutex_lock_release 的时候唤醒
    do_scheduler();

    // 被唤醒后，约定：这时自己已经是这把 mutex 的新 owner
    // （在 do_mutex_lock_release 里会把 m->pid 设置成被唤醒者的 pid）
}


// 释放
void do_mutex_lock_release(int mlock_idx)
{
    mutex_lock_t *m = &mlocks[mlock_idx];

    spin_lock_acquire(&m->lock);

    list_node_t *head = &m->block_queue;
    list_node_t *p    = head->next;

    if (p == head)
    {
        // 没有人在等待这把锁，简单标记为“无人持有”
        m->pid = -1;
        spin_lock_release(&m->lock);
    }
    else
    {
        // 队列里有人在等，选队头，把锁的“所有权”给他
        pcb_t *pcb = get_pcb(p);    // 或者 get_pcb_from_node(p)，按你工程里的实际函数名

        m->pid = pcb->pid;

        do_unblock(p);             // 唤醒被选中的那个等待者
        spin_lock_release(&m->lock);
    }
}


/*barriers*/

void init_barriers(void)
{
    for (int i = 0; i < BARRIER_NUM; i++)
    {
        barriers[i].goal = 0;
        barriers[i].wait_num = 0;
        barriers[i].state = FREE;
        barriers[i].wait_list.prev = barriers[i].wait_list.next = &barriers[i].wait_list;
    }
}

int do_barrier_init(int key, int goal)
{
    int free_idx = -1;

    for (int i = 0; i < BARRIER_NUM; i++)
    {
        if (barriers[i].state == USED && barriers[i].key == key)
        {
            // 已经有这个 key 的 barrier 了，直接改 goal
            barriers[i].goal = goal;
            return i;
        }
        if (barriers[i].state == FREE && free_idx < 0)
        {
            // 记录一个空闲位置
            free_idx = i;
        }
    }

    if (free_idx >= 0)
    {
        barriers[free_idx].key = key;
        barriers[free_idx].goal = goal;
        return free_idx;
    }

    return -1;
}

void do_barrier_wait(int bar_idx)
{
    barrier_t *b = &barriers[bar_idx];

    b->wait_num++;

    if (b->wait_num < b->goal)
    {
        // 人没到齐，阻塞当前进程
        do_block(&current_running->list, &b->wait_list);
        do_scheduler();
    }
    else
    {
        // 到齐了，唤醒所有在这个屏障上等待的进程
        free_block_list(&b->wait_list);
        b->wait_num = 0;
    }
}

void do_barrier_destroy(int bar_idx)
{
    free_block_list(&barriers[bar_idx].wait_list);
    barriers[bar_idx].key = 0;
    barriers[bar_idx].goal = 0;
    barriers[bar_idx].state = FREE;
}

/*condition*/

void init_conditions(void)
{
    for (int i = 0; i < CONDITION_NUM; i++)
    {
        conditions[i].key = 0;
        conditions[i].state = FREE;
        conditions[i].wait_list.prev = &conditions[i].wait_list;
        conditions[i].wait_list.next = &conditions[i].wait_list;
    }
}

int do_condition_init(int key)
{
    // 找key
    for (int i = 0; i < CONDITION_NUM; i++)
    {
        if (conditions[i].state == USED && conditions[i].key == key)
        {
            return i;
        }
    }
    // 找空闲
    for (int i = 0; i < CONDITION_NUM; i++)
    {
        if (conditions[i].state == FREE)
        {
            conditions[i].key = key;
            return i;
        }
    }
    return -1;
}
void do_condition_wait(int cond_idx, int mutex_idx)
{
    pcb_t *pcb = current_running;

    pcb->status = TASK_BLOCKED;
    add_node_to_q(&pcb->list, &conditions[cond_idx].wait_list);

    do_mutex_lock_release(mutex_idx);
    do_scheduler();
}

// 唤醒一个
void do_condition_signal(int cond_idx)
{
    list_node_t *head = &conditions[cond_idx].wait_list;
    list_node_t *p = head->next;

    if (p != head)
    {
        do_unblock(p);
    }
}

// 唤醒ALL
void do_condition_broadcast(int cond_idx)
{
    free_block_list(&conditions[cond_idx].wait_list);
}

void do_condition_destroy(int cond_idx)
{
    do_condition_broadcast(cond_idx);
    conditions[cond_idx].key = 0;
    conditions[cond_idx].state = FREE;
}

/*mailbox*/

void init_mbox(void)
{
    for (int i = 0; i < MBOX_NUM; i++)
    {
        mailboxes[i].name[0] = '\0';
        mailboxes[i].write_pos = 0;
        mailboxes[i].read_pos = 0;
        mailboxes[i].ref_count = 0;

        mailboxes[i].wait_full_queue.prev =
            mailboxes[i].wait_full_queue.next = &mailboxes[i].wait_full_queue;

        mailboxes[i].wait_empty_queue.prev =
            mailboxes[i].wait_empty_queue.next = &mailboxes[i].wait_empty_queue;
    }
}

int do_mbox_open(char *name)
{
    // 同名邮箱
    for (int i = 0; i < MBOX_NUM; i++)
    {
        if (mailboxes[i].name[0] != '\0' && strcmp(mailboxes[i].name, name) == 0)
        {
            mailboxes[i].ref_count++;
            return i;
        }
    }

    // 空闲
    for (int i = 0; i < MBOX_NUM; i++)
    {
        if (mailboxes[i].name[0] == '\0')
        {
            strcpy(mailboxes[i].name, name);
            mailboxes[i].ref_count++;
            return i;
        }
    }

    return -1;
}
void do_mbox_close(int mbox_idx)
{
    mailbox_t *mb = &mailboxes[mbox_idx];

    if (mb->ref_count > 0)
        mb->ref_count--;

    if (mb->ref_count == 0)
    {
        // 所有使用者都关闭后，释放这个槽位
        mb->name[0] = '\0';
        mb->write_pos = 0;
        mb->read_pos = 0;
    }
}

#define MBOX_COPY_TO_RING 0
#define MBOX_COPY_FROM_RING 1
// 环形缓冲区读写
static void mbox_ring_copy(char *ring_buf, char *linear_buf,
                           int start_pos, int len, int capacity, int mode)
{
    int physical_pos;

    if (mode == MBOX_COPY_TO_RING)
    {
        // linear_buf -> ring_buf
        for (int i = 0; i < len; i++)
        {
            physical_pos = (start_pos + i) % capacity;
            ring_buf[physical_pos] = linear_buf[i];
        }
    }
    else
    {
        // ring_buf -> linear_buf
        for (int i = 0; i < len; i++)
        {
            physical_pos = (start_pos + i) % capacity;
            linear_buf[i] = ring_buf[physical_pos];
        }
    }
}

int do_mbox_send(int mbox_idx, void *msg, int msg_length)
{
    mailbox_t *mb = &mailboxes[mbox_idx];
    int next_write_pos;
    int block_count = 0;

    // 邮箱空间不足，阻塞等待
    while ((next_write_pos = mb->write_pos + msg_length) >
           mb->read_pos + MAX_MBOX_LENGTH)
    {
        do_block(&current_running->list, &mb->wait_full_queue);
        do_scheduler();
        block_count++;
    }

    // 有空间
    mbox_ring_copy(mb->buffer, (char *)msg,
                   mb->write_pos, msg_length,
                   MAX_MBOX_LENGTH, MBOX_COPY_TO_RING);

    mb->write_pos = next_write_pos;

    free_block_list(&mb->wait_empty_queue);

    return block_count;
}

int do_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
    mailbox_t *mb = &mailboxes[mbox_idx];
    int next_read_pos;
    int block_count = 0;

    // 邮箱内容不足：阻塞等待
    while ((next_read_pos = mb->read_pos + msg_length) > mb->write_pos)
    {
        do_block(&current_running->list, &mb->wait_empty_queue);
        do_scheduler();
        block_count++;
    }

    // 有足够数据，进行拷贝
    mbox_ring_copy((char *)msg, mb->buffer,
                   mb->read_pos, msg_length,
                   MAX_MBOX_LENGTH, MBOX_COPY_FROM_RING);

    mb->read_pos = next_read_pos;

    free_block_list(&mb->wait_full_queue);

    return block_count;
}

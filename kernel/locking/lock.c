#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];
int lock_used_num = 0;

void init_locks(void)
{
    /*  initialize mlocks */
    for (int i = 0; i < LOCK_NUM; i++)
    {
        spin_lock_init(&mlocks[i].lock);
        mlocks[i].block_queue.prev = mlocks[i].block_queue.next = &mlocks[i].block_queue; // initialize block_queue
        mlocks[i].key = -1;
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
    if (lock->status == UNLOCKED)
    {
        lock->status = LOCKED;
        return 1;
    }
    return 0;
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /*  acquire spin lock */
    while (lock->status == UNLOCKED)
        ;
    lock->status = LOCKED;
}

void spin_lock_release(spin_lock_t *lock)
{
    /*  release spin lock */
    lock->status = UNLOCKED;
}

int do_mutex_lock_init(int key)
{
    /* initialize mutex lock */
    for (int i = 0; i < lock_used_num; i++)
    {
        if (mlocks[i].key == key)
            return i;
    }
    mlocks[lock_used_num].key = key;
    return lock_used_num++;
}

void do_mutex_lock_acquire(int mlock_idx) // 申请
{
    /* acquire mutex lock */
    if (spin_lock_try_acquire(&mlocks[mlock_idx].lock))
        return;
    // 获取锁失败
    do_block(&current_running->list, &mlocks[mlock_idx].block_queue);
    pcb_t *prior_running = current_running;
    current_running = get_pcb(seek_ready_node());
    current_running->status = TASK_RUNNING;
    switch_to(prior_running, current_running);
}

// 释放
void do_mutex_lock_release(int mlock_idx)
{
    /*  release mutex lock */
    list_node_t *head = &mlocks[mlock_idx].block_queue;
    list_node_t *p = head->next;

    if (p == head) // 没有阻塞
    {
        spin_lock_release(&mlocks[mlock_idx].lock);
    }
    else
    {
        do_unblock(p);
        spin_lock_release(&mlocks[mlock_idx].lock);
    }
}

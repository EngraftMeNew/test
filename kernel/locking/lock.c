#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM]; //互斥锁表，每个元素维护：底层自旋锁、阻塞等待队列、key
int lock_used_num = 0;

//锁机制的全局初始化
void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for(int i=0; i<LOCK_NUM; i++){
        spin_lock_init(&mlocks[i].lock);
        mlocks[i].block_queue.prev = mlocks[i].block_queue.next = & mlocks[i].block_queue;    // initialize block_queue
        mlocks[i].key = -1;
    }
}

//把自旋锁设为未加锁
void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
    lock -> status = UNLOCKED;
}

//尝试立即获取自旋锁
int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    if(lock->status == UNLOCKED){
        lock->status = LOCKED;
        return 1;
    }
    return 0;
}

//忙等自选，直到拿到锁
void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
    while(lock->status == LOCKED);
    lock->status = LOCKED;
}

//释放自旋锁
void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
    lock->status = UNLOCKED;
}

//互斥锁的初始化
int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    for(int i=0;i<lock_used_num;i++){
        if(mlocks[i].key == key)
            return i;
    }
    mlocks[lock_used_num].key = key;
    return lock_used_num++;
}

//申请
void do_mutex_lock_acquire(int mlock_idx)
{
    // 循环：拿不到就阻塞；被唤醒后继续尝试直到成功
    while (!spin_lock_try_acquire(&mlocks[mlock_idx].lock)) {
        // 放入该锁的阻塞队列并阻塞当前任务
        do_block(&current_running->list, &mlocks[mlock_idx].block_queue);

        // 交给调度器切走（醒来后从 while 继续）
        pcb_t *prior = current_running;
        list_node_t *node = seek_ready_node();     // 取下一个就绪
        current_running = get_pcb_from_node(node);
        current_running->status = TASK_RUNNING;
        switch_to(prior, current_running);
        // 醒来后回到 while 顶部再尝试加锁
    }
    // 成功加锁后返回
}



//释放
void do_mutex_lock_release(int mlock_idx)
{
    list_node_t *head = &mlocks[mlock_idx].block_queue;
    list_node_t *p = head->next;

    if (p == head) {
        // 无等待者，直接释放
        spin_lock_release(&mlocks[mlock_idx].lock);
    } else {
        // 有等待者：唤醒一个
        do_unblock(p);                    
        // 唤醒后要把锁“真正释放”，让被唤醒者下一轮能竞争到
        spin_lock_release(&mlocks[mlock_idx].lock);
    }
}


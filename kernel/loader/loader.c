#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <type.h>
#include <os/mm.h>

// [p1-task4]
uint64_t load_task_img(char *taskname)
{
    int i;
    int entry_addr;
    int start_sec;
    for (i = 0; i < TASK_MAXNUM; i++)
    {
        if (strcmp(taskname, tasks[i].task_name) == 0)
        {
            entry_addr = TASK_MEM_BASE + TASK_SIZE * i;
            start_sec = tasks[i].start_addr / 512; // 起始扇区：向下取整
            bios_sd_read(TMP_MEM_BASE, tasks[i].block_nums, start_sec);
            memcpy((uint8_t *)(uint64_t)(entry_addr), (uint8_t *)(uint64_t)(TMP_MEM_BASE + (tasks[i].start_addr - start_sec * 512)), tasks[i].block_nums * 512);
            return entry_addr; // 返回程序存储的起始位置
        }
    }
    // 匹配失败，提醒重新输入
    char *output_str = "Fail to find the task! Please try again!";
    for (i = 0; i < strlen(output_str); i++)
    {
        bios_putchar(output_str[i]);
    }
    bios_putchar('\n');
    return 0;
}

uint64_t map_task(char *taskname, uintptr_t pgdir)
{
    int i;

    for (i = 0; i < TASK_MAXNUM; i++)
    {
        if (strcmp(taskname, tasks[i].task_name) == 0)
        {
            uint64_t start_addr = (uint64_t)tasks[i].start_addr;
            uint64_t start_sec = start_addr / 512ULL;
            uint64_t offset = start_addr - start_sec * 512ULL;

            // 读入临时缓冲区
            bios_sd_read(TMP_MEM_BASE, tasks[i].block_nums, (int)start_sec);

            // 计算需要映射的用户虚拟区间，并向上取整到页边界
            uint64_t va_begin = USER_ENTRYPOINT;
            uint64_t va_end = USER_ENTRYPOINT + (uint64_t)tasks[i].p_memsz;
            va_end = (va_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

            // 分配映射
            uint64_t user_va;
            for (user_va = va_begin; user_va < va_end; user_va += PAGE_SIZE)
            {
                // alloc_page_helper 返回该页对应的 KVA
                alloc_page_helper((uintptr_t)user_va, pgdir);
            }

            // 拿到入口页 KVA
            uint64_t entry_kva = alloc_page_helper(USER_ENTRYPOINT, pgdir);

            memset((void *)entry_kva, 0, (size_t)tasks[i].p_memsz);

            void *src = (void *)pa2kva((uint64_t)TMP_MEM_BASE + offset);

            memcpy((void *)entry_kva, src, (size_t)tasks[i].p_memsz);

            return USER_ENTRYPOINT;
        }
    }

    char *output_str = "Fail to find the task! Please try again!";
    for (i = 0; i < (int)strlen(output_str); i++)
        bios_putchar(output_str[i]);
    bios_putchar('\n');
    return 0;
}

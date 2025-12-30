#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <type.h>
#include <os/mm.h>
#include <printk.h>
// [p1-task4]
uint64_t load_task_img(char *taskname)
{
    int i;
    uint64_t entry_addr;
    int start_sec;
    for (i = 0; i < TASK_MAXNUM; i++)
    {
        if (strcmp(taskname, tasks[i].task_name) == 0)
        {
            entry_addr = TASK_MEM_BASE + TASK_SIZE * i;
            start_sec = tasks[i].start_addr / 512; // 起始扇区：向下取整
            bios_sd_read(TMP_MEM_BASE, tasks[i].block_nums, start_sec);
            // memcpy((uint8_t *)(uint64_t)(entry_addr), (uint8_t *)pa2kva((uint64_t)(TMP_MEM_BASE + (tasks[i].start_addr - start_sec * 512))), tasks[i].p_memsz);
            memcpy((uint8_t *)(uint64_t)(entry_addr), (uint8_t *)pa2kva((uint64_t)(TMP_MEM_BASE + (tasks[i].start_addr - start_sec * 512))), tasks[i].p_memsz);
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
    for (int i = 0; i < TASK_MAXNUM; i++)
    {
        if (strcmp(taskname, tasks[i].task_name) != 0)
            continue;

        int start_sec = tasks[i].start_addr / 512;
        uint64_t img_off = tasks[i].start_addr - (uint64_t)start_sec * 512;

        bios_sd_read(TMP_MEM_BASE, tasks[i].block_nums, start_sec);
        uint8_t *src = (uint8_t *)pa2kva((uint64_t)(TMP_MEM_BASE + img_off));

        uint64_t va_start = USER_ENTRYPOINT;
        uint64_t va_end = USER_ENTRYPOINT + tasks[i].p_memsz;

        // 1) 映射/分配：覆盖整个 memsz
        for (uint64_t va = va_start; va < va_end; va += PAGE_SIZE)
        {
            alloc_page_helper(va, pgdir); // 只做一次，别重复
        }

        // 2) 拷贝：按 VA 逐页拷贝 filesz

        uint64_t remain = tasks[i].p_memsz;
        uint64_t va = va_start;
        while (remain > 0)
        {
            uint64_t kva = alloc_page_helper(va, pgdir); // 如果已映射，最好是返回已映射页
            uint8_t *dst = (uint8_t *)(kva);       // 若 pa 已经是 kva，这行改成 dst=(uint8_t*)pa;

            uint64_t n = (remain > PAGE_SIZE) ? PAGE_SIZE : remain;
            memcpy(dst, src, n);

            src += n;
            remain -= n;
            va += PAGE_SIZE;
        }

        return USER_ENTRYPOINT;
    }

    // not found
    char *output_str = "Fail to find the task! Please try again!";
    for (int i = 0; i < (int)strlen(output_str); i++)
        bios_putchar(output_str[i]);
    bios_putchar('\n');
    return 0;
}

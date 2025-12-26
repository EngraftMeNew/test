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
            uint64_t off = (uint64_t)tasks[i].start_addr - (uint64_t)start_sec * 512;
            memcpy((uint8_t *)entry_addr,
                   (uint8_t *)pa2kva((uint64_t)TMP_MEM_BASE + off),
                   (uint64_t)tasks[i].p_memsz);
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

        // 1) 从磁盘读到临时缓冲区（TMP_MEM_BASE 视为 PA）
        int start_sec = tasks[i].start_addr / 512;
        uint64_t file_off = (uint64_t)tasks[i].start_addr - (uint64_t)start_sec * 512;

        bios_sd_read(TMP_MEM_BASE, tasks[i].block_nums, start_sec);

        uint8_t *src = (uint8_t *)pa2kva((uint64_t)TMP_MEM_BASE + file_off);

        // 2) 计算需要映射/加载的范围
        uint64_t va0 = USER_ENTRYPOINT;                 // 0x10000
        uint64_t filesz = (uint64_t)tasks[i].task_size; // 文件中实际大小
        uint64_t memsz = (uint64_t)tasks[i].p_memsz;    // 内存中应占大小（含 bss）

        uint64_t va_end = va0 + memsz;

        printk("[map_task] memsz=0x%lx filesz=0x%lx\n", memsz, filesz);
        printk("[map_task] allocating va_end=0x%lx\n", va_end);

        // 3) 为 [va0, va_end) 的每个用户页建立映射，并按页把文件内容拷进去
        //    关键点：alloc_page_helper 返回的是 "该 va 对应物理页的 KVA"
        uint64_t copied = 0;
        for (uint64_t va = va0; va < va_end; va += PAGE_SIZE)
        {
            uint8_t *dst_kva = (uint8_t *)alloc_page_helper(va, pgdir);

            if (va == 0x11000)
                printk("[map_task] mapped va=0x11000 kva=%lx\n", (uintptr_t)dst_kva);

            // 这一页中要拷贝的文件字节数
            uint64_t remain = (filesz > copied) ? (filesz - copied) : 0;
            uint64_t ncopy = (remain > PAGE_SIZE) ? PAGE_SIZE : remain;

            if (ncopy > 0)
            {
                memcpy(dst_kva, src + copied, ncopy);
                copied += ncopy;
            }

            // 4) bss/尾部清零：该页剩余部分（或整页）清 0
            if (ncopy < PAGE_SIZE)
            {
                bzero(dst_kva + ncopy, PAGE_SIZE - ncopy);
            }
        }
        return USER_ENTRYPOINT;
    }

    char *output_str = "Fail to find the task! Please try again!";
    for (int j = 0; j < (int)strlen(output_str); j++)
        bios_putchar(output_str[j]);
    bios_putchar('\n');

    return 0;
}

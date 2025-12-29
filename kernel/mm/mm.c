#include <os/mm.h>
#include <os/string.h>
#include <printk.h>
#include <assert.h>
#include <os/task.h>
#include <pgtable.h>

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;

#define TOTAL_PAGES 65536
#define KERNELMEM_START 0xffffffc050000000lu
#define KERNELMEM_END 0xffffffc060000000lu

#define BITMAP(n) (n - KERNELMEM_START) / (8 * PAGE_SIZE)
#define BITMAP_OFFSET(n) ((n - KERNELMEM_START) / (PAGE_SIZE)) % 8
static uint8_t page_bitmap[TOTAL_PAGES / 8];

void init_memory_manager()
{
    bzero(page_bitmap, sizeof(page_bitmap));
}

bool is_memory_full()
{
    for (int i = 0; i < TOTAL_PAGES / 8; i++)
        if (page_bitmap[i] != 0xff)
            return false;
    return true;
}

void mark_used(ptr_t addr)
{
    page_bitmap[BITMAP(addr)] |= 1 << BITMAP_OFFSET(addr);
}

ptr_t allocPage(int numPage)
{
    assert(numPage == 1);
    while (1)
    {
        ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);

        if (ret >= KERNELMEM_END)
        {
            kernMemCurr = FREEMEM_KERNEL;
            if (is_memory_full())
            {
                printk("Memory is full\n");
                assert(0);
            }
            continue;
        }

        if (!(page_bitmap[BITMAP(ret)] & (1 << BITMAP_OFFSET(ret))))
        {
            mark_used(ret);
            kernMemCurr = ret + PAGE_SIZE;
            return ret;
        }

        kernMemCurr = ret + PAGE_SIZE;
    }
}

/*
// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
    // align LARGE_PAGE_SIZE
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
    return ret;
}
#endif
*/
void freePage(ptr_t baseAddr)
{
    // TODO [P4-task1] (design you 'freePage' here if you need):
    if (!baseAddr)
        return;

    baseAddr = ROUND(baseAddr, PAGE_SIZE); // 或者 assert 对齐

    if (baseAddr < KERNELMEM_START || baseAddr >= KERNELMEM_END)
        return; // 或 assert(0)

    page_bitmap[BITMAP(baseAddr)] &= ~(1 << BITMAP_OFFSET(baseAddr));
}

void *kmalloc(size_t size)
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
    (void)size;
    return NULL;
}

/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    PTE *dst = (PTE *)dest_pgdir;
    PTE *src = (PTE *)src_pgdir;

    // 先清空整个 root
    bzero(dst, PAGE_SIZE);

    // 只拷贝内核高半区映射（vpn2 256..511）
    for (int i = 256; i < 512; i++)
        dst[i] = src[i];
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir)
{
    // TODO [P4-task1] alloc_page_helper:
    va &= VA_MASK;
    uint64_t vpn2 = (va >> 30) & 0x1ff;
    uint64_t vpn1 = (va >> 21) & 0x1ff;
    uint64_t vpn0 = (va >> 12) & 0x1ff;

    PTE *pgd = (PTE *)pgdir;
    if (pgd[vpn2] == 0)
    {
        // 分配一个新的三级页目录，注意需要转化为实地址！
        set_pfn(&pgd[vpn2], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgd[vpn2], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pgd[vpn2])));
    }
    PTE *pmd = (uintptr_t *)pa2kva((get_pa(pgd[vpn2])));
    if (pmd[vpn1] == 0)
    {
        // 分配一个新的二级页目录
        set_pfn(&pmd[vpn1], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    }
    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
    if (pte[vpn0] == 0)
    {
        // 该虚地址从未被分配，分配一个新的页
        ptr_t pa = kva2pa(allocPage(1));
        set_pfn(&pte[vpn0], pa >> NORMAL_PAGE_SHIFT);
    }
    set_attribute(
        &pte[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                        _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_USER);
    return pa2kva(get_pa(pte[vpn0]));
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
    (void)key;
    return 0; 
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
    (void)addr;
}

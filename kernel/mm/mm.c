#include <os/mm.h>

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;

ptr_t allocPage(int numPage)
{
    // align PAGE_SIZE
    ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    kernMemCurr = ret + numPage * PAGE_SIZE;
    return ret;
}

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

void freePage(ptr_t baseAddr)
{
    // TODO [P4-task1] (design you 'freePage' here if you need):
}

void *kmalloc(size_t size)
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
}

/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
    memcpy(dest_pgdir, src_pgdir, PAGE_SIZE);
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir)
{
    va &= VA_MASK;

    const uint64_t VPN_MASK = (1UL << PPN_BITS) - 1; // 通常 PPN_BITS=9 => 0x1ff

    // Sv39: vpn2=v[38:30], vpn1=v[29:21], vpn0=v[20:12]
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & VPN_MASK;
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & VPN_MASK;
    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + 2 * PPN_BITS)) & VPN_MASK;

    PTE *pgd = (PTE *)pgdir;

    // level-2
    if (pgd[vpn2] == 0)
    {
        uintptr_t new_pt_kva = allocPage(1);
        uintptr_t new_pt_pa = kva2pa(new_pt_kva);

        set_pfn(&pgd[vpn2], new_pt_pa >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgd[vpn2], _PAGE_PRESENT);

        clear_pgdir(new_pt_kva); // 这里直接用 kva 更直观，等价于你原来的 pa2kva(get_pa(...))
    }

    PTE *pmd = (PTE *)pa2kva(get_pa(pgd[vpn2]));

    // level-1
    if (pmd[vpn1] == 0)
    {
        uintptr_t new_pt_kva = allocPage(1);
        uintptr_t new_pt_pa = kva2pa(new_pt_kva);

        set_pfn(&pmd[vpn1], new_pt_pa >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT);

        clear_pgdir(new_pt_kva);
    }

    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));

    // leaf
    if (pte[vpn0] == 0)
    {
        uintptr_t new_page_kva = allocPage(1);
        uintptr_t new_page_pa = kva2pa(new_page_kva);

        set_pfn(&pte[vpn0], new_page_pa >> NORMAL_PAGE_SHIFT);
    }

    set_attribute(&pte[vpn0],
                  _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                      _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY |
                      _PAGE_USER | _PAGE_GLOBAL);

    return pa2kva(get_pa(pte[vpn0]));
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
}

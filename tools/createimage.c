#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_FILE "./image"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

#define SECTOR_SIZE 512
#define BOOT_LOADER_SIG_OFFSET 0x1fe

// 0x1FE-0x1FF 要写 0x55, 0xAA
#define OS_SIZE_LOC 0x1fc
// info size location
#define APPINFO_SIZE_LOC 0x1f8
// task num location
#define TASKNUM_LOC 0x1f6

#define BOOT_LOADER_SIG_1 0x55
#define BOOT_LOADER_SIG_2 0xaa

#define BATCH_OFFSET_LOC 0x1f0 // 0x1f0..0x1f4
#define BATCH_SIZE 512

#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

/* TODO: [p1-task4] design your own task_info_t */
typedef struct
{
    char name[32]; // Task name
    int offset;    // Offset in the image file
    int size;      // Size of the task
} task_info_t;

#define TASK_MAXNUM 16
static task_info_t taskinfo[TASK_MAXNUM];

/* structure to store command line options */
static struct
{
    int vm;
    int extended;
} options;

/* prototypes of local functions */
static void create_image(int nfiles, char *files[]);
static void error(char *fmt, ...);
static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp);
static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr);
static uint64_t get_entrypoint(Elf64_Ehdr ehdr);
static uint32_t get_filesz(Elf64_Phdr phdr);
static uint32_t get_memsz(Elf64_Phdr phdr);
static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr);
static void write_padding(FILE *img, int *phyaddr, int new_phyaddr);
static void write_img_info(int nbytes_kernel, task_info_t *taskinfo,
                           short tasknum, FILE *img, int batch_offset);
//

int main(int argc, char **argv)
{
    char *progname = argv[0];

    /* process command line options */
    options.vm = 0;
    options.extended = 0;
    while ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == '-'))
    {
        char *option = &argv[1][2];

        if (strcmp(option, "vm") == 0)
        {
            options.vm = 1;
        }
        else if (strcmp(option, "extended") == 0)
        {
            options.extended = 1;
        }
        else
        {
            error("%s: invalid option\nusage: %s %s\n", progname,
                  progname, ARGS);
        }
        argc--;
        argv++;
    }
    if (options.vm == 1)
    {
        error("%s: option --vm not implemented\n", progname);
    }
    if (argc < 3)
    {
        /* at least 3 args (createimage bootblock main) */
        error("usage: %s %s\n", progname, ARGS);
    }
    create_image(argc - 1, argv + 1);
    return 0;
}

/* TODO: [p1-task4] assign your task_info_t somewhere in 'create_image' */
static void create_image(int nfiles, char *files[])
{
    int tasknum = nfiles - 2;
    int nbytes_kernel = 0;
    int phyaddr = 0;
    int appinfo_off = -1;
    int appinfo_size = (int)(sizeof(task_info_t) * tasknum);

    FILE *fp = NULL, *img = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Phdr phdr;

    /* open the image file */
    img = fopen(IMAGE_FILE, "w");
    assert(img != NULL);

    /* for each input file */
    for (int fidx = 0; fidx < nfiles; ++fidx)
    {

        int taskidx = fidx - 2;
        int start_addr = phyaddr;

        /* open input file */
        fp = fopen(*files, "r");
        assert(fp != NULL);

        /* read ELF header */
        read_ehdr(&ehdr, fp);
        printf("0x%04lx: %s\n", ehdr.e_entry, *files);

        /* for each program header */
        for (int ph = 0; ph < ehdr.e_phnum; ph++)
        {

            /* read program header */
            read_phdr(&phdr, fp, ph, ehdr);

            if (phdr.p_type != PT_LOAD)
                continue;

            /* write segment to the image */
            write_segment(phdr, fp, img, &phyaddr);

            /* update nbytes_kernel */
            if (strcmp(*files, "main") == 0)
            {
                nbytes_kernel += get_filesz(phdr);
            }
        }

        /* write padding bytes */
        /**
         * TODO:
         * 1. [p1-task3] do padding so that the kernel and every app program
         *  occupies the same number of sectors
         * 2. [p1-task4] only padding bootblock is allowed!
         */
        if (strcmp(*files, "bootblock") == 0)
        {
            write_padding(img, &phyaddr, SECTOR_SIZE);
        }
        /* 到 main 时预留 appinfo 区 */
        if (strcmp(*files, "main") == 0)
        {
            appinfo_off = phyaddr;
            write_padding(img, &phyaddr, appinfo_off + appinfo_size);
        }

        /* 记录 taskinfo（仅当是 app 时） */
        if (taskidx < 0 || taskidx >= tasknum)
        {
            /* 非 app，跳过 */
        }
        else
        {
            size_t n = sizeof(taskinfo[taskidx].name) - 1;
            strncpy(taskinfo[taskidx].name, *files, n);
            taskinfo[taskidx].name[n] = '\0';

            taskinfo[taskidx].offset = start_addr;
            taskinfo[taskidx].size = phyaddr - start_addr;

            printf("task %d: %s, offset: %d, size: %d\n",
                   taskidx, taskinfo[taskidx].name,
                   taskinfo[taskidx].offset, taskinfo[taskidx].size);
        }

        fclose(fp);
        files++;
    }
    /* padding for left space */
    fseek(img, phyaddr, SEEK_SET);
    write_padding(img, &phyaddr, NBYTES2SEC(phyaddr) * SECTOR_SIZE);

    /* padding for batch */
    int batch_offset = phyaddr;
    fseek(img, phyaddr, SEEK_SET);
    write_padding(img, &phyaddr, phyaddr + BATCH_SIZE);

    write_img_info(nbytes_kernel, taskinfo, tasknum, img, batch_offset);

    fclose(img);
}

static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp)
{
    int ret;

    ret = fread(ehdr, sizeof(*ehdr), 1, fp);
    assert(ret == 1);
    assert(ehdr->e_ident[EI_MAG1] == 'E');
    assert(ehdr->e_ident[EI_MAG2] == 'L');
    assert(ehdr->e_ident[EI_MAG3] == 'F');
}

static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph,
                      Elf64_Ehdr ehdr)
{
    int ret;

    fseek(fp, ehdr.e_phoff + ph * ehdr.e_phentsize, SEEK_SET);
    ret = fread(phdr, sizeof(*phdr), 1, fp);
    assert(ret == 1);
    if (options.extended == 1)
    {
        printf("\tsegment %d\n", ph);
        printf("\t\toffset 0x%04lx", phdr->p_offset);
        printf("\t\tvaddr 0x%04lx\n", phdr->p_vaddr);
        printf("\t\tfilesz 0x%04lx", phdr->p_filesz);
        printf("\t\tmemsz 0x%04lx\n", phdr->p_memsz);
    }
}

static uint64_t get_entrypoint(Elf64_Ehdr ehdr)
{
    return ehdr.e_entry;
}

static uint32_t get_filesz(Elf64_Phdr phdr)
{
    return phdr.p_filesz;
}

static uint32_t get_memsz(Elf64_Phdr phdr)
{
    return phdr.p_memsz;
}

static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr)
{
    if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD)
    {
        /* write the segment itself */
        /* NOTE: expansion of .bss should be done by kernel or runtime env! */
        if (options.extended == 1)
        {
            printf("\t\twriting 0x%04lx bytes\n", phdr.p_filesz);
        }
        fseek(fp, phdr.p_offset, SEEK_SET);
        while (phdr.p_filesz-- > 0)
        {
            fputc(fgetc(fp), img);
            (*phyaddr)++;
        }
    }
}

static void write_padding(FILE *img, int *phyaddr, int new_phyaddr)
{
    if (options.extended == 1 && *phyaddr < new_phyaddr)
    {
        printf("\t\twrite 0x%04x bytes for padding\n", new_phyaddr - *phyaddr);
    }

    while (*phyaddr < new_phyaddr)
    {
        fputc(0, img);
        (*phyaddr)++;
    }
}

static void write_at(FILE *img, long off, const void *ptr, size_t sz)
{
    if (fseek(img, off, SEEK_SET) != 0)
    {
        perror("fseek");
        exit(1);
    }
    if (fwrite(ptr, 1, sz, img) != sz)
    {
        perror("fwrite");
        exit(1);
    }
}

// - nbytes_kernel:    内核（main）的总字节数（仅统计 PT_LOAD 的 p_filesz 之和）
// - taskinfo:         任务信息数组（name/offset/size）
// - tasknum:          任务个数
// - img:              输出镜像文件指针
// - batch_offset:     结尾 batch 区在镜像中的字节偏移

static void write_img_info(int nbytes_kernel, task_info_t *taskinfo,
                           short tasknum, FILE *img, int batch_offset)
{
    uint16_t kernel_bytes = (uint16_t)nbytes_kernel;
    uint16_t tasknum_16 = (uint16_t)tasknum;
    uint32_t appinfo_off = (uint32_t)(SECTOR_SIZE + nbytes_kernel);

    // kernel字节数
    write_at(img, OS_SIZE_LOC, &kernel_bytes, sizeof kernel_bytes);
    // tasknum
    write_at(img, TASKNUM_LOC, &tasknum_16, sizeof tasknum_16);
    // appinfo offset
    write_at(img, APPINFO_SIZE_LOC, &appinfo_off, sizeof appinfo_off);
    // 写入taskinfo
    write_at(img, appinfo_off, taskinfo, sizeof(task_info_t) * (size_t)tasknum);
    // 写入batch offset
    write_at(img, BATCH_OFFSET_LOC, &batch_offset, sizeof batch_offset);
}

/* print an error message and exit */
static void error(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if (errno != 0)
    {
        perror(NULL);
    }
    exit(EXIT_FAILURE);
}

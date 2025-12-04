/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stddef.h>

#define SHELL_BEGIN 20

#define MAX_LEN 100
char buff[MAX_LEN];
int buf_len;

#define MAX_ARG_LEN 20
#define MAX_ARG_NUM 10
char argv[MAX_ARG_NUM][MAX_ARG_LEN];

int parse_args(const char *buff)
{
    int argc = 0;
    int i = 0;
    for (int j = 0; j < MAX_ARG_NUM; j++)
    {
        argv[j][0] = '\0';
    }
    while (*buff)
    {
        while (isspace(*buff))
            buff++;

        if (*buff == '\0')
            break;

        i = 0;
        while (*buff && !isspace(*buff) && i < MAX_ARG_LEN - 1)
        {
            argv[argc][i++] = *buff++;
        }
        argv[argc][i] = '\0'; // 添加字符串结尾符
        argc++;
    }
    return argc;
}

int main(void)
{
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
    printf("> root@UCAS_OS: ");

    while (1)
    {
        // TODO [P3-task1]: call syscall to read UART port
        int temp;
        buf_len = 0;
        int end = 0;
        int ch;
        // TODO [P3-task1]: parse input
        // note: backspace maybe 8('\b') or 127(delete)
        while (1)
        {
            while ((ch = sys_getchar()) == -1)
                ;
            if (ch == '\n' || ch == '\r')
            {
                // 回显换行
                sys_write_ch('\n');
                sys_reflush();
                buff[buf_len] = '\0'; // 封成 C 字符串
                end = 1;
                break; // 跳出去处理命令
            }
            if (ch == '\b' || ch == 127)
            {
                if (buf_len > 0)
                {
                    sys_write_ch(ch);
                    sys_reflush();
                    buf_len--;
                    buff[buf_len] = '\0';
                }
                continue;
            }
            if (buf_len < MAX_LEN - 1)
            {
                sys_write_ch(ch);

                sys_reflush();
                buff[buf_len++] = (char)ch;
            }
        }
        buff[buf_len] = '\0';
        // TODO [P3-task1]: ps, exec, kill, clear
        int argc = parse_args(buff);
        if (argc == 0)
        {
            sys_write("> root@UCAS_OS: ");
            continue;
        }

        // ps
        if (!strcmp(argv[0], "ps") && argc == 1)
        {
            sys_ps();
        }
        // clear
        else if (!strcmp(argv[0], "clear") && argc == 1)
        {
            sys_clear();
            sys_move_cursor(0, SHELL_BEGIN);
            sys_write("------------------- COMMAND -------------------\n");
        }
        // exec

        else if (!strcmp(argv[0], "exec"))
        {
            if (argc < 2)
            {
                printf("usage: exec <program> [args...] [&]\n");
                continue;
            }

            int background = 0;
            if (!strcmp(argv[argc - 1], "&"))
                background = 1;

            // 去掉 "exec" 本身，以及末尾的 "&"（如果有）
            int arg_start = 1;
            int arg_end = argc - (background ? 1 : 0); // 开区间 [arg_start, arg_end)

            int exec_argc = arg_end - arg_start; // 子进程看到的 argc
            char *exec_argv[MAX_ARG_NUM];

            // 让 exec_argv[0] = 程序名，后面是参数
            for (int i = 0; i < exec_argc; i++)
            {
                exec_argv[i] = argv[arg_start + i]; // argv[1] -> "add", argv[2] -> "5", ...
            }

            // name 就是程序名，也可以直接用 exec_argv[0]
            char *prog_name = exec_argv[0];

            pid_t pid = sys_exec(prog_name, exec_argc, exec_argv);
            if (pid == 0)
            {
                printf("pid =0 ,exec failed: %s\n", prog_name);
                continue;
            }

            printf("pid = %d\n", pid);

            if (!background)
            {
                sys_waitpid(pid);
            }
        }

        // kill
        else if (!strcmp(argv[0], "kill"))
        {
            if (argc < 2)
            {
                printf("usage: kill <pid>\n");
                continue;
            }

            int pid = atoi(argv[1]);
            int ret = sys_kill(pid);
            if (!ret)
                printf("kill failed: no such pid %d\n", pid);
            else
                printf("pid %d killed\n", pid);
        }

        // wait / waitpid
        else if (!strcmp(argv[0], "wait"))
        {
            if (argc < 2)
            {
                printf("usage: wait <pid>\n");
                continue;
            }

            int pid = atoi(argv[1]);
            int ret = sys_waitpid(pid);
            if (!ret)
                printf("wait failed: no such pid %d or already exited\n", pid);
            else
                printf("wait successfully\n", pid);
        }

        else
        {
            sys_write("Error: Unknown command\n");
        }

        // 再次打印提示符
        sys_write("> root@UCAS_OS: ");
        /************************************************************/
        /* Do not touch this comment. Reserved for future projects. */
        /************************************************************/
    }

    return 0;
}

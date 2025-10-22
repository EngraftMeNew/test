/* [p1-task5] */
#include <kernel.h>

#define  ARR_SIZE 100
#define WORD_SIZE 20

char buf[ARR_SIZE][WORD_SIZE];

int
strcmp(const char *str1, const char *str2)
{
    while (*str1 && *str2) {
        if (*str1 != *str2) {
            return (*str1) - (*str2);
        }
        ++str1;
        ++str2;
    }
    return (*str1) - (*str2);
}

char *
strcpy(char *dest, const char *src)
{
    char *tmp = dest;

    while (*src) {
        *dest++ = *src++;
    }

    *dest = '\0';

    return tmp;
}

int
get_word(char *str)
{
    int c;
    int i = 0;

    /* jump over unnecessary spaces */
    while (1) {
        while ((c = bios_getchar()) == -1)
            ;
        if (c != '\n' && c != '\r' && c != ' ' && c != '\t')
            break;
    }

    str[i++] = c;

    /* read the real word */
    while (1) {
        while ((c = bios_getchar()) == -1)
            ;



        /* reach the end of word */
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t' || i >= WORD_SIZE - 1)
            break;

        /* delete */
        if (c == 127) {
            if (i > 0) i--;
        } else {
            str[i++] = c;
        }
    }

    str[i] = '\0';
    return 0;
}

void
sort(char arr[][WORD_SIZE], int len)
{
    char temp[WORD_SIZE];
    for (int i = 0; i < len - 1; i++) {
        for (int j = 0; j < len - i - 1; j++) {
            if (strcmp(arr[j], arr[j + 1]) > 0) {
                strcpy(temp, arr[j]);
                strcpy(arr[j], arr[j + 1]);
                strcpy(arr[j + 1], temp);
            }
        }
    }
}

int
main(void)
{
    int i = 0;
    int len = 0;

    /* input */
    for (i = 0; i < ARR_SIZE; i++) {
        if (get_word(buf[i]) != 0)
            break;
        len++;
    }

    /* sort */
    sort(buf, len);

    /* output */
    for (i = 0; i < len; i++) {
        bios_putstr(buf[i]);
        bios_putstr("\n\r");
    }

    return 0;
}
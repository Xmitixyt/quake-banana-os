/*
 * sys_banana.c - Banana OS system interface for Quake
 */

#include "quakedef.h"
#include <banana.h>
#include <errno.h>

#define MAX_HANDLES 64
static FILE *sys_handles[MAX_HANDLES];

static int findhandle(void)
{
    for (int i = 1; i < MAX_HANDLES; i++) {
        if (!sys_handles[i])
            return i;
    }
    Sys_Error("out of file handles");
    return -1;
}

static int filelength(FILE *f)
{
    long pos = ftell(f);
    fseek(f, 0, SEEK_END);
    long end = ftell(f);
    fseek(f, pos, SEEK_SET);
    return (int)end;
}

int Sys_FileOpenRead(char *path, int *hndl)
{
    int i = findhandle();
    FILE *f = fopen(path, "rb");
    if (!f) {
        *hndl = -1;
        return -1;
    }
    sys_handles[i] = f;
    *hndl = i;
    return filelength(f);
}

int Sys_FileOpenWrite(char *path)
{
    int i = findhandle();
    FILE *f = fopen(path, "wb");
    if (!f)
        Sys_Error("Error opening %s", path);
    sys_handles[i] = f;
    return i;
}

void Sys_FileClose(int handle)
{
    if (handle > 0 && handle < MAX_HANDLES && sys_handles[handle]) {
        fclose(sys_handles[handle]);
        sys_handles[handle] = NULL;
    }
}

void Sys_FileSeek(int handle, int position)
{
    if (handle > 0 && handle < MAX_HANDLES && sys_handles[handle])
        fseek(sys_handles[handle], position, SEEK_SET);
}

int Sys_FileRead(int handle, void *dest, int count)
{
    if (handle > 0 && handle < MAX_HANDLES && sys_handles[handle])
        return (int)fread(dest, 1, count, sys_handles[handle]);
    return -1;
}

int Sys_FileWrite(int handle, void *data, int count)
{
    if (handle > 0 && handle < MAX_HANDLES && sys_handles[handle])
        return (int)fwrite(data, 1, count, sys_handles[handle]);
    return -1;
}

int Sys_FileTime(char *path)
{
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return 1;
    }
    return -1;
}

void Sys_mkdir(char *path)
{
    if (__banana && __banana->mkdir)
        __banana->mkdir(path);
}

void Sys_MakeCodeWriteable(unsigned long startaddr, unsigned long length)
{
}

void Sys_DebugLog(char *file, char *fmt, ...)
{
}

void Sys_Error(char *error, ...)
{
    va_list argptr;
    printf("Sys_Error: ");
    va_start(argptr, error);
    vprintf(error, argptr);
    va_end(argptr);
    printf("\n");
    Host_Shutdown();
    exit(1);
}

void Sys_Printf(char *fmt, ...)
{
    va_list argptr;
    va_start(argptr, fmt);
    vprintf(fmt, argptr);
    va_end(argptr);
}

void Sys_Quit(void)
{
    Host_Shutdown();
    exit(0);
}

double Sys_FloatTime(void)
{
    static unsigned int baseticks = 0;
    unsigned int t = banana_ticks();
    if (!baseticks) baseticks = t;
    return (double)(t - baseticks) * 0.001;
}

char *Sys_ConsoleInput(void)
{
    return NULL;
}

void Sys_Sleep(void)
{
    banana_yield();
}

qboolean isDedicated = false;

int fscanf(FILE *f, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int count = 0;
    while (*fmt) {
        if (isspace((unsigned char)*fmt)) {
            fmt++;
            continue;
        }
        if (*fmt == '%') {
            fmt++;
            char conv = *fmt++;
            int c;
            while ((c = fgetc(f)) != EOF && isspace(c)) ;
            if (c == EOF) break;
            ungetc(c, f);

            if (conv == 's') {
                char *out = va_arg(ap, char*);
                int n = 0;
                while ((c = fgetc(f)) != EOF && !isspace(c)) {
                    out[n++] = (char)c;
                }
                out[n] = 0;
                count++;
            } else if (conv == 'i' || conv == 'd') {
                int *out = va_arg(ap, int*);
                char num[64];
                int n = 0;
                while ((c = fgetc(f)) != EOF && !isspace(c) && n < 62) {
                    num[n++] = (char)c;
                }
                num[n] = 0;
                *out = atoi(num);
                count++;
            } else if (conv == 'f') {
                float *out = va_arg(ap, float*);
                char num[64];
                int n = 0;
                while ((c = fgetc(f)) != EOF && !isspace(c) && n < 62) {
                    num[n++] = (char)c;
                }
                num[n] = 0;
                *out = (float)atof(num);
                count++;
            }
        } else {
            fmt++;
        }
    }
    va_end(ap);
    return count;
}

void Sys_LowFPPrecision(void)
{
}

void Sys_HighFPPrecision(void)
{
}

void Sys_SetFPCW(void)
{
}

int main(int argc, char **argv)
{
    static quakeparms_t parms;

    parms.memsize = 24 * 1024 * 1024;
    parms.membase = malloc(parms.memsize);
    if (!parms.membase) {
        parms.memsize = 16 * 1024 * 1024;
        parms.membase = malloc(parms.memsize);
    }
    if (!parms.membase) {
        parms.memsize = 12 * 1024 * 1024;
        parms.membase = malloc(parms.memsize);
    }
    if (!parms.membase) {
        printf("Fatal: could not allocate Quake memory hunk\n");
        return 1;
    }

    COM_InitArgv(argc, argv);

    // Look for id1/pak0.pak in current directory or /apps/quake
    FILE *test_pak = fopen("id1/pak0.pak", "rb");
    if (test_pak) {
        fclose(test_pak);
        parms.basedir = ".";
    } else {
        parms.basedir = "/apps/quake";
    }

    parms.argc = com_argc;
    parms.argv = com_argv;

    printf("Starting Quake on Banana OS (basedir: %s)...\n", parms.basedir);
    Host_Init(&parms);

    double oldtime = Sys_FloatTime();
    while (1) {
        double newtime = Sys_FloatTime();
        double time = newtime - oldtime;
        if (time < (1.0 / 72.0)) {
            banana_yield();
            continue;
        }
        if (time > 0.1) time = 0.1;
        oldtime = newtime;
        Host_Frame(time);
    }

    return 0;
}

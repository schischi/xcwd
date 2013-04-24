/* This is fcwd written by Adrien Schildknecht (c) 2013
 * Email: adrien+dev@schischi.me
 * Feel free to copy and redistribute in terms of the
 * BSD license
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <glob.h>
#include <sys/stat.h>
#include <X11/Xlib.h>

#define XA_STRING (XInternAtom(dpy, "STRING", 0))

Display *dpy;

typedef struct processes_s *processes_t;
struct processes_s {
    struct proc_s {
        long pid;
        long ppid;
        char name[32];
    } *ps;
    size_t n;
};

int nameCmp(const void *p1, const void *p2)
{
    return strcasecmp(((struct proc_s *)p1)->name,
            ((struct proc_s *)p2)->name);
}

int ppidCmp(const void *p1, const void *p2)
{
    return ((struct proc_s *)p1)->ppid - ((struct proc_s *)p2)->ppid;
}

static Window focusedWindow()
{
    Window focuswin;
    int focusrevert;

    dpy = XOpenDisplay (NULL);
    if (!dpy)
        exit (1);
    XGetInputFocus (dpy, &focuswin, &focusrevert);
#ifdef DEBUG
    fprintf(stderr, "Window ID = %lu\n", focuswin);
#endif
    return focuswin;
}

static long windowPid(Window focuswin)
{
    Atom nameAtom = XInternAtom(dpy, "_NET_WM_PID", 1);
    Atom cardinalAtom = XInternAtom(dpy, "CARDINAL", 0);
    Atom type;
    int format;
    long pid = -1;
    unsigned long nitems, after;
    unsigned char *data = 0;

    if (XGetWindowProperty(dpy, focuswin, nameAtom, 0, 1024, 0, cardinalAtom,
                &type, &format, &nitems, &after, &data) == Success) {
        if (data) {
            pid = *((long*)data);
            XFree(data);
        }
    }
#ifdef DEBUG
    if(pid == -1)
        fprintf(stderr, "_NET_WM_PID not found\n");
    else
        fprintf(stderr, "_NET_WM_PID = %lu\n", pid);
#endif
    return pid;
}

static char* windowStrings(Window focuswin, size_t *size, char* hint)
{
    Atom nameAtom = XInternAtom(dpy, hint, 1);
    Atom type;
    int format;
    int i;
    unsigned long after;
    unsigned char *data = 0;
    char *ret = NULL;

    if (XGetWindowProperty(dpy, focuswin, nameAtom, 0, 1024, 0, AnyPropertyType,
                &type, &format, size, &after, &data) == Success) {
        if (data) {
            if(type == XA_STRING) {
                ret = malloc(sizeof(char) * *size);
#ifdef DEBUG
                fprintf(stderr, "%s = ", hint);
#endif
                for(i = 0; i < *size; ++i) {
#ifdef DEBUG
                    fprintf(stderr, "%c", data[i] == 0 ? ' ' : data[i]);
#endif
                    ret[i] = data[i];
                }
#ifdef DEBUG
                    fprintf(stderr, "\n");
#endif
            }
            XFree(data);
        }
#ifdef DEBUG
        else
            fprintf(stderr, "%s not found\n", hint);
#endif
    }
    return ret;
}

static void freeProcesses(processes_t p)
{
    free(p->ps);
    free(p);
}

static processes_t getProcesses(void)
{
    glob_t globbuf;
    unsigned int i, j;
    processes_t p;

    glob("/proc/[0-9]*", GLOB_NOSORT, NULL, &globbuf);
    p = malloc(sizeof(struct processes_s));
    p->ps = malloc(globbuf.gl_pathc * sizeof(struct proc_s));

#ifdef DEBUG
        fprintf(stderr, "Found %ld processes\n", globbuf.gl_pathc);
#endif
    for (i = j = 0; i < globbuf.gl_pathc; i++) {
        char name[32];
        FILE *tn;

        (void)globbuf.gl_pathv[globbuf.gl_pathc - i - 1];
        snprintf(name, sizeof(name), "%s%s",
                globbuf.gl_pathv[globbuf.gl_pathc - i - 1], "/stat");
        tn = fopen(name, "r");
        if (tn == NULL)
            continue;
        fscanf(tn, "%ld (%32[^)] %*3c %ld", &p->ps[j].pid, p->ps[j].name,
                &p->ps[j].ppid);
#ifdef DEBUG
        fprintf(stderr, "\t%-20s\tpid=%6ld\tppid=%6ld\n", p->ps[j].name,
                p->ps[j].pid, p->ps[j].ppid);
#endif
        fclose(tn);
        j++;
    }
    p->n = j;
    globfree(&globbuf);
    return p;
}

static long lastChild(processes_t p, long pid)
{
    struct proc_s key, *res;

    do {
        key.ppid = pid;
        res = (struct proc_s *)bsearch(&key, p->ps, p->n,
                sizeof(struct proc_s), ppidCmp);
        pid = res ? res->pid : -1;
    }while(pid != -1);
    return key.ppid;
}

static void readPath(long pid)
{
    char buf[255];
    char path[64];
    snprintf(path, sizeof(path), "/proc/%ld/cwd", pid);
#ifdef DEBUG
    fprintf(stderr, "Read %s\n", path);
#endif
    readlink(path, buf, 255);
    fprintf(stdout, "%s\n", buf);
}

int main(int argc, const char *argv[])
{
    processes_t p;
    Window w = focusedWindow();
    long pid;

    pid = windowPid(w);
    p = getProcesses();
    if(pid != -1) { // WM_NET_PID
        qsort(p->ps, p->n, sizeof(struct proc_s), ppidCmp);
    }
    else {
        size_t size;
        char* strings;
        int i;
        struct proc_s *res = NULL, key;
        qsort(p->ps, p->n, sizeof(struct proc_s), nameCmp);
        strings = windowStrings(w, &size, "WM_CLASS");
        for(i = 0; i < size; i += strlen(strings + i) + 1) {
#ifdef DEBUG
            fprintf(stderr, "pidof %s\n", strings + i);
#endif
            strcpy(key.name, strings + i);
            res = (struct proc_s *)bsearch(&key, p->ps, p->n,
                sizeof(struct proc_s), nameCmp);
            if(res)
                break;
        }
        if(res) {
            pid = res->pid;
#ifdef DEBUG
            fprintf(stderr, "Found %s (%ld)\n", res->name, res->pid);
#endif
        }
        if(size != 0)
            free(strings);
    }
    if(pid != -1) {
        pid = lastChild(p, pid);
        readPath(pid);
    }
    else {
#ifdef DEBUG
        fprintf(stderr, "getenv $HOME...\n");
#endif
        fprintf(stdout, "%s\n", getenv("HOME"));
    }
    freeProcesses(p);

    return EXIT_SUCCESS;
}


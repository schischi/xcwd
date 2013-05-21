/* This is fcwd written by Adrien Schildknecht (c) 2013
 * Email: adrien+dev@schischi.me
 * Feel free to copy and redistribute in terms of the
 * BSD license
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <glob.h>
#include <sys/stat.h>
#include <X11/Xlib.h>

#define DEBUG 0

#define XA_STRING   (XInternAtom(dpy, "STRING", 0))
#define XA_CARDINAL (XInternAtom(dpy, "CARDINAL", 0))
#define XA_WM_STATE (XInternAtom(dpy, "WM_STATE", 0))

#define LOG(fmt, ...) \
    do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

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
    Atom type;
    Window focuswin, root, *children;
    int format, status, focusrevert;
    unsigned long nitems, after;
    unsigned char *data;
    unsigned int nchildren;

    dpy = XOpenDisplay (NULL);
    if (!dpy)
        exit (1);
    XGetInputFocus (dpy, &focuswin, &focusrevert);
    root = XDefaultRootWindow(dpy);

    do {
        status = XGetWindowProperty(dpy, focuswin, XA_WM_STATE, 0, 1024, 0,
                XA_WM_STATE, &type, &format, &nitems, &after, &data);
        if (status == Success) {
            if (data) {
                XFree(data);
                LOG("Window ID = %lu\n", focuswin);
                return focuswin;
            }
        }
        else
            return 0;
        XQueryTree(dpy, focuswin, &root, &focuswin, &children, &nchildren);
        LOG("%s", "Current window does not have WM_STATE, getting parent\n");
    } while(focuswin != root);

    return 0;
}

static long windowPid(Window focuswin)
{
    Atom nameAtom = XInternAtom(dpy, "_NET_WM_PID", 1);
    Atom type;
    int format, status;
    long pid = -1;
    unsigned long nitems, after;
    unsigned char *data;

    status = XGetWindowProperty(dpy, focuswin, nameAtom, 0, 1024, 0,
            XA_CARDINAL, &type, &format, &nitems, &after, &data);
    if (status == Success && data) {
        pid = *((long*)data);
        XFree(data);
        LOG("_NET_WM_PID = %lu\n", pid);
    }
    else
        LOG("%s", "_NET_WM_PID not found\n");
    return pid;
}

static char* windowStrings(Window focuswin, long unsigned int *size, char* hint)
{
    Atom nameAtom = XInternAtom(dpy, hint, 1);
    Atom type;
    int format;
    unsigned int i;
    unsigned long after;
    unsigned char *data = 0;
    char *ret = NULL;

    if (XGetWindowProperty(dpy, focuswin, nameAtom, 0, 1024, 0, AnyPropertyType,
                &type, &format, size, &after, &data) == Success) {
        if (data) {
            if (type == XA_STRING) {
                ret = malloc(sizeof(char) * *size);
                LOG("%s = ", hint);
                for (i = 0; i < *size; ++i) {
                    LOG("%c", data[i] == 0 ? ' ' : data[i]);
                    ret[i] = data[i];
                }
                fprintf(stderr, "\n");
                LOG("%s", "\n");
            }
            XFree(data);
        }
        else
            LOG("%s not found\n", hint);
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

    LOG("Found %zu processes\n", globbuf.gl_pathc);
    for (i = j = 0; i < globbuf.gl_pathc; i++) {
        char name[32];
        FILE *tn;

        (void)globbuf.gl_pathv[globbuf.gl_pathc - i - 1];
        snprintf(name, sizeof(name), "%s%s",
                globbuf.gl_pathv[globbuf.gl_pathc - i - 1], "/stat");
        tn = fopen(name, "r");
        if (tn == NULL)
            continue;
        if(fscanf(tn, "%ld (%32[^)] %*3c %ld", &p->ps[j].pid,
                p->ps[j].name, &p->ps[j].ppid) != 3)
            return NULL;
        LOG("\t%-20s\tpid=%6ld\tppid=%6ld\n", p->ps[j].name, p->ps[j].pid,
                p->ps[j].ppid);
        fclose(tn);
        j++;
    }
    p->n = j;
    globfree(&globbuf);
    return p;
}

static int readPath(long pid)
{
    char buf[255];
    char path[64];
    ssize_t len;

    snprintf(path, sizeof(path), "/proc/%ld/cwd", pid);
    if ((len = readlink(path, buf, 255)) != -1)
        buf[len] = '\0';
    if(len <= 0) {
        LOG("Error readlink %s\n", path);
        return 0;
    }
    LOG("Read %s\n", path);
    fprintf(stdout, "%s\n", buf);
    return 1;
}

static void cwdOfDeepestChild(processes_t p, long pid)
{
    int i;
    struct proc_s key = {.ppid = pid}, *res = NULL, *lastRes = NULL;

    do {
        if(res) {
            lastRes = res;
            key.ppid = res->pid;
        }
        res = (struct proc_s *)bsearch(&key, p->ps, p->n,
                sizeof(struct proc_s), ppidCmp);
    } while(res);

    if(!lastRes) {
        readPath(pid);
        return;
    }

    for(i = 0; lastRes != p->ps && (lastRes - i)->ppid == lastRes->ppid; ++i)
        if(readPath((lastRes - i)->pid))
            return;
    for(i = 1; lastRes != p->ps + p->n && (lastRes + i)->ppid == lastRes->ppid; ++i)
        if(readPath((lastRes + i)->pid))
            return;
}

int getHomeDirectory()
{
    LOG("%s", "getenv $HOME...\n");
    fprintf(stdout, "%s\n", getenv("HOME"));
    return EXIT_FAILURE;
}

int main(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;

    processes_t p;
    long pid;
    Window w = focusedWindow();
    if (w == 0)
        return getHomeDirectory();

    pid = windowPid(w);
    p = getProcesses();
    if(p == NULL)
        return getHomeDirectory();
    if (pid != -1) {
        qsort(p->ps, p->n, sizeof(struct proc_s), ppidCmp);
    }
    else {
        long unsigned int size;
        unsigned int i;
        char* strings;
        struct proc_s *res = NULL, key;

        qsort(p->ps, p->n, sizeof(struct proc_s), nameCmp);
        strings = windowStrings(w, &size, "WM_CLASS");
        for(i = 0; i < size; i += strlen(strings + i) + 1) {
            LOG("pidof %s\n", strings + i);
            strcpy(key.name, strings + i);
            res = (struct proc_s *)bsearch(&key, p->ps, p->n,
                    sizeof(struct proc_s), nameCmp);
            if(res)
                break;
        }
        if (res) {
            pid = res->pid;
            LOG("Found %s (%ld)\n", res->name, res->pid);
        }
        if (size != 0)
            free(strings);
    }
    if (pid != -1)
        cwdOfDeepestChild(p, pid);
    else {
        LOG("%s", "getenv $HOME...\n");
        fprintf(stdout, "%s\n", getenv("HOME"));
    }
    freeProcesses(p);

    return EXIT_SUCCESS;
}


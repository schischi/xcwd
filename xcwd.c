/* This is xcwd written by Adrien Schildknecht (c) 2013-2014
 * Email: adrien+dev@schischi.me
 * Feel free to copy and redistribute in terms of the
 * BSD license
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <X11/Xlib.h>

#ifdef LINUX
# include <sys/stat.h>
# include <glob.h>
#endif

#ifdef BSD
# include <sys/sysctl.h>
# include <sys/user.h>
# include <libutil.h>
#endif

//#define DEBUG

#define XA_STRING   (XInternAtom(dpy, "STRING", 0))
#define XA_CARDINAL (XInternAtom(dpy, "CARDINAL", 0))
#define XA_WM_STATE (XInternAtom(dpy, "WM_STATE", 0))

#ifdef DEBUG
 #define LOG(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#else
 #define LOG(fmt, ...) do { } while (0)
#endif

Display *dpy;

struct proc_list {
    struct proc_s {
        long pid;
        long ppid;
        char name[32];
#ifdef BSD
        char cwd[MAXPATHLEN];
#endif
    } *ps;
    size_t n;
};

static int process_name_cmp(const void *p1, const void *p2)
{
    return strcasecmp(((struct proc_s *)p1)->name,
            ((struct proc_s *)p2)->name);
}

static int process_ppid_cmp(const void *p1, const void *p2)
{
    return ((struct proc_s *)p1)->ppid - ((struct proc_s *)p2)->ppid;
}

static Window focused_window(void)
{
    Atom type;
    Window focuswin, root, *children;
    int format, status;
    unsigned long nitems, after;
    unsigned int nchildren;
    unsigned char *data;

    dpy = XOpenDisplay (NULL);
    if (!dpy)
        exit(EXIT_FAILURE);
    XGetInputFocus (dpy, &focuswin, (int[1]){});
    root = XDefaultRootWindow(dpy);
    if(root == focuswin)
        return None;

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

static long pid_of_window(Window win)
{
    Atom nameAtom = XInternAtom(dpy, "_NET_WM_PID", 1);
    Atom type;
    int format, status;
    long pid = -1;
    unsigned long nitems, after;
    unsigned char *data;

    status = XGetWindowProperty(dpy, win, nameAtom, 0, 1024, 0,
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

static char* window_strings(Window win, long unsigned int *size, char *hint)
{
    Atom nameAtom = XInternAtom(dpy, hint, 1);
    Atom type;
    int format;
    unsigned int i;
    unsigned long after;
    unsigned char *data = 0;
    char *ret = NULL;

    if (XGetWindowProperty(dpy, win, nameAtom, 0, 1024, 0, AnyPropertyType,
                &type, &format, size, &after, &data) == Success) {
        if (data) {
            if (type == XA_STRING) {
                ret = malloc(sizeof(char) * *size);
                LOG("%s = ", hint);
                for (i = 0; i < *size; ++i) {
                    LOG("%c", data[i] == 0 ? ' ' : data[i]);
                    ret[i] = data[i];
                }
                LOG("%s", "\n");
            }
            XFree(data);
        }
        else
            LOG("%s not found\n", hint);
    }
    return ret;
}

static void processes_free(struct proc_list * p)
{
    free(p->ps);
    free(p);
}

static struct proc_list * processes_list(void)
{
    struct proc_list * p = NULL;
#ifdef LINUX
    glob_t globbuf;
    unsigned int i, j, k;
    char line[201] = {0};

    glob("/proc/[0-9]*", GLOB_NOSORT, NULL, &globbuf);
    p = malloc(sizeof(struct proc_list));
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
        if (fread(line, 200, 1, tn) == 0)
            continue;
        p->ps[j].pid = atoi(strtok(line, " "));
        k = snprintf(p->ps[j].name, 32, "%s", strtok(NULL, " ") + 1);
        p->ps[j].name[k - 1] = 0;
        strtok(NULL, " "); // discard process state
        p->ps[j].ppid = atoi(strtok(NULL, " "));
        LOG("\t%-20s\tpid=%6ld\tppid=%6ld\n", p->ps[j].name, p->ps[j].pid,
                p->ps[j].ppid);
        fclose(tn);
        j++;
    }
    p->n = j;
    globfree(&globbuf);
#endif
#ifdef BSD
    unsigned int count;
    p = malloc(sizeof(struct proc_list));
    struct kinfo_proc *kp;
    size_t len = 0;
    int name[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0 };
    sysctl(name, 4, NULL, &len, NULL, 0);
    kp = malloc(len);
    sysctl(name, 4, kp, &len, NULL, 0);
    count = len / sizeof(*kp);
    p->ps = calloc(count, sizeof(struct proc_s));
    p->n = count;

    unsigned int i;
    for(i = 0; i < count; ++i) {
        struct kinfo_file *files, *kif;
        int cnt, j;
        if(kp[i].ki_fd == NULL || kp[i].ki_pid == 0)
            continue;
        files = kinfo_getfile(kp[i].ki_pid, &cnt);
        for(j = 0; j < cnt; ++j) {
            kif = &files[j];
            if(kif->kf_fd != KF_FD_TYPE_CWD)
                continue;
            p->ps[i].pid = kp[i].ki_pid;
            p->ps[i].ppid = kp[i].ki_ppid;
            strncpy(p->ps[i].name, kp[i].ki_tdname, 32);
            strncpy(p->ps[i].cwd, kif->kf_path, MAXPATHLEN);
            LOG("\t%-20s\tpid=%6ld\tppid=%6ld\n", p->ps[i].name, p->ps[i].pid,
                p->ps[i].ppid);
        }

    }
    free(kp);
#endif
    return p;
}

static int read_process_path(struct proc_s *proc)
{
#ifdef LINUX
    char buf[255];
    char path[64];
    ssize_t len;

    snprintf(path, sizeof(path), "/proc/%ld/cwd", proc->pid);
    if ((len = readlink(path, buf, 255)) != -1)
        buf[len] = '\0';
    if(len <= 0) {
        LOG("Error readlink %s\n", path);
        return 0;
    }
    LOG("Read %s\n", path);
    if(access(buf, F_OK))
        return 0;
    fprintf(stdout, "%s\n", buf);
#endif
#ifdef BSD
    if(!strlen(proc->cwd)) {
        LOG("%ld cwd is empty\n", proc->pid);
        return 0;
    }
    if(access(proc->cwd, F_OK))
        return 0;
    fprintf(stdout, "%s\n", proc->cwd);
#endif
    return 1;
}

static int child_cwd(struct proc_list * p, struct proc_s *key)
{
    int i;
    struct proc_s *res =  (struct proc_s *)bsearch(key, p->ps, p->n,
                sizeof(struct proc_s), process_ppid_cmp);
    struct proc_s new_key = { .pid = key->ppid };

    if (res == NULL) {
        LOG("--> %ld pid\n", key->pid);
        return read_process_path(key);
    }
    for(i = 0; res != p->ps && (res - i)->ppid == res->ppid; ++i) {
        new_key.ppid = (res - i)->pid;
        LOG("--> %ld ppid\n", new_key.ppid);
        if(child_cwd(p, &new_key))
            return 1;
    }
    for(i = 1; res != p->ps + p->n && (res + i)->ppid == res->ppid; ++i) {
        new_key.ppid = (res + i)->pid;
        LOG("--> %ld ppid\n", new_key.ppid);
        if(child_cwd(p, &new_key))
            return 1;
    }
    return read_process_path(key);
}

static int deepest_child_cwd(struct proc_list * p, long pid)
{
    struct proc_s key = { .pid = pid, .ppid = pid};

    return child_cwd(p, &key);
}

int get_home_dir(void)
{
    LOG("%s", "getenv $HOME...\n");
    fprintf(stdout, "%s\n", getenv("HOME"));
    return EXIT_FAILURE;
}

int main(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;

    struct proc_list * p;
    long pid;
    int ret = EXIT_SUCCESS;
    Window w = focused_window();
    if (w == None)
        return get_home_dir();

    pid = pid_of_window(w);
    p = processes_list();
    if(!p)
        return get_home_dir();
    if(pid != -1)
        qsort(p->ps, p->n, sizeof(struct proc_s), process_ppid_cmp);
    else {
        long unsigned int size;
        unsigned int i;
        char* strings;
        struct proc_s *res = NULL, key;

        qsort(p->ps, p->n, sizeof(struct proc_s), process_name_cmp);
        strings = window_strings(w, &size, "WM_CLASS");
        for(i = 0; i < size; i += strlen(strings + i) + 1) {
            LOG("pidof %s\n", strings + i);
            strcpy(key.name, strings + i);
            res = (struct proc_s *)bsearch(&key, p->ps, p->n,
                    sizeof(struct proc_s), process_name_cmp);
            if(res)
                break;
        }
        if (res) {
            pid = res->pid;
            LOG("Found %s (%ld)\n", res->name, res->pid);
        }
        if (size)
            free(strings);
    }
    if (pid == -1 || !deepest_child_cwd(p, pid))
        ret = get_home_dir();
    processes_free(p);
    return ret;
}


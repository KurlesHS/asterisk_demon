#ifndef EPOOLHELPER_H
#define EPOOLHELPER_H

#include <string>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <map>

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))
#define MAX_EVENTS 64

using namespace std;
typedef void (*monitorCB)(string filename);
struct wdAndFilename
{
    int wd;
    string filename;
};

class monitorActivity
{
public:
    monitorActivity();
    ~monitorActivity();
    bool addPathToMonitor(const string &path, monitorCB cb);
    bool processMointor();
    bool isCreated();

private:
    int epfd;
    int infd;
    bool epollIsCreatedAndInicialized;
    struct epoll_event events[MAX_EVENTS];
    char buf[BUF_LEN];
    map<int, monitorCB> directoryToMonitor;
};

#endif // EPOOLHELPER_H

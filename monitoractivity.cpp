#include "monitoractivity.h"
#include <sys/types.h>
#include <sys/inotify.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <iostream>
monitorActivity::monitorActivity() : epfd(-1)
{
    // создаем epoll и inotify
    epfd = epoll_create (5);
    infd = inotify_init();
    epoll_event event;
    event.data.fd = infd;
    event.events = EPOLLIN | EPOLLET;
    // добавляем дескриптор inotify для мониторинга
    int res = epoll_ctl (epfd, EPOLL_CTL_ADD, infd, &event);
    epollIsCreatedAndInicialized = epfd >= 0 && infd >= 0 && res >= 0;


}

monitorActivity::~monitorActivity()
{
    // закрываем
    if (infd >= 0)
        close(infd);
    if (epfd >= 0)
        close(epfd);
}

bool monitorActivity::addPathToMonitor(const string &path, monitorCB cb)
{
    int wd = inotify_add_watch(infd, path.c_str(), IN_MODIFY);
    if (wd < 0)
        return false;
    directoryToMonitor[wd] = cb;
    return true;
}

bool monitorActivity::processMointor()
{
    int n = epoll_wait(epfd, events, MAX_EVENTS, 0);
    cout << "epoll events: " << n << endl;
    if (n <= 0)
        return false;
    for (int i = 0; i < n; ++i)
    {
        if ((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP) ||
                (!(events[i].events & EPOLLIN)))
        {
            /* произошла ошибка с этим файловым дескриптором,
             * почему то он не готов для чтения. Закрываем его от греха подальше */
            close (events[i].data.fd);

            continue;
        }
        // теперь пытаемся прочитать, что нам хочет сазать inotify
        int numRead = read(infd, buf, BUF_LEN);
        vector<wdAndFilename> listOfWd;
        for (char *p = buf; p < buf + numRead; ) {
            inotify_event *event = (struct inotify_event *) p;
            p += sizeof(struct inotify_event) + event->len;
            // skip directories
            if (event->mask & IN_ISDIR)
                continue;
            string filename = event->name;
            int wd = event->wd;

            // проверяем, не было ли уже в данном случае события модификации
            // файла для предотвращения избыточного вызова функции обратного вызова
            bool found = false;
            for (vector<wdAndFilename>::iterator vit = listOfWd.begin(); vit != listOfWd.end(); ++vit)
                if (vit->wd == wd && vit->filename == filename)
                {
                    found = true;
                    break;
                }
            if (found)
                continue;
            wdAndFilename wf;
            wf.filename = filename;
            wf.wd = wd;
            listOfWd.push_back(wf);
            map<int, monitorCB>::iterator it = directoryToMonitor.find(wd);
            // если нет данной директории для мониторинга в нашем списке
            // переходим к следующему событию
            if (it == directoryToMonitor.end())
                continue;

            // запускаем колбэк функцию
            (*it->second)(filename);

        }
    }
    return true;
}

bool monitorActivity::isCreated()
{
    return (epollIsCreatedAndInicialized);
}

#define BUFFERMAXLENGHT 4064
#include <iostream>
#include <string.h>

using namespace std;

#include <netlink/socket.h>
#include <netlink/socket_group.h>
#include <netlink/smart_buffer.h>
#include "tinyxml2/tinyxml2.h"
#include <time.h>

using namespace tinyxml2;

#include <map>
#include <list>
#include "md5.h"
enum notyfyStatys
{
    notStartNotified,
    notiyfyInProcess,
    notifiyIsFailed,
    notified,
    notificationConfirmed
};

enum serverState
{
    waitQuery,
    waitAdditionalData
};
const int timeoutWaitData = 1; // в секундах
const unsigned SERVER_PORT = 8080;
char buffer[BUFFERMAXLENGHT];

struct notyfied {
    notyfyStatys status;
    time_t statusChangedTime;
    string chanel;
    string filename;
};

struct socketConnectionInformation
{
    serverState currentCerverState;
    NL::SmartBuffer buffer;
    time_t changeStateTime;
};

struct serverInformation
{
    map<NL::Socket*, socketConnectionInformation*> sockets;
};

class OnAccept: public NL::SocketGroupCmd {

    void exec(NL::Socket* socket, NL::SocketGroup* group, void* reference) {

        NL::Socket* newConnection = socket->accept();

        socketConnectionInformation *sci = new socketConnectionInformation;
        sci->currentCerverState = waitQuery;
        sci->changeStateTime = time(NULL);
        sci->buffer.clear();
        serverInformation *si = static_cast<serverInformation*>(reference);
        if (si)
        {
            si->sockets[socket] = sci;
        }
        group->add(newConnection);
        cout << "\nConnection " << newConnection->hostTo() << ":" << newConnection->portTo() << " added...";
        cout.flush();
    }
};


class OnRead: public NL::SocketGroupCmd {

    void exec(NL::Socket* socket, NL::SocketGroup* group, void* reference) {
        cout << "\nREAD -- ";
        serverInformation *si = static_cast<serverInformation*>(reference);
        if (si)
        {
            si->sockets[socket]->currentCerverState = waitAdditionalData;
            si->sockets[socket]->changeStateTime = time(NULL);
            // читаем данные
            si->sockets[socket]->buffer.read(socket);
        }

    }
};


class OnDisconnect: public NL::SocketGroupCmd {

    void exec(NL::Socket* socket, NL::SocketGroup* group, void* reference) {

        serverInformation *si = static_cast<serverInformation*>(reference);
        if (si)
        {
           //удаляем данные по сокету
            socketConnectionInformation *sci = si->sockets[socket];
            si->sockets.erase(socket);
            if (sci)
                delete sci;
        }
        group->remove(socket);
        cout << "\nClient " << socket->hostTo() << " disconnected...";
        cout.flush();
        delete socket;
    }
};

int main() {

    timespec t1, t2;
    clockid_t clkId;
    clkId = CLOCK_PROCESS_CPUTIME_ID;
    clock_gettime(clkId, &t1);
    time_t _time = time(NULL);
    tm *dateTime = localtime(&_time);

    XMLDocument doc;
    if (doc.LoadFile("/home/alexey/xml.xml") == XML_NO_ERROR)
    {
        cout << "XML is loaded\n";
    } else
        cout << "Error while loading XML\n";

    XMLElement *node = doc.RootElement();

    while (node) {

        cout << node->Value() << " => "  << (node->GetText() ? node->GetText() : "ups") << endl;
        XMLElement *tempnode = node->NextSiblingElement();
        if (!tempnode)
            node = node->FirstChildElement();
        else
            node = tempnode;

    }

    //документ загружен, даже прошел валидацию можно парсить
    XMLElement *element = doc.RootElement();
    cout << strcmp(element->Value(), "body") << endl;
    bool parseSucces = true;
    string id, command, callerId, repeatCount, retryDelay;
    list<string> recepeterList;
    if (strcmp(element->Value(), "body") == 0)
    {
        // наше тело
        XMLElement *childElement = element->FirstChildElement("id");
        if (childElement)
            id.append(childElement->GetText());
        else
            parseSucces = false;
        childElement = element->FirstChildElement("command");
        if (childElement)
            command.append(childElement->GetText());
        else
            parseSucces = false;
        element = element->FirstChildElement("data");
        if (element)
        {
            // вытаскиваем данные
            childElement = element->FirstChildElement("callerId");
            if (childElement)
                callerId.append(childElement->GetText());
            else
                parseSucces = false;

            childElement = element->FirstChildElement("repeatCount");
            if (childElement)
                repeatCount.append(childElement->GetText());
            else
                parseSucces = false;

            childElement = element->FirstChildElement("retryDelay");
            if (childElement)
                retryDelay.append(childElement->GetText());
            else
                parseSucces = false;

            childElement = element->FirstChildElement("recepeterLists");
            if (childElement)
            {
                // вытаскиваем оповещаемых
                childElement = childElement->FirstChildElement("recepeter");
                while (childElement)
                {
                    string r(childElement->GetText());
                    recepeterList.push_back(r);
                    childElement = childElement->NextSiblingElement();
                }
            }


        }
    }

    cout << id.c_str() << " " << command << " " << callerId.c_str() << " " << recepeterList.size() << endl;

    clock_gettime(clkId, &t2);
    time_t _time2 = time(NULL);
    cout << "time in nanosec: " << t1.tv_nsec << " : " << t2.tv_nsec << " : " << (t2.tv_nsec + (_time2 - _time) * 1000000000) - t1.tv_nsec << " " << t1.tv_sec <<endl;
    cout << dateTime->tm_hour<<":"<<dateTime->tm_min<<":"<<dateTime->tm_sec<<endl;
    return 0;

    NL::init();
    memset(buffer, 0, BUFFERMAXLENGHT * sizeof(char));

    cout << "\nStarting Server...";
    cout.flush();

    NL::Socket socketServer(SERVER_PORT);

    NL::SocketGroup group;

    OnAccept onAccept;
    OnRead onRead;
    OnDisconnect onDisconnect;

    group.setCmdOnAccept(&onAccept);
    group.setCmdOnRead(&onRead);
    group.setCmdOnDisconnect(&onDisconnect);

    group.add(&socketServer);
    serverInformation *si = new serverInformation;
    while(true) {

        if(!group.listen(1000, static_cast<void*>(si)))
        {
            // проверяем, сколько сокетов находится в состоянии ожидании дополнительных данных
            map<NL::Socket*, socketConnectionInformation*>::iterator it;
            for ( it=si->sockets.begin() ; it != si->sockets.end(); it++ )
            {
                // first = key, second = value
                socketConnectionInformation *sci = it->second;
                NL::Socket *socket = it->first;
                cout << it->first << " => " << it->second << endl;
                if (sci->currentCerverState == waitAdditionalData)
                {
                    // проверяем таймаут
                    if ((time(NULL) - sci->changeStateTime) > timeoutWaitData)
                    {

                        // здесь мы уже все таймауты выдержали, можно парсить ответ
                        XMLDocument doc;
                        bool parseSucces = false;
                        if (doc.LoadFile(static_cast<const char*>(sci->buffer.buffer()), sci->buffer.size()))
                        {
                            //документ загружен, даже прошел валидацию можно парсить
                            XMLElement *element = doc.RootElement();
                            parseSucces = true;
                            string id, command;
                            if (strcmp(element->Value(), "body"))
                            {
                                // наше тело
                                XMLElement *childElement = element->NextSiblingElement("id");
                                if (childElement)
                                    id.append(childElement->GetText());
                                else
                                    parseSucces = false;
                                childElement = element->NextSiblingElement("command");
                                if (childElement)
                                    command.append(childElement->GetText());
                                else
                                    parseSucces = false;
                                element = element->FirstChildElement("data");
                                if (element)
                                {
                                    // вытаскиваем данные

                                }
                            }


                            // послали ответ, что всё хорошо, и мы поехали работать
                            socket->send("asdasdasd", 12);
                        } else
                        {
                            socket->disconnect();
                        }
                        sci->changeStateTime = time(NULL);
                        sci->currentCerverState = waitQuery;
                        sci->buffer.clear();
                        char tmpdir[256];
                        memset(tmpdir, 0, 256);


                    }
                }
            }
        }
    }
    delete si;
}

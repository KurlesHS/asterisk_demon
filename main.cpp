#include "main.h"
#include <cstdio>
#define BUFFERMAXLENGHT 4064


using namespace tinyxml2;

string sd;

const int timeoutWaitData = 1; // в секундах
const unsigned SERVER_PORT = 8080;
char buffer[BUFFERMAXLENGHT];

class OnAccept: public NL::SocketGroupCmd {

    void exec(NL::Socket* socket, NL::SocketGroup* group, void* reference) {

        NL::Socket* newConnection = socket->accept();
        cout << "Socket on connect: " << newConnection << endl;
        socketConnectionInformation *sci = new socketConnectionInformation;
        sci->currentCerverState = waitQuery;
        sci->changeStateTime = time(NULL);
        sci->buffer.clear();
        serverInformation *si = static_cast<serverInformation*>(reference);
        if (si)
        {
            si->sockets[newConnection] = sci;
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
            cout << "Socket on disconnect: " << socket << endl;
            socketConnectionInformation *sci = si->sockets[socket];
            si->sockets.erase(socket);
            if (sci)
                delete sci;
        }
        group->remove(socket);
        cout << "\nClient " << socket->hostTo() << " disconnected...\n";
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




    clock_gettime(clkId, &t2);
    time_t _time2 = time(NULL);
    cout << "time in nanosec: " << t1.tv_nsec << " : " << t2.tv_nsec << " : " << (t2.tv_nsec + (_time2 - _time) * 1000000000) - t1.tv_nsec << " " << t1.tv_sec <<endl;
    cout << dateTime->tm_hour<<":"<<dateTime->tm_min<<":"<<dateTime->tm_sec<<endl;

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
    si = new serverInformation;
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
                cout << sci << " => " << socket << endl;
                if (sci->currentCerverState == waitAdditionalData)
                {
                    // проверяем таймаут
                    if ((time(NULL) - sci->changeStateTime) > timeoutWaitData)
                    {
                        // здесь мы уже все таймауты выдержали, можно парсить ответ
                        XMLDocument doc;
                        bool parseSucces = false;
                        //cout << static_cast<const char*>(sci->buffer.buffer());
                        cout << sci->buffer.size() << " buffer size\n";
                        if (doc.LoadFile(static_cast<const char*>(sci->buffer.buffer()), sci->buffer.size()) == XML_NO_ERROR)
                        {
                            //документ загружен, даже прошел валидацию можно парсить
                            XMLElement *element = doc.RootElement();
                            cout << strcmp(element->Value(), "body") << endl;
                            string id, command;

                            // список команд, записываемых в call файл
                            map<string, string> callCommands;

                            // список оповещаемых
                            list<string> recepeterList;

                            if (strcmp(element->Value(), "body") == 0)
                            {
                                // наше тело
                                parseSucces = true;
                                XMLElement *childElement = element->FirstChildElement("Id");
                                if (childElement)
                                    id.append(childElement->GetText());
                                else
                                    parseSucces = false;
                                childElement = element->FirstChildElement("Command");
                                if (childElement)
                                    command.append(childElement->GetText());
                                else
                                    parseSucces = false;
                                element = element->FirstChildElement("Data");
                                if (element)
                                {
                                    // вытаскиваем данные

                                    // для начала список оповещаемых
                                    childElement = element->FirstChildElement("RecipientList");
                                    if (childElement)
                                    {
                                        childElement = childElement->FirstChildElement("Recipient");
                                        while (childElement)
                                        {
                                            string r(childElement->GetText());
                                            recepeterList.push_back(r);
                                            childElement = childElement->NextSiblingElement();
                                        }
                                    }
                                }

                                childElement = element->FirstChildElement();
                                while (childElement)
                                {
                                    // пропускаем список оповещаемых и элементы без текста
                                    if (childElement->GetText() && strcmp(childElement->Value(), "recepeterLists") != 0)
                                        callCommands[childElement->Value()] = childElement->GetText();
                                    childElement = childElement->NextSiblingElement();
                                }
                            }
                            if (recepeterList.size() > 0)
                            {
                                char md5[33];
                                si->callParametrs[id] = callCommands;
                                list<string>::iterator it;
                                // запёхиваем оповещаемых во внутреннюю базу
                                for (it = recepeterList.begin(); it != recepeterList.end(); ++it)
                                {
                                    //генерируем уникальное имя файла
                                    string tmp = id + *it;
                                    md5Compute(reinterpret_cast<const unsigned char*>(tmp.c_str()), tmp.size(), md5);
                                    string hash = md5 + 16;

                                    // экземпляр информации об абоненте
                                    notyfied n;

                                    // куда звонить
                                    n.chanel = *it;
                                    // имя файла
                                    n.filename = hash + ".call";
                                    // статус - не начато оповещение данного абонента
                                    n.status = notStartNotified;
                                    // текущее время
                                    n.statusChangedTime = time(NULL);
                                    // идентификатор запроса
                                    n.idQuery = id;
                                    si->abonentsToNotify.push_back(n);
                                }
                                generateCallFiles();
                            }


                            // послали ответ, что всё хорошо, и мы поехали работать
                            socket->send("susces", 7);
                        } else
                        {
                            socket->send("error", 6);
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

void generateCallFiles()
{

    int numFile = maxFilesInProcess - filesInProcess.size();
    // проверка на максимальное
    if (numFile > 0)
    {
        char md5[33];
        string tmpDir = "/tmp/";
        sprintf(buffer,"%ld",time(NULL));
        md5Compute(reinterpret_cast<unsigned char*>(buffer), strlen(buffer), md5);
        tmpDir.append("astdem.").append(md5 + 24);
        list<notyfied>::iterator lit;
        bool dirIsCreated = false;
        serverInformation *si2 = si;
        bool allRemoved = true;
        for (lit = si->abonentsToNotify.begin(); lit != si->abonentsToNotify.end(); ++lit)
        {
            if (lit->status == notStartNotified)
            {
                // нашли еще не оповещенного абонента
                if (numFile <= 0)
                    break;
                if (!dirIsCreated)
                {
                    mkdir(tmpDir.c_str(), 0777);
                    dirIsCreated = true;
                }
                string filePath = tmpDir + "/" + lit->filename;
                FILE * pFile;
                 pFile = fopen (filePath.c_str(),"w");

                 // something wrong
                 if (pFile == NULL)
                     continue;
                 --numFile;
                 string line = "Channel: " + lit->chanel + "\n";
                 fputs (line.c_str(), pFile);
                 // параметры звонка
                 map<string, string>::iterator it;
                 for (it = si->callParametrs[lit->idQuery].begin(); it != si->callParametrs[lit->idQuery].end(); ++it)
                 {
                     string line = it->first + ": " + it->second + "\n";
                     fputs (line.c_str(), pFile);
                 }
                 fclose (pFile);
                 string newFilePath = asteriskWorkPath + lit->filename;
                 if (rename(filePath.c_str(), newFilePath.c_str()) != 0)
                     allRemoved = false;

            }
        }
        if (allRemoved)
           rmdir(tmpDir.c_str());
        else
        {
            // мб в ручную удалить файлы?
            // пока - так же удаялем
            rmdir(tmpDir.c_str());
        }
    }
}

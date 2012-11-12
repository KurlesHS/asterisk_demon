#include "regexp/Matcher.h"
#include "regexp/Pattern.h"
#include "main.h"
#include <locale>
#include <cstdio>
#include <stdio.h>
#include <algorithm>
#include <monitoractivity.h>
#include <sys/stat.h>
#include <dirent.h>
#define BUFFERMAXLENGHT 4064


using namespace tinyxml2;

string sd;

const int timeoutWaitData = 1; // в секундах
unsigned SERVER_PORT = 3465;
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

    void exec(NL::Socket* socket, NL::SocketGroup* , void* reference) {

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

bool chkPath(string &path, const int &type)
{
    int res = access(path.c_str(), type);
    if (res != 0)
    {
        cout << "not have access to the directory \"" << path.c_str()<< "\"\n";
        return 0;
    }
    if (path.at(path.length() - 1) != '/')
        path.append("/");
    return true;
}


void logDirectoryActivity(string filename)
{
    static const string months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    static long int currentPos = 0;
    cout << "файл в log directory " << filename.c_str() << " изменен!\n";
    if (filename != "messages")
        return;
    string filepath = asteriskLogPath + filename;
    FILE *f = ::fopen(filepath.c_str(), "r");
    if (f == NULL)
        return;
    if (fseek (f, currentPos, SEEK_SET) != 0)
    {
        currentPos = 0;
        fseek(f, currentPos, SEEK_SET);
    }
    Pattern * pat = Pattern::compile("^\\[(\\w{3})\\s+(\\d+)\\s+(\\d+):(\\d+):(\\d+)\\]\\s+NOTICE\\[\\d+\\].+\\'(.+)\\'\\s+is\\s+now\\s+(\\w+)");
    int numl = 0;
    bool firstLine = true;
    while (!feof(f))
    {
        if (fgets(buffer, BUFFERMAXLENGHT, f))
        {
            firstLine = false;
            string str(buffer);
            Matcher *mat = pat->createMatcher(str);
            if (!mat)
                continue;
            ++numl;

            mat->matches();
            while (mat->findNextMatch())
            {
                chanelInfo ci;
                string month = mat->getGroup(1);
                string day = mat->getGroup(2);
                string hour = mat->getGroup(3);
                string minute = mat->getGroup(4);
                string seconds = mat->getGroup(5);
                string chanel = mat->getGroup(6);
                string status = mat->getGroup(7);
                time_t t= time(NULL);

                tm *currentDateTime = localtime(&t);
                currentDateTime->tm_hour = atoi(hour.c_str());
                currentDateTime->tm_min = atoi(minute.c_str());
                currentDateTime->tm_sec = atoi(seconds.c_str());
                currentDateTime->tm_mday = atoi(day.c_str());
                currentDateTime->tm_wday = 0;
                currentDateTime->tm_yday = 0;
                currentDateTime->tm_mon = 1;

                for (int i = 0; i < 12; ++i)
                {
                    if (months[i] == month)
                    {
                        currentDateTime->tm_mon = i + 1;
                        break;
                    }
                }
                mktime(currentDateTime);
                toLoverCase(status);
                chanelState state;
                if (status == "lagged")
                    state = chanelLaggedState;
                else if (status == "reachable")
                    state = chanelReachableState;
                else if (status == "unreachable")
                    state = chanelUnreachableState;
                else
                    state = chanelUnknowState;
                memcpy(&ci.time, currentDateTime, sizeof(tm));
                ci.state = state;
                chanelInfos[chanel] = ci;

                cout << chanel << ": " << status << " " << ci.state << endl;
            }
            delete mat;
            continue;
        } else
        {
            if (firstLine)
                fseek(f, 0, SEEK_SET);
            firstLine = false;
        }
    }
    delete pat;
    cout << "num lines readed: " << numl << endl;
    currentPos = ftell(f);
    fclose(f);
}

void outgoingDoneDirectoryActivity(string filename)
{
    cout << "файл outgoing_done " << filename.c_str() << " изменен!\n";

    size_t pos = filename.find(".call");
    if (pos == string::npos || filename.length() - pos != 5)
    {
        cout << "non *.call file\n";
        return;
    }
    string filepath = asteriskOutgoingDonePath  + filename;
    FILE *f = ::fopen(filepath.c_str(), "r");
    if (f == NULL)
        return;

    // TODO: стоит избавиться от регекспа для такого простого случая?
    Pattern * pat = Pattern::compile("\\bStatus:\\s*(\\w+)", Pattern::MULTILINE_MATCHING);
    while (!feof(f))
    {
        if (fgets(buffer, BUFFERMAXLENGHT, f))
        {

            string str(buffer);
            Matcher *mat = pat->createMatcher(str);
            if (!mat)
                continue;
            mat->matches();
            if (!mat->findFirstMatch())
            {
                delete mat;
                continue;
            }
            generateCallFiles();
            string status = mat->getGroup(1);
            cout << "status: " << status << endl;

            delete mat;
            vector<notyfied>::iterator lit;
            // ищем абонента
            for (lit = si->abonentsToNotify.begin(); lit != si->abonentsToNotify.end(); ++lit)
            {
                // нашли
                if (lit->filename == filename)
                {
                    lit->statusChangedTime = time(NULL);
                    if (lit->status == notiyfyInProcess)
                        lit->status = status == "completed" ? notifiedAbonent : notifiyIsFailed;
                    break;
                }
            }
            break;
        }
    }
    delete pat;
    fclose(f);
}

int main(int argc, char** argv)
{
    //FILE *f = ::fopen("/home/alexey/asterisk_log/test.txt", "r");

    /*
    string test = "HeLLo!";
    cout << test;
    toLoverCase(test);
    cout <<" " << test << endl;

    timespec t1, t2;
    clockid_t clkId;
    clkId = CLOCK_PROCESS_CPUTIME_ID;
    clock_gettime(clkId, &t1);
    time_t _time = time(NULL);
    tm *dateTime = localtime(&_time);


    clock_gettime(clkId, &t2);
    time_t _time2 = time(NULL);
    cout << "time in nanosec: " << t1.tv_nsec << " : " << t2.tv_nsec << " : " << (t2.tv_nsec + (_time2 - _time) * 1000000000) - t1.tv_nsec << " " << t1.tv_sec <<endl;
    cout << dateTime->tm_hour<<":"<<dateTime->tm_min<<":"<<dateTime->tm_sec<<endl;
*/
    int status;
    int pid;

    // если параметров командной строки меньше двух, то покажем как использовать демона
    if (argc < 6)
    {
        cout << "usage: asterisk_demon /path/to/tmp /path/to/outgoing path/to/outgoing_done /path/to/log portNumber\n";
        return 0;
    }

    // берём пути для работы
    asterislTempPath = argv[1];
    asteriskOutgoingPath = argv[2];
    asteriskOutgoingDonePath = argv[3];
    asteriskLogPath = argv[4];
    int c = atoi(argv[5]);
    if (c <= 0 || c > 65535)
    {
        // порт за пределами допустимых значений
        cout << "not valid port value\n";
        return -1;
    }
    SERVER_PORT = c;


    // проверяем их на доступность
    if (!chkPath(asteriskLogPath, R_OK))
        return -1;
    if (!chkPath(asteriskOutgoingPath, R_OK | W_OK))
        return -1;
    if (!chkPath(asteriskOutgoingDonePath, R_OK))
        return -1;
    if (!chkPath(asterislTempPath, R_OK | W_OK))
        return -1;

    // ключик для дебага
    {
        // если присутствует, то запускаем как обычную программу
        if (strcmp(argv[6], "-d") == 0)
            cout << "Debug session\n";
        return monitorProc();
    }
    // здесь - если не дебаг. Запускаем как демона



    // загружаем файл конфигурации
    /*
    status = LoadConfig(argv[1]);

    if (!status) // если произошла ошибка загрузки конфига
    {
        printf("Error: Load config failed\n");
        return -1;
    }
    */
    // создаем потомка
    pid = fork();

    if (pid == -1) // если не удалось запустить потомка
    {
        // выведем на экран ошибку и её описание
        printf("Error: Start Daemon failed (%s)\n", strerror(errno));

        return -1;
    }
    else if (!pid) // если это потомок
    {
        // данный код уже выполняется в процессе потомка
        // разрешаем выставлять все биты прав на создаваемые файлы,
        // иначе у нас могут быть проблемы с правами доступа
        umask(0);

        // создаём новый сеанс, чтобы не зависеть от родителя
        setsid();

        // переходим в корень диска, если мы этого не сделаем, то могут быть проблемы.
        // к примеру с размантированием дисков
        chdir("/");

        // закрываем дискрипторы ввода/вывода/ошибок, так как нам они больше не понадобятся
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        // Данная функция будет осуществлять слежение за процессом
        status = monitorProc();

        return status;
    }
    else // если это родитель
    {
        // завершим процес, т.к. основную свою задачу (запуск демона) мы выполнили
        return 0;
    }
    return 0;

}


int monitorProc() {


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

    // настраиваем epool
    monitorActivity epool;
    if (!epool.isCreated())
        return -1;

    epool.addPathToMonitor(asteriskOutgoingDonePath, outgoingDoneDirectoryActivity);
    epool.addPathToMonitor(asteriskLogPath, logDirectoryActivity);
    logDirectoryActivity("messages");

    while(true) {
        epool.processMointor();
        if(!group.listen(200, static_cast<void*>(si)))
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
                            vector<string> recepeterList;

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

                                toLoverCase(command);
                                if (parseSucces == false)
                                {
                                    sendResponse(socket, "Error");
                                } else if (command == "generatefiles")
                                {

                                    {
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
                                        vector<string>::iterator it;
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
                                        sendResponse(socket, "Ok", id.c_str());
                                    } else
                                    {
                                        sendResponse(socket, "EmptyRecepeterList", id.c_str());
                                    }
                                } else if (command == "queryresults")
                                {
                                    XMLDocument outDoc;
                                    XMLNode *body = outDoc.InsertFirstChild(outDoc.NewElement("body"));
                                    XMLNode *idElement;
                                    idElement = body->InsertFirstChild(outDoc.NewElement("Id"));
                                    idElement->InsertFirstChild(outDoc.NewText(id.c_str()));
                                    idElement = body->InsertEndChild(outDoc.NewElement("Status"));
                                    idElement->InsertFirstChild(outDoc.NewText("Ok"));
                                    idElement = body->InsertEndChild(outDoc.NewElement("Data"));
                                    idElement = idElement->InsertEndChild(outDoc.NewElement("RecipientList"));

                                    vector<notyfied>::iterator it;
                                    for (it = si->abonentsToNotify.begin(); it != si->abonentsToNotify.end();++it)
                                    {
                                        if (it->idQuery == id)
                                        {
                                            XMLElement *recipient = outDoc.NewElement("Recipient");
                                            tm *dateTime = localtime(&it->statusChangedTime);
                                            sprintf(buffer, "%02d.%02d.%02d %02d:%02d:%02d",
                                                    dateTime->tm_mday,
                                                    dateTime->tm_mon,
                                                    dateTime->tm_year % 100,
                                                    dateTime->tm_hour,
                                                    dateTime->tm_min,
                                                    dateTime->tm_sec
                                                    );

                                            recipient->SetAttribute("Time", buffer);
                                            recipient->InsertEndChild(outDoc.NewText(it->chanel.c_str()));
                                            string status;
                                            switch (it->status)
                                            {
                                            case notStartNotified:
                                                status = "notStartNotified";
                                                break;
                                            case notiyfyInProcess:
                                                status = "notiyfyInProcess";
                                                break;
                                            case notifiyIsFailed:
                                                status = "notifiyIsFailed";
                                                break;
                                            case notifiedAbonent:
                                                status = "notifiedAbonent";
                                                break;
                                            case notificationConfirmed:
                                                status = "notificationConfirmed";
                                                break;
                                            }
                                            recipient->SetAttribute("Status", status.c_str());
                                            idElement->InsertEndChild(recipient);
                                        }
                                    }
                                    XMLPrinter printer;
                                    outDoc.Print(&printer);
                                    socket->send(printer.CStr(), printer.CStrSize() - 1);
                                } else if (command == "querychanelsstate")
                                {
                                    XMLDocument outDoc;
                                    XMLNode *body = outDoc.InsertFirstChild(outDoc.NewElement("body"));
                                    XMLNode *idElement;
                                    idElement = body->InsertFirstChild(outDoc.NewElement("Id"));
                                    idElement->InsertFirstChild(outDoc.NewText(id.c_str()));
                                    idElement = body->InsertEndChild(outDoc.NewElement("Status"));
                                    idElement->InsertFirstChild(outDoc.NewText("Ok"));
                                    idElement = body->InsertEndChild(outDoc.NewElement("Data"));
                                    idElement = idElement->InsertEndChild(outDoc.NewElement("RecipientList"));
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
                                                childElement = childElement->NextSiblingElement();
                                                string status;
                                                tm _tm;
                                                map<string, chanelInfo>::iterator it = chanelInfos.find(r);
                                                XMLElement *recipient = outDoc.NewElement("Recipient");
                                                if (it != chanelInfos.end())
                                                {
                                                    memcpy(&_tm, &it->second.time, sizeof(tm));
                                                    switch(it->second.state)
                                                    {
                                                    case chanelUnknowState:
                                                        status = "chanelUnknowState";
                                                        break;
                                                    case chanelLaggedState:
                                                        status = "chanelLaggedState";
                                                        break;
                                                    case chanelReachableState:
                                                        status = "chanelReachableState";
                                                        break;
                                                    case chanelUnreachableState:
                                                        status = "chanelUnreachableState";
                                                        break;
                                                    }
                                                } else
                                                {
                                                    status = "chanelUnknowState";
                                                    time_t p_time = time(NULL);
                                                    tm *p_tm = localtime(&p_time);
                                                    memcpy(&_tm, p_tm, sizeof(tm));
                                                }
                                                sprintf(buffer, "%02d.%02d.%02d %02d:%02d:%02d",
                                                        _tm.tm_mday,
                                                        _tm.tm_mon,
                                                        _tm.tm_year % 100,
                                                        _tm.tm_hour,
                                                        _tm.tm_min,
                                                        _tm.tm_sec
                                                        );
                                                recipient->SetAttribute("Time", buffer);
                                                recipient->SetAttribute("Status", status.c_str());
                                                idElement->InsertEndChild(recipient);

                                                recipient->InsertEndChild(outDoc.NewText(r.c_str()));

                                            }
                                            XMLPrinter printer;
                                            outDoc.Print(&printer);
                                            socket->send(printer.CStr(), printer.CStrSize() - 1);
                                        } else
                                            sendResponse(socket, "Error", id.c_str());
                                    } else
                                    {
                                        // все каналы отображать
                                        childElement = childElement->NextSiblingElement();
                                        string status;
                                        map<string, chanelInfo>::iterator it;
                                        for (it = chanelInfos.begin(); it != chanelInfos.end(); ++it)
                                        {
                                            cout << it->first.c_str() << ": " << (int)it->second.state << endl;
                                            XMLElement *recipient = outDoc.NewElement("Recipient");
                                            switch(it->second.state)
                                            {
                                            case chanelUnknowState:
                                                status = "chanelUnknowState";
                                                break;
                                            case chanelLaggedState:
                                                status = "chanelLaggedState";
                                                break;
                                            case chanelReachableState:
                                                status = "chanelReachableState";
                                                break;
                                            case chanelUnreachableState:
                                                status = "chanelUnreachableState";
                                                break;
                                            }
                                            sprintf(buffer, "%02d.%02d.%02d %02d:%02d:%02d",
                                                    it->second.time.tm_mday,
                                                    it->second.time.tm_mon,
                                                    it->second.time.tm_year % 100,
                                                    it->second.time.tm_hour,
                                                    it->second.time.tm_min,
                                                    it->second.time.tm_sec
                                                    );
                                            recipient->SetAttribute("Time", buffer);
                                            recipient->SetAttribute("Status", status.c_str());
                                            idElement->InsertEndChild(recipient);
                                            recipient->InsertEndChild(outDoc.NewText(it->first.c_str()));
                                        }
                                        XMLPrinter printer;
                                        outDoc.Print(&printer);
                                        socket->send(printer.CStr(), printer.CStrSize() - 1);

                                    }


                                } else
                                {
                                    sendResponse(socket, "UnknowCommand", id.c_str());
                                }

                            }

                        } else
                        {
                            sendResponse(socket, "Error");
                        }
                        sci->changeStateTime = time(NULL);
                        sci->currentCerverState = waitQuery;
                        sci->buffer.clear();

                    }
                }
            }
        }
    }

    delete si;
}

void sendResponse (NL::Socket *socket, const string &status, const string &id)
{
    XMLDocument outDoc;
    XMLNode *body = outDoc.InsertFirstChild(outDoc.NewElement("body"));
    XMLNode *idElement;
    if (!id.empty())
    {
        idElement = body->InsertFirstChild(outDoc.NewElement("Id"));
        idElement->InsertFirstChild(outDoc.NewText(id.c_str()));
    }
    idElement = body->InsertEndChild(outDoc.NewElement("Status"));
    idElement->InsertFirstChild(outDoc.NewText(status.c_str()));
    XMLPrinter printer;
    outDoc.Print(&printer);
    socket->send(printer.CStr(), printer.CStrSize() - 1);
}

void generateCallFiles()
{
    int numFile = maxFilesInProcess - filesInProcess.size();
    // проверка на максимальное
    if (numFile > 0)
    {
        char md5[33];
        string tmpDir = asterislTempPath;
        sprintf(buffer,"%ld",time(NULL));
        md5Compute(reinterpret_cast<unsigned char*>(buffer), strlen(buffer), md5);
        tmpDir.append("astdem.").append(md5 + 24);
        vector<notyfied>::iterator lit;
        bool dirIsCreated = false;
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
                string newFilePath = asteriskOutgoingPath + lit->filename;
                if (rename(filePath.c_str(), newFilePath.c_str()) != 0)
                    allRemoved = false;
                lit->status = notiyfyInProcess;
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

void toLoverCase(string &str)
{
    std::transform(str.begin(), str.end(), str.begin(),
                   std::bind2nd(std::ptr_fun(&std::tolower<char>), std::locale("")));
}

#include "regexp/Matcher.h"
#include "regexp/Pattern.h"
#include "main.h"
#include <locale>
#include <cstdio>
#include <algorithm>
#include <monitoractivity.h>
#include <sys/stat.h>
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

void logDirectoryActivity(string filename)
{
    static long currentPos = 0;
    cout << "файл в log directory " << filename.c_str() << " изменен!\n";
    if (filename != "messages")
        return;
    string filepath = asteriskLogPath + filename;
    FILE *f = ::fopen(filepath.c_str(), "r");
    if (f == NULL)
        return;
    Pattern * pat = Pattern::compile("^\\[(\\w{3}) (\\d+) (\\d+):(\\d+):(\\d+)\\]\\s+NOTICE\\[\\d+\\].+\\'(.+)\\'\\s+is now\\s(\\w+)");
    while (!feof(f))
    {
        if (fgets(buffer, BUFFERMAXLENGHT, f))
        {
            string str(buffer);
            Matcher *mat = pat->createMatcher(str);
            if (!mat)
                continue;
            mat->matches();
            while (mat->findNextMatch())
            {
                string month = mat->getGroup(1);
                string day = mat->getGroup(2);
                string hour = mat->getGroup(3);
                string minute = mat->getGroup(4);
                string seconds = mat->getGroup(5);
                string chanel = mat->getGroup(6);
                string status = mat->getGroup(7);
                toLoverCase(status);

                if (status == "lagged")
                {

                } else if (status == "reachable")
                {

                } else if (status == "unreachable")
                {

                } else
                {

                }
            }
            delete mat;
            continue;


        }
    }
}

void outgoingDirectoryActivity(string filename)
{
    cout << "файл outgoing_done " << filename.c_str() << " изменен!\n";

    size_t pos = filename.find(".call");
    if (pos == string::npos || filename.length() - pos != 5)
    {
        cout << "non *.call file\n";
        return;
    }
    string filepath = asteriskOutgoingPath  + filename;
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

    int status;
    int pid;

    // если параметров командной строки меньше двух, то покажем как использовать демона
    if (argc == 2)
    {
        if (strcmp(argv[1], "-d") == 0)
            cout << "Debug session\n";
        return monitorProc();
    }

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

    epool.addPathToMonitor(asteriskOutgoingPath, outgoingDirectoryActivity);

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
        string tmpDir = "/tmp/";
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
                string newFilePath = asteriskIncomingPath + lit->filename;
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

#ifndef MAIN_H
#define MAIN_H

#include <map>
#include <vector>
#include <string>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>


#include "netlink/socket.h"
#include "netlink/socket_group.h"
#include "netlink/smart_buffer.h"
#include "tinyxml2/tinyxml2.h"
#include "md5.h"

using namespace std;

const int maxFilesInProcess = 10;
const string asteriskIncomingPath = "/home/alexey/asterisk_incoming/";
const string asteriskOutgoingPath = "/home/alexey/asterisk_outgoing/";
const string asteriskLogPath = "/home/alexey/asterisk_log/";


enum notyfyStatys
{
    notStartNotified,
    notiyfyInProcess,
    notifiyIsFailed,
    notifiedAbonent,
    notificationConfirmed
};

enum serverState
{
    waitQuery,
    waitAdditionalData
};

struct notyfied {
    notyfyStatys status;
    time_t statusChangedTime;
    string chanel;
    string filename;
    string idQuery;
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
    vector<notyfied> abonentsToNotify;
    map< string, map<string, string> > callParametrs;
};

int main();
void generateCallFiles();
void toLoverCase(string &str);

serverInformation *si;
vector<string> filesInProcess;


#endif // MAIN_H

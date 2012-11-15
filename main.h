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
static string asteriskOutgoingPath;
static string asteriskOutgoingDonePath;
static string asteriskLogPath;
static string asterislTempPath;

enum notyfyStatys
{
    notStartNotified,
    notiyfyInProcess,
    notifiyIsFailed,
    notifiedAbonent,
    notificationConfirmed
};

enum chanelState
{
    chanelUnknowState,
    chanelLaggedState,
    chanelReachableState,
    chanelUnreachableState
};

struct chanelInfo
{
    chanelState state;
    tm time;
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
int main(int argc, char** argv);
int monitorProc();
void generateCallFiles();
void toLoverCase(string &str);
void sendResponse (NL::Socket *socket, const string &status, const string &id = "", const string &command="");
void logDirectoryActivity(string filename);
void outgoingDoneDirectoryActivity(string filename);
bool chkPath(string &path, const int &type);
void parseLogFile(FILE *f);

serverInformation *si;
vector<string> filesInProcess;
map<string, chanelInfo> chanelInfos;



#endif // MAIN_H

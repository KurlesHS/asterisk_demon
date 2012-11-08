#define BUFFERMAXLENGHT 4064
#include <iostream>
#include <string.h>

using namespace std;

#include <netlink/socket.h>
#include <netlink/socket_group.h>
#include <netlink/smart_buffer.h>
#include "tinyxml/tinyxml.h"
#include <regex.h>
#include <map>
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
            if (si->sockets[socket]->currentCerverState == waitQuery)
                si->sockets[socket]->buffer.clear();
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

#define WORD unsigned short
#define BYTE char

WORD CRC16 (const BYTE *nData, WORD wLength)
{
static const WORD wCRCTable[] = {
   0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
   0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
   0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
   0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
   0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
   0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
   0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
   0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
   0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
   0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
   0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
   0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
   0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
   0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
   0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
   0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
   0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
   0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
   0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
   0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
   0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
   0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
   0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
   0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
   0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
   0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
   0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
   0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
   0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
   0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
   0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
   0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040 };

BYTE nTemp;
WORD wCRCWord = 0xFFFF;

   while (wLength--)
   {
      nTemp = *nData++ ^ wCRCWord;
      wCRCWord >>= 8;
      wCRCWord  ^= wCRCTable[nTemp];
   }
   return wCRCWord;

}

int main() {


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
                cout << it->first << " => " << it->second << endl;
                if (sci->currentCerverState == waitAdditionalData)
                {
                    if ((time(NULL) - sci->changeStateTime) > timeoutWaitData)
                    {
                        // здесь мы уже все таймауты выдержали, можно парсить ответ
                        TiXmlDocument doc;

                    }
                }

            }


        }
    }
    delete si;
}

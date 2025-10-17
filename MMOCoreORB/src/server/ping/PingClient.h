/*
                Copyright <SWGEmu>
        See file COPYING for copying conditions.
*/

#ifndef PINGCLIENT_H_
#define PINGCLIENT_H_

#include "server/network/DatagramServiceThread.h"
#include "server/network/BaseClientProxy.h"
#include "server/network/Socket.h"
#include "server/network/SocketAddress.h"
#include "engine/engine.h"

class PingClient : public BaseClientProxy {
public:
    PingClient(DatagramServiceThread* serv, Socket* sock, SocketAddress& addr)
        : BaseClientProxy(sock, addr) {
        // Use getIPAddress() instead of removed getFullIPAddress()
        setLoggingName("PingClient " + getIPAddress());
        setLogging(false);
        setLogLevel(Logger::FATAL);

        init(serv);
    }

    virtual ~PingClient() {
    }

    void disconnect(bool doLock = true) {
        if (isDisconnected())
            return;

        String time;
        Logger::getTime(time);

        StringBuffer msg;
        msg << time << " [PingServer] disconnecting client '" << getIPAddress() << "'\n";
        Logger::console.log(msg);

        BaseClientProxy::disconnect(doLock);
    }

    void sendMessage(Message* msg) {
        BaseClientProxy::sendPacket(cast<BasePacket*>(msg));
    }

    // Optional: accessors for convenience
    sys::lang::String getIPAddress() const {
        return BaseClientProxy::getIPAddress();
    }

    int getPort() const {
        return BaseClientProxy::getPort();
    }
};

#endif /* PINGCLIENT_H_ */

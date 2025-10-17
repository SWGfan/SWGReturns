/*
                Copyright <SWGEmu>
        See file COPYING for copying conditions.
*/

#ifndef PINGCLIENT_H_
#define PINGCLIENT_H_

#include "engine/engine.h"

class PingClient : public BaseClientProxy {
private:
    int port; // store port manually from SocketAddress

public:
    PingClient(DatagramServiceThread* serv, Socket* sock, SocketAddress& addr)
        : BaseClientProxy(sock, addr), port(addr.getPort()) {
        // Use getIPAddress() for logging
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

    // Accessor for IP address
    sys::lang::String getIPAddress() const {
        return BaseClientProxy::getIPAddress();
    }

    // Accessor for port (stored manually)
    int getPort() const {
        return port;
    }
};

#endif /* PINGCLIENT_H_ */

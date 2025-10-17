/*
                Copyright <SWGEmu>
        See file COPYING for copying conditions.*/

#ifndef PINGCLIENT_H_
#define PINGCLIENT_H_

#include "server/Socket.h"
#include "server/SocketAddress.h"
#include "server/DatagramServiceThread.h"
#include "server/ServiceHandler.h"
#include "server/BaseClientProxy.h"

class PingClient : public ServiceClient {
public:
    PingClient(BaseClientProxy* session)
        : ServiceClient(session) {
        // Use getIPAddress() which returns a sys::lang::String (or std::string equivalent)
        // If you want ip:port, you can use:
        // setLoggingName("PingClient " + getIPAddress() + ":" + String::valueOf(getPort()));
        setLoggingName("PingClient " + getIPAddress());
        setLogging(false);
    }

    virtual ~PingClient() {
    }

    // If your code expects these helpers on the client object, forward to underlying session
    sys::lang::String getIPAddress() const {
        return ServiceClient::getIPAddress();
    }

    int getPort() const {
        return ServiceClient::getPort();
    }

    // Other PingClient-specific methods can go here
};

#endif /* PINGCLIENT_H_ */

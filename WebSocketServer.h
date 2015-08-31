#include "WebSocketWritable.h"
#include "WebSocket.h"
#include <SPI.h>
#include <Ethernet.h>

#ifndef H_WEBSOCKETSERVER
#define H_WEBSOCKETSERVER

class WebSocketServer;
class InboundWebSocket : public WebSocket {
protected:
	friend class WebSocketServer;

	bool sendInboundHandshakeResponse( char *key );
	bool inboundHandshake();

	WebSocketServer	*m_server;

public:
	InboundWebSocket( WebSocketServer *server, EthernetClient cli );
	WebSocketServer *server() { return m_server; }
};

class WebSocketServer : public WebSocketWritable {
protected:
friend class WebSocket;
    // Callback functions definition.
    typedef void Callback(InboundWebSocket &socket, void *opaque);

    // Pointer to the callback function the user should provide
    Callback *onConnect;
    Callback *onDisconnect;

    void *m_connectOpaque;
    void *m_disconnectOpaque;

private:
    const char *m_server_urlPrefix;

    EthernetServer m_server;

    byte m_maxConnections;
    byte m_connectionCount;

    // Pointer array of client slots:
    InboundWebSocket **m_connections;

public:
    // Constructor.
    WebSocketServer(const char *urlPrefix = "/", int inPort = 80, byte maxConnections = 4, word maxFrameSize = 96);
    ~WebSocketServer();

    // Callbacks
    void registerConnectCallback(Callback *callback, void *opaque) { onConnect = callback; m_connectOpaque = opaque; }
    void registerDisconnectCallback(Callback *callback, void *opaque) { onDisconnect = callback; m_disconnectOpaque = opaque; }

    // Start listening for connections.
    void begin() { m_server.begin(); }

    // Main listener for incoming data. Should be called from the loop.
    void listen();

    // Connection count
    byte connectionCount() { return m_connectionCount; }

    // Broadcast to all connected clients.
    byte send(char *str, word length);
};

#endif

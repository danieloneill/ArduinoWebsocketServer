#include <Arduino.h> // Arduino 1.0 or greater is required
#include <stdlib.h>
#include <stdarg.h>

#include <SPI.h>
#include <Ethernet.h>

#include "WebSocketWritable.h"

#ifndef WEBSOCKET_H_
#define WEBSOCKET_H_

#ifndef htons
#define htons(x) ( ((x)<<8) | (((x)>>8)&0xFF) )
#endif

// CRLF characters to terminate lines/handshakes in headers.
#define CRLF "\r\n"

typedef struct {
    bool isMasked;
    bool isFinal;
    byte opcode;
    byte mask[4];
    word length;
    char *data;
} Frame;

// Frame.
extern Frame frame;

// Shared with WebSocketServer
extern word frameCapacity; // Maximum amount of data the frame can accept.
extern bool initialised;

class WebSocket : public WebSocketWritable {
public:
    typedef enum {DISCONNECTED=0, HANDSHAKE=1, CONNECTED=2} State;

protected:
    typedef void Callback(WebSocket &socket, void *opaque);
    typedef void DataCallback(WebSocket &socket, char *socketString, word frameLength, void *opaque);

    Callback *onConnect;
    Callback *onDisconnect;
    DataCallback *onData;

    void *m_connectOpaque;
    void *m_disconnectOpaque;
    void *m_dataOpaque;

    EthernetClient m_socket;

    // Connection state:
    State m_state;

    // Are we transmitting keep-alive packets? (PING)
    unsigned long m_keepaliveInterval;

    // If no traffic is received in this many MS, close the socket.
    unsigned long m_timeout;

    // Just to keep track of the last timestamp.
    unsigned long m_lastPacketTime, m_lastPingTime;

public:
    WebSocket(word maxFrameSize = 96);
    ~WebSocket();

    void registerDataCallback(DataCallback *callback, void *opaque=NULL) { onData = callback; m_dataOpaque = opaque; }
    void registerConnectCallback(Callback *callback, void *opaque=NULL) { onDisconnect = callback; m_connectOpaque = opaque; }
    void registerDisconnectCallback(Callback *callback, void *opaque=NULL) { onDisconnect = callback; m_disconnectOpaque = opaque; }

    bool connect(const char *url);

    // Are we connected?
    bool connected() { return m_socket.connected(); }

    // Outbound may be in HANDSHAKE, inbound will be eitheir DISCONNECTED or CONNECTED
    bool status() { return 0 + m_state; }

    // To get things like host/port info:
    EthernetClient socket() { return m_socket; }

    // Embeds data in frame and sends to client.
    byte send(char *str, word length);

    // Handle incoming data.
    void listen();

    // Disconnect user gracefully.
    void close();

    // Set to keepalive frequency in milliseconds, or 0 for "don't transmit".
    void setKeepalive(unsigned int interval);

    // Set to connection timeout in milliseconds, or 0 for "never timeout". It still can if the underlying socket dies.
    void setTimeout(unsigned int deadline);

    // Called also by WebSocketServer:
    static void initialise( word maxFrameSize );

    // Free as much RAM as possible, requiring WebSocket::initialise() to be called before resuming use.
    static void deinitialise();

    void printStatus() {
		Serial.print(F("State: "));
		if( m_state == DISCONNECTED )
			Serial.println(F("DISCONNECTED (0)"));
		else if( m_state == CONNECTED )
			Serial.println(F("CONNECTED (1)"));
		else if( m_state == HANDSHAKE)
			Serial.println(F("HANDSHAKE (2)"));
	}

private:
    // Discovers if the client's header is requesting an upgrade to a
    // websocket connection.
    bool outboundHandshake(); // Called for receiving end of handshake.

    // Outbound handshake:
    bool sendOutboundHandshakeRequest(const char *url, const char *host, word port);

    // Reads a frame from client. Returns false if user disconnects, 
    // or unhandled frame is received. Server must then disconnect, or an error occurs.
    bool getFrame();

    // Calculate if the socket is timed-out or not.
    bool checkTimeout();

protected:
    // Generate a Base64-encoded SHA1 SUM of the static key, optionally prefixed by a provided key:
    char *checksum( char *key=NULL );

    // Update state
    void setStatus( State state ) { printStatus(); m_state = state; printStatus(); }
};

#endif

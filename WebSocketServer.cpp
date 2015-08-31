#include "WebSocketWritable.h"
#include "WebSocketServer.h"
#include "WebSocket.h"
#include "sha1.h"
#include "Base64.h"

//#define DEBUG 1

WebSocketServer::WebSocketServer(const char *urlPrefix, int inPort, byte maxConnections, word maxFrameSize) :
    m_server(inPort),
    m_server_urlPrefix(urlPrefix),
    m_maxConnections(maxConnections),
    m_connectionCount(0)
{
#ifdef DEBUG
    Serial.print(F("1 Frame capacity: "));
    Serial.println(frameCapacity);
#endif

    WebSocket::initialise(maxFrameSize);

#ifdef DEBUG
    Serial.print(F("2 Frame capacity: "));
    Serial.println(frameCapacity);
#endif

    m_connections = new InboundWebSocket*[ m_maxConnections ];
    for( byte x=0; x < m_maxConnections; x++ )
        m_connections[x] = NULL;

    onConnect = NULL;
    onDisconnect = NULL;
}

WebSocketServer::~WebSocketServer()
{
    for( byte x=0; x < m_maxConnections; x++ )
    {
        InboundWebSocket *s = m_connections[x];
        if( s )
        {
            if( s->connected() )
                s->close();
            delete s;
        }
    }
    delete m_connections;
}

byte WebSocketServer::send( char *data, word length )
{
    if( 1 != m_server.write((uint8_t) 0x81) ) // Txt frame opcode
        return 0;
    if( length > 125 )
    {
        if( 1 != m_server.write((uint8_t) 0x7E) ) // 16-bit length follows
            return 0;
        word lenNBO = htons(length);
        if( 2 != m_server.write((const uint8_t *)&lenNBO, 2) ) // Length of data in a word, endian swapped.
            return 0;
    }
    else
    {
        if( 1 != m_server.write((uint8_t) length) ) // Length of dledata in a byte
            return 0;
    }

    return m_server.write( (const uint8_t *)data, length );
}

void WebSocketServer::listen() {
    // First check existing connections:
    for( byte x=0; x < m_maxConnections; x++ )
    {
        if( !m_connections[x] )
            continue;

        InboundWebSocket *s = m_connections[x];
        if( !s->connected() )
        {
            if( onDisconnect )
                onDisconnect(*s, m_disconnectOpaque);

            m_connectionCount--;
            delete s;
            m_connections[x] = NULL;
            continue;
        }

        s->listen();
    }

    EthernetClient cli = m_server.available();
    if( !cli )
        return;

    // Find a slot:
    for( byte x=0; x < m_maxConnections; x++ )
    {
        if( m_connections[x] && m_connections[x]->socket() == cli )
            return;

        if( m_connections[x] )
            continue;

        InboundWebSocket *s = new InboundWebSocket(this, cli);
        if( s->status() == WebSocket::DISCONNECTED )
        {
            delete s;
            return;
        }

        m_connections[x] = s;
        m_connectionCount++;

        // Complete a handshake:
        if( s->status() == WebSocket::HANDSHAKE )
        {
            if( !s->inboundHandshake() )
            {
                s->close();
                continue;
            }

            s->setStatus( WebSocket::CONNECTED );

            if( onConnect )
                onConnect(*s, m_connectOpaque);
        }
        
        return;
    }

    // No room!
#ifdef DEBUG
    Serial.println(F("Cannot accept new websocket client, maxConnections reached!"));
#endif
    cli.stop();
}

InboundWebSocket::InboundWebSocket( WebSocketServer *server, EthernetClient cli ) :
    WebSocket(),
    m_server(server)
{
    m_socket = cli;
    setStatus( WebSocket::HANDSHAKE );
}

bool InboundWebSocket::sendInboundHandshakeResponse( char *key )
{
    if( strlen(key) + 101 > frameCapacity )
    {
        // Buffer isn't large enough!
        close();
        return false;
    }

    char *csum = checksum(key);
    snprintf_P( frame.data, frameCapacity, PSTR("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n"), csum );
    delete csum;

    m_socket.write( frame.data );
#ifdef DEBUG
    Serial.println( frame.data );
#endif
    return true;
}

bool InboundWebSocket::inboundHandshake() {
    char bite;
    char key[32];

    bool hasUpgrade = false;
    bool hasConnection = false;
    bool isSupportedVersion = false;
    bool hasHost = false;
    bool hasKey = false;

#ifdef DEBUG
    Serial.print(F("Frame capacity: "));
    Serial.println(frameCapacity);
#endif

    word counter = 0;
    while( counter < frameCapacity && (bite = m_socket.read()) != -1 )
    {
        if( bite == '\r' ) // Ignored.
            continue;

        if( bite != '\n' )
        {
            frame.data[counter++] = bite;
            frame.data[counter] = '\0';
            continue;
        }

#ifdef DEBUG
        Serial.print("Got header: ");
        Serial.println(frame.data);
#endif

        // Ignore case when comparing and allow 0-n whitespace after ':'. See the spec:
        // http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html
        if( strstr_P( frame.data, PSTR("Upgrade: ") ) )                 hasUpgrade = true;
        else if( strstr_P( frame.data, PSTR("Connection: ") ) )         hasConnection = true;
        else if( strstr_P( frame.data, PSTR("Host: ") ) )               hasHost = true;
        else if( strstr_P( frame.data, PSTR("Sec-WebSocket-Version: ") ) && strstr_P( frame.data, PSTR("13") ) )
            isSupportedVersion = true;
        else if( strstr_P( frame.data, PSTR("Sec-WebSocket-Key: ") ) )
        {
            hasKey = true;
            strtok(frame.data, " ");
            strncpy(key, strtok(NULL, " "), sizeof(key));
        }

        counter = 0; // Start saving new header string
    }

    // Assert that we have all headers that are needed. If so, go ahead and
    // send response headers.
    if( !hasUpgrade || !hasConnection || !isSupportedVersion || !hasHost || !hasKey || !sendInboundHandshakeResponse(key) )
    {
        // Nope, failed handshake. Disconnect
#ifdef DEBUG
        Serial.print(F("Handshake failed! Upgrade:"));
        Serial.print( hasUpgrade );
        Serial.print(F(", Connection:"));
        Serial.print( hasConnection );
        Serial.print(F(", Host:"));
        Serial.print( hasHost );
        Serial.print(F(", Key:"));
        Serial.print( hasKey );
        Serial.print(F(", Version:"));
        Serial.println( isSupportedVersion );
#endif
        return false;
    }

    setStatus( WebSocket::CONNECTED );
    Serial.println(F("Leaving inbound connection jazz, m_state is CONNECTED."));
    return true;
}

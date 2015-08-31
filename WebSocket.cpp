#include "WebSocket.h"
#include "WebSocketServer.h"

#include "Base64.h"
#include "sha1.h"

//#define DEBUG 1

Frame frame;
word frameCapacity = 0;
bool initialised = false;

// Utility:
void WebSocket::initialise( word maxFrameSize )
{
    // This buffer is shared between WebSocket/WebSocketServer contexts:
    if( maxFrameSize > frameCapacity )
    {
        if( initialised )
            delete frame.data;
        frameCapacity = maxFrameSize;
        frame.data = new char[maxFrameSize];
    }

#ifdef DEBUG
    Serial.print(F("Frame capacity: "));
    Serial.println(frameCapacity);
#endif

    initialised = true;
}

void WebSocket::deinitialise()
{
    if( initialised ) return;

    frameCapacity = 0;
    delete frame.data;
    initialised = false;
}

WebSocket::WebSocket( word maxFrameSize ) :
    onConnect(NULL),
    onDisconnect(NULL),
    onData(NULL),
    m_socket(NULL),
    m_state(DISCONNECTED),
    m_keepaliveInterval(10000),
    m_timeout(30000),
    m_lastPacketTime(0),
    m_lastPingTime(0)
{
    // In case it hasn't been done:
    WebSocket::initialise(maxFrameSize);
#ifdef DEBUG
    Serial.println(F("WebSocket::WebSocket()"));
#endif
}

WebSocket::~WebSocket()
{
    if( connected() )
        close();
}

bool WebSocket::connect( const char *url )
{
    char host[32];
    word port;
    char resource[64];
    resource[0] = '/'; // Resources expect a leading slash.
    resource[1] = '\0';
    if( EOF == sscanf( url, "ws://%31[^:]:%99d/%63[^\n]", host, &port, &resource[1] ) )
    {
#ifdef DEBUG
        Serial.println(F("Malformed URL, expected 'ws://<host>:<port>[/resource]' format."));
#endif
        return false;
    }

    if( !m_socket.connect( host, port ) )
    {
#ifdef DEBUG
        Serial.println(F("Connection to remote server failed."));
#endif
        return false;
    }

    if( !sendOutboundHandshakeRequest( resource, host, port ) )
    {
        close();
        return false;
    }

    return true;
}

void WebSocket::close() {
#ifdef DEBUG
    Serial.println(F("Disconnecting"));
#endif
    setStatus( DISCONNECTED );
    if( onDisconnect )
        onDisconnect(*this, m_disconnectOpaque);

    m_socket.flush();
    delayMicroseconds(10000);
    m_socket.stop();
}

void WebSocket::listen()
{
    if( !m_socket.available() )
        return;

    if( !m_socket.connected() )
        close();
    else if( m_state == CONNECTED && !getFrame() )
        // Got unhandled frame, disconnect
        close();
    else if( m_state == HANDSHAKE && !outboundHandshake() )
        close();
}

char *WebSocket::checksum( char *key )
{
    Sha1.init();
    if( key )
        Sha1.print(key);
    Sha1.print( F("258EAFA5-E914-47DA-95CA-C5AB0DC85B11") ); // Add the omni-valid GUID
    uint8_t *hash = Sha1.result();

    char *buf = new char[ 29 ];
    base64_encode( buf, (char*)hash, 20 );
    return buf;
}

bool WebSocket::sendOutboundHandshakeRequest(const char *resource, const char *host, word port)
{
    char *csum = checksum();
    word written = snprintf_P( frame.data, frameCapacity, PSTR("GET %s HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n"), resource, host, port, csum);
    delete csum;

#ifdef DEBUG
    Serial.println(written);
    Serial.write((const uint8_t *)frame.data, written);
#endif
    m_socket.write( (const uint8_t *)frame.data, written );

    setStatus( HANDSHAKE );
}

bool WebSocket::outboundHandshake()
{
    bool hasUpgrade = false;
    bool hasConnection = false;
    bool hasAccept = false;

    byte counter = 0;
    char bite;

    // Receive result:
    while( counter < frameCapacity && (bite = m_socket.read()) != -1 )
    {
        frame.data[counter++] = bite;
        frame.data[counter] = '\0';
        if( '\n' != bite ) continue;

#ifdef DEBUG
        Serial.print("Got header: ");
        Serial.println(frame.data);
#endif

        if( strstr_P( frame.data, PSTR("Upgrade: ") ) )         hasUpgrade = true;
        else if( strstr_P( frame.data, PSTR("Connection: ") ) ) hasConnection = true;
        else if( strstr_P( frame.data, PSTR("Sec-WebSocket-Accept: ") ) )       hasAccept = true;

        counter = 0; // Start saving new header string
    }

    // Assert that we have all headers that are needed. If so, go ahead and
    // send response headers.
    if( !hasUpgrade || !hasConnection || !hasAccept )
    {
#ifdef DEBUG
        Serial.println(F("Handshake failed!"));
#endif
        close();
        return false;
    }

    m_state = CONNECTED;
    if( onConnect )
        onConnect(*this, m_connectOpaque);

#ifdef DEBUG
    Serial.println(F("Handshake complete!"));
#endif

    return true;
}

bool WebSocket::getFrame()
{
    byte bite;

    // Get opcode
    bite = m_socket.read();

    frame.opcode = bite & 0xf; // Opcode
    frame.isFinal = bite & 0x80; // Final frame?
    // Determine length (only accept <= 64 for now)
    bite = m_socket.read();
    frame.length = bite & 0x7f; // Length of payload
    if( frame.length == 126 )
    {
        // 16-bit length:
        uint8_t *len8 = (uint8_t*)&frame.length;
        len8[1] = m_socket.read();
        len8[0] = m_socket.read();
    }

    if (frame.length > frameCapacity) {
#ifdef DEBUG
        Serial.print(F("Too big frame to handle. Length: "));
        Serial.println(frame.length);
#endif
        m_socket.write((uint8_t) 0x08);
        m_socket.write((uint8_t) 0x02);
        m_socket.write((uint8_t) 0x03);
        m_socket.write((uint8_t) 0xf1);
        return false;
    }
    // Client should always send mask, but check just to be sure
    frame.isMasked = bite & 0x80;
    if (frame.isMasked) {
        frame.mask[0] = m_socket.read();
        frame.mask[1] = m_socket.read();
        frame.mask[2] = m_socket.read();
        frame.mask[3] = m_socket.read();
    }

    // Clear any frame data that may have come previously
    //memset(frame.data, 0, frameCapacity);


    // Get message bytes and unmask them if necessary
    for (int i = 0; i < frame.length; i++) {
        if (frame.isMasked) {
            frame.data[i] = m_socket.read() ^ frame.mask[i % 4];
        } else {
            frame.data[i] = m_socket.read();
        }
    }
    frame.data[frame.length] = '\0';

    //
    // Frame complete!
    //
    if (!frame.isFinal) {
        // We don't handle fragments! Close and disconnect.
#ifdef DEBUG
        Serial.println(F("Non-final frame, doesn't handle that."));
#endif
        m_socket.print((uint8_t) 0x08);
        m_socket.write((uint8_t) 0x02);
        m_socket.write((uint8_t) 0x03);
        m_socket.write((uint8_t) 0xf1);
        return false;
    }

    uint8_t cmdPair[2];
    switch (frame.opcode) {
        case 0x01: // Txt frame
            // Call the user provided function
            if( onData )
                onData(*this, frame.data, frame.length, m_dataOpaque);
            break;

        case 0x08:
            // Close frame. Answer with close and terminate tcp connection
            // TODO: Receive all bytes the client might send before closing? No?
#ifdef DEBUG
            Serial.println(F("Close frame received. Closing in answer."));
#endif
            m_socket.write( (uint8_t)0x88 );
            m_socket.write( (uint8_t)0x0 );
            return false;

        case 0x09: // PING
            m_socket.write( (uint8_t)0x8A );
            m_socket.write( (uint8_t)0x0 );
            break;

        case 0x0A: // PONG
            break;

        default:
            // Unexpected. Ignore. Probably should blow up entire universe here, but who cares.
#ifdef DEBUG
            Serial.print(F("Unhandled frame ignored: "));
            Serial.println(frame.opcode);
#endif
            return false;
    }

    // Update 'last packet' time:
    m_lastPacketTime = millis();

    return true;
}

byte WebSocket::send( char *data, word length )
{
    if( CONNECTED != m_state )
    {
#ifdef DEBUG
        Serial.println(F("No connection to client, no data sent."));
#endif
        return 0;
    }

    if( 1 != m_socket.write((uint8_t) 0x81) ) // Txt frame opcode
        return 0;
    if( length > 125 )
    {
        if( 1 != m_socket.write((uint8_t) 0x7E) ) // 16-bit length follows
            return 0;
        word lenNBO = htons(length);
        if( 2 != m_socket.write((const uint8_t *)&lenNBO, 2) ) // Length of data in a word, endian swapped.
            return 0;
    }
    else
    {
        if( 1 != m_socket.write((uint8_t) length) ) // Length of data in a byte
            return 0;
    }

    return m_socket.write( (const uint8_t *)data, length );
}

void WebSocket::setKeepalive(unsigned int interval)
{
    m_keepaliveInterval = interval;
}

void WebSocket::setTimeout(unsigned int deadline)
{
    m_timeout = deadline;
}

bool WebSocket::checkTimeout()
{
    unsigned long now = millis();
    if( m_lastPacketTime + m_timeout > now )
    {
        m_lastPacketTime = now;
        close();
        return false;
    }

    // Send a ping:
    if( m_lastPingTime + m_keepaliveInterval > now )
    {
        m_lastPingTime = now;
        m_socket.write( (uint8_t)0x89 );
        m_socket.write( (uint8_t)0x0 );
    }

    return true;
}

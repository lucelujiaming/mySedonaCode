//
// Copyright (c) 2007 Tridium, Inc.
// Licensed under the Academic Free License version 3.0
//
// History:
//   05 Sep 06  Brian Frank  Creation
//   07 May 07  Brian Frank  Port from C++ old Sedona
//   09 Jul 12  Elizabeth McKenney/Clif Turman IPV6 support
//   09 Aug 12  Clif Turman  Add QNX Specific variant
//
#include <time.h>
#include <arpa/inet.h>
#include "inet_util_std.h"

//
// Define the multicast group address for Discover
// (must match address used by SoxClient, defined in sedona.jar)
//
#ifdef SOCKET_FAMILY_INET
 #define DISCOVER_MULTICAST_GROUP_ADDRESS "239.255.18.76"   // RFC 2365
#elif defined( SOCKET_FAMILY_INET6 )
 #define DISCOVER_MULTICAST_GROUP_ADDRESS "FF02::1"  //"FF02::137"       // use Niagara's
#endif



// defined in inet_util_std.c (may be commented out)
//extern void printAddr(const char* label, void* loc, int len);


//////////////////////////////////////////////////////////////////////////
// Datagram
//////////////////////////////////////////////////////////////////////////

// TODO: this is pretty hackish, but lets us map
// the compiler memory layout to/from C

struct UdpDatagram
{
  void*    addr;
  int32_t  port;
  uint8_t* buf;
  int32_t  off;
  int32_t  len;
  int32_t  scope;    // only used by IPv6, BUT in 1.2.16+ UdpDatagram.sedona for both
  int32_t  flow;     // only used by IPv6, BUT in 1.2.16+ UdpDatagram.sedona for both
};

static void getUdpDatagram(int8_t* sedona, struct UdpDatagram* c)
{
  if (sizeof(void*) == 4)
  {
    c->addr  = getRef(sedona, 0);
    c->port  = getInt(sedona, 4);
    c->buf   = getRef(sedona, 8);
    c->off   = getInt(sedona, 12);
    c->len   = getInt(sedona, 16);
#ifdef SOCKET_FAMILY_INET6
    c->scope = getInt(sedona, 20);   // don't touch these for IPv4
    c->flow  = getInt(sedona, 24);   // don't touch these for IPv4
#endif
  }
  else
  {
    printf("UdpSocket unsupported pointer size\n");
  }
}

static void setUdpDatagram(int8_t* sedona, struct UdpDatagram* c)
{
  if (sizeof(void*) == 4)
  {
    setRef(sedona, 0,  c->addr);
    setInt(sedona, 4,  c->port);
    setRef(sedona, 8,  c->buf);
    setInt(sedona, 12, c->off);
    setInt(sedona, 16, c->len);
#ifdef SOCKET_FAMILY_INET6
    setInt(sedona, 20, c->scope);   // don't touch these for IPv4
    setInt(sedona, 24, c->flow);    // don't touch these for IPv4
#endif
  }
  else
  {
    printf("UdpSocket unsupported pointer size\n");
  }
}




//////////////////////////////////////////////////////////////////////////
// Native Methods
//////////////////////////////////////////////////////////////////////////
 
//
// What is the maximum number of bytes which can 
// sent by this UDP implementation.
//
// static int maxPacketSize()
//
Cell inet_UdpSocket_maxPacketSize(SedonaVM* vm, Cell* params)
{                  
  // for now limit to 512 which is max SoxService buffer       
  Cell ret;
  ret.ival = 512;  
  return ret;       
}                                

//
// What is the ideal maximum number of bytes sent by this 
// UDP implementation.  This is typically driven by the 
// lower levels of the IP stack - for instance when running 
// 6LoWPAN over 802.15.4, this is the max UDP packet size 
// which doesn't require fragmenting across multiple 
// 802.15.4 frames.
//
// static int idealPacketSize()
//
Cell inet_UdpSocket_idealPacketSize(SedonaVM* vm, Cell* params)
{                                
  // for now limit to 512 which is max SoxService buffer       
  Cell ret;
  ret.ival = 512;
  return ret;       
}                                

//
// Initialize this socket which allocates a socket handle
// to this instance.  This method must be called before using
// the socket.  Return true on success, false on failure.
//
// bool open()
//
Cell inet_UdpSocket_open(SedonaVM* vm, Cell* params)
{
  void* self    = params[0].aval;
  bool closed   = getClosed(self);
  socket_t sock = getSocket(self);

#ifdef _WIN32
  // windoze startup
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
  {
    return falseCell;
  }
#endif

  // check for already initialized
  if (!closed || sock != -1) 
  {
     return falseCell;
  }

  // create socket
#ifdef SOCKET_FAMILY_INET
  sock = socket(AF_INET, SOCK_DGRAM,  0);
#elif defined( SOCKET_FAMILY_INET6 )
  sock = socket(AF_INET6, SOCK_DGRAM,  0);
#endif

#ifdef _WIN32
  if (sock == INVALID_SOCKET) 
  {
    return falseCell;
  }
#else
  if (sock < 0) 
  {
    return falseCell;
  }
#endif
  
  int so_broadcast=1;
  setsockopt(sock, SOL_SOCKET, SO_BROADCAST, 
      (char *)&so_broadcast, sizeof(so_broadcast));

  // make socket non-blocking
  if (inet_setNonBlocking(sock) != 0)
  {
    printf("inet_UdpSocket_open::closesocket with %d.\r\n", sock);
    closesocket(sock);
    return falseCell;
  }

  // udate UdpSocket instance
  setClosed(self, 0);
  printf("inet_UdpSocket_open::setSocket with %d.\r\n", sock);
  setSocket(self, sock);

  return trueCell;
}

//
// Bind this socket the specified well-known port on this
// host. Return true on success, false on failure.
//
// bool bind(int port)
//
Cell inet_UdpSocket_bind(SedonaVM* vm, Cell* params)
{
  void* self    = params[0].aval;
  int32_t port  = params[1].ival;
  bool closed   = getClosed(self);
  socket_t sock = getSocket(self);
  int rc;

  if (closed) return falseCell;

  rc = inet_bind(sock, port);

  if (rc == SOCKET_ERROR)
  {
    printf("  Failed to bind: %s\n", ERRNO_MSG());
    return falseCell;
  }

  return trueCell;
}

//
// Join this socket the specified multicast group address.
// Return true on success, false on failure.
//
// bool join(Str groupaddr)
//
Cell inet_UdpSocket_join(SedonaVM* vm, Cell* params)
{
  void* self       = params[0].aval;

  bool closed   = getClosed(self);
  socket_t sock = getSocket(self);
  
  int rc;

  if (closed) return falseCell;

  //
  // Join all-hosts multicast address group (used for device discovery)
  //
#if defined( SOCKET_FAMILY_INET )
  {
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(DISCOVER_MULTICAST_GROUP_ADDRESS);
    mreq.imr_interface.s_addr = INADDR_ANY;
    rc = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq));
  }
#elif defined( SOCKET_FAMILY_INET6 )
  {
    struct ipv6_mreq mreq;
    inet_pton( AF_INET6, DISCOVER_MULTICAST_GROUP_ADDRESS, &(mreq.ipv6mr_multiaddr) );

#ifdef __QNX__
    mreq.ipv6mr_interface = 2;      // Hardcoded for now.  How to get this through api's?
#else
    mreq.ipv6mr_interface = 0;
#endif

    rc = setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *)&mreq, sizeof(mreq));
  }
#endif 


  if (rc==SOCKET_ERROR) 
  {
    // Print error details, since there are different reasons join might fail
    printf("  Error joining multicast group %s: %s\n", DISCOVER_MULTICAST_GROUP_ADDRESS, 
                                                       ERRNO_MSG());
    return falseCell;
  }

  //printf(" Joined multicast group %s\n", DISCOVER_MULTICAST_GROUP_ADDRESS);   // debug
  return trueCell;
}



//
// Send the specified datagram which encapsulates both the
// destination address and the data to send.  Return true
// on success, false on failure.  If the number of bytes
// sent does not match datagram.len then this call will
// fail and return false.
//
// bool send(UdpDatagram datagram)
//
Cell inet_UdpSocket_send(SedonaVM* vm, Cell* params)
{
  void* self      = params[0].aval;
  void* sDatagram = params[1].aval;
  socket_t sock   = getSocket(self);
  bool closed     = getClosed(self);

  struct UdpDatagram datagram;
  uint8_t* buf;
  int32_t len;
  int rc;

  // On QNX, sendto requires the exact length of the buffer to be used
#if defined( SOCKET_FAMILY_INET )
  socklen_t addrLen = sizeof(struct sockaddr_in);
#elif defined( SOCKET_FAMILY_INET6 )
  socklen_t addrLen = sizeof(struct sockaddr_in6);
#endif

  struct sockaddr_storage addr;
  memset(&addr, 0, sizeof(addr));

  if (closed) printf("  send error! UDP socket is closed\n");

  if (closed) return falseCell;
  if (sDatagram == NULL) return falseCell;

  // Copy shared struct into local var
  getUdpDatagram(sDatagram, &datagram);
  if (datagram.buf == NULL) return falseCell;
  if (datagram.addr == 0) return falseCell;

  // Set up args for sendto()
  inet_toSockaddr(&addr, datagram.addr, datagram.port, datagram.scope, datagram.flow);
  buf = datagram.buf + datagram.off;
  len = datagram.len;

  rc = sendto(sock, buf, len, 0, (const struct sockaddr *)&addr, addrLen);

  if (rc==SOCKET_ERROR) 
  {
    printf("  sendto error: %s\n", ERRNO_MSG());
    return falseCell;
  }

  return trueCell;
}

//
// Receive a datagram into the specified structure.  The
// datagram.buf must reference a valid byte buffer - bytes
// are read in starting at datagram.buf[0] with at most datagram.len
// bytes being received.  If successful then return true,
// datagram.len reflects the actual number of bytes received,
// and datagram.addr/datagram.port reflect the source socket
// address.  Note datagram.addr is only valid until the next
// call to receive().  On failure or if no packets are pending
// to read, then return false, len=0, port=-1, and addr=null.
//
// Note if the number of bytes available to be read is greater
// then len than this call works differently dependent on the
// platform.  In Java it silently ignores the remainder of
// the bytes (wrong way), and in C++ it returns false since
// the message received is not the same as the message sent.
//
// bool receive(UdpDatagram datagram)
//
Cell inet_UdpSocket_receive(SedonaVM* vm, Cell* params)
{
  void* self      = params[0].aval;
  void* sDatagram = params[1].aval;
  socket_t sock   = getSocket(self);
  bool closed     = getClosed(self);

  struct UdpDatagram datagram;
  uint8_t* buf;
  int32_t len;

  int r;
  void* receiveIpAddr;

  struct sockaddr_storage addr;
  unsigned int addrLen = sizeof(addr);

  memset(&addr, 0, sizeof(addr));

  // we store an inline IpAddr in UdpSocket to use as the
  // storage location for the address to return in the datagram
  // the compiler lays it out at offset 8 (after closed and socket)
  receiveIpAddr = getInline(self, 8);

  if (closed) return falseCell;
  if (sDatagram == NULL) return falseCell;

  // Copy shared struct into local var
  getUdpDatagram(sDatagram, &datagram);
  if (datagram.buf == NULL) return falseCell;

  // Set up args for recvfrom() 
  buf = datagram.buf + datagram.off;
  len = datagram.len;

  r = recvfrom(sock, buf, len, 0, (struct sockaddr *)&addr, &addrLen);

  // Update shared struct from contents of local var
  if (r == SOCKET_ERROR)
  {
    datagram.len  = 0;
    datagram.addr = NULL;
    datagram.port = -1;
    setUdpDatagram(sDatagram, &datagram);
    return falseCell;
  }

  inet_fromSockaddr(&addr, receiveIpAddr, &datagram.port, &datagram.scope, &datagram.flow);
  datagram.len  = r;
  datagram.addr = receiveIpAddr;
  setUdpDatagram(sDatagram, &datagram);
  return trueCell;
}

//
// Shutdown and close this socket.
//
// void close()
//
Cell inet_UdpSocket_close(SedonaVM* vm, Cell* params)
{
  void* self    = params[0].aval;
  socket_t sock = getSocket(self);
  bool closed   = getClosed(self);
  
  printf("inet_UdpSocket_close::sock with %d.\r\n", sock);
  if (!closed)
  {
    printf("inet_UdpSocket_close::closesocket with %d.\r\n", sock);
    closesocket(sock);
    setClosed(self, 1);
    setSocket(self, -1);
  }
  return nullCell;
}

// Bacbet device list searching
// #define PORT 47808
#define MAXDATASIZE 256


#define SEND_WHOIS_PACKET                     1
#define SEND_WHOIS_ROUTER_To_NETWORK_PACKET   2

// 0000 C0 A8 A8 EB BA C0 81 0A 00 07 01 80 00
#define WHOIS_ROUTER_To_NETWORK_PACKET "\x81\x0a\x00\x07\x01\x80\x00"
#define WHOIS_ROUTER_To_NETWORK_LENGTH 7


// 0000 C0 A8 A8 02 BA C0 81 0A 00 0C 01 20 FF FF 00 FF 
// 0010 10 08
#define WHOIS_PACKET "\x81\x0a\x00\x0c\x01\x20\xff\xff\x00\xff\x10\x08"
#define WHOIS_LENGTH 12

#ifdef _WIN32
void revertShort(unsigned short &sValue)
{
    unsigned char * ptrValue = (unsigned char *)(&sValue);
    unsigned char cTmp;
    cTmp = ptrValue[0];
    ptrValue[0] = ptrValue[1];
    ptrValue[1] = cTmp;
}

void revertInt(unsigned int &uValue)
{
    unsigned char * ptrValue = (unsigned char *)(&uValue);
    unsigned char cTmp;

    cTmp = ptrValue[0];
    ptrValue[0] = ptrValue[3];
    ptrValue[3] = cTmp;

    cTmp = ptrValue[1];
    ptrValue[1] = ptrValue[2];
    ptrValue[2] = cTmp;
}
#endif


unsigned int convertIntFromBuffer(unsigned char * buffer)
{
    unsigned int uRet = 0x00;
    uRet = buffer[0];
    uRet = uRet * 0x100 + buffer[1];
    uRet = uRet * 0x100 + buffer[2];
    uRet = uRet * 0x100 + buffer[3];
    return uRet;
}

unsigned short convertShortFromBuffer(unsigned char * buffer)
{
    unsigned short uRet = 0x00;
    uRet = buffer[0];
    uRet = uRet * 0x100 + buffer[1];
    return uRet;
}


#define SERVICE_CHOICE_IAM    0x00
int dealWhoIsResponse(struct sockaddr_in user_addr, unsigned char * buffer, int iLen, 
                 unsigned int  * ipArrayList, 
                 unsigned int  * controlDstSpecList, 
                 unsigned int  * NPDUList, 
                 unsigned int  * objectIdentifierList, 
                 unsigned int  * maxADPUList,
                 unsigned int  iListIdx)
{
    int iOffset = 0;
    printf("IP = %s. we Receive %d .\r\n", inet_ntoa(user_addr.sin_addr), iLen);
    ipArrayList[iListIdx] = (unsigned int)(user_addr.sin_addr.s_addr);
    printf("Type: %X. Function: %X. BVLC-Length: %X. ", 
        buffer[0], buffer[1], (int)convertShortFromBuffer(buffer + 2));
    printf("Version: %X. Control: %X.\r\n", buffer[4], buffer[5]);
    controlDstSpecList[iListIdx] = buffer[5];
    
    int macAddressLength = buffer[8];
    // Source specifier: SNET, SLEN and SADR absent
    if((buffer[5] & 0x08) == 0x00)   // 0x20
    {
        // Destination MAC Layer Address Length
        iOffset = 10 + macAddressLength;
        printf("Unconfirmed Service Choice: %X. ", buffer[iOffset + 1]);
        if(buffer[iOffset + 1] == SERVICE_CHOICE_IAM)
        {
            NPDUList[iListIdx] = 0x00;
            iOffset += 2;
            int objectIdentifierLen = buffer[iOffset] % 0x10;
            if(objectIdentifierLen == 0x04)
            {
                iOffset++;
                unsigned int objectIdentifier = convertIntFromBuffer(buffer + iOffset);
                objectIdentifierList[iListIdx] = objectIdentifier;
                printf("Object Type: %X. Instance Number: %X. ", objectIdentifier >> 22, 
                    objectIdentifier & 0x3FFFFF);
                iOffset += objectIdentifierLen;
                int maxADPULength = buffer[iOffset] % 0x08;
                iOffset++;
                if(maxADPULength == 0x02)
                {
                    int maxADPU = convertShortFromBuffer(buffer + iOffset);
                    printf("maxADPU: %d.\r\n", maxADPU);
                    maxADPUList[iListIdx] = maxADPU;
                }
                else
                {
                    printf("Error maxADPU with 0x%X.\r\n", objectIdentifierLen);
                    maxADPUList[iListIdx] = -1;
                    return 0;
                }
            }
            else
            {
                printf("Error objectIdentifierLen with 0x%X.\r\n", objectIdentifierLen);
                objectIdentifierList[iListIdx] = -1;
                return 0;
            }
        }
        else
        {
            printf("Error SERVICE_CHOICE with 0x%X.\r\n", buffer[iOffset + 1]);
            return 0;
        }
    }
    // Source specifier: SNET, SLEN and SADR present, 
    //                   SLEN=0 invalid, SLEN specifies length of SADR
    if((buffer[5] & 0x08) == 0x08)   // 0x28
    {
        iOffset = 9;
        int sourceNetworkAddress = convertShortFromBuffer(buffer + iOffset);
        iOffset += 2;
        printf("sourceNetworkAddress: %d. sourceMACLayerAddressLength: %d. ",
            sourceNetworkAddress, buffer[iOffset]);
        if(buffer[iOffset] == 0x01)
        {
            iOffset++;
            int sadr = buffer[iOffset];
            printf("SADR: %d. ", sadr);
            NPDUList[iListIdx] = sadr * 0x10000 + sourceNetworkAddress;
            iOffset++;
            printf("Hop Count: %d.\r\n", buffer[iOffset]);
        }
        else
        {
            printf("Error ourceMACLayerAddressLength with 0x%X.\r\n", buffer[iOffset]);
            NPDUList[iListIdx] = -1;
            return 0;
        }
        iOffset += 2;
        printf("Unconfirmed Service Choice: %X. ", buffer[iOffset]);
        if(buffer[iOffset] == SERVICE_CHOICE_IAM)
        {
            iOffset++;
            int objectIdentifierLen = buffer[iOffset] % 0x10;  
            if(objectIdentifierLen == 0x04)
            {
                iOffset++;
                unsigned int objectIdentifier = convertIntFromBuffer(buffer + iOffset);
                objectIdentifierList[iListIdx] = objectIdentifier;
                printf("Object Type: %X. Instance Number: %X. ", objectIdentifier >> 22, 
                    objectIdentifier & 0x3FFFFF);
                iOffset += objectIdentifierLen;
                int maxADPULength = buffer[iOffset] % 0x08;
                iOffset++;  
                if(maxADPULength == 0x02)
                {
                    int maxADPU = convertShortFromBuffer(buffer + iOffset);
                    maxADPUList[iListIdx] = maxADPU;
                    printf("maxADPU: %d.\r\n", maxADPU);
                }
                else
                {
                    printf("Error maxADPU with 0x%X.\r\n", objectIdentifierLen);
                    maxADPUList[iListIdx] = -1;
                    return 0;
                } 
            }
            else
            {
                printf("Error objectIdentifierLen with 0x%X.\r\n", objectIdentifierLen);
                objectIdentifierList[iListIdx] = -1;
                return 0;
            }
        }
        else
        {
            printf("Error SERVICE_CHOICE with 0x%X.\r\n", buffer[iOffset]);
            return 0;
        }
    }
    return iOffset;
}

void recvSleep(float fMilliSeconds)
{
    if(fMilliSeconds > 0)
    {
#ifdef _WIN32
        if(fMilliSeconds < 1)
            Sleep(1);
        else
            Sleep(1 * fMilliSeconds);
#else
        if(fMilliSeconds >= 1)
            usleep(1000 * fMilliSeconds);
#endif
    }
}

socket_t initializeSendSocket(char * networkIP, int port, 
                    struct sockaddr_in* my_addr)
{
    socket_t clientSendSocket;
    struct sockaddr_in user_addr;
    
#ifdef _WIN32
    WSADATA wsadata;
    if (0 == WSAStartup(MAKEWORD(2, 2), &wsadata))
    {
        printf("Socket opened r\n");
    }
    else
    {
        printf("Socket open failed \r\n");
        return -1;
    }
#endif
    clientSendSocket = socket(AF_INET, SOCK_DGRAM, 0);

    my_addr->sin_family=AF_INET;
    my_addr->sin_port=htons(port);
	// NOTICE: It should not change into 192.168.168.255, otherwise the socket would crash.
    my_addr->sin_addr.s_addr=inet_addr("255.255.255.255");
    memset(&(my_addr->sin_zero), 0x00, 8);

    int so_broadcast=1;
    setsockopt(clientSendSocket, SOL_SOCKET, SO_BROADCAST, 
        (char *)&so_broadcast, sizeof(so_broadcast));

    inet_setNonBlocking(clientSendSocket);
    
    user_addr.sin_family=AF_INET;
    user_addr.sin_port=htons(port);
    // user_addr->sin_addr.s_addr=htonl(INADDR_ANY);
    // printf("set IP to INADDR_ANY and bind to clientSocket. \r\n");
    user_addr.sin_addr.s_addr=inet_addr(networkIP);
    printf("set user_addr to networkIP (%s:%d) and bind to clientSendSocket(%d). \r\n", networkIP, port, clientSendSocket);
    memset(&(user_addr.sin_zero), 0x00, 8);
    
    if((bind(clientSendSocket, (struct sockaddr *)&user_addr,
                                    sizeof(struct sockaddr)))==-1)
    {
        printf("initializeSendSocket::bind failed we will close socket with %d. \r\n", clientSendSocket);
        closesocket(clientSendSocket);
		// We have to return -1 here otherwise we will create clientRecvSocket incorrectly
        return -1;
    }
    
    return clientSendSocket;
}


// #define RECV_PORT 47808
socket_t initializeRecvSocket(int port)
{
    socket_t clientRecvSocket;
    
#ifdef _WIN32
    WSADATA wsadata;
    if (0 == WSAStartup(MAKEWORD(2, 2), &wsadata))
    {
        printf("Socket opened r\n");
    }
    else
    {
        printf("Socket open failed \r\n");
        return -1;
    }
#endif
    clientRecvSocket = socket(AF_INET, SOCK_DGRAM, 0);
    inet_setNonBlocking(clientRecvSocket);
    
    struct sockaddr_in  recv_255_addr;
    bzero((char *)&recv_255_addr, sizeof(recv_255_addr));
    recv_255_addr.sin_family=AF_INET;
    recv_255_addr.sin_addr.s_addr=inet_addr("192.168.168.255");
    // recv_255_addr.sin_port=htons(RECV_PORT);
    recv_255_addr.sin_port=htons(port);
    memset(&(recv_255_addr.sin_zero), 0x00, 8);

    
    if((bind(clientRecvSocket, (struct sockaddr *)&recv_255_addr,
                                    sizeof(struct sockaddr)))==-1)
    {
        printf("bind failed. \r\n");
        return -1;
    }
    return clientRecvSocket;
}

#define RECV_TIMEOUT    0x02
int sendWhoIsBroadcast(int iRetryCount, int iInstanceNumber, 
                     socket_t clientSendSocket, socket_t clientRecvSocket, 
                     struct sockaddr_in my_addr, 
                     unsigned int  * ipArrayList, 
                     unsigned int  * controlDstSpecList, 
                     unsigned int  * NPDUList, 
                     unsigned int  * objectIdentifierList, 
                     unsigned int  * maxADPUList)
{
    int i = 0;
    int iSendCount = 0;
    int iRecvCount = 0;
    unsigned char buf[MAXDATASIZE];
    unsigned int size;
    int iListLen = 0;
    
    int iClientCount = 0;

    time_t current_time, now;
    
    struct sockaddr_in  recv_addr;
    bzero((char *)&recv_addr, sizeof(recv_addr));
//    recv_2_addr.sin_family=AF_INET;
//    recv_2_addr.sin_addr.s_addr=inet_addr("192.168.168.2");
//    recv_2_addr.sin_port=htons(PORT);
//    memset(&(recv_2_addr.sin_zero), 0x00, 8);

    // printf("Enter sendBroadcast \r\n");
    for(i=0; i<iRetryCount; i++)
    {
        memset(buf, 0x00, MAXDATASIZE);
        memcpy(buf, WHOIS_PACKET, WHOIS_LENGTH);
        if(iInstanceNumber != 0)
        {
           buf[3]                = 0x12;
           buf[WHOIS_LENGTH]     = 0x0A;
           buf[WHOIS_LENGTH + 1] = iInstanceNumber / 0x100;
           buf[WHOIS_LENGTH + 2] = iInstanceNumber % 0x100;
           buf[WHOIS_LENGTH + 3] = 0x1A;
           buf[WHOIS_LENGTH + 4] = iInstanceNumber / 0x100;
           buf[WHOIS_LENGTH + 5] = iInstanceNumber % 0x100;
           
           iSendCount = sendto(clientSendSocket, (char *)buf, WHOIS_LENGTH + 6, 0, 
                (struct sockaddr *)&my_addr, sizeof(my_addr));
        }
        else 
        {
           iSendCount = sendto(clientSendSocket, (char *)buf, WHOIS_LENGTH, 0, 
                (struct sockaddr *)&my_addr, sizeof(my_addr));
        }
#ifdef _WIN32
        recvSleep(1);
#else
        recvSleep(0);
#endif
        // printf("send %d OK \r\n", iSendCount);
        size = sizeof(struct sockaddr_in);

        current_time = now = time(NULL);
        while(1)
        {
            memset(buf, 0x00, MAXDATASIZE);
            // printf("------------------try to  recvfrom------------------ \r\n");
            iRecvCount = recvfrom(clientRecvSocket, (char *)buf, 
                MAXDATASIZE, 0, (struct sockaddr *)&recv_addr, (socklen_t *)&size);
            if (iRecvCount == -1)
            {
            //    printf("recvfrom return -1 because of (%d):(%s) \r\n",
            //        errno, strerror(errno));
                now = time(NULL);
                if(now - current_time > RECV_TIMEOUT)
                {
                    // printf("we detect %d clients.\r\n", iClientCount);
                    break;
                }
                else
                {
                    recvSleep(1);
                    continue;
                }
            }
            
            if((iRecvCount == iSendCount)
                && (memcmp(buf, WHOIS_PACKET, WHOIS_LENGTH) == 0))
            {
                printf("Omit broadcast sent to me \r\n");
                continue;
            }
            else if (iRecvCount > 0)
            {
                // printf("we Receive %d .\r\n", iRecvCount);
                iListLen = dealWhoIsResponse(recv_addr, buf, iRecvCount, 
                         ipArrayList, 
                         controlDstSpecList, 
                         NPDUList, 
                         objectIdentifierList, 
                         maxADPUList,
                         iClientCount);
                iClientCount++;
                recvSleep(1);
            }
        }
        printf("-------------------(%d)----------------------- \r\n", i + 1);
    }
    return iClientCount;
}

#define  SEND_BROADCAST_TIMES    1
Cell inet_UdpSocket_getBacnetDeviceList(SedonaVM* vm, Cell* params)
{
    int iClientCount = 0;
  // void* self              = params[0].aval;
  char*     ipAddress               = params[1].aval;
  int32_t   port                    = params[2].ival;
  uint32_t* ipArrayList             = params[3].aval;
  uint32_t* controlDstSpecList      = params[4].aval;
  uint32_t* NPDUList                = params[5].aval;
  uint32_t* objectIdentifierList    = params[6].aval;
  uint32_t* maxADPUList             = params[7].aval;
  
  
    printf("Enter inet_UdpSocket_getBacnetDeviceList(%s) \r\n", ipAddress);
    // unsigned int iListLen = 0;
    // unsigned int ipArrayList[10];
    // unsigned int objIDList[10];
    memset(ipArrayList, 0x00, sizeof(int) * 10);
    memset(controlDstSpecList, 0x00, sizeof(int) * 10);
    memset(NPDUList, 0x00, sizeof(int) * 10);
    memset(objectIdentifierList, 0x00, sizeof(int) * 10);
    memset(maxADPUList, 0x00, sizeof(int) * 10);


    socket_t clientSendSocket;
    struct sockaddr_in  my_addr;
    // struct sockaddr_in  recv_addr;
    clientSendSocket = initializeSendSocket(ipAddress, port, &my_addr); // , &recv_addr);
    if (clientSendSocket > 0)
    {
        socket_t clientRecvSocket;
        // printf("Call sendBroadcast \r\n");
        clientRecvSocket = initializeRecvSocket(port);
        iClientCount = sendWhoIsBroadcast(SEND_BROADCAST_TIMES, 0, 
            clientSendSocket, clientRecvSocket, my_addr, // recv_addr, 
            ipArrayList, 
            controlDstSpecList, 
            NPDUList, 
            objectIdentifierList, 
            maxADPUList);
        closesocket(clientRecvSocket);
    }
	if(clientSendSocket > 0)
	{
        closesocket(clientSendSocket);
	}
    
    Cell ret;
    ret.ival = iClientCount;  
    return ret;
}


Cell inet_UdpSocket_getBacnetDevice(SedonaVM* vm, Cell* params)
{
    int iClientCount = 0;
  // void* self              = params[0].aval;
  char*     ipAddress               = params[1].aval;
  int32_t   port                    = params[2].ival;
  int32_t   iInstanceNumber         = params[3].ival;
  uint32_t* ipArrayList             = params[4].aval;
  uint32_t* controlDstSpecList      = params[5].aval;
  uint32_t* NPDUList                = params[6].aval;
  uint32_t* objectIdentifierList    = params[7].aval;
  uint32_t* maxADPUList             = params[8].aval;
  
  
    printf("Enter inet_UdpSocket_getBacnetDevice(%s) \r\n", ipAddress);
    // unsigned int iListLen = 0;
    // unsigned int ipArrayList[10];
    // unsigned int objIDList[10];
    memset(ipArrayList, 0x00, sizeof(int) * 10);
    memset(controlDstSpecList, 0x00, sizeof(int) * 10);
    memset(NPDUList, 0x00, sizeof(int) * 10);
    memset(objectIdentifierList, 0x00, sizeof(int) * 10);
    memset(maxADPUList, 0x00, sizeof(int) * 10);


    socket_t clientSendSocket;
    struct sockaddr_in  my_addr;
    // struct sockaddr_in  recv_addr;
    clientSendSocket = initializeSendSocket(ipAddress, port, &my_addr); // , &recv_addr);
    if (clientSendSocket > 0)
    {
        socket_t clientRecvSocket;
        // printf("Call sendBroadcast \r\n");
        clientRecvSocket = initializeRecvSocket(port);
        iClientCount = sendWhoIsBroadcast(SEND_BROADCAST_TIMES, iInstanceNumber, 
            clientSendSocket, clientRecvSocket, my_addr, // recv_addr, 
            ipArrayList, 
            controlDstSpecList, 
            NPDUList, 
            objectIdentifierList, 
            maxADPUList);
        closesocket(clientRecvSocket);
    }
	if(clientSendSocket > 0)
	{
        closesocket(clientSendSocket);
	}
    
    Cell ret;
    ret.ival = iClientCount;  
    return ret;
}


int g_iClientCount = 0;
#define SERVICE_CHOICE_IAM_ROUTER    0x01
int dealWhoIsRouterResponse(struct sockaddr_in user_addr, unsigned char * buffer, int iLen, 
                 unsigned int  * ipArrayList, 
                 unsigned int  * networkNumberList)
{
    int i = 0;
    int iOffset = 0;
    printf("IP = %s. we Receive %d .\r\n", inet_ntoa(user_addr.sin_addr), iLen);
    printf("Type: %X. Function: %X. BVLC-Length: %X. ", 
        buffer[0], buffer[1], (int)convertShortFromBuffer(buffer + 2));
    printf("Version: %X. Control: %X.\r\n", buffer[4], buffer[5]);
    
    // Source specifier: SNET, SLEN and SADR absent
    if((buffer[5] & 0x08) == 0x00)   // 0x80
    {
        iOffset = 6;
        if(buffer[iOffset] == SERVICE_CHOICE_IAM_ROUTER)
        {
            iOffset++;
            int iNetworkNumberCount = iLen - iOffset;
            if(iNetworkNumberCount % 2 == 0)
            {
                iNetworkNumberCount = iNetworkNumberCount / 2;
                for(i =0; i < iNetworkNumberCount; i++)
                {
                    int iNetworkNumber = (int)convertShortFromBuffer(buffer + iOffset + 2 * i);
                    printf("Network Number: %d. \r\n", iNetworkNumber);
                    ipArrayList[g_iClientCount] = (unsigned int)(user_addr.sin_addr.s_addr);
                    networkNumberList[g_iClientCount] = iNetworkNumber;
                    g_iClientCount++;
                }
            }
            else
            {
                printf("Error iNetworkNumberCount with 0x%X.\r\n", iNetworkNumberCount);
                return 0;
            }
        }
        else
        {
            printf("Error SERVICE_CHOICE with 0x%X.\r\n", buffer[iOffset]);
            return 0;
        }
    }
    else if((buffer[5] & 0x08) == 0x08)   // 0x88
    {
        printf("Error Control: %X.\r\n", buffer[5]);
    }
    return iOffset;
}

int sendWhoIsRouterBroadcast(int iRetryCount,
                     int iSubNetworkID, 
                     socket_t clientSendSocket, socket_t clientRecvSocket, 
                     struct sockaddr_in my_addr, 
                     unsigned int  * ipArrayList, 
                     unsigned int  * networkNumberList)
{
    int i = 0;
    int iSendCount = 0;
    int iRecvCount = 0;
    unsigned char buf[MAXDATASIZE];
    unsigned int size;
    int iListLen = 0;
    
    g_iClientCount = 0;
    
    time_t current_time, now;
    
    struct sockaddr_in  recv_addr;
    bzero((char *)&recv_addr, sizeof(recv_addr));
    printf("Enter sendWhoIsRouterBroadcast with iSubNetworkID = %d\r\n", iSubNetworkID);
    for(i=0; i<iRetryCount; i++)
    {
        memset(buf, 0x00, MAXDATASIZE);
        memcpy(buf, WHOIS_ROUTER_To_NETWORK_PACKET, WHOIS_ROUTER_To_NETWORK_LENGTH);
        if(iSubNetworkID != 0)
        {
           buf[3]                                  = 0x09;
           buf[WHOIS_ROUTER_To_NETWORK_LENGTH]     = iSubNetworkID / 0x100;
           buf[WHOIS_ROUTER_To_NETWORK_LENGTH + 1] = iSubNetworkID % 0x100;
           
            printf("Enter sendto with iSubNetworkID = %d\r\n", iSubNetworkID);
           iSendCount = sendto(clientSendSocket, (char *)buf, 
                WHOIS_ROUTER_To_NETWORK_LENGTH + 2, 0, 
                (struct sockaddr *)&my_addr, sizeof(my_addr));
            
        }
        else
        {
            iSendCount = sendto(clientSendSocket, (char *)buf, 
                  WHOIS_ROUTER_To_NETWORK_LENGTH, 0, 
                  (struct sockaddr *)&my_addr, sizeof(my_addr));
        }
#ifdef _WIN32
        recvSleep(1);
#else
        recvSleep(0);
#endif
        // printf("send %d OK \r\n", iSendCount);
        size = sizeof(struct sockaddr_in);

        current_time = now = time(NULL);
        while(1)
        {
            memset(buf, 0x00, MAXDATASIZE);
            // printf("------------------try to  recvfrom------------------ \r\n");
            iRecvCount = recvfrom(clientRecvSocket, (char *)buf, 
                MAXDATASIZE, 0, (struct sockaddr *)&recv_addr, (socklen_t *)&size);
            if (iRecvCount == -1)
            {
            //    printf("recvfrom return -1 because of (%d):(%s) \r\n",
            //        errno, strerror(errno));
                now = time(NULL);
                if(now - current_time > RECV_TIMEOUT)
                {
                    // printf("we detect %d clients.\r\n", iClientCount);
                    break;
                }
                else
                {
                    recvSleep(1);
                    continue;
                }
            }
            
            if((iRecvCount == iSendCount)
                && (memcmp(buf, WHOIS_PACKET, WHOIS_LENGTH) == 0))
            {
                printf("Omit broadcast sent to me \r\n");
                continue;
            }
            else if (iRecvCount > 0)
            {
                // printf("we Receive %d .\r\n", iRecvCount);
                iListLen = dealWhoIsRouterResponse(recv_addr, buf, iRecvCount, 
                         ipArrayList, 
                         networkNumberList);
                g_iClientCount++;
                recvSleep(1);
            }
        }
        printf("-------------------(%d)----------------------- \r\n", i + 1);
    }
    return g_iClientCount;
}


Cell inet_UdpSocket_getBacnetRouterDeviceList(SedonaVM* vm, Cell* params)
{
    int iClientCount = 0;
  // void* self              = params[0].aval;
  char*     ipAddress               = params[1].aval;
  int32_t   port                    = params[2].ival;
  uint32_t* ipArrayList             = params[3].aval;
  uint32_t* networkNumberList       = params[4].aval;
  
  
    printf("Enter inet_UdpSocket_getBacnetRouterDeviceList(%s) \r\n", ipAddress);
    // unsigned int iListLen = 0;
    // unsigned int ipArrayList[10];
    // unsigned int objIDList[10];
    memset(ipArrayList, 0x00, sizeof(int) * 10);
    memset(networkNumberList, 0x00, sizeof(int) * 10);


    socket_t clientSendSocket;
    struct sockaddr_in  my_addr;
    // struct sockaddr_in  recv_addr;
    clientSendSocket = initializeSendSocket(ipAddress, port, &my_addr); // , &recv_addr);
    if (clientSendSocket > 0)
    {
        socket_t clientRecvSocket;
        // printf("Call sendBroadcast \r\n");
        clientRecvSocket = initializeRecvSocket(port);
        iClientCount = sendWhoIsRouterBroadcast(SEND_BROADCAST_TIMES, 0, 
            clientSendSocket, clientRecvSocket, my_addr, // recv_addr, 
            ipArrayList, 
            networkNumberList);
        closesocket(clientRecvSocket);
    }
	if(clientSendSocket > 0)
	{
        closesocket(clientSendSocket);
	}
    
    Cell ret;
    ret.ival = iClientCount - 1;  
    return ret;
}

Cell inet_UdpSocket_getBacnetRouterDevice(SedonaVM* vm, Cell* params)
{
    int iClientCount = 0;
  // void* self              = params[0].aval;
  char*     ipAddress               = params[1].aval;
  int32_t   port                    = params[2].ival;
  int32_t   iSubNetworkID           = params[3].ival;
  uint32_t* ipArrayList             = params[4].aval;
  uint32_t* networkNumberList       = params[5].aval;
  
  
    printf("Enter inet_UdpSocket_getBacnetRouterDevice(%s) \r\n", ipAddress);
    // unsigned int iListLen = 0;
    // unsigned int ipArrayList[10];
    // unsigned int objIDList[10];
    memset(ipArrayList, 0x00, sizeof(int) * 10);
    memset(networkNumberList, 0x00, sizeof(int) * 10);


    socket_t clientSendSocket;
    struct sockaddr_in  my_addr;
    // struct sockaddr_in  recv_addr;
    clientSendSocket = initializeSendSocket(ipAddress, port, &my_addr); // , &recv_addr);
    if (clientSendSocket > 0)
    {
        socket_t clientRecvSocket;
        // printf("Call sendBroadcast \r\n");
        clientRecvSocket = initializeRecvSocket(port);
        iClientCount = sendWhoIsRouterBroadcast(SEND_BROADCAST_TIMES, 
            iSubNetworkID, 
            clientSendSocket, clientRecvSocket, my_addr, // recv_addr, 
            ipArrayList, 
            networkNumberList);
        closesocket(clientRecvSocket);
    }
	if(clientSendSocket > 0)
	{
        closesocket(clientSendSocket);
	}
    
//    printf("clientSendSocket ipArrayList[0] = %u \r\n", ipArrayList[0]);
//    printf("clientSendSocket networkNumberList[0] = %d \r\n", networkNumberList[0]);
    Cell ret;
    ret.ival = iClientCount - 1;  
    return ret;
}




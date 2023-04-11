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
    return falseCell;
#endif

  // check for already initialized
  if (!closed || sock != -1) return falseCell;

  // create socket
#ifdef SOCKET_FAMILY_INET
  sock = socket(AF_INET, SOCK_DGRAM,  0);
#elif defined( SOCKET_FAMILY_INET6 )
  sock = socket(AF_INET6, SOCK_DGRAM,  0);
#endif

#ifdef _WIN32
  if (sock == INVALID_SOCKET) return falseCell;
#else
  if (sock < 0) return falseCell;
#endif

  // make socket non-blocking
  if (inet_setNonBlocking(sock) != 0)
  {
    closesocket(sock);
    return falseCell;
  }

  // udate UdpSocket instance
  setClosed(self, 0);
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

  if (!closed)
  {
    closesocket(sock);
    setClosed(self, 1);
    setSocket(self, -1);
  }
  return nullCell;
}

// Bacbet device list searching
#define PORT 47808
#define MAXDATASIZE 256

#define WHOIS_PACKET "\x81\x0a\x00\x0c\x01\x20\xff\xff\x00\xff\x10\x08"
#define WHOIS_LENGTH 12

#pragma pack(1)
// BACnet Virtual Link Control
typedef struct bacnet_head_t {
	unsigned char type;
	unsigned char function;
	unsigned short bvlcLength;
} bacnet_head;

typedef struct bacnet_NPDU_DstSpec_t {
	unsigned char  version;
	unsigned char  control;
	unsigned short dstAddr;
	unsigned char  dstAddrLen;
	unsigned char  hopCount;
} bacnet_NPDU_DstSpec;

typedef struct bacnet_NPDU_SrcSpec_t {
	unsigned char  version;
	unsigned char  control;
	unsigned short dstAddr;
	unsigned char  dstAddrLen;
	unsigned short srcAddr;
	unsigned char  srcAddrLen;
	unsigned char  sadr;
	unsigned char  hopCount;
} bacnet_NPDU_SrcSpec;

typedef struct bacnet_APDU_t {
	unsigned char  apnuType;
	unsigned char  serviceChoice;
	unsigned char  objectIdentifierLen;
	unsigned int   objectIdentifierAndType;
	unsigned char  apnuLenLen;
	unsigned short apnuLen;
	unsigned short segment;
	unsigned char  vendorIDLen;
	unsigned short vendorID;
} bacnet_APDU;

typedef struct bacnet_iam_dstSpec_t {
	bacnet_head         headObject;
	bacnet_NPDU_DstSpec npduDstObject;
	bacnet_APDU         apnuObject;
} bacnet_iam_dstSpec;

typedef struct bacnet_iam_srcSpec_t {
	bacnet_head         headObject;
	bacnet_NPDU_SrcSpec npduSrcObject;
	bacnet_APDU         apnuObject;
} bacnet_iam_srcSpec;
#pragma pack()

socket_t initializeSocket(char * networkIP, 
					struct sockaddr_in* my_addr, 
					struct sockaddr_in* user_addr)
{
	socket_t clientSocket;
	
	int so_broadcast=1;
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
	clientSocket = socket(AF_INET, SOCK_DGRAM, 0);

	my_addr->sin_family=AF_INET;
	my_addr->sin_port=htons(PORT);
	my_addr->sin_addr.s_addr=inet_addr("255.255.255.255");
	memset(&(my_addr->sin_zero), 0x00, 8);

	setsockopt(clientSocket, SOL_SOCKET, SO_BROADCAST, 
		(char *)&so_broadcast, sizeof(so_broadcast));

	inet_setNonBlocking(clientSocket);
	
	user_addr->sin_family=AF_INET;
	user_addr->sin_port=htons(PORT);
	// user_addr.sin_addr.s_addr=htonl(INADDR_ANY);
	user_addr->sin_addr.s_addr=inet_addr(networkIP);
	memset(&(user_addr->sin_zero), 0x00, 8);
	
	if((bind(clientSocket, (struct sockaddr *)user_addr,
									sizeof(struct sockaddr)))==-1)
	{
		return -1;
	}
	return clientSocket;
}

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

void copyBacnetIamDstSpec(bacnet_iam_dstSpec * objIAMDstPtr, unsigned char * buffer, int iLen)
{
	memcpy(objIAMDstPtr, buffer, iLen);
#ifdef _WIN32
	revertShort(objIAMDstPtr->headObject.bvlcLength);
	revertShort(objIAMDstPtr->npduDstObject.dstAddr);
	revertInt(objIAMDstPtr->apnuObject.objectIdentifierAndType);
	revertShort(objIAMDstPtr->apnuObject.apnuLen);
	revertShort(objIAMDstPtr->apnuObject.segment);
	revertShort(objIAMDstPtr->apnuObject.vendorID);
#endif
}


void copyBacnetIamSrcSpec(bacnet_iam_srcSpec * objIAMSrcPtr, unsigned char * buffer, int iLen)
{
	memcpy(objIAMSrcPtr, buffer, iLen);
#ifdef _WIN32
	revertShort(objIAMSrcPtr->headObject.bvlcLength);
	revertShort(objIAMSrcPtr->npduSrcObject.dstAddr);
	revertShort(objIAMSrcPtr->npduSrcObject.srcAddr);
	revertInt(objIAMSrcPtr->apnuObject.objectIdentifierAndType);
	revertShort(objIAMSrcPtr->apnuObject.apnuLen);
	revertShort(objIAMSrcPtr->apnuObject.segment);
	revertShort(objIAMSrcPtr->apnuObject.vendorID);
#endif
}

int dealResponse(struct sockaddr_in user_addr, unsigned char * buffer, int iLen, 
				 unsigned int * ipArrayList, unsigned int * objIDList, unsigned int iListLen)
{
	unsigned char control;
	bacnet_iam_dstSpec objIAMDst ;
	bacnet_iam_srcSpec objIAMSrc ;
	printf("IP = %s. we Receive %d .\r\n", inet_ntoa(user_addr.sin_addr), iLen);
	if(iLen == sizeof(objIAMDst))
	{
		copyBacnetIamDstSpec(&objIAMDst, buffer, iLen);
		control = objIAMDst.npduDstObject.control;
		printf("Receive bacnet_iam_dstSpec(%02X) with %d \r\n", control, iLen);
		printf("objectIdentifierType = %d \r\n", 
			objIAMDst.apnuObject.objectIdentifierAndType >> 22);
		printf("objectIdentifier = %d \r\n", 
			objIAMDst.apnuObject.objectIdentifierAndType & 0x3FFFFF);
		
	}
	else if(iLen == sizeof(objIAMSrc))
	{
		copyBacnetIamSrcSpec(&objIAMSrc, buffer, iLen);
		control = objIAMSrc.npduSrcObject.control;
		printf("Receive bacnet_iam_srcSpec(%02X) with %d \r\n", control, iLen);
		printf("Source Network Address: %d \r\n", objIAMSrc.npduSrcObject.srcAddr);
		printf("objectIdentifierType: %d \r\n", 
			objIAMSrc.apnuObject.objectIdentifierAndType >> 22);
		printf("objectIdentifier: %d \r\n", 
			objIAMSrc.apnuObject.objectIdentifierAndType & 0x3FFFFF);

		memcpy(&(ipArrayList[iListLen]), &(user_addr.sin_addr), sizeof(int));
		objIDList[iListLen] = objIAMSrc.apnuObject.objectIdentifierAndType & 0x3FFFFF;
		iListLen++;
	}
	
	return iListLen;
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

int sendBroadcast(int iRetryCount, socket_t clientSocket, 
					struct sockaddr_in my_addr, 
					struct sockaddr_in user_addr, 
					unsigned int * ipArrayList, unsigned int * objIDList)
{
    int i = 0;
	int iSendCount = 0;
	int iRecvCount = 0;
	unsigned char buf[MAXDATASIZE];
	unsigned int size;
	int iListLen = 0;
	
		printf("Enter sendBroadcast \r\n");
	for(i=0; i<iRetryCount; i++)
	{
		memset(buf, 0x00, MAXDATASIZE);
		memcpy(buf, WHOIS_PACKET, WHOIS_LENGTH);
		iSendCount = sendto(clientSocket, (char *)buf, WHOIS_LENGTH, 0, 
			(struct sockaddr *)&my_addr, sizeof(my_addr));
#ifdef _WIN32
		recvSleep(1);
#else
		recvSleep(5);
#endif
		printf("send %d OK \r\n", iSendCount);
		size = sizeof(user_addr);
		while(1)
		{
			memset(buf, 0x00, MAXDATASIZE);
			printf("try to  recvfrom \r\n");
			iRecvCount = recvfrom(clientSocket, (char *)buf, 
				MAXDATASIZE, 0, (struct sockaddr *)&user_addr, &size);
			if (iRecvCount == -1)
			{
				printf("recvfrom over \r\n");
				break;
			}
			if((iRecvCount == iSendCount)
				&& (memcmp(buf, WHOIS_PACKET, WHOIS_LENGTH) == 0))
			{
				printf("Omit broadcast sent to me \r\n");
				continue;
			}
			else if (iRecvCount > 0)
			{
				printf("we Receive %d .\r\n", iRecvCount);
				iListLen = dealResponse(user_addr, buf, iRecvCount, ipArrayList, objIDList, iListLen);
				recvSleep(1);
			}
		}
		printf("-------------------%d----------------------- \r\n", i + 1);
	}
	return iListLen;
}

#define  SEND_BROADCAST_TIMES    1
Cell inet_UdpSocket_getBacnetDeviceList(SedonaVM* vm, Cell* params)
{
  // void* self              = params[0].aval;
  char*     ipAddress      = params[1].aval;
  uint32_t* ipArrayList    = params[2].aval;
  uint32_t* objIDList      = params[3].aval;
  int32_t* iListLenPtr     = params[4].aval;
  
  
	printf("Enter inet_UdpSocket_getBacnetDeviceList(%s) \r\n", ipAddress);
	// unsigned int iListLen = 0;
	// unsigned int ipArrayList[10];
	// unsigned int objIDList[10];
	memset(ipArrayList, 0x00, sizeof(int) * 10);
	memset(objIDList, 0x00, sizeof(int) * 10);
	memset(iListLenPtr, 0x00, sizeof(int) * 1);


	socket_t clientSocket;
	struct sockaddr_in  my_addr;
	struct sockaddr_in  user_addr;
	clientSocket = initializeSocket(ipAddress, &my_addr, &user_addr);
	if (clientSocket > 0)
	{
		printf("Call sendBroadcast \r\n");
		*iListLenPtr = sendBroadcast(SEND_BROADCAST_TIMES, 
			clientSocket, my_addr, user_addr, 
			ipArrayList, objIDList);
	}
	closesocket(clientSocket);
	if (*iListLenPtr <= 0)
	{
    	return falseCell;
	}
	else
	{
    	return trueCell;
	}
}




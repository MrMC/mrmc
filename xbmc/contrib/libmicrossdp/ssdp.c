/*
 * libmicrossdp
 * Copyright 2014-2015 Alexander von Gluck IV <kallisti5@unixzen.com>
 * Released under the terms of the MIT license
 *
 * Authors:
 *  Alexander von Gluck IV <kallisti5@unixzen.com>
 */


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>

#include "ssdp.h"


void
ssdp_packettodevice(char* buffer, struct upnp_device* device)
{
	assert(device);

	// TODO: Could be more robust.

	char* match;
	// Match LOCATION / Location
	if ((match = strstr(buffer, "LOCATION:")) != NULL) {
		int len = strcspn(match, "\r\n");
		if (len >= SSDP_TXT_LEN - 10)
			len = SSDP_TXT_LEN - 10;
		strncpy(device->location, match + 10, len - 10);
	} else if ((match = strstr(buffer, "Location:")) != NULL) {
		int len = strcspn(match, "\r\n");
		if (len >= SSDP_TXT_LEN - 10)
			len = SSDP_TXT_LEN - 10;
		strncpy(device->location, match + 10, len - 10);
	}

        strncpy(device->response, buffer, SSDP_PACKET_BUFFER);
}


int
ssdp_discovery(int family, unsigned int category, struct upnp_device* devices)
{
	struct in_addr localInterface;
	struct sockaddr_in groupSock;
	struct sockaddr_in localSock;
	struct ip_mreq group;

	assert(devices);

	int udpSocket = socket(family, SOCK_DGRAM, 0);

	// Reuse socket
	int reuse = 1;
	setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
	// 100ms second timeout on recvfrom
	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
	char loopch = 0;
	setsockopt(udpSocket, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&loopch, sizeof(loopch));

	groupSock.sin_family = family;
	// TODO: Change for IPv6
	groupSock.sin_addr.s_addr = inet_addr("239.255.255.250");
	groupSock.sin_port = htons(1900);

	//localInterface.s_addr = inet_addr("10.80.65.13");
	//setsockopt(udpSocket, IPPROTO_IP, IP_MULTICAST_IF, (char*)&localInterface, sizeof(localInterface));

	localSock.sin_family = family;
	localSock.sin_port = htons(1900);
	localSock.sin_addr.s_addr = INADDR_ANY;
	
	bind(udpSocket, (struct sockaddr*)&localSock, sizeof(localSock));
	group.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
	//group.imr_interface.s_addr = inet_addr("10.80.65.13");

	setsockopt(udpSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group));

	// ssdp:all?

	int dcpIndex = 0;
	int count = 0;

	for (dcpIndex = 0; dcpIndex < (sizeof(kKnownDCP) / sizeof(kKnownDCP[0])); dcpIndex++) {
		// Skip dcp's not matching our category filter
		if (((kKnownDCP[dcpIndex].category & category) == 0) || count >= SSDP_MAX)
			continue;

		//printf("seeking: %s\n", kKnownDCP[dcpIndex].type);

		// form our query
		char search[SSDP_PACKET_BUFFER] = "\0";
		snprintf(search, SSDP_PACKET_BUFFER, "M-SEARCH * HTTP/1.1\r\nHOST: %s:%d\r\nST: %s\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\n\r\n",
			"239.255.255.250", 1900, kKnownDCP[dcpIndex].type);
		//printf("query:\n%s", search);

		// ask
		sendto(udpSocket, search, SSDP_PACKET_BUFFER, 0, (struct sockaddr*)&groupSock, sizeof(groupSock));

		// parse responses
		time_t start = time(NULL);
		while ((time(NULL) - start) < 3) {
			struct sockaddr_in si_other;
			socklen_t slen = sizeof(si_other);
			char buffer[SSDP_PACKET_BUFFER] = "";
			recvfrom(udpSocket, buffer, SSDP_PACKET_BUFFER, 0, (struct sockaddr*)&si_other, &slen);
			// HTTP/1.1's are responses. NOTIFY are random broadcasts
			if (strstr(buffer, "HTTP/1.1 200 OK") != NULL) {
				#if 1
				printf("------------------\n");
				printf("response:\n");
				printf("%s\n", buffer);
				#endif
				ssdp_packettodevice(buffer, &devices[count]);
				count++;
				// Reset timer when response
				start = time(NULL);
			}
		}
	}

	shutdown(udpSocket, 0);

	return count;
}

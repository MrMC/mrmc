/*
 * libmicrossdp
 * Copyright 2014-2015 Alexander von Gluck IV <kallisti5@unixzen.com>
 * Released under the terms of the MIT license
 *
 * Authors:
 *	Alexander von Gluck IV <kallisti5@unixzen.com>
 */
#ifndef INCLUDE_SSDP_H
#define INCLUDE_SSDP_H


/* Category of device */
#define SSDP_CAT_NONE				(0 << 0)
#define SSDP_CAT_AV					(1)
#define SSDP_CAT_DATASTORE			(1 << 1)
#define SSDP_CAT_DEVICEMANAGEMENT	(1 << 2)
#define SSDP_CAT_FRIENDLYDEVICES	(1 << 3)
#define SSDP_CAT_HOMEAUTOMATION		(1 << 4)
#define SSDP_CAT_IOTMANAGEMENT		(1 << 5)
#define SSDP_CAT_MULTISCREEN		(1 << 6)
#define SSDP_CAT_NETWORKING			(1 << 7)
#define SSDP_CAT_TELEPHONY			(1 << 8)	
#define SSDP_CAT_PRINTER			(1 << 9)	
#define SSDP_CAT_REMOTEACCESS		(1 << 10)	
#define SSDP_CAT_REMOTING			(1 << 11)	
#define SSDP_CAT_SCANNER			(1 << 12)	
#define SSDP_CAT_CONTENTSYNC		(1 << 13)	
#define SSDP_CAT_SECURITY			(1 << 14)	
#define SSDP_CAT_LOWPOWER			(1 << 15)	
#define SSDP_CAT_QOS				(1 << 16)	
#define SSDP_CAT_BASIC				(1 << 17)

#define SSDP_PACKET_BUFFER			1024
#define SSDP_TXT_LEN				512

#define SSDP_MAX					255

/* Device definitions */
const struct upnp_dcp {
	unsigned int	category;
	const char*		type;
} kKnownDCP[] = {
	{SSDP_CAT_AV,				"urn:schemas-upnp-org:device:MediaServer:1"},
	{SSDP_CAT_AV,				"urn:schemas-upnp-org:device:MediaServer:2"},
	{SSDP_CAT_AV,				"urn:schemas-upnp-org:device:MediaServer:3"},
	{SSDP_CAT_AV,				"urn:schemas-upnp-org:device:MediaServer:4"},
	{SSDP_CAT_AV,				"urn:schemas-upnp-org:device:MediaRenderer:1"},
	{SSDP_CAT_AV,				"urn:schemas-upnp-org:device:MediaRenderer:2"},
	{SSDP_CAT_AV,				"urn:schemas-upnp-org:device:MediaRenderer:3"},
	{SSDP_CAT_AV,				"roku:ecp"},
	{SSDP_CAT_BASIC,			"urn:schemas-upnp-org:device:Basic:1"},
	{SSDP_CAT_DATASTORE,		"urn:schemas-upnp-org:device:DataStore:1"},
	{SSDP_CAT_DEVICEMANAGEMENT,	"urn:schemas-upnp-org:device:DeviceManagment:1"},
	{SSDP_CAT_DEVICEMANAGEMENT,	"urn:schemas-upnp-org:device:DeviceManagment:2"},
	{SSDP_CAT_HOMEAUTOMATION,	"urn:schemas-upnp-org:device:SolarProtectionBlind:1"},
	{SSDP_CAT_HOMEAUTOMATION,	"urn:schemas-upnp-org:device:LightingControls:1"},
	{SSDP_CAT_HOMEAUTOMATION,	"urn:schemas-upnp-org:device:HVAC:1"},
	{SSDP_CAT_HOMEAUTOMATION,	"urn:schemas-upnp-org:device:DigitalSecurityCamera:1"},
	{SSDP_CAT_IOTMANAGEMENT,	"urn:schemas-upnp-org:device:IoTManagementAndControlArchitecture:1"},
	{SSDP_CAT_MULTISCREEN,		"urn:schemas-upnp-org:device:ScreenDevice:1"},
	{SSDP_CAT_MULTISCREEN,		"urn:schemas-upnp-org:device:ScreenDevice:2"},
	// TODO: Networking
	{SSDP_CAT_TELEPHONY,		"urn:schemas-upnp-org:device:Telephony:1"},
	{SSDP_CAT_TELEPHONY,		"urn:schemas-upnp-org:device:Telephony:2"},
	{SSDP_CAT_PRINTER,			"urn:schemas-upnp-org:device:Printer:1"},
	// TODO: Remote Access
	// TODO: Remoting
	{SSDP_CAT_SCANNER,			"urn:schemas-upnp-org:device:Scan:1"}
	// TODO: Content Sync
	// TODO: Security
	// TODO: Low Power
	// TODO: QOS
};


struct upnp_device {
	unsigned int    category;
	char			response[SSDP_PACKET_BUFFER];
	char			location[SSDP_TXT_LEN];
};

#ifdef __cplusplus
extern "C" {
#endif

int ssdp_discovery(int family, unsigned int category, struct upnp_device* devices);

#ifdef __cplusplus
}
#endif

#endif /*INCLUDE_SSDP_H*/

//========================================================================
// GLFW 3.1 OS X - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2010 Camilla Berglund <elmindreda@elmindreda.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

#include <stdlib.h>
#include <limits.h>

#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <CoreVideo/CVBase.h>
#include <CoreVideo/CVDisplayLink.h>

#include "utils/log.h"

// Returns the io_service_t corresponding to a CG display ID, or 0 on failure.
// The io_service_t should be released with IOObjectRelease when not needed.
//
io_service_t IOServicePortFromCGDisplayID(CGDirectDisplayID displayID)
{
    io_iterator_t iter;
    io_service_t serv, servicePort = 0;
    
    CFMutableDictionaryRef matching = IOServiceMatching("IODisplayConnect");
    
    // releases matching for us
    kern_return_t err = IOServiceGetMatchingServices(kIOMasterPortDefault,
                             matching,
                             &iter);
    if (err)
    {
        return 0;
    }
    
    while ((serv = IOIteratorNext(iter)) != 0)
    {
        CFDictionaryRef info;
        CFIndex vendorID, productID;
        CFNumberRef vendorIDRef, productIDRef;
        Boolean success;
        
        info = IODisplayCreateInfoDictionary(serv,
                             kIODisplayOnlyPreferredName);
        
        vendorIDRef = (CFNumberRef)CFDictionaryGetValue(info,
                           CFSTR(kDisplayVendorID));
        productIDRef = (CFNumberRef)CFDictionaryGetValue(info,
                            CFSTR(kDisplayProductID));
        
        success = CFNumberGetValue(vendorIDRef, kCFNumberCFIndexType,
                                   &vendorID);
        success &= CFNumberGetValue(productIDRef, kCFNumberCFIndexType,
                                    &productID);

        if (!success)
        {
            CFRelease(info);
            continue;
        }
        
        if (CGDisplayVendorNumber(displayID) != vendorID ||
            CGDisplayModelNumber(displayID) != productID)
        {
            CFRelease(info);
            continue;
        }
        
        // we're a match
        servicePort = serv;
        CFRelease(info);
        break;
    }
    
    IOObjectRelease(iter);
    return servicePort;
}

// Get the name of the specified display
//
static const char* getDisplayName(CGDirectDisplayID displayID)
{
    char* name;
    CFDictionaryRef info, names;
    CFStringRef value;
    CFIndex size;
    
    io_service_t serv = IOServicePortFromCGDisplayID(displayID);
    if (!serv)
    {
        return strdup("Unknown");
    }
    
    info = IODisplayCreateInfoDictionary(serv,
                         kIODisplayOnlyPreferredName);
    
    IOObjectRelease(serv);
    
    names = (CFDictionaryRef)CFDictionaryGetValue(info, CFSTR(kDisplayProductName));
    
    if (!names || !CFDictionaryGetValueIfPresent(names, CFSTR("en_US"),
                             (const void**) &value))
    {
        // This may happen if a desktop Mac is running headless
      CLog::Log(LOGDEBUG, "Failed to retrieve display name");
        CFRelease(info);
        return strdup("Unknown");
    }
    
    size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value),
                         kCFStringEncodingUTF8);
    name = (char*)calloc(size + 1, sizeof(char));
    CFStringGetCString(value, name, size, kCFStringEncodingUTF8);
    
    CFRelease(info);
    
    return name;
}


// Starts reservation for display fading
//
static CGDisplayFadeReservationToken beginFadeReservation(void)
{
    CGDisplayFadeReservationToken token = kCGDisplayFadeReservationInvalidToken;

    if (CGAcquireDisplayFadeReservation(5, &token) == kCGErrorSuccess)
        CGDisplayFade(token, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0.0, 0.0, 0.0, TRUE);

    return token;
}

// Ends reservation for display fading
//
static void endFadeReservation(CGDisplayFadeReservationToken token)
{
    if (token != kCGDisplayFadeReservationInvalidToken)
    {
        CGDisplayFade(token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, FALSE);
        CGReleaseDisplayFadeReservation(token);
    }
}

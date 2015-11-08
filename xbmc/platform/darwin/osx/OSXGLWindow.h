#pragma once

/*
 *      Copyright (C) 2010-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#import <Cocoa/Cocoa.h>

@interface OSXGLWindow : NSWindow <NSWindowDelegate>
{
}

+(void) SetMenuBarVisible;
+(void) SetMenuBarInvisible;

-(id) initWithContentRect:(NSRect)box styleMask:(uint)style;
-(void) dealloc;
-(BOOL) windowShouldClose:(id) sender;
-(void) windowDidExpose:(NSNotification *) aNotification;
-(void) windowDidMove:(NSNotification *) aNotification;
-(void) windowDidResize:(NSNotification *) aNotification;
-(void) windowDidMiniaturize:(NSNotification *) aNotification;
-(void) windowDidDeminiaturize:(NSNotification *) aNotification;
-(void) windowDidBecomeKey:(NSNotification *) aNotification;
-(void) windowDidResignKey:(NSNotification *) aNotification;
-(NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize;
-(void) windowWillStartLiveResize:(NSNotification *)aNotification;
-(void) windowDidEndLiveResize:(NSNotification *)aNotification;
-(void) windowDidEnterFullScreen: (NSNotification*)pNotification;
-(void) windowWillEnterFullScreen: (NSNotification*)pNotification;
-(void) windowDidExitFullScreen: (NSNotification*)pNotification;
-(void) windowWillExitFullScreen: (NSNotification*)pNotification;
-(void) windowDidChangeScreen:(NSNotification *)notification;

/* Window event handling */
-(void) mouseDown:(NSEvent *) theEvent;
-(void) rightMouseDown:(NSEvent *) theEvent;
-(void) otherMouseDown:(NSEvent *) theEvent;
-(void) mouseUp:(NSEvent *) theEvent;
-(void) rightMouseUp:(NSEvent *) theEvent;
-(void) otherMouseUp:(NSEvent *) theEvent;
-(void) mouseMoved:(NSEvent *) theEvent;
-(void) mouseDragged:(NSEvent *) theEvent;
-(void) rightMouseDragged:(NSEvent *) theEvent;
-(void) otherMouseDragged:(NSEvent *) theEvent;
-(void) scrollWheel:(NSEvent *) theEvent;

- (BOOL) canBecomeKeyWindow;
@end

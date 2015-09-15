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

#include "system_gl.h"
#include "platform/darwin/osx/CocoaInterface.h"

#import "OSXGLView.h"

@implementation OSXGLView

- (id)initWithFrame: (NSRect)frameRect
{
  NSOpenGLPixelFormatAttribute wattrs[] =
  {
    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFANoRecovery,
    NSOpenGLPFAAccelerated,
    NSOpenGLPFAColorSize,           (NSOpenGLPixelFormatAttribute)32,
    NSOpenGLPFAAlphaSize,           (NSOpenGLPixelFormatAttribute)8,
    NSOpenGLPFADepthSize,           (NSOpenGLPixelFormatAttribute)24,
    (NSOpenGLPixelFormatAttribute) 0
  };
  
  self = [super initWithFrame: frameRect];
  if (self)
  {
    m_pixFmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:wattrs];
    m_glcontext = [[NSOpenGLContext alloc] initWithFormat:m_pixFmt shareContext:nil];

    GLint swapInterval = 1;
    [m_glcontext setValues:&swapInterval forParameter:NSOpenGLCPSwapInterval];

    m_trackingArea = nullptr;
    [self updateTrackingAreas];
  }
  
  return self;
}

- (void)dealloc
{
  //NSLog(@"OSXGLView dealoc");
  if (m_trackingArea != nullptr)
  {
    [self removeTrackingArea:m_trackingArea];
    [m_trackingArea release], m_trackingArea = nullptr;
  }

  [NSOpenGLContext clearCurrentContext];
  [m_glcontext clearDrawable];
  [m_glcontext release];
  [m_pixFmt release];

  [super dealloc];
}

- (void)drawRect:(NSRect)rect
{
  static BOOL firstRender = YES;
  if (firstRender)
  {
    //NSLog(@"OSXGLView drawRect setView");
    [m_glcontext setView:self];
    firstRender = NO;
    
    // clear screen on first render
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0, 0, 0, 0);

    //[m_glcontext update];
  }
}

-(void)updateTrackingAreas
{
  //NSLog(@"updateTrackingAreas");
  if (m_trackingArea != nullptr)
  {
    [self removeTrackingArea:m_trackingArea];
    [m_trackingArea release];
  }
  
  const int opts = (NSTrackingMouseEnteredAndExited |
                    NSTrackingMouseMoved |
                    NSTrackingActiveAlways);
  m_trackingArea = [ [NSTrackingArea alloc] initWithRect:[self bounds]
                                                 options:opts
                                                   owner:self
                                                userInfo:nil];
  [self addTrackingArea:m_trackingArea];
  [super updateTrackingAreas];
}

- (void)mouseEntered:(NSEvent*)theEvent
{
  //NSLog(@"mouseEntered");
  Cocoa_HideMouse();
  [self displayIfNeeded];
}

- (void)mouseMoved:(NSEvent*)theEvent
{
  //NSLog(@"mouseMoved");
  [self displayIfNeeded];
}

- (void)mouseExited:(NSEvent*)theEvent
{
  //NSLog(@"mouseExited");
  Cocoa_ShowMouse();
  [self displayIfNeeded];
}

- (NSOpenGLContext *)getGLContext
{
  return m_glcontext;
}
@end

/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "ActorProtocol.h"

#define USE_FREE_MESSAGE_QUEUE 0

using namespace Actor;

Message::Message()
 : isSync(false)
 , data(nullptr)
 , replyMessage(nullptr)
 , event(nullptr)
{
}

Message::~Message()
{
  if (data && data != buffer)
    delete [] data;
  data = nullptr;

  if (event)
    delete event, event = nullptr;
}

void Message::Release()
{
  bool skip;
  origin->Lock();
  skip = isSync ? !isSyncFini : false;
  isSyncFini = true;
  origin->Unlock();

  if (skip)
    return;

  // free data buffer
  if (data && data != buffer)
    delete [] data;
  data = nullptr;

  // delete event in case of sync message
  if (event)
    delete event, event = nullptr;

  origin->ReturnMessage(this);
}

bool Message::Reply(int sig, void *data /* = NULL*/, int size /* = 0 */)
{
  if (!isSync)
  {
    if (isOut)
      return origin->SendInMessage(sig, data, size);
    else
      return origin->SendOutMessage(sig, data, size);
  }

  origin->Lock();

  if (!isSyncTimeout)
  {
    Message *msg = origin->GetMessage();
    msg->signal = sig;
    msg->isOut = !isOut;
    replyMessage = msg;
    if (data)
    {
      if (size > MSG_INTERNAL_BUFFER_SIZE)
        msg->data = new uint8_t[size];
      else
        msg->data = msg->buffer;
      memcpy(msg->data, data, size);
    }
  }

  origin->Unlock();

  if (event)
    event->Set();

  return true;
}

Protocol::Protocol(std::string name, CEvent *inEvent, CEvent *outEvent)
 : portName(name)
 , containerInEvent(inEvent)
 , containerOutEvent(outEvent)
 , inDefered(false)
 , outDefered(false)
{
}

Protocol::~Protocol()
{
  Purge();
#if USE_FREE_MESSAGE_QUEUE
  while (!freeMessageQueue.empty())
  {
    Message *msg = freeMessageQueue.front();
    freeMessageQueue.pop();
    delete msg;
  }
#endif
}

Message *Protocol::GetMessage()
{
  Message *msg;

  CSingleLock lock(criticalSection);

#if USE_FREE_MESSAGE_QUEUE
  if (!freeMessageQueue.empty())
  {
    msg = freeMessageQueue.front();
    freeMessageQueue.pop();
  }
  else
#endif
    msg = new Message();

  msg->isSync = false;
  msg->isSyncFini = false;
  msg->isSyncTimeout = false;
#if USE_FREE_MESSAGE_QUEUE
  if (msg->data && msg->data != msg->buffer)
    delete [] msg->data;
#endif
  msg->data = nullptr;
  msg->event = nullptr;
  msg->payloadSize = 0;
  msg->replyMessage = nullptr;
  msg->origin = this;

  return msg;
}

void Protocol::ReturnMessage(Message *msg)
{
  CSingleLock lock(criticalSection);

#if USE_FREE_MESSAGE_QUEUE
  freeMessageQueue.push(msg);
#else
  delete msg;
#endif
}

bool Protocol::SendOutMessage(int signal, void *data /* = NULL */, int size /* = 0 */, Message *outMsg /* = NULL */)
{
  Message *msg;
  if (outMsg)
    msg = outMsg;
  else
    msg = GetMessage();

  msg->signal = signal;
  msg->isOut = true;

  if (data)
  {
    if (size > MSG_INTERNAL_BUFFER_SIZE)
      msg->data = new uint8_t[size];
    else
      msg->data = msg->buffer;
    memcpy(msg->data, data, size);
  }

  {
    CSingleLock lock(criticalSection);
    outMessages.push(msg);
  }
  containerOutEvent->Set();

  return true;
}

bool Protocol::SendInMessage(int signal, void *data /* = NULL */, int size /* = 0 */, Message *outMsg /* = NULL */)
{
  Message *msg;
  if (outMsg)
    msg = outMsg;
  else
    msg = GetMessage();

  msg->signal = signal;
  msg->isOut = false;

  if (data)
  {
    if (size > MSG_INTERNAL_BUFFER_SIZE)
      msg->data = new uint8_t[size];
    else
      msg->data = msg->buffer;
    memcpy(msg->data, data, size);
  }

  {
    CSingleLock lock(criticalSection);
    inMessages.push(msg);
  }
  containerInEvent->Set();

  return true;
}


bool Protocol::SendOutMessageSync(int signal, Message **retMsg, int timeout, void *data /* = NULL */, int size /* = 0 */)
{
  Message *msg = GetMessage();
  msg->isOut = true;
  msg->isSync = true;
  msg->event = new CEvent;
  msg->event->Reset();
  SendOutMessage(signal, data, size, msg);

  if (!msg->event->WaitMSec(timeout))
  {
    msg->origin->Lock();
    if (msg->replyMessage)
      *retMsg = msg->replyMessage;
    else
    {
      *retMsg = nullptr;
      msg->isSyncTimeout = true;
    }
    msg->origin->Unlock();
  }
  else
    *retMsg = msg->replyMessage;

  msg->Release();

  if (*retMsg)
    return true;
  else
    return false;
}

bool Protocol::ReceiveOutMessage(Message **msg)
{
  CSingleLock lock(criticalSection);

  if (outMessages.empty() || outDefered)
    return false;

  *msg = outMessages.front();
  outMessages.pop();

  return true;
}

bool Protocol::ReceiveInMessage(Message **msg)
{
  CSingleLock lock(criticalSection);

  if (inMessages.empty() || inDefered)
    return false;

  *msg = inMessages.front();
  inMessages.pop();

  return true;
}


void Protocol::Purge()
{
  Message *msg;

  while (ReceiveInMessage(&msg))
    msg->Release();

  while (ReceiveOutMessage(&msg))
    msg->Release();
}

void Protocol::PurgeIn(int signal)
{
  std::queue<Message*> msgs;

  CSingleLock lock(criticalSection);

  while (!inMessages.empty())
  {
    Message *msg = inMessages.front();
    inMessages.pop();
    if (msg->signal != signal)
      msgs.push(msg);
  }
  while (!msgs.empty())
  {
    Message *msg = msgs.front();
    msgs.pop();
    inMessages.push(msg);
  }
}

void Protocol::PurgeOut(int signal)
{
  std::queue<Message*> msgs;

  CSingleLock lock(criticalSection);

  while (!outMessages.empty())
  {
    Message *msg = outMessages.front();
    outMessages.pop();
    if (msg->signal != signal)
      msgs.push(msg);
  }
  while (!msgs.empty())
  {
    Message *msg = msgs.front();
    msgs.pop();
    outMessages.push(msg);
  }
}

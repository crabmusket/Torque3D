//-----------------------------------------------------------------------------
// Copyright (c) 2012 Daniel Buckmaster
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------

#ifndef _AIACTION_H_
#define _AIACTION_H_

#include "console/scriptObjects.h"
#include "platform/types.h"

class AIAction : public ScriptObject
{
   typedef ScriptObject Parent;

public:
   /// @name Status
   /// @{

   /// Statuses an Action can take on.
   enum Status {
      Stopped,
      Failed,
      Waiting,
      Working,
      Complete,
      NumStatuses
   };

   /// Convert a string name to a Status.
   static Status getStatus(const char *s);
   /// Convert a Status to a string.
   static const char *getStatusName(S32 status);

   /// @}

   /// @name AIAction
   /// @{

   AIAction();
   ~AIAction();

   /// Character 'resource' that we make use of.
   StringTableEntry resource;

   /// Is this action happy to wait in the queue?
   bool allowWait;

   /// Does this action respond to special events?
   bool receiveEvents;

   /// Called when this action is begun, either for the first time or after being put on hold.
   virtual void start(SimObject *obj, const char *data, bool resume);
   /// Called each 'tick'.
   virtual Status update(SimObject *obj, const char *data, F32 time);
   /// Called when an event happens in the BehaviorManager. Should not be called if receiveEvents is false.
   virtual Status event(SimObject *obj, const char *data, const char *event, const char *evtData);
   /// Called when the action is put on hold or ended.
   virtual void end(SimObject *obj, const char *data, AIAction::Status status);

   /// @}

   DECLARE_CALLBACK(void, onStart, (SimObjectId obj, const char *data, bool resume));
   DECLARE_CALLBACK(StringTableEntry, onUpdate, (SimObjectId obj, const char *data, F32 time));
   DECLARE_CALLBACK(StringTableEntry, onEvent, (SimObjectId obj, const char *data, const char *event, const char *evtData));
   DECLARE_CALLBACK(void, onEnd, (SimObjectId obj, const char *data, const char *status));

   /// @name Inherited
   /// @{

   bool onAdd();
   void onRemove();

   static void initPersistFields();

   DECLARE_CONOBJECT(AIAction);

   /// @}

protected:
private:
};

#endif // _AIACTION_H_

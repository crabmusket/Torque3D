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

#include "aiAction.h"
#include "console/engineAPI.h"

typedef AIAction::Status AIActionStatus;
DefineEnumType(AIActionStatus);

ImplementEnumType( AIActionStatus,
   "The possible states an AIAction can be in.\n")
   { AIAction::Stopped,  "Stopped",  "AIAction stopped by the manager.\n" },
   { AIAction::Failed,   "Failed",   "AIAction knowingly failed to execute.\n" },
   { AIAction::Waiting,  "Waiting",  "AIAction is waiting in the queue.\n" },
   { AIAction::Working,  "Working",  "AIAction is in progress.\n" },
   { AIAction::Complete, "Complete", "AIAction has completed successfully to the best of our knowledge.\n" },
EndImplementEnumType;

IMPLEMENT_CONOBJECT(AIAction);

AIAction::AIAction()
{
   resource = NULL;
   allowWait = true;
   receiveEvents = false;
}

AIAction::~AIAction()
{
}

void AIAction::initPersistFields()
{
   addGroup("Action parameters");

   addField("resource", TypeString, Offset(resource, AIAction),
      "The resource that this action occupies while being executed.");

   addField("allowWait", TypeBool, Offset(allowWait, AIAction),
      "Will this action wait in the queue?");

   addField("receiveEvents", TypeBool, Offset(receiveEvents, AIAction),
      "Will this action respond to events?");

   endGroup("Action parameters");

   Parent::initPersistFields();
}

bool AIAction::onAdd()
{
   if(!Parent::onAdd())
      return false;

   if(!resource)
      resource = StringTable->insert("");

   return true;
}

void AIAction::onRemove()
{
   Parent::onRemove();
}

void AIAction::start(SimObject *obj, const char *data, bool resume)
{
   onStart_callback(obj? obj->getId(): 0, data ? data : "", resume);
}

AIAction::Status AIAction::update(SimObject *obj, const char *data, F32 time)
{
   StringTableEntry result = onUpdate_callback(obj? obj->getId(): 0, data ? data : "", time);
   Status s = getStatus(result);
   if(s != Failed && s != Working && s != Complete)
   {
      Con::warnf("Warning: AIAction::update returned %s; counting as failure.", result);
      s = Failed;
   }
   return s;
}

AIAction::Status AIAction::event(SimObject *obj, const char *data, const char *event)
{
   StringTableEntry result = onEvent_callback(obj? obj->getId(): 0, data ? data : "", event);
   Status s = getStatus(result);
   if(s != Failed && s != Working && s != Complete)
   {
      Con::warnf("Warning: AIAction::event returned %s; counting as failure.", result);
      s = Failed;
   }
   return s;
}

void AIAction::end(SimObject *obj, const char *data, Status status)
{
   onEnd_callback(obj? obj->getId(): 0, data ? data : "", getStatusName(status));
}

IMPLEMENT_CALLBACK(AIAction, onStart, void, (SimObjectId obj, const char *data, bool resume), (obj, data, resume),
                   "Called when this action starts to execute within a BehaviorManager.");
IMPLEMENT_CALLBACK(AIAction, onUpdate, StringTableEntry, (SimObjectId obj, const char *data, F32 time), (obj, data, time),
                   "Called every time this action is updated. Must return one of 'Completed', 'Failed', and 'Working'.");
IMPLEMENT_CALLBACK(AIAction, onEvent, StringTableEntry, (SimObjectId obj, const char *data, const char *event), (obj, data, event),
                   "Called when a special event occurs.");
IMPLEMENT_CALLBACK(AIAction, onEnd, void, (SimObjectId obj, const char *data, const char *status), (obj, data, status),
                   "Called when this action is ended by the BehaviorManager.");

AIAction::Status AIAction::getStatus(const char *s)
{
   const EngineEnumTable& enums = *( TYPE< AIActionStatus >()->getEnumTable() );
   const U32 numValues = enums.getNumValues();
   for(U32 i = 0; i < numValues; i++)
      if(dStricmp(s, enums[i].mName) == 0)
         return (Status)enums[i].getInt();
   return NumStatuses;
}

const char *AIAction::getStatusName(S32 status)
{
   const EngineEnumTable& enums = *( TYPE< AIActionStatus >()->getEnumTable() );
   const U32 numValues = enums.getNumValues();
   for(U32 i = 0; i < numValues; i++)
      if(enums[i].getInt() == status)
         return enums[i].getName();
   return "";
}

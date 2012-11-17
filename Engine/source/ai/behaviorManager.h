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

#ifndef _BEHAVIORMANAGER_H_
#define _BEHAVIORMANAGER_H_

#include "console/scriptObjects.h"
#include "platform/types.h"

#include "aiAction.h"

#include <map>
#include <vector>

class BehaviorManager : public ScriptObject
{
   typedef ScriptObject Parent;

public:
   bool startAction(AIAction *action, F32 priority, const char *data = NULL, SimObject *from = NULL);
   void stopAction(AIAction *action, const char *data = NULL);
   void stopActionsFrom(SimObject *from);
   void stopAll();

   void event(const char *name);

   enum Constants {
      MaxResources = 8,
   };
   StringTableEntry mResourceNames[MaxResources];
   
   BehaviorManager();
   ~BehaviorManager();

   bool onAdd();
   void onRemove();

   static void initPersistFields();

   DECLARE_CONOBJECT(BehaviorManager);

protected:
private:
   struct ActionInstance {
      bool waiting;
      SimObjectPtr<SimObject> from;
      StringTableEntry data;
      AIAction *action;
      F32 priority;
      ActionInstance(AIAction *ac, F32 p, StringTableEntry d, SimObject *f)
      {
         from = f;
         data = d;
         action = ac;
         priority = p;
         waiting = false;
      }
      bool operator<(const ActionInstance &rhs) const
      { return priority < rhs.priority; }
   };
   typedef std::vector<ActionInstance> ActionQueue;
   typedef std::map<StringTableEntry, ActionQueue> ActionMap;

   /// Action queues for all resources we provide.
   ActionMap mResources;

   /// Stops actions being added while updates are going on.
   bool mLocked;

   void _endAction(ActionInstance &ac, AIAction::Status s);
   void _startAction(ActionInstance &ac);
};

#endif // _BEHAVIORMANAGER_H_

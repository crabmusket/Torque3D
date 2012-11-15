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

#include "behaviorManager.h"
#include "console/engineAPI.h"

#include <algorithm>

IMPLEMENT_CONOBJECT(BehaviorManager);

BehaviorManager::BehaviorManager()
{
   for (U32 i = 0; i < MaxResources; i++)
      mResourceNames[i] = NULL;
}

BehaviorManager::~BehaviorManager()
{
}

bool BehaviorManager::onAdd()
{
   if(!Parent::onAdd())
      return false;

   for (U32 i = 0; i < MaxResources; i++)
   {
      if (mResourceNames[i])
         mResources[mResourceNames[i]] = ActionQueue();
   }

   return true;
}

void BehaviorManager::onRemove()
{
   Parent::onRemove();
}

void BehaviorManager::initPersistFields()
{
   addGroup("Actions");

   addField("resources", TypeString, Offset(mResourceNames, BehaviorManager), MaxResources,
      "The names of the resources available to this BehaviorManager.");

   endGroup("Actions");

   Parent::initPersistFields();
}

bool BehaviorManager::startAction(AIAction *action, F32 priority, const char *data, SimObject *from)
{
   if (action == NULL)
      return false;

   ActionMap::iterator res = mResources.find(action->resource);
   if (res == mResources.end())
      return false;

   ActionInstance instance(action, priority, data, from);

   ActionQueue &queue = res->second;
   U32 size = queue.size();
   if (size)
   {
      // Check whether this action displaces the currently-running action.
      if (priority < queue[0].priority)
      {
         // It doesn't, so stick it somewhere in the queue if it can wait.
         if (action->allowWait)
         {
            queue.push_back(instance);
            std::stable_sort(queue.begin(), queue.end());
         }
         else
         {
            return false;
         }
      }
      else
      {
         // Is the current action happy to wait in the queue?
         if (queue[0].action->allowWait)
         {
            // Make the currently-working instance wait, because it's going to be replaced.
            queue[0].action->end(NULL, queue[0].data, AIAction::Waiting);
            // Add the new instance.
            queue.insert(queue.begin(), instance);
         }
         else
         {
            // Not happy to be replaced, so just end it.
            queue[0].action->end(NULL, queue[0].data, AIAction::Stopped);
            // And replace it with the new action.
            queue[0] = instance;
         }
         // Start up the incoming action.
         queue[0].action->start(NULL, data, false);
      }
   }
   else
   {
      // Push new instance and start it up.
      queue.push_back(instance);
      queue[0].action->start(NULL, data, false);
   }

   return true;
}

DefineEngineMethod(BehaviorManager, startAction, bool, (AIAction *action, F32 priority, const char *data, SimObject *from), (NULL, NULL),
   "Start running an action with a given priority and data payload.")
{
   return object->startAction(action, priority, data, from);
}

void BehaviorManager::stopAction(AIAction *action, const char *data)
{
}

void BehaviorManager::stopActionsFrom(SimObject *from)
{
}
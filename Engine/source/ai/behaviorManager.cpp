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
   mLocked = false;
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
   stopAll();

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
   if (action == NULL || mLocked)
      return false;

   ActionMap::iterator res = mResources.find(action->resource);
   if (res == mResources.end())
      return false;

   mLocked = true;

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
            // Couldn't wait, so just fail.
            mLocked = false;
            return false;
         }
      }
      else
      {
         // Is the current action happy to wait in the queue?
         if (queue[0].action->allowWait)
         {
            // Make the currently-working instance wait, because it's going to be replaced.
            _endAction(*queue.begin(), AIAction::Waiting);
            // Add the new instance.
            queue.insert(queue.begin(), instance);
         }
         else
         {
            // Not happy to be replaced, so just end it.
            _endAction(*queue.begin(), AIAction::Stopped);
            // And replace it with the new action.
            *queue.begin() = instance;
         }
         // Start up the incoming action.
         _startAction(*queue.begin());
      }
   }
   else
   {
      // Push new instance and start it up.
      queue.push_back(instance);
      _startAction(*queue.begin());
   }

   mLocked = false;

   return true;
}

DefineEngineMethod(BehaviorManager, startAction, bool, (AIAction *action, F32 priority, const char *data, SimObject *from), (NULL, NULL),
   "Start running an action with a given priority and data payload.")
{
   return object->startAction(action, priority, data, from);
}

void BehaviorManager::stopAction(AIAction *action, const char *data)
{
   if (!action || mLocked)
      return;

   mLocked = true;

   for (ActionMap::iterator res = mResources.begin(); res != mResources.end(); res++)
   {
      // Search all actions in the queue for the right one.
      ActionQueue &queue = res->second;
      ActionQueue::iterator ac = queue.begin();
      while (ac != queue.end())
      {
         if (ac->action == action && (data == NULL || !dStrcmp(ac->data, data)))
         {
            _endAction(*ac, AIAction::Stopped);
            ac = queue.erase(ac);
         }
         else
            ac++;
      }
   }

   mLocked = false;
}

DefineEngineMethod(BehaviorManager, stopAction, void, (AIAction *action, const char *data), (NULL),
   "Stop all instances of a given action, optionally with a specific data payload.")
{
   object->stopAction(action, data);
}

void BehaviorManager::stopActionsFrom(SimObject *from)
{
   if (!from || mLocked)
      return;

   mLocked = true;

   for (ActionMap::iterator res = mResources.begin(); res != mResources.end(); res++)
   {
      // Check all actions in the resource queue.
      ActionQueue &queue = res->second;
      ActionQueue::iterator ac = queue.begin();
      while (ac != queue.end())
      {
         if (ac->from == from)
         {
            _endAction(*ac, AIAction::Stopped);
            ac = queue.erase(ac);
         }
         else
            ac++;
      }
   }

   mLocked = false;
}

DefineEngineMethod(BehaviorManager, stopActionsFrom, void, (SimObject *from),,
   "Stop all action instances from a specific origin.")
{
   object->stopActionsFrom(from);
}

void BehaviorManager::stopAll()
{
   if (mLocked)
      return;

   mLocked = true;

   bool remainder;
   // Destroy the currently-active actions first, then the next ones, etc.
   do
   {
      remainder = false;
      for (ActionMap::iterator res = mResources.begin(); res != mResources.end(); res++)
      {
         // Destroy the first action in the resource queue.
         ActionQueue &queue = res->second;
         ActionQueue::iterator ac = queue.begin();
         if (ac != queue.end())
         {
            _endAction(*ac, AIAction::Stopped);
            ac = queue.erase(ac);
         }
         // Need to keep iterating if there are still actions in the queue.
         remainder |= queue.size() > 0;
         // No actions are started in this process.
      }
   } while (remainder);

   mLocked = false;
}

DefineEngineMethod(BehaviorManager, stopAll, void, (),,
   "Stop all running and queued actions.")
{
   object->stopAll();
}

void BehaviorManager::event(const char *name)
{
   if (!name || mLocked)
      return;

   mLocked = true;

   // Iterate over every currently-running action and hand them all the event.
   for (ActionMap::iterator res = mResources.begin(); res != mResources.end(); res++)
   {
      ActionQueue &queue = res->second;
      if (!queue.size())
         continue;

      ActionQueue::iterator ac = queue.begin();
      if (ac->action->receiveEvents)
      {
         // Notify the action of the event.
         AIAction::Status s = ac->action->event(NULL, ac->data, name);
         if (s != AIAction::Working)
         {
            // Action has ended because of the event.
            _endAction(*ac, s);
            ac = queue.erase(ac);
            // Give the next action in the queue a chance to run, if there is one.
            if (ac != queue.end())
            {
               _startAction(*ac);
               ac->waiting = false;
            }
         }
      }
   }

   mLocked = false;
}

DefineEngineMethod(BehaviorManager, event, void, (const char *name),,
   "Notify all running actions of an event.")
{
   object->event(name);
}

void BehaviorManager::_endAction(ActionInstance &ac, AIAction::Status s)
{
   // Call the action's end function to let it do what it needs to.
   ac.action->end(NULL, ac.data, s);
   // If it's just a temporary end, flag that.
   if (s == AIAction::Waiting)
      ac.waiting = true;
}

void BehaviorManager::_startAction(ActionInstance &ac)
{
   // Call the action's start function.
   ac.action->start(NULL, ac.data, ac.waiting);
}

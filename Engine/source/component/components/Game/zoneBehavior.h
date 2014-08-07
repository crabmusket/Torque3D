//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _ZONE_BEHAVIOR_H_
#define _ZONE_BEHAVIOR_H_

#ifndef _COMPONENT_H_
#include "component/components/component.h"
#endif

//////////////////////////////////////////////////////////////////////////
/// 
/// 
//////////////////////////////////////////////////////////////////////////
class ZoneBehavior : public Component
{
   typedef Component Parent;

public:
   ZoneBehavior();
   virtual ~ZoneBehavior();
   DECLARE_CONOBJECT(ZoneBehavior);

   virtual bool onAdd();
   virtual void onRemove();
   static void initPersistFields();

   virtual U32 packUpdate(NetConnection *con, U32 mask, BitStream *stream);
   virtual void unpackUpdate(NetConnection *con, BitStream *stream);

   //override to pass back a ExampleBehaviorInstance
   virtual ComponentInstance *createInstance();
};

class ZoneBehaviorInstance : public ComponentInstance
{
   typedef ComponentInstance Parent;

public:
   ZoneBehaviorInstance(Component *btemplate = NULL);
   virtual ~ZoneBehaviorInstance();
   DECLARE_CONOBJECT(ZoneBehaviorInstance);

   virtual bool onAdd();
   virtual void onRemove();
   static void initPersistFields();

   virtual void onComponentAdd();
   virtual void onComponentRemove();

   virtual U32 packUpdate(NetConnection *con, U32 mask, BitStream *stream);
   virtual void unpackUpdate(NetConnection *con, BitStream *stream);
};

#endif // _ZONE_BEHAVIOR_H_

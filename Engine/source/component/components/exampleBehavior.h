//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _EXAMPLEBEHAVIOR_H_
#define _EXAMPLEBEHAVIOR_H_

#ifndef _COMPONENT_H_
#include "component/components/component.h"
#endif

//////////////////////////////////////////////////////////////////////////
/// 
/// 
//////////////////////////////////////////////////////////////////////////
class ExampleBehavior : public Component
{
   typedef Component Parent;

public:
   ExampleBehavior();
   virtual ~ExampleBehavior();
   DECLARE_CONOBJECT(ExampleBehavior);

   virtual bool onAdd();
   virtual void onRemove();
   static void initPersistFields();

   virtual U32 packUpdate(NetConnection *con, U32 mask, BitStream *stream);
   virtual void unpackUpdate(NetConnection *con, BitStream *stream);

   //override to pass back a ExampleBehaviorInstance
   virtual ComponentInstance *createInstance();
};

class ExampleBehaviorInterface
{
public:
   virtual bool doSomething() = 0;
};

class ExampleBehaviorInstance : public ComponentInstance,
   public ExampleBehaviorInterface
{
   typedef ComponentInstance Parent;

protected:
   virtual bool doSomething() { return true; }

public:
   ExampleBehaviorInstance(Component *btemplate = NULL);
   virtual ~ExampleBehaviorInstance();
   DECLARE_CONOBJECT(ExampleBehaviorInstance);

   virtual bool onAdd();
   virtual void onRemove();
   static void initPersistFields();

   virtual void onComponentAdd();
   virtual void onComponentRemove();

   virtual U32 packUpdate(NetConnection *con, U32 mask, BitStream *stream);
   virtual void unpackUpdate(NetConnection *con, BitStream *stream);
};

#endif // _EXAMPLEBEHAVIOR_H_

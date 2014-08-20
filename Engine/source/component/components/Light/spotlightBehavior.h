//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _SPOTLIGHT_BEHAVIOR_H_
#define _SPOTLIGHT_BEHAVIOR_H_

#ifndef _COMPONENT_H_
#include "component/components/component.h"
#endif
#ifndef _LIGHTBASE_BEHAVIOR_H_
#include "component/components/Light/lightBaseBehavior.h"
#endif

//////////////////////////////////////////////////////////////////////////
/// 
/// 
//////////////////////////////////////////////////////////////////////////
class SpotLightBehavior : public Component
{
   typedef Component Parent;

public:
   SpotLightBehavior();
   virtual ~SpotLightBehavior();
   DECLARE_CONOBJECT(SpotLightBehavior);

   virtual bool onAdd();
   virtual void onRemove();
   static void initPersistFields();

   virtual U32 packUpdate(NetConnection *con, U32 mask, BitStream *stream);
   virtual void unpackUpdate(NetConnection *con, BitStream *stream);

   //override to pass back a SpotLightBehaviorInstance
   virtual ComponentInstance *createInstance();
};

class SpotLightBehaviorInstance : public LightBaseBehaviorInstance
{
   typedef LightBaseBehaviorInstance Parent;

protected:
   F32 mRange;

   F32 mInnerConeAngle;

   F32 mOuterConeAngle;

   // LightBase
   void _conformLights();
   void _renderViz( SceneRenderState *state );

public:
   SpotLightBehaviorInstance(Component *btemplate = NULL);
   virtual ~SpotLightBehaviorInstance();
   DECLARE_CONOBJECT(SpotLightBehaviorInstance);

   virtual bool onAdd();
   virtual void onRemove();
   static void initPersistFields();

   // SceneObject
   virtual void setScale( const VectorF &scale );

   virtual void onComponentAdd();
   virtual void onComponentRemove();

   virtual U32 packUpdate(NetConnection *con, U32 mask, BitStream *stream);
   virtual void unpackUpdate(NetConnection *con, BitStream *stream);
};

#endif // _SPOTLIGHT_BEHAVIOR_H_
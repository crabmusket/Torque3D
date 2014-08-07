//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#include "platform/platform.h"
#include "console/consoleTypes.h"
#include "component/components/game/zoneBehavior.h"
#include "core/util/safeDelete.h"
#include "console/consoleTypes.h"
#include "console/consoleObject.h"
#include "core/stream/bitStream.h"
#include "console/engineAPI.h"
#include "sim/netConnection.h"

//////////////////////////////////////////////////////////////////////////
// Constructor/Destructor
//////////////////////////////////////////////////////////////////////////

ZoneBehavior::ZoneBehavior()
{
   mNetFlags.set(Ghostable | ScopeAlways);
}

ZoneBehavior::~ZoneBehavior()
{
   for(S32 i = 0;i < mFields.size();++i)
   {
      ComponentField &field = mFields[i];
      SAFE_DELETE_ARRAY(field.mFieldDescription);
   }

   SAFE_DELETE_ARRAY(mDescription);
}

IMPLEMENT_CO_NETOBJECT_V1(ZoneBehavior);

//////////////////////////////////////////////////////////////////////////
ComponentInstance *ZoneBehavior::createInstance()
{
   ZoneBehaviorInstance *instance = new ZoneBehaviorInstance(this);

   setupFields( instance );

   if(instance->registerObject())
      return instance;

   delete instance;
   return NULL;
}

bool ZoneBehavior::onAdd()
{
   if(! Parent::onAdd())
      return false;

   return true;
}

void ZoneBehavior::onRemove()
{
   Parent::onRemove();
}
void ZoneBehavior::initPersistFields()
{
   Parent::initPersistFields();
}

U32 ZoneBehavior::packUpdate(NetConnection *con, U32 mask, BitStream *stream)
{
   U32 retMask = Parent::packUpdate(con, mask, stream);
   return retMask;
}

void ZoneBehavior::unpackUpdate(NetConnection *con, BitStream *stream)
{
   Parent::unpackUpdate(con, stream);
}

//==========================================================================================
//==========================================================================================
ZoneBehaviorInstance::ZoneBehaviorInstance( Component *btemplate ) 
{
   mTemplate = btemplate;
   mOwner = NULL;

   mNetFlags.set(Ghostable);
}

ZoneBehaviorInstance::~ZoneBehaviorInstance()
{
}
IMPLEMENT_CO_NETOBJECT_V1(ZoneBehaviorInstance);

bool ZoneBehaviorInstance::onAdd()
{
   if(! Parent::onAdd())
      return false;

   return true;
}

void ZoneBehaviorInstance::onRemove()
{
   Parent::onRemove();
}

//This is mostly a catch for situations where the behavior is re-added to the object and the like and we may need to force an update to the behavior
void ZoneBehaviorInstance::onComponentAdd()
{
   Parent::onComponentAdd();
}

void ZoneBehaviorInstance::onComponentRemove()
{
   Parent::onComponentRemove();
}

void ZoneBehaviorInstance::initPersistFields()
{
   Parent::initPersistFields();
}

U32 ZoneBehaviorInstance::packUpdate(NetConnection *con, U32 mask, BitStream *stream)
{
   U32 retMask = Parent::packUpdate(con, mask, stream);
   return retMask;
}

void ZoneBehaviorInstance::unpackUpdate(NetConnection *con, BitStream *stream)
{
   Parent::unpackUpdate(con, stream);
}
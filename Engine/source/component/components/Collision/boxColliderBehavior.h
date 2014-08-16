//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _BOX_COLLISION_BEHAVIOR_H_
#define _BOX_COLLISION_BEHAVIOR_H_

#ifndef __RESOURCE_H__
#include "core/resource.h"
#endif
#ifndef _TSSHAPE_H_
#include "ts/tsShape.h"
#endif
#ifndef _SCENERENDERSTATE_H_
#include "scene/sceneRenderState.h"
#endif
#ifndef _MBOX_H_
#include "math/mBox.h"
#endif
#ifndef _Entity_H_
#include "T3D/Entity.h"
#endif

#ifndef _RENDER_INTERFACES_H_
#include "component/components/render/renderInterfaces.h"
#endif

#ifndef _COLLISION_INTERFACES_H_
#include "component/components/collision/collisionInterfaces.h"
#endif

class TSShapeInstance;
class SceneRenderState;
class BoxColliderBehaviorInstance;
//////////////////////////////////////////////////////////////////////////
/// 
/// 
//////////////////////////////////////////////////////////////////////////
class bBoxConvex: public Convex
{
   Point3F getVertex(S32 v);
   void emitEdge(S32 v1,S32 v2,const MatrixF& mat,ConvexFeature* cf);
   void emitFace(S32 fi,const MatrixF& mat,ConvexFeature* cf);
public:
   //
   Point3F mCenter;
   VectorF mSize;

   bBoxConvex() { mType = BoxConvexType; }
   void init(SceneObject* obj) { mObject = obj; }

   Point3F support(const VectorF& v) const;
   void getFeatures(const MatrixF& mat,const VectorF& n, ConvexFeature* cf);
   void getPolyList(AbstractPolyList* list);
};


class BoxColliderBehavior : public Component
{
   typedef Component Parent;

public:
   BoxColliderBehavior();
   virtual ~BoxColliderBehavior();
   DECLARE_CONOBJECT(BoxColliderBehavior);

   static void initPersistFields();

   //override to pass back a BoxColliderBehaviorInstance
   virtual ComponentInstance *createInstance();
};

class BoxColliderBehaviorInstance : public ComponentInstance,
   public CollisionInterface,
   public PrepRenderImageInterface,
   public BuildConvexInterface
{
   typedef ComponentInstance Parent;

protected:
   Point3F colliderScale;

public:
   BoxColliderBehaviorInstance(Component *btemplate = NULL);
   virtual ~BoxColliderBehaviorInstance();
   DECLARE_CONOBJECT(BoxColliderBehaviorInstance);

   virtual bool onAdd();
   virtual void onRemove();
   static void initPersistFields();

   virtual void processTick(const Move* move);

   virtual void prepRenderImage( SceneRenderState *state );

   virtual U32 packUpdate(NetConnection *con, U32 mask, BitStream *stream);
   virtual void unpackUpdate(NetConnection *con, BitStream *stream);

   virtual void onComponentRemove();
   virtual void onComponentAdd();

   void prepCollision();
   void _updatePhysics();

   virtual bool checkCollisions( const F32 travelTime, Point3F *velocity, Point3F start );

   virtual bool buildConvex(const Box3F& box, Convex* convex);
   virtual bool castRay(const Point3F &start, const Point3F &end, RayInfo* info);

   virtual bool updateCollisions(F32 time, VectorF vector, VectorF velocity);

   virtual void updateWorkingCollisionSet(const U32 mask);
};

#endif // _COMPONENT_H_

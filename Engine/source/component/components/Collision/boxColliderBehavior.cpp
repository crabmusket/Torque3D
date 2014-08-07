//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "component/components/Collision/boxColliderBehavior.h"
#include "component/components/Physics/physicsBehavior.h"

//#include "platform/platform.h"
#include "console/consoleTypes.h"
#include "core/util/safeDelete.h"
#include "core/resourceManager.h"
//#include "core/stream/fileStream.h"
#include "console/consoleTypes.h"
#include "console/consoleObject.h"
//#include "ts/tsShapeInstance.h"
#include "core/stream/bitStream.h"
#include "scene/sceneRenderState.h"
#include "gfx/gfxTransformSaver.h"
#include "gfx/gfxDrawUtil.h"
//#include "gfx/primBuilder.h"
#include "console/engineAPI.h"
//#include "lighting/lightQuery.h"
#include "T3D/physics/physicsPlugin.h"
#include "T3D/physics/physicsBody.h"
#include "T3D/physics/physicsCollision.h"
#include "T3D/gameBase/gameConnection.h"
#include "collision/extrudedPolyList.h"
#include "math/mathIO.h"
#include "gfx/sim/debugDraw.h"  

#include "component/components/Physics/physicsInterfaces.h"

#include "collision/concretePolyList.h"

extern bool gEditingMission;

//////////////////////////////////////////////////////////////////////////
// Constructor/Destructor
//////////////////////////////////////////////////////////////////////////
static struct Face {
   S32 vertex[4];
   S32 axis;
   bool flip;
} sFace[] =
{
   { {0,4,5,1}, 1,true  },
   { {0,2,6,4}, 0,true  },
   { {3,7,6,2}, 1,false },
   { {3,1,5,7}, 0,false },
   { {0,1,3,2}, 2,true  },
   { {4,6,7,5}, 2,false },
};

Point3F bBoxConvex::support(const VectorF& v) const
{
   Point3F p = mCenter;
   p.x += (v.x >= 0)? mSize.x: -mSize.x;
   p.y += (v.y >= 0)? mSize.y: -mSize.y;
   p.z += (v.z >= 0)? mSize.z: -mSize.z;
   return p;
}

Point3F bBoxConvex::getVertex(S32 v)
{
   Point3F p = mCenter;
   p.x += (v & 1)? mSize.x: -mSize.x;
   p.y += (v & 2)? mSize.y: -mSize.y;
   p.z += (v & 4)? mSize.z: -mSize.z;
   return p;
}

inline bool isOnPlane(Point3F p,PlaneF& plane)
{
   F32 dist = mDot(plane,p) + plane.d;
   return dist < 0.1 && dist > -0.1;
}

void bBoxConvex::getFeatures(const MatrixF& mat,const VectorF& n, ConvexFeature* cf)
{
   cf->material = 0;
   cf->object = mObject;
   cf->mVertexList.setSize(8);
   cf->mEdgeList.setSize(12);
   cf->mFaceList.setSize(6);

   Point3F points[8] = { Point3F(mCenter + Point3F(-mSize.x, -mSize.y, -mSize.z)),
      Point3F(mCenter + Point3F(-mSize.x, -mSize.y,  mSize.z)),
      Point3F(mCenter + Point3F( mSize.x, -mSize.y,  mSize.z)),
      Point3F(mCenter + Point3F( mSize.x, -mSize.y, -mSize.z)),
      Point3F(mCenter + Point3F(-mSize.x,  mSize.y, -mSize.z)),
      Point3F(mCenter + Point3F(-mSize.x,  mSize.y,  mSize.z)),
      Point3F(mCenter + Point3F( mSize.x,  mSize.y,  mSize.z)),
      Point3F(mCenter + Point3F( mSize.x,  mSize.y, -mSize.z)) };

   Point2I edges[12] = { Point2I(0,1), Point2I(1,2), Point2I(2,3), Point2I(3,0),
      Point2I(4,5), Point2I(5,6), Point2I(6,7), Point2I(7,4),
      Point2I(0,4), Point2I(1,5), Point2I(2,6), Point2I(3,7) };

   Point3I faces[6] = { Point3I(0,1,2), Point3I(4,5,6), Point3I(0,1,4),
      Point3I(3,2,6), Point3I(0,4,3), Point3I(1,5,6) };


   for(U32 i = 0; i < 8; i++)
   {
      mat.mulP(points[i],&cf->mVertexList[i]);
   }

   //edges
   for(U32 i = 0; i < 12; i++)
   {
      ConvexFeature::Edge edge;
      edge.vertex[0] = edges[i].x;
      edge.vertex[1] = edges[i].y;
      cf->mEdgeList[i] = edge;
   }

   //face
   for(U32 i = 0; i < 6; i++)
   {
      ConvexFeature::Face face;
      face.normal = PlaneF(cf->mVertexList[faces[i].x], cf->mVertexList[faces[i].y], cf->mVertexList[faces[i].z]);

      face.vertex[0] = faces[i].x;
      face.vertex[1] = faces[i].y;
      face.vertex[2] = faces[i].z;

      cf->mFaceList[i] = face;
   }
}

void bBoxConvex::getPolyList(AbstractPolyList* list)
{
   list->setTransform(&getTransform(), getScale());
   list->setObject(getObject());

   Point3F centerPos = getObject()->getPosition();

   U32 base = list->addPoint(centerPos + Point3F(-mSize.x, -mSize.y, -mSize.z));
   list->addPoint(centerPos + Point3F( mSize.x, -mSize.y, -mSize.z));
   list->addPoint(centerPos + Point3F(-mSize.x,  mSize.y, -mSize.z));
   list->addPoint(centerPos + Point3F( mSize.x,  mSize.y, -mSize.z));
   list->addPoint(centerPos + Point3F(-mSize.x, -mSize.y,  mSize.z));
   list->addPoint(centerPos + Point3F( mSize.x, -mSize.y,  mSize.z));
   list->addPoint(centerPos + Point3F(-mSize.x,  mSize.y,  mSize.z));
   list->addPoint(centerPos + Point3F( mSize.x,  mSize.y,  mSize.z));

   for (U32 i = 0; i < 6; i++) {
      list->begin(0, i);
      list->vertex(base + sFace[i].vertex[0]);
      list->vertex(base + sFace[i].vertex[1]);
      list->vertex(base + sFace[i].vertex[2]);
      //list->vertex(base + sFace[i].vertex[3]);

      list->plane(base + sFace[i].vertex[0],
         base + sFace[i].vertex[1],
         base + sFace[i].vertex[2]);
      list->end();

      //
      list->begin(0, i+1);
      list->vertex(base + sFace[i].vertex[2]);
      list->vertex(base + sFace[i].vertex[3]);
      list->vertex(base + sFace[i].vertex[0]);
      //list->vertex(base + sFace[i].vertex[3]);

      list->plane(base + sFace[i].vertex[2],
         base + sFace[i].vertex[3],
         base + sFace[i].vertex[0]);
      list->end();
   }
}

//
BoxColliderBehavior::BoxColliderBehavior()
{
   mNetFlags.set(Ghostable | ScopeAlways);

   addComponentField("colliderSize", "If enabled, will stop colliding objects from passing through.", "Vector", "0.5 0.5 0.5", "");
}

BoxColliderBehavior::~BoxColliderBehavior()
{
   for(S32 i = 0;i < mFields.size();++i)
   {
      ComponentField &field = mFields[i];
      SAFE_DELETE_ARRAY(field.mFieldDescription);
   }

   SAFE_DELETE_ARRAY(mDescription);
}

IMPLEMENT_CO_NETOBJECT_V1(BoxColliderBehavior);

//////////////////////////////////////////////////////////////////////////
ComponentInstance *BoxColliderBehavior::createInstance()
{
   BoxColliderBehaviorInstance *instance = new BoxColliderBehaviorInstance(this);

   setupFields( instance );

   if(instance->registerObject())
      return instance;

   delete instance;
   return NULL;
}

void BoxColliderBehavior::initPersistFields()
{
   Parent::initPersistFields();
}

//==========================================================================================
//==========================================================================================
BoxColliderBehaviorInstance::BoxColliderBehaviorInstance( Component *btemplate ) 
{
   mTemplate = btemplate;
   mOwner = NULL;

   mConvexList = NULL;

   mWorkingQueryBox.minExtents.set(-1e9f, -1e9f, -1e9f);
   mWorkingQueryBox.maxExtents.set(-1e9f, -1e9f, -1e9f);

   mNetFlags.set(Ghostable);

   CollisionMoveMask = ( TerrainObjectType     | PlayerObjectType  | 
      StaticShapeObjectType | VehicleObjectType |
      VehicleBlockerObjectType | DynamicShapeObjectType | StaticObjectType);

}

BoxColliderBehaviorInstance::~BoxColliderBehaviorInstance()
{
   delete mConvexList;
   mConvexList = NULL;
}
IMPLEMENT_CO_NETOBJECT_V1(BoxColliderBehaviorInstance);

bool BoxColliderBehaviorInstance::onAdd()
{
   if(! Parent::onAdd())
      return false;

   return true;
}

void BoxColliderBehaviorInstance::onRemove()
{
   if(mConvexList != NULL)
      mConvexList->nukeList();
   Parent::onRemove();
}

void BoxColliderBehaviorInstance::onComponentRemove()
{
   SAFE_DELETE( (dynamic_cast<Entity*>(mOwner))->mPhysicsRep );
   mOwner->disableCollision();

   Parent::onComponentRemove();
}

void BoxColliderBehaviorInstance::onComponentAdd()
{
   Parent::onComponentAdd();
   if(isServerObject())
      prepCollision();
}

void BoxColliderBehaviorInstance::initPersistFields()
{
   Parent::initPersistFields();
   addField( "colliderSize", TypePoint3F, Offset(colliderScale, BoxColliderBehaviorInstance), "");
}

U32 BoxColliderBehaviorInstance::packUpdate(NetConnection *con, U32 mask, BitStream *stream)
{
   U32 retMask = Parent::packUpdate(con, mask, stream);

   if ( mask & InitialUpdateMask )
   {
      if(mOwner){
         if(con->getGhostIndex(mOwner) != -1)
         {
            stream->writeFlag( true );
            mathWrite( *stream, colliderScale );
         }
         else
         {
            retMask |= InitialUpdateMask; //try it again untill our dependency is ghosted
            stream->writeFlag( false );
         }
      }
      else
         stream->writeFlag( false );
   }

   return retMask;
}

void BoxColliderBehaviorInstance::unpackUpdate(NetConnection *con, BitStream *stream)
{
   Parent::unpackUpdate(con, stream);

   if ( stream->readFlag() ) // UpdateMask
   {
      mathRead(*stream, &colliderScale);
      prepCollision();
   }
}

void BoxColliderBehaviorInstance::prepCollision()
{
   if(!mOwner)
      return;

   // Allow the ShapeInstance to prep its collision if it hasn't already
   if(colliderScale.isZero()){
      mOwner->disableCollision();
      return;
   }

   if(mConvexList != NULL)
      mConvexList->nukeList();

   BoxConvex* bC = new BoxConvex();
   bC->init(mOwner);
   bC->mSize = colliderScale;
   mOwner->getObjBox().getCenter(&bC->mCenter);
   mConvexList = bC;

   mOwner->enableCollision();
   _updatePhysics();
}

void BoxColliderBehaviorInstance::_updatePhysics()
{
   SAFE_DELETE( (dynamic_cast<Entity*>(mOwner))->mPhysicsRep );

   if ( !PHYSICSMGR )
      return;

   PhysicsCollision *colShape = NULL;
   MatrixF offset( true );
   offset.setPosition( mOwner->getPosition() );
   colShape = PHYSICSMGR->createCollision();
   //colShape->addBox( mOwner->getObjBox().getExtents() * 0.5f * mOwner->getScale(), offset ); 
   colShape->addBox( colliderScale, offset );

   if ( colShape )
   {
      PhysicsWorld *world = PHYSICSMGR->getWorld( mOwner->isServerObject() ? "server" : "client" );
      (dynamic_cast<Entity*>(mOwner))->mPhysicsRep = PHYSICSMGR->createBody();
      (dynamic_cast<Entity*>(mOwner))->mPhysicsRep->init( colShape, 0, 0, mOwner, world );
      (dynamic_cast<Entity*>(mOwner))->mPhysicsRep->setTransform( mOwner->getTransform() );
   }
}

bool BoxColliderBehaviorInstance::buildConvex(const Box3F& box, Convex* convex)
{
   S32 id = mOwner->getId();

   bool svr = isServerObject();

   // These should really come out of a pool
   if(!mConvexList)
      return false;

   mConvexList->collectGarbage();

   Box3F realBox = box;
   mOwner->getWorldToObj().mul(realBox);
   realBox.minExtents.convolveInverse(mOwner->getScale());
   realBox.maxExtents.convolveInverse(mOwner->getScale());

   if (realBox.isOverlapped(mOwner->getObjBox()) == false)
      return false;

   // Just return a box convex for the entire shape...
   Convex* cc = 0;
   CollisionWorkingList& wl = convex->getWorkingList();
   for (CollisionWorkingList* itr = wl.wLink.mNext; itr != &wl; itr = itr->wLink.mNext)
   {
      if (itr->mConvex->getType() == BoxConvexType &&
         itr->mConvex->getObject() == mOwner)
      {
         cc = itr->mConvex;
         break;
      }
   }
   if (cc)
      return true;

   // Create a new convex.
   BoxConvex* cp = new BoxConvex;
   mConvexList->registerObject(cp);
   convex->addToWorkingList(cp);
   cp->init(mOwner);

   mOwner->getObjBox().getCenter(&cp->mCenter);
   cp->mSize = colliderScale;

   return true;
}

bool BoxColliderBehaviorInstance::castRay(const Point3F &start, const Point3F &end, RayInfo* info)
{
   F32 st, et, fst = 0.0f, fet = 1.0f;
   Box3F objectBox = Box3F(-colliderScale,colliderScale);
   F32 *bmin = &objectBox.minExtents.x;
   F32 *bmax = &objectBox.maxExtents.x;
   F32 const *si = &start.x;
   F32 const *ei = &end.x;

   for ( U32 i = 0; i < 3; i++ )
   {
      if (*si < *ei) 
      {
         if ( *si > *bmax || *ei < *bmin )
            return false;
         F32 di = *ei - *si;
         st = ( *si < *bmin ) ? ( *bmin - *si ) / di : 0.0f;
         et = ( *ei > *bmax ) ? ( *bmax - *si ) / di : 1.0f;
      }
      else 
      {
         if ( *ei > *bmax || *si < *bmin )
            return false;
         F32 di = *ei - *si;
         st = ( *si > *bmax ) ? ( *bmax - *si ) / di : 0.0f;
         et = ( *ei < *bmin ) ? ( *bmin - *si ) / di : 1.0f;
      }
      if ( st > fst ) fst = st;
      if ( et < fet ) fet = et;
      if ( fet < fst )
         return false;
      bmin++; bmax++;
      si++; ei++;
   }

   info->normal = start - end;
   info->normal.normalizeSafe();
   mOwner->getTransform().mulV( info->normal );

   info->t = fst;
   info->object = mOwner;
   info->point.interpolate( start, end, fst );
   info->material = NULL;

   //do our callback
   //onRaycastCollision_callback( this, enter );
   return true;
}

bool BoxColliderBehaviorInstance::updateCollisions(F32 time, VectorF vector, VectorF velocity)
{
   Polyhedron cPolyhedron;
   cPolyhedron.buildBox(mOwner->getTransform(), mOwner->getObjBox(), true);
   ExtrudedPolyList extrudePoly;

   extrudePoly.extrude(cPolyhedron, velocity);
   extrudePoly.setVelocity(velocity);
   extrudePoly.setCollisionList(&mCollisionList);

   Box3F plistBox = mOwner->getObjBox();
   mOwner->getTransform().mul(plistBox);
   Point3F oldMin = plistBox.minExtents;
   Point3F oldMax = plistBox.maxExtents;
   plistBox.minExtents.setMin(oldMin + (velocity * time) - Point3F(0.1f, 0.1f, 0.1f));
   plistBox.maxExtents.setMax(oldMax + (velocity * time) + Point3F(0.1f, 0.1f, 0.1f));

   // Build list from convex states here...
   CollisionWorkingList& rList = mConvexList->getWorkingList();
   CollisionWorkingList* pList = rList.wLink.mNext;
   while (pList != &rList) {
      Convex* pConvex = pList->mConvex;
      if (pConvex->getObject()->getTypeMask() & CollisionMoveMask) {
         Box3F convexBox = pConvex->getBoundingBox();
         if (plistBox.isOverlapped(convexBox))
         {
            //if(!dStrcmp(pConvex->getObject()->getClassName(), "Entity"))
            //	becauseShutUp = pConvex;

            pConvex->getPolyList(&extrudePoly);
            //andTheOtherOne = pConvex;
         }
      }
      pList = pList->wLink.mNext;
   }

   if(mCollisionList.getCount() > 0)
   {
      //becauseShutUp->getPolyList(&extrudePoly);
      //andTheOtherOne->getPolyList(&extrudePoly);

      return true;
   }
   else
      return false;
}

void BoxColliderBehaviorInstance::updateWorkingCollisionSet(const U32 mask)
{
   // First, we need to adjust our velocity for possible acceleration.  It is assumed
   // that we will never accelerate more than 20 m/s for gravity, plus 10 m/s for
   // jetting, and an equivalent 10 m/s for jumping.  We also assume that the
   // working list is updated on a Tick basis, which means we only expand our
   // box by the possible movement in that tick.
   VectorF velocity = Point3F(0,0,0);

   if(VelocityInterface *vI = mOwner->getComponent<VelocityInterface>())
      velocity = vI->getVelocity();

   Point3F scaledVelocity = velocity * TickSec;
   F32 len    = scaledVelocity.len();
   F32 newLen = len + (10.0f * TickSec);

   // Check to see if it is actually necessary to construct the new working list,
   // or if we can use the cached version from the last query.  We use the x
   // component of the min member of the mWorkingQueryBox, which is lame, but
   // it works ok.
   bool updateSet = false;

   Box3F convexBox = mConvexList->getBoundingBox(mOwner->getTransform(), mOwner->getScale());
   F32 l = (newLen * 1.1f) + 0.1f;  // from Convex::updateWorkingList
   const Point3F  lPoint( l, l, l );
   convexBox.minExtents -= lPoint;
   convexBox.maxExtents += lPoint;

   // Check containment
   if (mWorkingQueryBox.minExtents.x != -1e9f)
   {
      if (mWorkingQueryBox.isContained(convexBox) == false)
         // Needed region is outside the cached region.  Update it.
            updateSet = true;
   }
   else
   {
      // Must update
      updateSet = true;
   }
   // Actually perform the query, if necessary
   if (updateSet == true) {
      const Point3F  twolPoint( 2.0f * l, 2.0f * l, 2.0f * l );
      mWorkingQueryBox = convexBox;
      mWorkingQueryBox.minExtents -= twolPoint;
      mWorkingQueryBox.maxExtents += twolPoint;

      mOwner->disableCollision();
      mConvexList->updateWorkingList(mWorkingQueryBox, U32(-1));
      //isGhost() ? sClientCollisionContactMask : sServerCollisionContactMask);
      mOwner->enableCollision();
   }
}

void BoxColliderBehaviorInstance::prepRenderImage( SceneRenderState *state )
{
   if(gEditingMission)
   {
      if(mConvexList == NULL)
         return;

      DebugDrawer * debugDraw = DebugDrawer::get();  
      if (debugDraw)  
      {  
         Point3F min = mWorkingQueryBox.minExtents;
         Point3F max = mWorkingQueryBox.maxExtents;

         debugDraw->drawBox(mWorkingQueryBox.minExtents, mWorkingQueryBox.maxExtents, ColorI(255, 0, 255, 128));  
      } 

      // Box for the center of Mass
      /*GFXStateBlockDesc desc;
      desc.setBlend(false, GFXBlendSrcAlpha, GFXBlendInvSrcAlpha);
      desc.setZReadWrite(false);
      desc.fillMode = GFXFillWireframe;

      GFX->getDrawUtil()->drawCube( desc, Point3F(0.1f,0.1f,0.1f), Point3F(0,0,0), ColorI(255, 255, 255), &mOwner->getRenderTransform() );

      mConvexList->render();*/
   }
}
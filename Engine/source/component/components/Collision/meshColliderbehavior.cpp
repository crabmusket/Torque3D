//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "component/components/Collision/meshColliderBehavior.h"

//#include "platform/platform.h"
#include "console/consoleTypes.h"
#include "core/util/safeDelete.h"
#include "core/resourceManager.h"
//#include "core/stream/fileStream.h"
#include "console/consoleTypes.h"
#include "console/consoleObject.h"
//#include "ts/tsShapeInstance.h"
#include "core/stream/bitStream.h"
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

#include "opcode/Opcode.h"
#include "opcode/Ice/IceAABB.h"
#include "opcode/Ice/IcePoint.h"
#include "opcode/OPC_AABBTree.h"
#include "opcode/OPC_AABBCollider.h"

#include "component/components/Physics/physicsInterfaces.h"

//#include "math/mGeometry.h"

#include "component/components/render/renderInterfaces.h"

#include "collision/concretePolyList.h"

extern bool gEditingMission;

//
MeshColliderBehavior::MeshColliderBehavior()
{
   mNetFlags.set(Ghostable | ScopeAlways);

   addComponentField("collisionMasks", "A mask list of all the collision masks that apply to this collider.", "collisionMask", "", "");

   addComponentField("collisionCheckMasks", "A mask list of all the collision masks that this collider checks against when testing collisions.", "collisionMask", "", "");

   mFriendlyName = "Mesh Collider";
   mComponentType = "Collision";

   mDescription = getDescriptionText("Enables collision against this entity's visible mesh.");

   mNetworked = true;
}

MeshColliderBehavior::~MeshColliderBehavior()
{
   for(S32 i = 0;i < mFields.size();++i)
   {
      ComponentField &field = mFields[i];
      SAFE_DELETE_ARRAY(field.mFieldDescription);
   }

   SAFE_DELETE_ARRAY(mDescription);
}

IMPLEMENT_CO_NETOBJECT_V1(MeshColliderBehavior);

//////////////////////////////////////////////////////////////////////////
ComponentInstance *MeshColliderBehavior::createInstance()
{
   MeshColliderBehaviorInstance *instance = new MeshColliderBehaviorInstance(this);

   setupFields( instance );

   if(instance->registerObject())
      return instance;

   delete instance;
   return NULL;
}

void MeshColliderBehavior::initPersistFields()
{
   Parent::initPersistFields();
}

//==========================================================================================
//==========================================================================================
MeshColliderBehaviorInstance::MeshColliderBehaviorInstance( Component *btemplate ) 
{
   mTemplate = btemplate;
   mOwner = NULL;

   mConvexList = NULL;
   mTimeoutList = NULL;

   mWorkingQueryBox.minExtents.set(-1e9f, -1e9f, -1e9f);
   mWorkingQueryBox.maxExtents.set(-1e9f, -1e9f, -1e9f);

   mNetFlags.set(Ghostable);

   CollisionMoveMask = ( TerrainObjectType     | PlayerObjectType  | 
      StaticShapeObjectType | VehicleObjectType |
      VehicleBlockerObjectType | DynamicShapeObjectType);

   mDebugRender.shapeInstance = NULL;
}

MeshColliderBehaviorInstance::~MeshColliderBehaviorInstance()
{
   delete mConvexList;
   mConvexList = NULL;
}
IMPLEMENT_CO_NETOBJECT_V1(MeshColliderBehaviorInstance);

bool MeshColliderBehaviorInstance::onAdd()
{
   if(! Parent::onAdd())
      return false;

   return true;
}

void MeshColliderBehaviorInstance::onRemove()
{
   if(mConvexList != NULL)
      mConvexList->nukeList();
   Parent::onRemove();
}

void MeshColliderBehaviorInstance::onComponentRemove()
{
   SAFE_DELETE( (dynamic_cast<Entity*>(mOwner))->mPhysicsRep );
   mOwner->disableCollision();

   Parent::onComponentRemove();
}

void MeshColliderBehaviorInstance::onComponentAdd()
{
   Parent::onComponentAdd();

   if(isServerObject())
      prepCollision();
}

void MeshColliderBehaviorInstance::initPersistFields()
{
   Parent::initPersistFields();
}

U32 MeshColliderBehaviorInstance::packUpdate(NetConnection *con, U32 mask, BitStream *stream)
{
   U32 retMask = Parent::packUpdate(con, mask, stream);

   return retMask;
}

void MeshColliderBehaviorInstance::unpackUpdate(NetConnection *con, BitStream *stream)
{
   Parent::unpackUpdate(con, stream);

   prepCollision();
}

TSShapeInstance* MeshColliderBehaviorInstance::getShapeInstance()
{
   TSShapeInstanceInterface* shapeInterface = mOwner->getComponent<TSShapeInstanceInterface>();

   if(!shapeInterface)
      return NULL;

   TSShapeInstance* shapeInstance = shapeInterface->getShapeInstance();
   if(shapeInstance)
      return shapeInstance;

   return NULL;
}

void MeshColliderBehaviorInstance::update()
{
   Parent::update();

   if(isServerObject())
      prepCollision();
}

void MeshColliderBehaviorInstance::prepCollision()
{
   if(!mOwner)
      return;

   //Check if our owner has any renderable TSShapes
   TSShapeInstance *shapeInstance = getShapeInstance();

   if( !shapeInstance )
      return;

   bool server = isServerObject();

   shapeInstance->prepCollision();

   // Cleanup any old collision data
   mCollisionDetails.clear();

   if(mConvexList != NULL)
      mConvexList->nukeList();

   Vector<S32> losDetails;

   shapeInstance->getShape()->findColDetails( true, &mCollisionDetails, &losDetails );

   if(!mCollisionDetails.empty())
   {
      mOwner->enableCollision();

      mConvexList = new MeshColliderPolysoupConvex;

      Convex *c = new Convex();
      for (U32 i = 0; i < mCollisionDetails.size(); i++)
         buildConvexOpcode( shapeInstance, mCollisionDetails[i], mOwner->getWorldBox(), c, mConvexList );
   }
   else
      mOwner->disableCollision();

   _updatePhysics();
}

void MeshColliderBehaviorInstance::processTick(const Move* move)
{
   mCollisionList.clear();

   if(VelocityInterface *vI = mOwner->getComponent<VelocityInterface>())
   {
      VectorF velocity = vI->getVelocity();

      //No need to update our working set if we don't have our convex or we're not moving
      if(mConvexList && mConvexList->getObject() && !velocity.isZero())
         updateWorkingCollisionSet((U32)-1);
   }
}

bool MeshColliderBehaviorInstance::checkCollisions( const F32 travelTime, Point3F *velocity, Point3F start )
{
   if(!mConvexList || !mConvexList->getObject())
      return false;

   Point3F end = start + *velocity * travelTime;

   VectorF vector = end - start;

   mCollisionList.clear();

   if (checkEarlyOut(start, *velocity, travelTime, mOwner->getObjBox(), mOwner->getScale(), mConvexList->getBoundingBox(), 0xFFFFFF, mConvexList->getWorkingList()))
      return false;

   bool collided = updateCollisions(travelTime, vector, *velocity);

   handleCollisionList(mCollisionList, *velocity);

   //Basically, if this is false, we still track collisions as normal, but we don't
   //stop the colliding objects, instead only firing off callbacks.
   //This lets use act like a trigger.
   bool mBlockColliding = true;
   if(mBlockColliding)
      return collided;
   else
      return false;
}

void MeshColliderBehaviorInstance::_updatePhysics()
{
   SAFE_DELETE( (dynamic_cast<Entity*>(mOwner))->mPhysicsRep );

   if ( !PHYSICSMGR )
      return;

   /*PhysicsCollision *colShape = NULL;
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
   }*/
}

bool MeshColliderBehaviorInstance::castRay(const Point3F &start, const Point3F &end, RayInfo* info)
{
   TSShapeInstance *shapeInstance = getShapeInstance();
   if ( !shapeInstance )
      return false;

   // Cast the ray against the currently visible detail
   RayInfo localInfo;
   bool res = shapeInstance->castRayOpcode( shapeInstance->getCurrentDetail(), start, end, &localInfo );

   if ( res )
   {
      *info = localInfo;
      info->object = this->getBehaviorOwner();
      return true;
   }

   //do our callback
   //onRaycastCollision_callback( this, enter );
   return false;
}

bool MeshColliderBehaviorInstance::updateCollisions(F32 time, VectorF vector, VectorF velocity)
{
   //mOwner->getObjBox().getCenter(&mConvexList->mCenter);

   //the way this sets up, we basically make a convex per poly, which is why polysoup collisions can be really inefficient.
   //we'll basically cycle through our convex list, collect only polies that can be extruded(so front-facing to our end point)
   //and then extrude them one at a time untill they're all done, or we collide
   MeshColliderPolysoupConvex *conv = dynamic_cast<MeshColliderPolysoupConvex*>(mConvexList);
   while(conv != NULL)
   {
      conv = dynamic_cast<MeshColliderPolysoupConvex*>(conv->mNext);
   }

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
            pConvex->getPolyList(&extrudePoly);
         }
      }
      pList = pList->wLink.mNext;
   }

   if(mCollisionList.getCount() > 0)
      return true;
   else
      return false;
}

void MeshColliderBehaviorInstance::updateWorkingCollisionSet(const U32 mask)
{
   //if we have no convex, we cannot collide
   if(!mConvexList)
      return; 

   // First, we need to adjust our velocity for possible acceleration.  It is assumed
   // that we will never accelerate more than 20 m/s for gravity, plus 10 m/s for
   // jetting, and an equivalent 10 m/s for jumping.  We also assume that the
   // working list is updated on a Tick basis, which means we only expand our
   // box by the possible movement in that tick.
   VectorF velocity;
   VelocityInterface* velInterface = mOwner->getComponent<VelocityInterface>();
   if(!velInterface)
      velocity = Point3F(0,0,0);
   else 
      velocity = velInterface->getVelocity();

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

void MeshColliderBehaviorInstance::prepRenderImage( SceneRenderState *state )
{
   if(gEditingMission && mOwner->isSelected())
   {
      TSShapeInstance* shapeInstance = getShapeInstance();

      if(shapeInstance != NULL && mDebugRender.shapeInstance != shapeInstance)
      {
         mDebugRender.shapeInstance = shapeInstance;
         mDebugRender.triCount = 0;
         mDebugRender.vertA.clear();
         mDebugRender.vertB.clear();
         mDebugRender.vertC.clear();

         Vector<S32> losDetails;

         shapeInstance->getShape()->findColDetails( true, &mCollisionDetails, &losDetails );

         if(!mCollisionDetails.empty())
         {
            TSShape* shape = shapeInstance->getShape();

            for(U32 i=0; i < mCollisionDetails.size(); i++)
            {
               // get subshape and object detail
               const TSDetail * detail = &shape->details[mCollisionDetails[i]];
               S32 ss = detail->subShapeNum;
               S32 od = detail->objectDetailNum;

               // nothing emitted yet...
               bool emitted = false;

               S32 start = shape->subShapeFirstObject[ss];
               S32 end   = shape->subShapeNumObjects[ss] + start;
               if (start<end)
               {
                  MatrixF initialMat = mOwner->getObjToWorld();
                  Point3F initialScale = mOwner->getScale();

                  // set up for first object's node
                  MatrixF mat;
                  MatrixF scaleMat(true);
                  F32* p = scaleMat;
                  p[0]  = initialScale.x;
                  p[5]  = initialScale.y;
                  p[10] = initialScale.z;
                  const MatrixF * previousMat = &shapeInstance->mMeshObjects[start].getTransform();
                  mat.mul(initialMat,scaleMat);
                  mat.mul(*previousMat);

                  // Update our bounding box...
                  Box3F localBox = mOwner->getWorldBox();
                  MatrixF otherMat = mat;
                  otherMat.inverse();
                  otherMat.mul(localBox);

                  // run through objects and collide
                  Opcode::VertexPointers vp;
                  for (S32 i=start; i<end; i++)
                  {
                     TSShapeInstance::MeshObjectInstance * meshInstance = &shapeInstance->mMeshObjects[i];

                     TSMesh * mesh = meshInstance->getMesh(od);

                     mDebugRender.triCount = mesh->mOptTree->GetMeshInterface()->GetNbTriangles();
                     for(U32 j=0; j < mDebugRender.triCount; j++)
                     {
                        mesh->mOptTree->GetMeshInterface()->GetTriangle( vp, j );

                        Point3F a( vp.Vertex[0]->x, vp.Vertex[0]->y, vp.Vertex[0]->z );
                        Point3F b( vp.Vertex[1]->x, vp.Vertex[1]->y, vp.Vertex[1]->z );
                        Point3F c( vp.Vertex[2]->x, vp.Vertex[2]->y, vp.Vertex[2]->z );

                        MatrixF meshToObjectMat = meshInstance->getTransform();

                        // Transform the result into object space!
                        meshToObjectMat.mulP( a );
                        meshToObjectMat.mulP( b );
                        meshToObjectMat.mulP( c );

                        //build the shape-center offset
                        Point3F center = getShapeInstance()->getShape()->center;
                        Point3F position = mOwner->getPosition();

                        //mOwner->getTransform().mulP(center);

                        Point3F posOffset = position;// - center;

                        a += posOffset;
                        b += posOffset;
                        c += posOffset;

                        mDebugRender.vertA.push_back(a);
                        mDebugRender.vertB.push_back(b);
                        mDebugRender.vertC.push_back(c);
                     }
                  }
               }
            }
         }
      }

      ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
      ri->renderDelegate.bind( this, &MeshColliderBehaviorInstance::renderConvex );
      ri->objectIndex = -1;
      ri->type = RenderPassManager::RIT_Editor;
      state->getRenderPass()->addInst( ri );
   }
}

void MeshColliderBehaviorInstance::renderConvex( ObjectRenderInst *ri, SceneRenderState *state, BaseMatInstance *overrideMat )
{
   GFX->enterDebugEvent( ColorI(255,0,255), "MeshColliderBehaviorInstance_renderConvex" );

   GFXStateBlockDesc desc;
   desc.setZReadWrite( true, false );
   desc.setBlend( true );
   desc.fillMode = GFXFillWireframe;

   GFXDrawUtil *drawer = GFX->getDrawUtil();

   if(mDebugRender.shapeInstance == NULL)
      return;

   //if ( ri->objectIndex != -1 )
   //{
     // MountedImage &image = mMountedImageList[ ri->objectIndex ];

      //if ( image.shapeInstance )
      //{
         //MatrixF mat;
         //getRenderImageTransform( ri->objectIndex, &mat );         

         //const Box3F &objBox = image.shapeInstance[getImageShapeIndex(image)]->getShape()->bounds;
         Frustum f = state->getCullingFrustum();
         Box3F bounds;
         U32 clipMask;

         //drawer->drawCube( desc, objBox, ColorI( 255, 255, 255 ), &mat );
         for(U32 i=0; i < mDebugRender.triCount; i++)
         {
            bounds = Box3F();
            bounds.extend(mDebugRender.vertA[i]);
            bounds.extend(mDebugRender.vertB[i]);
            bounds.extend(mDebugRender.vertC[i]);

            clipMask = f.testPlanes( bounds, Frustum::PlaneMaskAll );
            if ( clipMask == -1 )
               continue;

            drawer->drawTriangle(desc, mDebugRender.vertA[i], mDebugRender.vertB[i], mDebugRender.vertC[i], ColorI(255, 0, 255, 128));
         }
      //}
   //}

   GFX->leaveDebugEvent();
}

bool MeshColliderBehaviorInstance::buildConvex(const Box3F& box, Convex* convex)
{
   TSShapeInstance *shapeInstance = getShapeInstance();
   if(!shapeInstance)
      return false;

   // These should really come out of a pool
   mConvexList->collectGarbage();

   Con::printf("MESHCOLLIDER: buildConvex CALLED");
   Con::printf("MESHCOLLIDER: Entity bounds.min: %g %g %g, max: %g %g %g", mOwner->getWorldBox().minExtents.x, mOwner->getWorldBox().minExtents.y, mOwner->getWorldBox().minExtents.z,
                                                                           mOwner->getWorldBox().maxExtents.x, mOwner->getWorldBox().maxExtents.y, mOwner->getWorldBox().maxExtents.z);

   for (U32 i = 0; i < mCollisionDetails.size(); i++)
      buildConvexOpcode( shapeInstance, mCollisionDetails[i], box, convex, mConvexList );

   return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool MeshColliderBehaviorInstance::buildConvexOpcode( TSShapeInstance* sI, S32 dl, const Box3F &bounds, Convex *c, Convex *list )
{
   AssertFatal(dl>=0 && dl < sI->getShape()->details.size(),"TSShapeInstance::buildConvexOpcode");

   TSShape* shape = sI->getShape();

   const MatrixF &objMat = mOwner->getObjToWorld();
   const Point3F &objScale = mOwner->getScale();

   // get subshape and object detail
   const TSDetail * detail = &shape->details[dl];
   S32 ss = detail->subShapeNum;
   S32 od = detail->objectDetailNum;

   // nothing emitted yet...
   bool emitted = false;

   S32 start = shape->subShapeFirstObject[ss];
   S32 end   = shape->subShapeNumObjects[ss] + start;
   if (start<end)
   {
      MatrixF initialMat = objMat;
      Point3F initialScale = objScale;

      // set up for first object's node
      MatrixF mat;
      MatrixF scaleMat(true);
      F32* p = scaleMat;
      p[0]  = initialScale.x;
      p[5]  = initialScale.y;
      p[10] = initialScale.z;
      const MatrixF * previousMat = &sI->mMeshObjects[start].getTransform();
      mat.mul(initialMat,scaleMat);
      mat.mul(*previousMat);

      // Update our bounding box...
      Box3F localBox = bounds;
      MatrixF otherMat = mat;
      otherMat.inverse();
      otherMat.mul(localBox);

      // run through objects and collide
      for (S32 i=start; i<end; i++)
      {
         TSShapeInstance::MeshObjectInstance * meshInstance = &sI->mMeshObjects[i];

         if (od >= meshInstance->object->numMeshes)
            continue;

         if (&meshInstance->getTransform() != previousMat)
         {
            // different node from before, set up for this node
            previousMat = &meshInstance->getTransform();

            if (previousMat != NULL)
            {
               mat.mul(initialMat,scaleMat);
               mat.mul(*previousMat);

               // Update our bounding box...
               otherMat = mat;
               otherMat.inverse();
               localBox = bounds;
               otherMat.mul(localBox);
            }
         }

         // collide... note we pass the original mech transform
         // here so that the convex data returned is in mesh space.
         TSMesh * mesh = meshInstance->getMesh(od);
         if ( mesh && !meshInstance->forceHidden && meshInstance->visible > 0.01f && localBox.isOverlapped( mesh->getBounds() ) )
            emitted |= buildMeshOpcode(mesh, *previousMat, localBox, c, list);
         else
            emitted |= false;
      }
   }

   return emitted;
}

bool MeshColliderBehaviorInstance::buildMeshOpcode(  TSMesh *mesh, const MatrixF &meshToObjectMat, 
                                                   const Box3F &nodeBox, Convex *convex, Convex *list)
{
   PROFILE_SCOPE( MeshCollider_buildConvexOpcode );

   // This is small... there is no win for preallocating it.
   Opcode::AABBCollider opCollider;
   opCollider.SetPrimitiveTests( true );

   // This isn't really needed within the AABBCollider as 
   // we don't use temporal coherance... use a static to 
   // remove the allocation overhead.
   static Opcode::AABBCache opCache;

   //-JR
   Con::printf("MESHCOLLIDER: TSMesh bounds.min: %g %g %g, max: %g %g %g", nodeBox.minExtents.x, nodeBox.minExtents.y, nodeBox.minExtents.z,
                                                                           nodeBox.maxExtents.x, nodeBox.maxExtents.y, nodeBox.maxExtents.z);
   //-JR

   IceMaths::AABB opBox;
   opBox.SetMinMax(  Point( nodeBox.minExtents.x, nodeBox.minExtents.y, nodeBox.minExtents.z ),
                     Point( nodeBox.maxExtents.x, nodeBox.maxExtents.y, nodeBox.maxExtents.z ) );
   Opcode::CollisionAABB opCBox(opBox);

   if( !opCollider.Collide( opCache, opCBox, *mesh->mOptTree ) )
      return false;

   U32 cnt = opCollider.GetNbTouchedPrimitives();
   const udword *idx = opCollider.GetTouchedPrimitives();

   Opcode::VertexPointers vp;
   for ( S32 i = 0; i < cnt; i++ )
   {
      // First, check our active convexes for a potential match (and clean things
      // up, too.)
      const U32 curIdx = idx[i];

      // See if the square already exists as part of the working set.
      bool gotMatch = false;
      CollisionWorkingList& wl = convex->getWorkingList();
      for ( CollisionWorkingList* itr = wl.wLink.mNext; itr != &wl; itr = itr->wLink.mNext )
      {
         if( itr->mConvex->getType() != TSPolysoupConvexType )
            continue;

         const MeshColliderPolysoupConvex *chunkc = static_cast<MeshColliderPolysoupConvex*>( itr->mConvex );

         if( chunkc->getObject() != mOwner )
            continue;

         if( chunkc->mesh != mesh )
            continue;

         if( chunkc->idx != curIdx )
            continue;

         // A match! Don't need to add it.
         gotMatch = true;
         break;
      }

      if( gotMatch )
         continue;

      // Get the triangle...
      mesh->mOptTree->GetMeshInterface()->GetTriangle( vp, idx[i] );

      Point3F a( vp.Vertex[0]->x, vp.Vertex[0]->y, vp.Vertex[0]->z );
      Point3F b( vp.Vertex[1]->x, vp.Vertex[1]->y, vp.Vertex[1]->z );
      Point3F c( vp.Vertex[2]->x, vp.Vertex[2]->y, vp.Vertex[2]->z );

      // Transform the result into object space!
      meshToObjectMat.mulP( a );
      meshToObjectMat.mulP( b );
      meshToObjectMat.mulP( c );

      //build the shape-center offset
      /*Point3F center = getShapeInstance()->getShape()->center;
      Point3F position = mOwner->getPosition();

      //mOwner->getTransform().mulP(center);

      Point3F posOffset = position;// - center;

      a += posOffset;
      b += posOffset;
      c += posOffset;*/
      //

      PlaneF p( c, b, a );
      Point3F peak = ((a + b + c) / 3.0f) - (p * 0.15f);

      // Set up the convex...
      MeshColliderPolysoupConvex *cp = new MeshColliderPolysoupConvex();

      list->registerObject( cp );
      convex->addToWorkingList( cp );

      cp->mesh    = mesh;
      cp->idx     = curIdx;
      cp->mObject = mOwner;

      cp->normal = p;
      cp->verts[0] = a;
      cp->verts[1] = b;
      cp->verts[2] = c;
      cp->verts[3] = peak;

      // Update the bounding box.
      Box3F &bounds = cp->box;
      bounds.minExtents.set( F32_MAX,  F32_MAX,  F32_MAX );
      bounds.maxExtents.set( -F32_MAX, -F32_MAX, -F32_MAX );

      bounds.minExtents.setMin( a );
      bounds.minExtents.setMin( b );
      bounds.minExtents.setMin( c );
      bounds.minExtents.setMin( peak );

      bounds.maxExtents.setMax( a );
      bounds.maxExtents.setMax( b );
      bounds.maxExtents.setMax( c );
      bounds.maxExtents.setMax( peak );
   }

   return true;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
MeshColliderPolysoupConvex::MeshColliderPolysoupConvex()
   :  box( 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f ),
   normal( 0.0f, 0.0f, 0.0f, 0.0f ),
   idx( 0 ),
   mesh( NULL )
{
   mType = TSPolysoupConvexType;

   for ( U32 i = 0; i < 4; ++i )
   {
      verts[i].set( 0.0f, 0.0f, 0.0f );
   }
}

Point3F MeshColliderPolysoupConvex::support(const VectorF& vec) const
{
   F32 bestDot = mDot( verts[0], vec );

   const Point3F *bestP = &verts[0];
   for(S32 i=1; i<4; i++)
   {
      F32 newD = mDot(verts[i], vec);
      if(newD > bestDot)
      {
         bestDot = newD;
         bestP = &verts[i];
      }
   }

   return *bestP;
}

Box3F MeshColliderPolysoupConvex::getBoundingBox() const
{
   Box3F wbox = box;
   wbox.minExtents.convolve( mObject->getScale() );
   wbox.maxExtents.convolve( mObject->getScale() );
   mObject->getTransform().mul(wbox);
   return wbox;
}

Box3F MeshColliderPolysoupConvex::getBoundingBox(const MatrixF& mat, const Point3F& scale) const
{
   AssertISV(false, "MeshColliderPolysoupConvex::getBoundingBox(m,p) - Not implemented. -- XEA");
   return box;
}

void MeshColliderPolysoupConvex::getPolyList(AbstractPolyList *list)
{
   // Transform the list into object space and set the pointer to the object
   MatrixF i( mObject->getTransform() );
   Point3F iS( mObject->getScale() );

   list->setTransform(&i, iS);
   list->setObject(mObject);

   // Add only the original collision triangle
   S32 base =  list->addPoint(verts[0]);
   list->addPoint(verts[2]);
   list->addPoint(verts[1]);

   list->begin(0, (U32)idx ^ (U32)mesh);
   list->vertex(base + 2);
   list->vertex(base + 1);
   list->vertex(base + 0);
   list->plane(base + 0, base + 1, base + 2);
   list->end();
}

void MeshColliderPolysoupConvex::getFeatures(const MatrixF& mat,const VectorF& n, ConvexFeature* cf)
{
   cf->material = 0;
   cf->object = mObject;

   // For a tetrahedron this is pretty easy... first
   // convert everything into world space.
   Point3F tverts[4];
   mat.mulP(verts[0], &tverts[0]);
   mat.mulP(verts[1], &tverts[1]);
   mat.mulP(verts[2], &tverts[2]);
   mat.mulP(verts[3], &tverts[3]);

   // points...
   S32 firstVert = cf->mVertexList.size();
   cf->mVertexList.increment(); cf->mVertexList.last() = tverts[0];
   cf->mVertexList.increment(); cf->mVertexList.last() = tverts[1];
   cf->mVertexList.increment(); cf->mVertexList.last() = tverts[2];
   cf->mVertexList.increment(); cf->mVertexList.last() = tverts[3];

   //    edges...
   cf->mEdgeList.increment();
   cf->mEdgeList.last().vertex[0] = firstVert+0;
   cf->mEdgeList.last().vertex[1] = firstVert+1;

   cf->mEdgeList.increment();
   cf->mEdgeList.last().vertex[0] = firstVert+1;
   cf->mEdgeList.last().vertex[1] = firstVert+2;

   cf->mEdgeList.increment();
   cf->mEdgeList.last().vertex[0] = firstVert+2;
   cf->mEdgeList.last().vertex[1] = firstVert+0;

   cf->mEdgeList.increment();
   cf->mEdgeList.last().vertex[0] = firstVert+3;
   cf->mEdgeList.last().vertex[1] = firstVert+0;

   cf->mEdgeList.increment();
   cf->mEdgeList.last().vertex[0] = firstVert+3;
   cf->mEdgeList.last().vertex[1] = firstVert+1;

   cf->mEdgeList.increment();
   cf->mEdgeList.last().vertex[0] = firstVert+3;
   cf->mEdgeList.last().vertex[1] = firstVert+2;

   //    triangles...
   cf->mFaceList.increment();
   cf->mFaceList.last().normal = PlaneF(tverts[2], tverts[1], tverts[0]);
   cf->mFaceList.last().vertex[0] = firstVert+2;
   cf->mFaceList.last().vertex[1] = firstVert+1;
   cf->mFaceList.last().vertex[2] = firstVert+0;

   cf->mFaceList.increment();
   cf->mFaceList.last().normal = PlaneF(tverts[1], tverts[0], tverts[3]);
   cf->mFaceList.last().vertex[0] = firstVert+1;
   cf->mFaceList.last().vertex[1] = firstVert+0;
   cf->mFaceList.last().vertex[2] = firstVert+3;

   cf->mFaceList.increment();
   cf->mFaceList.last().normal = PlaneF(tverts[2], tverts[1], tverts[3]);
   cf->mFaceList.last().vertex[0] = firstVert+2;
   cf->mFaceList.last().vertex[1] = firstVert+1;
   cf->mFaceList.last().vertex[2] = firstVert+3;

   cf->mFaceList.increment();
   cf->mFaceList.last().normal = PlaneF(tverts[0], tverts[2], tverts[3]);
   cf->mFaceList.last().vertex[0] = firstVert+0;
   cf->mFaceList.last().vertex[1] = firstVert+2;
   cf->mFaceList.last().vertex[2] = firstVert+3;

   // All done!
}

void MeshColliderPolysoupConvex::getAllPolyList(AbstractPolyList *list)
{
   if(mNext)
   {
      MeshColliderPolysoupConvex *itr = dynamic_cast<MeshColliderPolysoupConvex*>(mNext);
      while(itr)
      {
         itr->getPolyList(list);
         itr = dynamic_cast<MeshColliderPolysoupConvex*>(itr->mNext);
      }
   }
}

void MeshColliderPolysoupConvex::render()
{
   ConcretePolyList polyList;
   getAllPolyList( &polyList );
   polyList.render();
}
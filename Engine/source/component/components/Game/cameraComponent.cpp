//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "console/consoleTypes.h"
#include "component/components/Game/cameraComponent.h"
#include "core/util/safeDelete.h"
#include "core/resourceManager.h"
#include "core/stream/fileStream.h"
#include "console/consoleTypes.h"
#include "console/consoleObject.h"
#include "ts/tsShapeInstance.h"
#include "core/stream/bitStream.h"
//#include "console/consoleInternal.h"
#include "gfx/gfxTransformSaver.h"
#include "console/engineAPI.h"
#include "lighting/lightQuery.h"
#include "T3D/gameBase/gameConnection.h"

IMPLEMENT_CALLBACK( cameraComponentInstance, validateCameraFov, F32, (F32 fov), (fov),
                   "@brief Called on the server when the client has requested a FOV change.\n\n"

                   "When the client requests that its field of view should be changed (because "
                   "they want to use a sniper scope, for example) this new FOV needs to be validated "
                   "by the server.  This method is called if it exists (it is optional) to validate "
                   "the requested FOV, and modify it if necessary.  This could be as simple as checking "
                   "that the FOV falls within a correct range, to making sure that the FOV matches the "
                   "capabilities of the current weapon.\n\n"

                   "Following this method, ShapeBase ensures that the given FOV still falls within "
                   "the datablock's cameraMinFov and cameraMaxFov.  If that is good enough for your "
                   "purposes, then you do not need to define the validateCameraFov() callback for "
                   "your ShapeBase.\n\n"

                   "@param fov The FOV that has been requested by the client.\n"
                   "@return The FOV as validated by the server.\n\n"

                   "@see ShapeBaseData\n\n");

//////////////////////////////////////////////////////////////////////////
// Constructor/Destructor
//////////////////////////////////////////////////////////////////////////

cameraComponent::cameraComponent()
{
   mNetFlags.set(Ghostable | ScopeAlways);
}

cameraComponent::~cameraComponent()
{
   for(S32 i = 0;i < mFields.size();++i)
   {
      ComponentField &field = mFields[i];
      SAFE_DELETE_ARRAY(field.mFieldDescription);
   }

   SAFE_DELETE_ARRAY(mDescription);
}

IMPLEMENT_CO_NETOBJECT_V1(cameraComponent);

//////////////////////////////////////////////////////////////////////////
ComponentInstance *cameraComponent::createInstance()
{
   cameraComponentInstance *instance = new cameraComponentInstance(this);

   setupFields( instance );

   if(instance->registerObject())
      return instance;

   delete instance;
   return NULL;
}

bool cameraComponent::onAdd()
{
   if(! Parent::onAdd())
      return false;

   return true;
}

void cameraComponent::onRemove()
{
   Parent::onRemove();
}
void cameraComponent::initPersistFields()
{
   Parent::initPersistFields();
}

U32 cameraComponent::packUpdate(NetConnection *con, U32 mask, BitStream *stream)
{
   U32 retMask = Parent::packUpdate(con, mask, stream);
   return retMask;
}

void cameraComponent::unpackUpdate(NetConnection *con, BitStream *stream)
{
   Parent::unpackUpdate(con, stream);
}

//==========================================================================================
//==========================================================================================
cameraComponentInstance::cameraComponentInstance( Component *btemplate ) 
{
   mTemplate = btemplate;
   mOwner = NULL;

   mNetFlags.set(Ghostable);
}

cameraComponentInstance::~cameraComponentInstance()
{
}
IMPLEMENT_CO_NETOBJECT_V1(cameraComponentInstance);

bool cameraComponentInstance::onAdd()
{
   if(! Parent::onAdd())
      return false;

   return true;
}

void cameraComponentInstance::onRemove()
{
   Parent::onRemove();
}

//This is mostly a catch for situations where the behavior is re-added to the object and the like and we may need to force an update to the behavior
void cameraComponentInstance::onComponentAdd()
{
   Parent::onComponentAdd();
}

void cameraComponentInstance::onComponentRemove()
{
   Parent::onComponentRemove();
}


void cameraComponentInstance::initPersistFields()
{
   Parent::initPersistFields();

}

bool cameraComponentInstance::isValidCameraFov(F32 fov)
{
   return((fov >= cameraMinFov) && (fov <= cameraMaxFov));
}

void cameraComponentInstance::setCameraFov(F32 fov)
{
   // On server allow for script side checking
   if ( !isGhost() && isMethod( "validateCameraFov" ) )
   {
      fov = validateCameraFov_callback( fov );
   }

   mCameraFov = mClampF(fov, cameraMinFov, cameraMaxFov);
}

void cameraComponentInstance::onCameraScopeQuery(NetConnection *cr, CameraScopeQuery * query)
{
   // update the camera query
   query->camera = this;

   if(GameConnection * con = dynamic_cast<GameConnection*>(cr))
   {
      // get the fov from the connection (in deg)
      F32 fov;
      if (con->getControlCameraFov(&fov))
      {
         query->fov = mDegToRad(fov/2);
         query->sinFov = mSin(query->fov);
         query->cosFov = mCos(query->fov);
      }
      else
      {
         query->fov = mDegToRad(mCameraFov/2);
         query->sinFov = mSin(query->fov);
         query->cosFov = mCos(query->fov);
      }
   }

   // use eye rather than camera transform (good enough and faster)
   MatrixF camTransform = mOwner->getTransform();
   camTransform.getColumn(3, &query->pos);
   camTransform.getColumn(1, &query->orientation);

   // Get the visible distance.
   if (mOwner->getSceneManager() != NULL)
      query->visibleDistance = mOwner->getSceneManager()->getVisibleDistance();

   //mOwner->Parent::onCameraScopeQuery( cr, query );
}

bool cameraComponentInstance::getCameraTransform(F32* pos,MatrixF* mat)
{
   // Returns camera to world space transform
   // Handles first person / third person camera position

   //if (isServerObject() && mShapeInstance)
   //   mShapeInstance->animateNodeSubtrees(true);

   if (*pos != 0)
   {
      F32 min,max;
      Point3F offset;
      MatrixF trans,rot;
      getCameraParameters(&min,&max,&offset,&rot);
      trans = mOwner->getRenderTransform();

      EulerF rotTest = trans.toEuler();
      rotTest = EulerF(mRadToDeg(rotTest.x), mRadToDeg(rotTest.y), mRadToDeg(rotTest.z));
      if(!mIsZero(rotTest.y))
         bool breakp = 0;

      mat->mul(trans,rot);

      rotTest = mat->toEuler();
      rotTest = EulerF(mRadToDeg(rotTest.x), mRadToDeg(rotTest.y), mRadToDeg(rotTest.z));
      if(!mIsZero(rotTest.y))
         bool breakp = 0;

      // Use the eye transform to orient the camera
      VectorF vp,vec;
      vp.x = vp.z = 0;
      vp.y = -(max - min) * *pos;
      trans.mulV(vp,&vec);

      VectorF minVec;
      vp.y = -min;
      trans.mulV( vp, &minVec );

      // Use the camera node's pos.
      Point3F osp,sp;
      /*if (mDataBlock->cameraNode != -1) {
      mShapeInstance->mNodeTransforms[mDataBlock->cameraNode].getColumn(3,&osp);

      // Scale the camera position before applying the transform
      const Point3F& scale = getScale();
      osp.convolve( scale );

      getRenderTransform().mulP(osp,&sp);
      }
      else*/
      mOwner->getRenderTransform().getColumn(3,&sp);

      // Make sure we don't extend the camera into anything solid
      Point3F ep = sp + minVec + vec + offset;
      /*disableCollision();
      if (isMounted())
      getObjectMount()->disableCollision();
      RayInfo collision;
      if( mContainer && mContainer->castRay(sp, ep,
      (0xFFFFFFFF & ~(WaterObjectType      |
      GameBaseObjectType   |
      TriggerObjectType    |
      DefaultObjectType)),
      &collision) == true) {
      F32 vecLenSq = vec.lenSquared();
      F32 adj = (-mDot(vec, collision.normal) / vecLenSq) * 0.1;
      F32 newPos = getMax(0.0f, collision.t - adj);
      if (newPos == 0.0f)
      eye.getColumn(3,&ep);
      else
      ep = sp + offset + (vec * newPos);
      }*/
      mat->setColumn(3,ep);

      //if (isMounted())
      //   getObjectMount()->enableCollision();
      //enableCollision();
   }
   else
   {
      MatrixF rMat = mOwner->getRenderTransform();
      mat->set(rMat.toEuler(), rMat.getPosition());
   }

   // Apply Camera FX.
   //mat->mul( gCamFXMgr.getTrans() );
   return true;
}

void cameraComponentInstance::getCameraParameters(F32 *min,F32* max,Point3F* off,MatrixF* rot)
{
   *min = 0.2f;
   *max = 0.f;
   off->set(0,0,0);
   rot->identity();
}
U32 cameraComponentInstance::packUpdate(NetConnection *con, U32 mask, BitStream *stream)
{
   U32 retmask = Parent::packUpdate(con, mask, stream);
   return retmask;
}

void cameraComponentInstance::unpackUpdate(NetConnection *con, BitStream *stream)
{
   Parent::unpackUpdate(con, stream);
}

ConsoleMethod(cameraComponentInstance, getMode, const char*, 2, 2, "() - We get the first behavior of the requested type on our owner object.\n"
              "@return (string name) The type of the behavior we're requesting")
{
   return "fly";
}
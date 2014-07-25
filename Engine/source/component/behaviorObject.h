#ifndef _BEHAVIOR_OBJECT_H_
#define _BEHAVIOR_OBJECT_H_

#ifndef _GAMEBASE_H_
#include "T3D/gameBase/gameBase.h"
#endif
#ifndef _ICALLMETHOD_H_
#include "console/ICallMethod.h"
#endif
#ifndef _CONSOLEINTERNAL_H_
#include "console/consoleInternal.h"
#endif

#ifndef _BEHAVIORTEMPLATE_H_
#include "component/behaviors/behaviorTemplate.h"
#endif
#ifndef _BEHAVIORINSTANCE_H_
#include "component/behaviors/behaviorInstance.h"
#endif

#ifndef _PLATFORM_THREADS_MUTEX_H_
#include "platform/threads/mutex.h"
#endif
#ifndef _STRINGFUNCTIONS_H_
#include "core/strings/stringFunctions.h"
#endif
#ifndef _SIMSET_H_
#include "console/simSet.h"
#endif
#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif
#ifndef _STRINGTABLE_H_
#include "core/stringTable.h"
#endif
#ifndef _SIMOBJECT_H_
#include "console/simObject.h"
#endif

#include "core/util/safeDelete.h"

#ifdef TORQUE_DEBUG
#ifndef _TDICTIONARY_H_
#include "core/util/tDictionary.h"
#endif
#endif

class BehaviorTemplate;

class BehaviorObject : public GameBase, public ICallMethod
{
   typedef GameBase Parent;
   friend class BehaviorInstance;

   //////////////////////////////////////////////////////////////////////////
   // ICallMethod stuff
   // Allows us to handle console method calls
#ifdef TORQUE_DEBUG
   typedef Map<StringTableEntry, S32> callMethodMetricType;

   // Call Method Debug Stat.
   callMethodMetricType mCallMethodMetrics;
#endif

protected:
   /// Internal callMethod : Actually does component notification and script method execution
   ///  @attention This method does some magic to the argc argv to make Con::execute act properly
   ///   as such it's internal and should not be exposed or used except by this class
   virtual const char* _callMethod( U32 argc, const char *argv[], bool callThis = true );

   virtual const char *_callBehaviorMethod( U32 argc, const char *argv[], bool callThis /* = true  */ );

public:

#ifdef TORQUE_DEBUG
   /// Call Method Metrics.
   const callMethodMetricType& getCallMethodMetrics( void ) const { return mCallMethodMetrics; };

   /// Inject Method Call.
   void injectMethodCall( const char* method );
#endif

   /// Call Method
   virtual const char* callMethodArgList( U32 argc, const char *argv[], bool callThis = true );

   /// Call Method format string
   const char* callMethod( S32 argc, const char* methodName, ... );

   //////////////////////////////////////////////////////////////////////////
   // DynamicConsoleMethodComponent Overrides
   virtual bool handlesConsoleMethod( const char *fname, S32 *routingId );

   //////////////////////////////////////////////////////////////////////////
   // Behavior stuff
   // These are the functions that handle behavior management
private:
   SimSet mBehaviors;
   Vector<BehaviorInstance*> mToLoadBehaviors;
   bool   mStartBehaviorUpdate;
   bool   mDelayUpdate;
   bool   mLoadedBehaviors;

protected:
   enum MaskBits 
   {
      BehaviorsMask				 = Parent::NextFreeMask << 0,
      NextFreeMask             = Parent::NextFreeMask << 1
   };

   // [neo, 5/10/2007 - #3010]
   bool  mInBehaviorCallback; ///<  

   /// Should adding of behaviors be deferred from onAdd() to be called by derived classe manually?
   virtual bool deferAddingBehaviors() const { return false; }

public:
   virtual void write( Stream &stream, U32 tabStop, U32 flags = 0  );

   // [neo, 5/11/2007]
   // Refactored onAdd() code into this method so it can be deferred by derived classes.
   /// Attach behaviors listed as special fields (derived classes can override this and defer!)
   virtual void addBehaviors();

   //////////////////////////////////////////////////////////////////////////
   /// Behavior interface  (Move this to protected?)
   virtual bool addBehavior( BehaviorInstance *bi );
   virtual bool removeBehavior( BehaviorInstance *bi, bool deleteBehavior = true );
   virtual const SimSet &getAllBehaviors() const { return mBehaviors; };
   virtual BehaviorInstance *getBehavior( StringTableEntry behaviorTemplateName );
   virtual BehaviorInstance *getBehavior( S32 index );
   virtual BehaviorInstance *getBehaviorByType( StringTableEntry behaviorTypeName );
   virtual BehaviorInstance *getBehavior( BehaviorTemplate *bTemplate );

   template <class T>
   T *getBehavior();
   template <class T>
   Vector<T*> getBehaviors();

   virtual void clearBehaviors(bool deleteBehaviors = true);
   virtual bool reOrder( BehaviorInstance *obj, U32 desiredIndex /* = 0 */ );
   virtual U32 getBehaviorCount() const { return mBehaviors.size(); }
   void updateBehaviors();
   void setBehaviorsDirty();
   /// This forces the network update of a single specific behavior
   /// The force update arg is for special-case behaviors that are explicitly networked to certain clients
   /// Such as the camera, and it will override the check if the template defines if it's networked or not
   void setBehaviorDirty(BehaviorInstance *bI, bool forceUpdate = false);
   void callOnBehaviors( String function );

public:
   BehaviorObject();
   ~BehaviorObject(){}

   bool onAdd();
   void onRemove();

   virtual void onPostAdd();

   virtual void addObject( SimObject* object );

   Box3F getObjectBox() { return mObjBox; }
   MatrixF getWorldToObj() { return mWorldToObj; }
   MatrixF getObjToWorld() { return mObjToWorld; }

   virtual void setObjectBox(Box3F objBox){}
   virtual void setWorldBox(Box3F wrldBox) { mWorldBox = wrldBox; }

   bool areBehaviorsLoaded() { return mLoadedBehaviors; }

   U32 packUpdate(NetConnection *con, U32 mask, BitStream *stream);
   void unpackUpdate(NetConnection *con, BitStream *stream);

   S32 isBehaviorPackable(NetConnection *con, BehaviorInstance* bI);

   DECLARE_CONOBJECT(BehaviorObject);
};

template <class T>
T *BehaviorObject::getBehavior()
{
   for( SimSet::iterator b = mBehaviors.begin(); b != mBehaviors.end(); b++ )
   {
      T* t = dynamic_cast<T *>(*b);
      if(t) {
         return t;
      }
   }
   return NULL;
}

template <class T>
Vector<T*> BehaviorObject::getBehaviors() {
   Vector<T*> v;
   mBehaviors.findObjectByType<T>(v);
   return v;
}
#endif
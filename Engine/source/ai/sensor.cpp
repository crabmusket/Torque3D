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

#include "sensor.h"
#include "platform/platform.h"
#include "console/consoleTypes.h"
#include "console/consoleInternal.h"
#include "core/stream/bitStream.h"
#include "sim/netConnection.h"
#include "math/mathIO.h"
#include "platform/profiler.h"
#include "T3D/shapeBase.h"
#include "console/engineAPI.h"
#include "collision/boxConvex.h"
#include "math/mTransform.h"
#include "gfx/gfxDrawUtil.h"

extern bool gEditingMission;

SensorRule::Factory SensorRule::gSensorRuleMap;

//-----------------------------------------------------------------------------

IMPLEMENT_CO_DATABLOCK_V1(SensorData);

SensorData::SensorData()
{
   engagementRange = 0.0f;
   typemask = -1;

   for(U32 i = 0; i < MaxRules; i++)
   {
      ruleStrings[i] = "";
      rules[i] = NULL;
      ranges[i].set(0.0f, 1.0f);
   }
}

SensorData::~SensorData()
{
   for(U32 i = 0; i < MaxRules; i++)
      delete rules[i];
}

//-----------------------------------------------------------------------------

void SensorData::initPersistFields()
{
   addField("engagementRange", TypeF32, Offset(engagementRange, SensorData),
      "Maximum range at which objects are considered for detection by this sensor.");

   addField("rules", TypeRealString, Offset(ruleStrings, SensorData), MaxRules,
      "Rules for this sensor represented as strings.");
   addField("range", TypePoint2F, Offset(ranges, SensorData), MaxRules,
      "Pairs of floating-point numbers representing the min and max contribution from this rule.");

   addField("typemask", TypeS32, Offset(typemask, SensorData), MaxRules,
      "Type mask for objects to even be considered by this sensor.");

   Parent::initPersistFields();
}

void SensorData::packData(BitStream* stream)
{
   Parent::packData(stream);
}

void SensorData::unpackData(BitStream* stream)
{
   Parent::unpackData(stream);
}

bool SensorData::onAdd()
{
   return Parent::onAdd();
}

bool SensorData::preload(bool server, String &errorStr)
{
   if(!Parent::preload(server, errorStr))
      return false;

   if(!server)
      return true;

   U32 j = 0;
   for(U32 i = 0; i < MaxRules; i++)
   {
      if(ruleStrings[i].length())
      {
         U32 space = ruleStrings[i].find(' ');
         if(space != String::NPos)
         {
            String name = ruleStrings[i].substr(0, space);
            String data = ruleStrings[i].substr(space + 1);
            SensorRule *rule = NULL;
            SensorRule::Factory::const_iterator it = SensorRule::gSensorRuleMap.find(name.c_str());
            if(it != SensorRule::gSensorRuleMap.end())
               rule = it->second();
            else
            {
               Con::errorf("Unable to find Sensor rule type '%s' for SensorData %d", name.c_str(), getId());
               continue;
            }
            if(rule->init(data.c_str()))
               rules[j++] = rule;
            else
            {
               Con::errorf("Unable to initialise rule '%s' for SensorData %d", ruleStrings[i].c_str(), getId());
               delete rule;
            }
         }
      }
   }

   return true;
}

//-----------------------------------------------------------------------------

F32 SensorData::test(SceneObject *obj, const MatrixF &trans, SceneObject *other, const MatrixF &otrans)
{
   if(other && !(other->getTypeMask() & typemask))
      return 0.0f;
   F32 total = 1.0f;
   for(U32 i = 0; i < MaxRules; i++)
   {
      if(rules[i])
      {
         F32 vis = rules[i]->check(obj, trans, other, otrans);
         vis = ranges[i].x + vis * (ranges[i].y - ranges[i].x);
         total *= vis;
         if(total == 0.0f)
            break;
      }
   }
   return total;
}

DefineEngineMethod(SensorData, test, F32, (SimObjectId objid, TransformF trans, SimObjectId othid, TransformF otrans),,
   "Test the visibility of a given object from a particular transform")
{
   SceneObject *obj, *other;
   Sim::findObject(objid, obj);
   Sim::findObject(othid, other);
   return object->test(obj, trans.getMatrix(), other, otrans.getMatrix());
}

//-----------------------------------------------------------------------------

IMPLEMENT_CO_NETOBJECT_V1(Sensor);

Sensor::Sensor()
{
   mTypeMask |= TriggerObjectType;
   mNetFlags.clear(Ghostable); // Don't want to exist client-side unless we have to
   mDataBlock = NULL;

   mObject = NULL;
   mCallbackObject = NULL;
   mAttention = 1.0f;
   mNeedUpdate = true;

   mDebugRender = false;
}

Sensor::~Sensor()
{
   mContacts.clear();
}

bool Sensor::_setObject(void *obj, const char *array, const char *data)
{
   Sensor *s = static_cast<Sensor*>(obj);
   S32 id = dAtoi(data);
   SceneObject *ob;
   if(id > 0)
   {
      if(Sim::findObject(id, ob))
         s->mObject = ob;
   }
   else
   {
      if(Sim::findObject(data, ob))
         s->mObject = ob;
   }
   return false;
}

const char* Sensor::_getObject(void *obj, const char *data)
{
   Sensor *s = static_cast<Sensor*>(obj);
   return s->mObject.isNull() ? "" : s->mObject->getIdString();
}

bool Sensor::_setCallbackObject(void *obj, const char *array, const char *data)
{
   Sensor *s = static_cast<Sensor*>(obj);
   S32 id = dAtoi(data);
   SimObject *ob;
   if(id > 0)
   {
      if(Sim::findObject(id, ob))
         s->mCallbackObject = ob;
   }
   else
   {
      if(Sim::findObject(data, ob))
         s->mCallbackObject = ob;
   }
   return false;
}

const char* Sensor::_getCallbackObject(void *obj, const char *data)
{
   Sensor *s = static_cast<Sensor*>(obj);
   return s->mCallbackObject.isNull() ? "" : s->mCallbackObject->getIdString();
}

void Sensor::initPersistFields()
{
   addGroup("Sensor");

   addProtectedField("object", TypeString, NULL, &_setObject, &_getObject,
      "Object this Sensor is attached to.");
   addProtectedField("callbackObject", TypeString, NULL, &_setCallbackObject, &_getCallbackObject,
      "Object this Sensor should callback on. If none provided, will callback on its datablock.");

   endGroup("Sensor");

   Parent::initPersistFields();
}

bool Sensor::onAdd()
{
   if(!Parent::onAdd() || !mDataBlock)
      return false;

   if(gEditingMission)
      mNetFlags.set(Ghostable);

   mObjBox.minExtents.set(-mDataBlock->engagementRange);
   mObjBox.maxExtents.set( mDataBlock->engagementRange);
   resetWorldBox();

   return true;
}

void Sensor::onRemove()
{
   Parent::onRemove();
}

bool Sensor::onNewDataBlock(GameBaseData* dptr, bool reload)
{
   mDataBlock = dynamic_cast<SensorData*>(dptr);
   if(!mDataBlock || !Parent::onNewDataBlock(dptr, reload))
      return false;

   Polyhedron p;
   p.buildBox(MatrixF(true), Box3F(mDataBlock->engagementRange * 2));
   setTriggerPolyhedron(p);

   forceUpdate();

   return true;
}

//-----------------------------------------------------------------------------

F32 Sensor::checkVisibility(SceneObject *potential)
{
   return mDataBlock->test(mObject, getObjectTransform(), potential, potential->getTransform());
}

void Sensor::potentialEnterObject(GameBase *obj)
{
   if(obj == this || obj == mObject)
      return;
   if(!(obj->getTypeMask() & mDataBlock->typemask))
      return;
   mPotentialContacts.push_back(obj);
}

const Point3F& Sensor::getObjectPosition()
{
   return mObject.isNull()
      ? getPosition()
      : mObject->getPosition();
}

const MatrixF& Sensor::getObjectTransform()
{
   return mObject.isNull()
      ? getTransform()
      : mObject->getTransform();
}

//-----------------------------------------------------------------------------

void Sensor::processTick(const Move* move)
{
   Parent::processTick(move);

   if(!isServerObject())
      return;

   PROFILE_START(SensorProcessTick);

   // Check to see whether we're more than 10% distant from our cached box's centre.
   Point3F center = getBoxCenter();
   bool updated = false;
   if(mNeedUpdate || (getObjectPosition() - center).len() > mDataBlock->engagementRange * 0.1f)
   {
      PROFILE_START(SensorPotentialUpdate);
      // Flag that we've updated this tick.
      updated = true;
      mNeedUpdate = false;
      // Update our cache box.
      setTransform(getObjectTransform());
      // Container find objects.
      SimpleQueryList found;
      getContainer()->findObjects(getWorldBox(), mDataBlock->typemask, SimpleQueryList::insertionCallback, &found);
      for(U32 i = 0; i < found.mList.size(); i++)
      {
         if(found.mList[i]->getTypeMask() & GameBaseObjectType)
            potentialEnterObject((GameBase*)found.mList[i]);
      }
      PROFILE_END();
   }

   // potentialEnterObject pushes objects onto the potential contact list. Check that
   // list now to see if we can make any real contacts out of it.
   for(PotentialContactList::iterator c = mPotentialContacts.begin(); c != mPotentialContacts.end(); c++)
   {
      if(c->isNull())
         continue;
      GameBase *obj = c->getPointer();
      F32 vis = checkVisibility(obj);
      if(vis > 0.1f && std::find(mContacts.begin(), mContacts.end(), obj) == mContacts.end())
      {
         mContacts.push_back(Contact());
         mContacts.back().object = obj;
         mContacts.back().visibility = vis;
         throwCallback("onNewContact", obj, vis);
      }
   }
   mPotentialContacts.clear();

   // Update contacts.
   for(ContactList::iterator c = mContacts.begin(); c != mContacts.end(); c++)
   {
      F32 oldvis = c->visibility;
      F32 newvis = checkVisibility(c->object);
      c->visibility = newvis;
      c->timeSinceUpdate = 0;
      if(newvis > 0.0f)
      {
         c->lastLocation = c->object->getPosition();
         c->lastVelocity = c->object->getVelocity();
         c->timeSinceSeen = 0;
      }
      else
         c->timeSinceSeen++;
      if(newvis == 0.0f && oldvis != 0.0f)
         throwCallback("onContactLost", c->object, newvis);
      else if(newvis != 0.0f && oldvis == 0.0f)
         throwCallback("onContactSighted", c->object, newvis);
      else
      {
         // This will probably go away. I just needed a quick constant.
         static const F32 visStep = 0.2f;
         for (F32 v = visStep; v < 1.0f; v += visStep)
         {
            if((newvis > v && oldvis <= v) || (newvis <= v && oldvis > v))
            {
               throwCallback("onContactVisibilityChanged", c->object, newvis);
               break;
            }
         }
      }
   }
   setMaskBits(ContactUpdateMask);

   PROFILE_END();
}

void Sensor::interpolateTick(F32 delta)
{
   Parent::interpolateTick(delta);
}

void Sensor::advanceTime(F32 dt)
{
   Parent::advanceTime(dt);
}

void Sensor::throwCallback(const char* callback, SceneObject* contact, F32 visibility)
{
   SimObject *call = NULL;
   SimObject *obj = NULL;
   if(mCallbackObject.isNull())
   {
      call = mDataBlock;
      obj = this;
   }
   else
   {
      call = mCallbackObject;
      obj = mObject;
   }

   Con::executef(call, callback,
      obj ? obj->getIdString() : "0",
      contact ? contact->getIdString() : "0",
      Con::getFloatArg(visibility));
}

//-----------------------------------------------------------------------------

U32 Sensor::getContactCount()
{
   return mContacts.size();
}

//-----------------------------------------------------------------------------

U32 Sensor::packUpdate(NetConnection* conn, U32 mask, BitStream* stream)
{
   U32 retMask = Parent::packUpdate(conn, mask, stream);

   return retMask;
}

void Sensor::unpackUpdate(NetConnection* conn,BitStream* stream)
{
   Parent::unpackUpdate(conn, stream);
}

//-----------------------------------------------------------------------------

void Sensor::onEditorEnable()
{
   // Allow ghosting while editing to show debug render.
   mNetFlags.set(Ghostable);
}

void Sensor::onEditorDisable()
{
   mNetFlags.clear(Ghostable);
}

//-----------------------------------------------------------------------------

void Sensor::prepRenderImage(SceneRenderState* state)
{
   if(!isSelected())
      return;

   ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
   ri->renderDelegate.bind(this, &Sensor::render);
   ri->type = RenderPassManager::RIT_Editor;      
   ri->translucentSort = true;
   ri->defaultKey = 1;
   state->getRenderPass()->addInst(ri);
}

void Sensor::render(ObjectRenderInst *ri, SceneRenderState *state, BaseMatInstance *overrideMat)
{
   if(overrideMat)
      return;

   GFXStateBlockDesc desc;
   desc.setZReadWrite(true, false);
   desc.setBlend(true);
   desc.setCullMode(GFXCullNone);

   GFXDrawUtil *drawer = GFX->getDrawUtil();

   // Render faces.
   drawer->drawCube(desc, getWorldBox(), ColorI(255, 192, 0, 45));

   // Render wireframe.
   desc.setFillModeWireframe();
   drawer->drawCube(desc, getWorldBox(), ColorI::BLACK);
}

//-----------------------------------------------------------------------------

class DistanceRule : public SensorRule {
   /// Distance.
   F32 dist;

   DefineSensorRule(DistanceRule);

public:
   bool init(const String &data)
   {
      dist = dAtof(data.c_str());
      return dist > 0.0f;
   }

   F32 check(SceneObject *obj, const MatrixF &trans, SceneObject *other, const MatrixF &otrans) const
   {
      if(!other)
         return 0.0f;
      F32 d = (otrans.getPosition() - trans.getPosition()).len();
      return 1.0f - mClampF(d / dist, 0.0f, 1.0f);
   }
};

ImplementSensorRule(DistanceRule, Distance);

//-----------------------------------------------------------------------------

class AngleRule : public SensorRule {
   /// Cosine of angle.
   F32 cos;

   DefineSensorRule(AngleRule);

public:
   bool init(const String &data)
   {
      F32 ang = dAtof(data.c_str());
      cos = mCos(mDegToRad(ang));
      return ang > 0.0f;
   }

   F32 check(SceneObject *obj, const MatrixF &trans, SceneObject *other, const MatrixF &otrans) const
   {
      Point3F d = otrans.getPosition() - trans.getPosition();
      // Put d into local coordinates.
      MatrixF wtrans = trans;
      wtrans.affineInverse();
      wtrans.mulV(d);
      d.normalizeSafe();
      // Dot with forward vector is equivalent to extracting y component.
      F32 dot = d.y;
      return mClampF((cos - dot) / (cos - 1), 0.0f, 1.0f);
   }
};

ImplementSensorRule(AngleRule, Angle);

//-----------------------------------------------------------------------------

class Angle2DRule : public SensorRule {
   /// Cosine of horizontal angle.
   F32 cosH;
   /// Cosine of vertical angle.
   F32 cosV;

   DefineSensorRule(Angle2DRule);

public:
   bool init(const String &data)
   {
      if(dSscanf(data.c_str(), "%f %f", &cosH, &cosV) == 2)
      {
         cosH = mCos(mDegToRad(cosH));
         cosV = mCos(mDegToRad(cosV));
      }
      else if(dSscanf(data.c_str(), "r %f %f", &cosH, &cosV) == 2)
      {
         cosH = mCos(cosH);
         cosV = mCos(cosV);
      }
      else
         return false;
      return true;
   }

   F32 check(SceneObject *obj, const MatrixF &trans, SceneObject *other, const MatrixF &otrans) const
   {
      Point3F d = otrans.getPosition() - trans.getPosition();
      // Put d into local coordinates.
      MatrixF wtrans = trans;
      wtrans.affineInverse();
      wtrans.mulV(d);
      d.normalizeSafe();
      // Dot with forward vector is equivalent to extracting y component.
      F32 dot = d.y;
      // Put in xz plane.
      d.y = 0.0f;
      d.normalizeSafe();
      // At z=0, cos is horiz. At |z|=1, cos is vert.
      F32 cos = (1.0f - mFabs(d.z)) * cosH + mFabs(d.z) * cosV;
      // Check dot against cos.
      return mClampF((cos - dot) / (cos - 1), 0.0f, 1.0f);
   }
};

ImplementSensorRule(Angle2DRule, Angle2D);

//-----------------------------------------------------------------------------

class TypemaskRule : public SensorRule {
   /// Object typemask.
   U32 mask;

   DefineSensorRule(TypemaskRule);

public:
   bool init(const String &data)
   {
      mask = dAtoi(data.c_str());
      return mask != 0;
   }

   F32 check(SceneObject *obj, const MatrixF &trans, SceneObject *other, const MatrixF &otrans) const
   {
      if(!other)
         return 0.0f;
      if(other->getTypeMask() & mask)
         return 1.0f;
      return 0.0f;
   }
};

ImplementSensorRule(TypemaskRule, Typemask);

//-----------------------------------------------------------------------------

class RaycastRule : public SensorRule {
   /// Number of rays to cast.
   U32 rayCount;
   /// Typemask to cast rays against.
   U32 typemask;

   DefineSensorRule(RaycastRule);

public:
   bool init(const String &data)
   {
      if(dSscanf(data.c_str(), "%d %d", &rayCount, &typemask) != 2)
      {
         rayCount = 1;
         typemask = dAtoi(data.c_str());
      }
      return rayCount > 0 && typemask > 0;
   }

   F32 check(SceneObject *obj, const MatrixF &trans, SceneObject *other, const MatrixF &otrans) const
   {
      if(!obj)
         return 0.0f;

      // Disable objects for proper raycasting.
      if(obj)   obj->disableCollision();
      if(other) other->disableCollision();

      F32 vis = 0.0f;
      RayInfo info;
      if(!obj->getContainer()->castRay(trans.getPosition(), otrans.getPosition(), typemask, &info))
         vis = 1.0f;

      // Re-enable collision.
      if(obj)   obj->enableCollision();
      if(other) other->enableCollision();

      return vis;
   }
};

ImplementSensorRule(RaycastRule, Raycast);

//-----------------------------------------------------------------------------


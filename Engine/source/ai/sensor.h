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

#ifndef _SENSOR_H_
#define _SENSOR_H_

class SensorRule;

#include "T3D/trigger.h"

#include <map>
#include <vector>
#include <string>

struct SensorData : public TriggerData {
   typedef TriggerData Parent;

   enum Constants {
      MaxRules = 8,
   };

public:
   /// Typemask determining what object types are allowed to be detected.
   U32 typemask;

   /// Strings representing each rule.
   String ruleStrings[MaxRules];
   /// Actual SensorRule objects.
   SensorRule *rules[MaxRules];
   /// Min/max visibility per rule.
   Point2F ranges[MaxRules];

   /// Maximum range to which we will consider contacts
   F32 engagementRange;

   /// Increment in visibility levels used in onContactVisibilityChanged callback.
   F32 visibilityStep;

   /// Determine how visible an object is under our sensor model.
   F32 test(SceneObject *obj, const MatrixF &trans,
            SceneObject *oth, const MatrixF &otrans);

   DECLARE_CONOBJECT(SensorData);
   SensorData();
   ~SensorData();
   bool onAdd();
   bool preload(bool server, String &errorStr);

   void packData(BitStream* stream);
   void unpackData(BitStream* stream);
   static void initPersistFields();
};

//-----------------------------------------------------------------------------

class Sensor : public Trigger {
   typedef Trigger Parent;
public:
   enum MaskBits {
      StaticUpdateMask  = Parent::NextFreeMask << 0,
      ContactUpdateMask = Parent::NextFreeMask << 1,
      NextFreeMask      = Parent::NextFreeMask << 2,
   };

   /// Calculate the visibility level [0...1] of an object
   F32 checkVisibility(SceneObject* potential);

   /// Force an update next tick
   void forceUpdate() { mNeedUpdate = true; }

   /// Toggle special rendering stuff
   void setDebugRendering(bool render) { mDebugRender = render; }

   void potentialEnterObject(GameBase *);
   void potentialExitObject(GameBase *);

   /// Returns the current number of contacts
   U32 getContactCount();

   Point3F getObjectPosition();
   const MatrixF& getObjectTransform();

   Sensor();
   ~Sensor();

   bool onAdd();
   void onRemove();
   bool onNewDataBlock(GameBaseData* dptr, bool reload);

   void onEditorEnable();
   void onEditorDisable();

   static void initPersistFields();

	void prepRenderImage(SceneRenderState *state);
   void render(ObjectRenderInst *ri, SceneRenderState *state, BaseMatInstance *overrideMat);

   U32 packUpdate(NetConnection* conn, U32 mask, BitStream* stream);
   void unpackUpdate(NetConnection* conn, BitStream* stream);

   DECLARE_CONOBJECT(Sensor);

protected:
   virtual void processTick(const Move* move);
   virtual void interpolateTick(F32 delta);
   virtual void advanceTime(F32 dt);

private:
   enum Constants {
   };

   SensorData* mDataBlock;

   /// Make sure we update next tick.
   bool mNeedUpdate;

   /// An object we are attached to.
   SimObjectPtr<SceneObject> mObject;
   static bool _setObject(void *object, const char *index, const char *data);
   static const char *_getObject(void *object, const char *data);

   /// Object we should direct callbacks to.
   SimObjectPtr<SimObject> mCallbackObject;
   static bool _setCallbackObject(void *object, const char *index, const char *data);
   static const char *_getCallbackObject(void *object, const char *data);

   /// The 'level of attention', which affects the frequency of contact updates, and the thresholds for detecting and losing targets.
   F32 mAttention;

   /// A 'thing' we have noticed
   struct Contact
	{
      SimObjectPtr<SceneObject> object; ///< Object related to this contact.
      F32 visibility;       ///< How well we can see it.
      Point3F lastLocation; ///< Last place we saw/heard the contact.
      Point3F lastVelocity; ///< Last speed the contact had.
      F32 timeSinceSeen;    ///< How long since we lost sight of the contact.
      F32 timeSinceUpdate;  ///< How long since we last updated.
      Contact()
      {
			object = NULL;
         visibility = 0;
         lastLocation = lastVelocity = Point3F(0,0,0);
         timeSinceSeen = 0;
         timeSinceUpdate = 0;
      }
      bool operator<(const Contact &other) const { return visibility < other.visibility; }
      bool operator==(const Contact &other) const { return object == other.object; }
      bool operator==(const SceneObject *obj) const { return object == obj; }
      bool operator!=(const Contact &other) const { return object != other.object; }
      bool operator!=(const SceneObject *obj) const { return object != obj; }
   };
   typedef std::vector<Contact> ContactList;
   ContactList mContacts;
   typedef std::vector<SimObjectPtr<GameBase> > PotentialContactList;
   PotentialContactList mPotentialContacts;

   void throwCallback(const char* callback, SceneObject* contact, F32 visibility);

   bool mDebugRender;
};

//-----------------------------------------------------------------------------

/// Abstract base class used to define the SensorRule interface.
class SensorRule {
public:
   typedef std::map<String, SensorRule*(*)()> Factory;
   /// Map names of SensorRule types to functions that create them.
   static Factory gSensorRuleMap;

   /// Initialise this rule with a data string.
   virtual bool init(const String &data) = 0;
   /// Get the visibility of some object under this rule.
   virtual F32 check(SceneObject *obj, const MatrixF &trans,
                     SceneObject *oth, const MatrixF &otrans) const = 0;
};

/// Simply creates a new SensorRule of an indeterminate subclass.
template<typename T> SensorRule * createSensorRule() { return new T; }

/// A temporary struct that exists only to add a mapping to the factory.
template<typename T> struct SensorRuleRegister {
   SensorRuleRegister(StringTableEntry name)
   {
      SensorRule::gSensorRuleMap.insert(std::make_pair(name, &createSensorRule<T>));
   }
};

/// Defines a new SensorRule by creating a SensorRuleRegister member named __classRegister.
#define DefineSensorRule(cls) \
   static SensorRuleRegister< cls > __classRegister;

/// Initialises a SensorRule's __classRergister member.
#define ImplementSensorRule(cls, name) \
   SensorRuleRegister< cls > cls::__classRegister(#name);

#endif

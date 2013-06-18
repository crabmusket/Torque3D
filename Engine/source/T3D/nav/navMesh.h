//
// T3D 1.1 NavMesh class and supporting methods.
// Daniel Buckmaster, 2011
// With huge thanks to:
//    Lethal Concept http://www.lethalconcept.com/
//    Mikko Mononen http://code.google.com/p/recastnavigation/
//

#ifndef _NAVMESH_H_
#define _NAVMESH_H_

#include "scene/sceneObject.h"
#include "collision/concretePolyList.h"
#include "util/messaging/eventManager.h"
#include "platform/threads/thread.h"
#include "platform/threads/mutex.h"

#include "navMeshLoader.h"
#include "nav.h"

#include "recast/Recast.h"
#include "recast/DetourNavMesh.h"
#include "recast/DetourNavMeshBuilder.h"

namespace Nav {
   static void RCtoPolyList(NavModelData *data, AbstractPolyList *list);

   static const int NAVMESHSET_MAGIC = 'M'<<24 | 'S'<<16 | 'E'<<8 | 'T'; //'MSET';
   static const int NAVMESHSET_VERSION = 1;

   struct NavMeshSetHeader
   {
      int magic;
      int version;
      int numTiles;
      dtNavMeshParams params;
   };

   struct NavMeshTileHeader
   {
      dtTileRef tileRef;
      int dataSize;
   };

   /// @class NavMesh
   /// Represents a set of bounds within which a Recast navigation mesh is generated.
   /// @see NavMeshPolyList
   /// @see Trigger
   class NavMesh : public SceneObject {
      typedef SceneObject Parent;
      friend class NavPath;

   public:
      /// @name NavMesh build
      /// @{

      /// Do we build in a separate thread?
      bool mBuildThreaded;

      /// Initiates the navmesh build process, which includes notifying the
      /// clients and posting an event.
      bool build();
      /// Save the navmesh to a file.
      bool save();

      /// Load a saved navmesh from a file.
      bool load();

      /// Data file to store this nav mesh in. (From engine executable dir.)
      StringTableEntry mFileName;

      /// Cell width and height.
      F32 mCellSize, mCellHeight;
      /// @name Actor data
      /// @{
      F32 mWalkableHeight,
         mWalkableClimb,
         mWalkableRadius,
         mWalkableSlope;
      /// @}
      /// @name Generation data
      /// @{
      U32 mBorderSize;
      F32 mDetailSampleDist, mDetailSampleMaxError;
      U32 mMaxEdgeLen;
      F32 mMaxSimplificationError;
      static const U32 mMaxVertsPerPoly;
      U32 mMinRegionArea;
      U32 mMergeRegionArea;
      U32 mTileSize;
      /// @}

      /// Save imtermediate navmesh creation data?
      bool mSaveIntermediates;

      /// Get the EventManager for this NavMesh.
      EventManager *getEventManager() { return mEventManager; }

      /// @}

      /// @name SimObject
      /// @{

      virtual void onEditorEnable();
      virtual void onEditorDisable();

      void write(Stream &stream, U32 tabStop, U32 flags);

      /// @}

      /// @name SceneObject
      /// @{

      static void initPersistFields();

      bool onAdd();
      void onRemove();

      enum flags {
         BuildFlag    = Parent::NextFreeMask << 0,
         LoadFlag     = Parent::NextFreeMask << 1,
         NextFreeMask = Parent::NextFreeMask << 2,
      };

      U32 packUpdate(NetConnection *conn, U32 mask, BitStream *stream);
      void unpackUpdate(NetConnection *conn, BitStream *stream);

      /// @}

      /// @name Rendering
      /// @{

      /// Different rendering options provided by duDebugDraw.
      enum RenderMode {
         RENDER_NAVMESH,
         RENDER_CONTOURS,
         RENDER_POLYMESH,
         RENDER_DETAILMESH,
         RENDER_PORTALS,
      };

      /// Current rendering mode.
      RenderMode mRenderMode;

      bool mRenderInput;
      bool mRenderConnections;

      void prepRenderImage(SceneRenderState *state);
      void render(ObjectRenderInst *ri, SceneRenderState *state, BaseMatInstance *overrideMat);

      /// @}

      NavMesh();
      ~NavMesh();
      DECLARE_CONOBJECT(NavMesh);

   protected:

      dtNavMesh const* getNavMesh() { return nm; }

   private:
      /// Initiates the build process in a separate thread.
      bool buildThreaded();

      /// Runs the build process. Not threadsafe,. so take care to synchronise
      /// calls to this method properly!
      bool buildProcess();

      /// Generates a navigation mesh for the collection of objects in this
      /// mesh. Returns true if successful. Stores the created mesh in tnm.
      bool generateMesh();

      /// Performs the Recast part of the build process.
      bool createPolyMesh(rcConfig &cfg, NavModelData &data, rcContext *ctx);

      /// Performs the Detour part of the build process.
      bool createNavMesh(dtNavMeshCreateParams &params);

      /// Stores the input geometry. */
      ConcretePolyList *mInPolys;

      /// @name Intermediate data
      /// @{

      rcHeightfield        *hf;
      rcCompactHeightfield *chf;
      rcContourSet         *cs;
      rcPolyMesh           *pm;
      rcPolyMeshDetail     *pmd;
      dtNavMesh            *nm;
      dtNavMesh           *tnm;

      /// Free all stored working data.
      /// @param freeAll Force all data to be freed, retain none.
      void freeIntermediates(bool freeAll);

      /// @}

      /// Called when the 'build' box is checked in the editor.
      static bool editorBuildFlag(void *obj, const char *index, const char *data);
      /// Called when a script gets the value of 'build'.
      static const char *getBuild(void *obj, const char *data) {return "0"; }

      /// Used to perform non-standard validation. detailSampleDist can be 0, or >= 0.9.
      static bool setProtectedDetailSampleDist(void *obj, const char *index, const char *data);

      /// Use this object to manage update events.
      EventManager *mEventManager;

      /// @name Threaded updates
      /// @{

      /// A thread for us to update in.
      Thread *mThread;

      /// A mutex for NavMesh builds.
      Mutex mBuildLock;

      /// A mutex for accessing our actual navmesh.
      Mutex mNavMeshLock;

      /// A simple flag to say we are building.
      bool mBuilding;

      /// Thread launch function.
      static void launchThreadedBuild(void *data);

      /// @}
   };
};

typedef Nav::NavMesh::RenderMode NavMeshRenderMode;
DefineEnumType( NavMeshRenderMode );

#endif

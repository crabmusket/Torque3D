//
// T3D 1.1 NavMesh class and supporting methods.
// Daniel Buckmaster, 2011
// With huge thanks to:
//    Lethal Concept http://www.lethalconcept.com/
//    Mikko Mononen http://code.google.com/p/recastnavigation/
//

#include <stdio.h>

#include "navMesh.h"
#include "navMeshDraw.h"
#include "recast/DetourDebugDraw.h"
#include "recast/RecastDebugDraw.h"

#include "math/mathUtils.h"
#include "math/mRandom.h"
#include "console/consoleTypes.h"
#include "console/engineAPI.h"
#include "console/typeValidators.h"
//#include "coverPoint.h"

#include "scene/sceneRenderState.h"
#include "gfx/gfxDrawUtil.h"
#include "renderInstance/renderPassManager.h"
#include "gfx/primBuilder.h"

#include "core/stream/bitStream.h"
#include "math/mathIO.h"

extern bool gEditingMission;

ImplementEnumType(NavMeshRenderMode,
   "The rendering mode for this NavMesh.\n")
   { Nav::NavMesh::RENDER_NAVMESH,    "Navmesh",    "Renders the final navmesh that is created.\n"    },
   { Nav::NavMesh::RENDER_CONTOURS,   "Contours",   "Renders region outlines.\n"    },
   { Nav::NavMesh::RENDER_POLYMESH,   "Polymesh",   "Renders the poly mesh created by Recast.\n"    },
   { Nav::NavMesh::RENDER_DETAILMESH, "Detailmesh", "Renders the detailed poly mesh.\n"    },
   { Nav::NavMesh::RENDER_PORTALS,    "Portals",    "Renders region portals.\n"    },
EndImplementEnumType;

namespace Nav {
   void RCtoPolyList(NavModelData *data, AbstractPolyList *list)
   {
      for(U32 i = 0; i < data->getVertCount(); i++)
      {
         U32 vert = i * 3;
         list->addPoint(RCtoDTS(data->verts[vert],
            data->verts[vert + 1],
            data->verts[vert + 2]));
      }
      for(U32 i = 0; i < data->getTriCount(); i++)
      {
         U32 tri = i * 3;
         S32 v1 = data->tris[tri] - 1;
         S32 v2 = data->tris[tri + 1] - 1;
         S32 v3 = data->tris[tri + 2] - 1;

         list->begin(0, i);
         list->vertex(v1);
         list->vertex(v2);
         list->vertex(v3);
         list->plane(v1, v2, v3);
         list->end();
      }
   }

   /* ------------------------------------------------------------
       ----------------------- NavMesh ------------------------- */

   IMPLEMENT_CO_NETOBJECT_V1(NavMesh);

   const U32 NavMesh::mMaxVertsPerPoly = 3;

   NavMesh::NavMesh()
   {
      mTypeMask |= MarkerObjectType;
      mFileName = StringTable->insert("");

      mBuildThreaded = true;

      mRenderMode = RENDER_NAVMESH;
      mRenderInput = false;
      mRenderConnections = false;

      mEventManager = NULL;

      mSaveIntermediates = false;

      mInPolys = NULL;

      mCellSize = mCellHeight = 0.3f;
      mWalkableHeight = 2.0f;
      mWalkableClimb = 1.0f;
      mWalkableRadius = 0.5f;
      mWalkableSlope = 45.0f;
      mBorderSize = 1;
      mDetailSampleDist = 6.0f;
      mDetailSampleMaxError = 1.0f;
      mMaxEdgeLen = 12;
      mMaxSimplificationError = 1.3f;
      mMinRegionArea = 8;
      mMergeRegionArea = 20;
      mTileSize = 32;

      hf  = NULL;
      chf = NULL;
      cs  = NULL;
      pm  = NULL;
      pmd = NULL;
      nm  = NULL;
      tnm = NULL;

      mThread = NULL;
      mBuilding = false;
   }

   NavMesh::~NavMesh()
   {
      if(mThread)
      {
         mThread->join();
         delete mThread;
         mThread = NULL;
      }
      freeIntermediates(true);
      dtFreeNavMesh(nm);
      nm = NULL;
      dtFreeNavMesh(tnm);
      tnm = NULL;
   }

   bool NavMesh::editorBuildFlag(void *obj, const char *index, const char *data)
   {
      NavMesh *object = static_cast<NavMesh*>(obj);
      if(dAtob(data))
         object->buildThreaded();
      return false;
   }

   bool NavMesh::setProtectedDetailSampleDist(void *obj, const char *index, const char *data)
   {
      F32 dist = dAtof(data);
      if(dist == 0.0f || dist >= 0.9f)
      {
         Con::errorf("NavMesh::detailSampleDist must be 0 or greater than 0.9!");
         return true;
      }
      return false;
   }

   FRangeValidator ValidCellSize(0.01f, 10.0f);
   FRangeValidator ValidSlopeAngle(0.0f, 89.9f);
   IRangeValidator PositiveInt(0, S32_MAX);

   void NavMesh::initPersistFields()
   {
      addGroup("NavMesh Build");

      addField("saveIntermediates", TypeBool, Offset(mSaveIntermediates, NavMesh),
         "Store intermediate results for debug rendering.");

      addField("buildThreaded", TypeBool, Offset(mBuildThreaded, NavMesh),
         "Does this NavMesh build in a separate thread?");

      addProtectedField("build", TypeBool, NULL,
         &editorBuildFlag, &getBuild,
         "Check this box to build the NavMesh.");

      endGroup("NavMesh Build");

      addGroup("NavMesh Options");

      addField("fileName", TypeString, Offset(mFileName, NavMesh),
         "Name of the data file to store this navmesh in (relative to engine executable).");

      addFieldV("borderSize", TypeS32, Offset(mBorderSize, NavMesh), &PositiveInt,
         "Size of the non-walkable border around the navigation mesh (in voxels).");

      addFieldV("cellSize", TypeF32, Offset(mCellSize, NavMesh), &ValidCellSize,
         "Length/width of a voxel.");
      addFieldV("cellHeight", TypeF32, Offset(mCellHeight, NavMesh), &ValidCellSize,
         "Height of a voxel.");

      addFieldV("actorHeight", TypeF32, Offset(mWalkableHeight, NavMesh), &CommonValidators::PositiveFloat,
         "Height of an actor.");
      addFieldV("actorClimb", TypeF32, Offset(mWalkableClimb, NavMesh), &CommonValidators::PositiveFloat,
         "Maximum climbing height of an actor.");
      addFieldV("actorRadius", TypeF32, Offset(mWalkableRadius, NavMesh), &CommonValidators::PositiveFloat,
         "Radius of an actor.");
      addFieldV("walkableSlope", TypeF32, Offset(mWalkableSlope, NavMesh), &ValidSlopeAngle,
         "Maximum walkable slope in degrees.");

      endGroup("NavMesh Options");

      addGroup("NavMesh Advanced Options");

      addProtectedField("detailSampleDist", TypeF32, Offset(mDetailSampleDist, NavMesh),
         &setProtectedDetailSampleDist, &defaultProtectedGetFn,
         "Sets the sampling distance to use when generating the detail mesh.");
      addFieldV("detailSampleError", TypeF32, Offset(mDetailSampleMaxError, NavMesh), &CommonValidators::PositiveFloat,
         "The maximum distance the detail mesh surface should deviate from heightfield data.");
      addFieldV("maxEdgeLen", TypeS32, Offset(mDetailSampleDist, NavMesh), &PositiveInt,
         "The maximum allowed length for contour edges along the border of the mesh.");
      addFieldV("simplificationError", TypeF32, Offset(mMaxSimplificationError, NavMesh), &CommonValidators::PositiveFloat,
         "The maximum distance a simplfied contour's border edges should deviate from the original raw contour.");
      addFieldV("minRegionArea", TypeS32, Offset(mMinRegionArea, NavMesh), &PositiveInt,
         "The minimum number of cells allowed to form isolated island areas.");
      addFieldV("mergeRegionArea", TypeS32, Offset(mMergeRegionArea, NavMesh), &PositiveInt,
         "Any regions with a span count smaller than this value will, if possible, be merged with larger regions.");
      addFieldV("tileSize", TypeS32, Offset(mTileSize, NavMesh), &PositiveInt,
         "The horizontal size of tiles.");

      endGroup("NavMesh Advanced Options");

      addGroup("NavMesh Rendering");

      addField("renderMode", TYPEID<RenderMode>(), Offset(mRenderMode, NavMesh),
         "Sets the rendering mode of this navmesh.");
      addField("renderInput", TypeBool, Offset(mRenderInput, NavMesh),
         "Render the input geometry used to create the mesh.");
      addField("renderConnections", TypeBool, Offset(mRenderConnections, NavMesh),
         "Render the connections between regions in the mesh.");

      endGroup("NavMesh Rendering");

      Parent::initPersistFields();
   }

   bool NavMesh::onAdd()
   {
      if(!Parent::onAdd())
         return false;

      mObjBox.set(Point3F(-1.0f, -1.0f, -1.0f),
         Point3F( 1.0f,  1.0f,  1.0f));
      resetWorldBox();

      addToScene();

      if(gEditingMission)
         mNetFlags.set(Ghostable);

      if(isServerObject())
      {
         mEventManager = new EventManager();
         if(!mEventManager->registerObject())
         {
            Con::errorf("Could not register EventManager for NavMesh %d!", getId());
            delete mEventManager;
         }
         else
         {
            mEventManager->setMessageQueue(mEventManager->getIdString());
            mEventManager->registerEvent("NavMeshLoad");
            mEventManager->registerEvent("NavMeshBuild");
         }
      }

      load();

      return true;
   }

   void NavMesh::onRemove()
   {
      removeFromScene();

      if(mEventManager)
         mEventManager->deleteObject();

      Parent::onRemove();
   }

   bool NavMesh::build()
   {
      if(mBuildThreaded)
         return buildThreaded();

      // If we're running a local server, just steal the server's navmesh
      MutexHandle handle;
      if(!handle.lock(&mBuildLock, true) || isClientObject())
         return false;

      return buildProcess();
   }

   bool NavMesh::buildThreaded()
   {
      MutexHandle handle;
      if(!handle.lock(&mBuildLock, true) || isClientObject())
         return false;

      if(mThread)
      {
         mThread->join();
         delete mThread;
         mThread = NULL;
      }
      mThread = new Thread((ThreadRunFunction)launchThreadedBuild, this);
      mThread->start();

      return true;
   }

   void NavMesh::launchThreadedBuild(void *data)
   {
      NavMesh *pThis = (NavMesh*)data;
      pThis->buildProcess();
   }

   bool NavMesh::buildProcess()
   {
      mBuilding = true;

      // Create mesh
      bool success = generateMesh();

      mNavMeshLock.lock();
      // Copy new navmesh into old.
      dtNavMesh *old = nm;
      nm = tnm; // I am trusting that this is atomic.
      dtFreeNavMesh(old);
      tnm = NULL;
      mNavMeshLock.unlock();

      // Free structs used during build
      freeIntermediates(false);

      // Alert event manager that we have been built.
      if(mEventManager)
         mEventManager->postEvent("NavMeshBuild", "");

      mBuilding = false;

      return success;
   }

   bool NavMesh::generateMesh()
   {
      // Parse objects from level into RC-compatible format
      NavModelData data = NavMeshLoader::mergeModels(
         NavMeshLoader::parseTerrainData(getWorldBox(), 0),
         NavMeshLoader::parseStaticObjects(getWorldBox()),
         true);

      // Check for no geometry
      if(!data.getVertCount())
         return false;

      // Free intermediate and final results
      freeIntermediates(true);

      // Create mInPolys if we don't have one already
      if(!mInPolys && mSaveIntermediates)
         mInPolys = new ConcretePolyList();

      // Reconstruct input geometry from out data
      if(mSaveIntermediates)
         RCtoPolyList(&data, mInPolys);

      // Recast initialisation data
      rcContext ctx(false);
      rcConfig cfg;

      dMemset(&cfg, 0, sizeof(cfg));
      cfg.cs = mCellSize;
      cfg.ch = mCellHeight;
      rcCalcBounds(data.verts, data.getVertCount(), cfg.bmin, cfg.bmax);
      rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);
      cfg.walkableHeight = mCeil(mWalkableHeight / mCellHeight);
      cfg.walkableClimb = mCeil(mWalkableClimb / mCellHeight);
      cfg.walkableRadius = mCeil(mWalkableRadius / mCellSize);
      cfg.walkableSlopeAngle = mWalkableSlope;
      cfg.borderSize = mBorderSize;
      cfg.detailSampleDist = mDetailSampleDist;
      cfg.detailSampleMaxError = mDetailSampleMaxError;
      cfg.maxEdgeLen = mMaxEdgeLen;
      cfg.maxSimplificationError = mMaxSimplificationError;
      cfg.maxVertsPerPoly = 3;
      cfg.minRegionArea = mMinRegionArea;
      cfg.mergeRegionArea = mMergeRegionArea;
      cfg.tileSize = mTileSize;

      if(!createPolyMesh(cfg, data, &ctx))
         return false;

      //Detour initialisation data
      dtNavMeshCreateParams params;
      dMemset(&params, 0, sizeof(params));

      params.walkableHeight = cfg.walkableHeight;
      params.walkableRadius = cfg.walkableRadius;
      params.walkableClimb = cfg.walkableClimb;
      params.tileX = 0;
      params.tileY = 0;
      params.tileLayer = 0;
      rcVcopy(params.bmax, cfg.bmax);
      rcVcopy(params.bmin, cfg.bmin);
      params.buildBvTree = true;
      params.ch = cfg.ch;
      params.cs = cfg.cs;

      params.verts = pm->verts;
      params.vertCount = pm->nverts;
      params.polys = pm->polys;
      params.polyAreas = pm->areas;
      params.polyFlags = pm->flags;
      params.polyCount = pm->npolys;
      params.nvp = pm->nvp;

      params.detailMeshes = pmd->meshes;
      params.detailVerts = pmd->verts;
      params.detailVertsCount = pmd->nverts;
      params.detailTris = pmd->tris;
      params.detailTriCount = pmd->ntris;

      if(!createNavMesh(params))
         return false;

      return true;
   }

   bool NavMesh::createPolyMesh(rcConfig &cfg, NavModelData &data, rcContext *ctx)
   {
      // Create a heightfield to voxelise our input geometry
      hf = rcAllocHeightfield();
      if(!hf || !rcCreateHeightfield(ctx, *hf, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch))
      {
         Con::errorf("Could not generate rcHeightField for NavMesh %s", getIdString());
         return false;
      }

      unsigned char *areas = new unsigned char[data.getTriCount()];
      if (!areas)
      {
         Con::errorf("Out of memory (area flags) for NavMesh %s", getIdString());
         return false;
      }
      memset(areas, 0, data.getTriCount()*sizeof(unsigned char));

      // Subtract 1 from all indices!
      for(U32 i = 0; i < data.getTriCount(); i++)
      {
         data.tris[i*3]--;
         data.tris[i*3+1]--;
         data.tris[i*3+2]--;
      }

      // Filter triangles by angle and rasterize
      rcMarkWalkableTriangles(ctx, cfg.walkableSlopeAngle,
         data.getVerts(), data.getVertCount(),
         data.getTris(), data.getTriCount(), areas);
      rcRasterizeTriangles(ctx, data.getVerts(), data.getVertCount(),
         data.getTris(), areas, data.getTriCount(),
         *hf, cfg.walkableClimb);

      delete [] areas;

      // Filter out areas with low ceilings and other stuff
      rcFilterLowHangingWalkableObstacles(ctx, cfg.walkableClimb, *hf);
      rcFilterLedgeSpans(ctx, cfg.walkableHeight, cfg.walkableClimb, *hf);
      rcFilterWalkableLowHeightSpans(ctx, cfg.walkableHeight, *hf);

      chf = rcAllocCompactHeightfield();
      if(!chf || !rcBuildCompactHeightfield(ctx, cfg.walkableHeight, cfg.walkableClimb, *hf, *chf))
      {
         Con::errorf("Could not generate rcCompactHeightField for NavMesh %s", getIdString());
         return false;
      }
      if(!rcErodeWalkableArea(ctx, cfg.walkableRadius, *chf))
      {
         Con::errorf("Could not erode walkable area for NavMesh %s", getIdString());
         return false;
      }
      if(false)
      {
         if(!rcBuildRegionsMonotone(ctx, *chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea))
         {
            Con::errorf("Could not build regions for NavMesh %s", getIdString());
            return false;
         }
      }
      else
      {
         if(!rcBuildDistanceField(ctx, *chf))
            return false;
         if(!rcBuildRegions(ctx, *chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea))
            return false;
      }

      cs = rcAllocContourSet();
      if(!cs || !rcBuildContours(ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cs))
      {
         Con::errorf("Could not construct rcContourSet for NavMesh %s", getIdString());
         return false;
      }

      pm = rcAllocPolyMesh();
      if(!pm || !rcBuildPolyMesh(ctx, *cs, cfg.maxVertsPerPoly, *pm))
      {
         Con::errorf("Could not construct rcPolyMesh for NavMesh %s", getIdString());
         return false;
      }

      pmd = rcAllocPolyMeshDetail();
      if(!pmd || !rcBuildPolyMeshDetail(ctx, *pm, *chf, cfg.detailSampleDist, cfg.detailSampleMaxError, *pmd))
      {
         Con::errorf("Could not construct rcPolyMeshDetail for NavMesh %s", getIdString());
         return false;
      }

      return true;
   }

   bool NavMesh::createNavMesh(dtNavMeshCreateParams &params)
   {
      unsigned char *tileData = NULL;
      S32 tileDataSize = 0;
      if(!dtCreateNavMeshData(&params, &tileData, &tileDataSize))
      {
         Con::errorf("Could not construct NavMeshData for NavMesh %s", getIdString());
         return false;
      }

      tnm = dtAllocNavMesh();
      if(!tnm)
      {
         Con::errorf("Out of memory allocating dtNavMesh for NavMesh %s", getIdString());
         return false;
      }
      dtStatus s = tnm->init(tileData, tileDataSize, DT_TILE_FREE_DATA);
      if(dtStatusFailed(s))
      {
         Con::errorf("Could not initialise dtNavMesh for NavMesh %s", getIdString());
         return false;
      }

      // Initialise all flags to something helpful.
      for(U32 i = 0; i < tnm->getMaxTiles(); ++i)
      {
         const dtMeshTile* tile = ((const dtNavMesh*)tnm)->getTile(i);
         if(!tile->header) continue;
         const dtPolyRef base = tnm->getPolyRefBase(tile);
         for(U32 j = 0; j < tile->header->polyCount; ++j)
         {
            const dtPolyRef ref = base | j;
            unsigned short f = 0;
            tnm->getPolyFlags(ref, &f);
            tnm->setPolyFlags(ref, f | 1);
         }
      }

      return true;
   }

   void NavMesh::freeIntermediates(bool freeAll)
   {
      mNavMeshLock.lock();

      rcFreeHeightField(hf);          hf = NULL;
      rcFreeCompactHeightfield(chf); chf = NULL;

      if(!mSaveIntermediates || freeAll)
      {
         rcFreeContourSet(cs);        cs = NULL;
         rcFreePolyMesh(pm);          pm = NULL;
         rcFreePolyMeshDetail(pmd);  pmd = NULL;
         delete mInPolys;
         mInPolys = NULL;
      }

      mNavMeshLock.unlock();
   }

   void NavMesh::prepRenderImage(SceneRenderState *state)
   {
      ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
      ri->renderDelegate.bind(this, &NavMesh::render);
      ri->type = RenderPassManager::RIT_Editor;      
      ri->translucentSort = true;
      ri->defaultKey = 1;
      state->getRenderPass()->addInst(ri);
   }

   void NavMesh::render(ObjectRenderInst *ri, SceneRenderState *state, BaseMatInstance *overrideMat)
   {
      if(overrideMat)
         return;

      if(state->isReflectPass())
         return;

      PROFILE_SCOPE(NavMesh_Render);

      GFXDrawUtil *drawer = GFX->getDrawUtil();

      GFXStateBlockDesc desc;
      desc.setZReadWrite(true, false);
      desc.setBlend(true);
      desc.setCullMode(GFXCullNone);

      drawer->drawCube(desc, getWorldBox(), ColorI(136, 255, 228, 45));
      desc.setFillModeWireframe();
      drawer->drawCube(desc, getWorldBox(), ColorI::BLACK);

      // Recast debug draw
      duDebugDrawTorque dd;
      NetObject *no = getServerObject();
      if(no && isSelected())
      {
         NavMesh *n = static_cast<NavMesh*>(no);
         RenderMode mode = mRenderMode;
         bool build = n->mBuilding;
         if(build)
         {
            mode = RENDER_NAVMESH;
            dd.overrideColour(duRGBA(255, 0, 0, 80));
         }
         n->mNavMeshLock.lock();
         switch(mode)
         {
         case RENDER_NAVMESH:    if(n->nm) duDebugDrawNavMesh          (&dd, *n->nm, 0); break;
         case RENDER_CONTOURS:   if(n->cs) duDebugDrawContours         (&dd, *n->cs); break;
         case RENDER_POLYMESH:   if(n->pm) duDebugDrawPolyMesh         (&dd, *n->pm); break;
         case RENDER_DETAILMESH: if(n->pmd) duDebugDrawPolyMeshDetail  (&dd, *n->pmd); break;
         case RENDER_PORTALS:    if(n->nm) duDebugDrawNavMeshPortals   (&dd, *n->nm); break;
         }
         if(n->cs && mRenderConnections && !build)   duDebugDrawRegionConnections(&dd, *n->cs);
         if(n->mInPolys && mRenderInput && !build)   n->mInPolys->render();
         n->mNavMeshLock.unlock();
      }
   }

   void NavMesh::onEditorEnable()
   {
      mNetFlags.set(Ghostable);
   }

   void NavMesh::onEditorDisable()
   {
      mNetFlags.clear(Ghostable);
   }

   U32 NavMesh::packUpdate(NetConnection *conn, U32 mask, BitStream *stream)
   {
      U32 retMask = Parent::packUpdate(conn, mask, stream);

      if(mask & InitialUpdateMask)
      {
         mask &= ~BuildFlag;
         retMask &= ~BuildFlag;
      }

      mathWrite(*stream, getTransform());
      mathWrite(*stream, getScale());

      stream->writeFlag(mSaveIntermediates);

      stream->writeString(mFileName);

      stream->writeInt(mRenderMode, 8);
      stream->writeFlag(mRenderInput);
      stream->writeFlag(mRenderConnections);

      stream->write(mCellSize);
      stream->write(mCellHeight);
      stream->write(mWalkableHeight);
      stream->write(mWalkableClimb);
      stream->write(mWalkableRadius);
      stream->write(mWalkableSlope);

      return retMask;
   }

   void NavMesh::unpackUpdate(NetConnection *conn, BitStream *stream)
   {
      Parent::unpackUpdate(conn, stream);

      mathRead(*stream, &mObjToWorld);
      mathRead(*stream, &mObjScale);

      setTransform(mObjToWorld);

      mSaveIntermediates = stream->readFlag();

      mFileName = stream->readSTString();

      mRenderMode = (RenderMode)stream->readInt(8);
      mRenderInput = stream->readFlag();
      mRenderConnections = stream->readFlag();

      stream->read(&mCellSize);
      stream->read(&mCellHeight);
      stream->read(&mWalkableHeight);
      stream->read(&mWalkableClimb);
      stream->read(&mWalkableRadius);
      stream->read(&mWalkableSlope);
   }

   bool NavMesh::load()
   {
      if(!dStrlen(mFileName))
         return false;

      FILE* fp = fopen(mFileName, "rb");
      if(!fp)
         return false;

      // Read header.
      NavMeshSetHeader header;
      fread(&header, sizeof(NavMeshSetHeader), 1, fp);
      if(header.magic != NAVMESHSET_MAGIC)
      {
         fclose(fp);
         return 0;
      }
      if(header.version != NAVMESHSET_VERSION)
      {
         fclose(fp);
         return 0;
      }

      MutexHandle handle;
      handle.lock(&mNavMeshLock, true);

      if(nm)
         dtFreeNavMesh(nm);
      nm = dtAllocNavMesh();
      if(!nm)
      {
         fclose(fp);
         return false;
      }

      dtStatus status = nm->init(&header.params);
      if(dtStatusFailed(status))
      {
         fclose(fp);
         return false;
      }

      // Read tiles.
      for(U32 i = 0; i < header.numTiles; ++i)
      {
         NavMeshTileHeader tileHeader;
         fread(&tileHeader, sizeof(tileHeader), 1, fp);
         if(!tileHeader.tileRef || !tileHeader.dataSize)
            break;

         unsigned char* data = (unsigned char*)dtAlloc(tileHeader.dataSize, DT_ALLOC_PERM);
         if(!data) break;
         memset(data, 0, tileHeader.dataSize);
         fread(data, tileHeader.dataSize, 1, fp);

         nm->addTile(data, tileHeader.dataSize, DT_TILE_FREE_DATA, tileHeader.tileRef, 0);
      }

      fclose(fp);

      if(isServerObject())
      {
         setMaskBits(LoadFlag);
         if(mEventManager)
            mEventManager->postEvent("NavMeshLoad", "");
      }

      return true;
   }

   bool NavMesh::save()
   {
      if(!dStrlen(mFileName) || !nm)
         return false;

      // Save our navmesh into a file to load from next time
      FILE* fp = fopen(mFileName, "wb");
      if(!fp)
         return false;

      MutexHandle handle;
      handle.lock(&mNavMeshLock, true);

      // Store header.
      NavMeshSetHeader header;
      header.magic = NAVMESHSET_MAGIC;
      header.version = NAVMESHSET_VERSION;
      header.numTiles = 0;
      for(U32 i = 0; i < nm->getMaxTiles(); ++i)
      {
         const dtMeshTile* tile = ((const dtNavMesh*)nm)->getTile(i);
         if (!tile || !tile->header || !tile->dataSize) continue;
         header.numTiles++;
      }
      memcpy(&header.params, nm->getParams(), sizeof(dtNavMeshParams));
      fwrite(&header, sizeof(NavMeshSetHeader), 1, fp);

      // Store tiles.
      for(U32 i = 0; i < nm->getMaxTiles(); ++i)
      {
         const dtMeshTile* tile = ((const dtNavMesh*)nm)->getTile(i);
         if(!tile || !tile->header || !tile->dataSize) continue;

         NavMeshTileHeader tileHeader;
         tileHeader.tileRef = nm->getTileRef(tile);
         tileHeader.dataSize = tile->dataSize;
         fwrite(&tileHeader, sizeof(tileHeader), 1, fp);

         fwrite(tile->data, tile->dataSize, 1, fp);
      }

      fclose(fp);

      return true;
   }

   void NavMesh::write(Stream &stream, U32 tabStop, U32 flags)
   {
      save();
      Parent::write(stream, tabStop, flags);
   }

   DefineEngineMethod(NavMesh, build, bool, (),,
      "@brief Create a Recast nav mesh.")
   {
      return object->build();
   }

   DefineEngineMethod(NavMesh, load, bool, (),,
      "@brief Load this NavMesh from its file.")
   {
      return object->load();
   }

   DefineEngineMethod(NavMesh, save, void, (),,
      "@brief Save this NavMesh to its file.")
   {
      object->save();
   }

   DefineEngineMethod(NavMesh, getEventManager, S32, (),,
      "@brief Get the EventManager object associated with this NavMesh.")
   {
      return object->getEventManager()? object->getEventManager()->getId() : 0;
   }
};

//-----------------------------------------------------------------------------
// Copyright (c) 2012 GarageGames, LLC
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

/*
   Resource stream -> Buffer
   Buffer -> Collada DOM
   Collada DOM -> TSShapeLoader
   TSShapeLoader installed into TSShape
*/

//-----------------------------------------------------------------------------

#include "platform/platform.h"

#include "ts/assimp/assimpShapeLoader.h"
#include "ts/assimp/assimpAppNode.h"
#include "ts/assimp/assimpAppMaterial.h"

#include "core/util/tVector.h"
#include "core/strings/findMatch.h"
#include "core/stream/fileStream.h"
#include "core/fileObject.h"
#include "ts/tsShape.h"
#include "ts/tsShapeInstance.h"
#include "materials/materialManager.h"
#include "console/persistenceManager.h"
#include "ts/tsShapeConstruct.h"
#include "core/util/zip/zipVolume.h"
#include "gfx/bitmap/gBitmap.h"

// assimp include files. 
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/types.h>

MODULE_BEGIN( AssimpShapeLoader )
   MODULE_INIT_AFTER( ShapeLoader )
   MODULE_INIT
   {
      TSShapeLoader::addFormat("DirectX X", "x");
      TSShapeLoader::addFormat("Autodesk FBX", "fbx");
      TSShapeLoader::addFormat("Blender 3D", "blend" );
      TSShapeLoader::addFormat("3ds Max 3DS", "3ds");
      TSShapeLoader::addFormat("3ds Max ASE", "ase");
      TSShapeLoader::addFormat("Wavefront Object", "obj");
      TSShapeLoader::addFormat("Industry Foundation Classes (IFC/Step)", "ifc");
      TSShapeLoader::addFormat("XGL", "xgl");
      TSShapeLoader::addFormat("Stanford Polygon Library", "ply");
      TSShapeLoader::addFormat("AutoCAD DXF", "dxf");
      TSShapeLoader::addFormat("LightWave", "lwo");
      TSShapeLoader::addFormat("LightWave Scene", "lws");
      TSShapeLoader::addFormat("Modo", "lxo");
      TSShapeLoader::addFormat("Stereolithography", "stl");
      TSShapeLoader::addFormat("AC3D", "ac");
      TSShapeLoader::addFormat("Milkshape 3D", "ms3d");
      TSShapeLoader::addFormat("TrueSpace COB", "cob");
      TSShapeLoader::addFormat("TrueSpace SCN", "scn");
      TSShapeLoader::addFormat("Ogre XML", "xml");
      TSShapeLoader::addFormat("Irrlicht Mesh", "irrmesh");
      TSShapeLoader::addFormat("Irrlicht Scene", "irr");
      TSShapeLoader::addFormat("Quake I", "mdl" );
      TSShapeLoader::addFormat("Quake II", "md2" );
      TSShapeLoader::addFormat("Quake III Mesh", "md3");
      TSShapeLoader::addFormat("Quake III Map/BSP", "pk3");
      TSShapeLoader::addFormat("Return to Castle Wolfenstein", "mdc");
      TSShapeLoader::addFormat("Doom 3", "md5" );
      TSShapeLoader::addFormat("Valve SMD", "smd");
      TSShapeLoader::addFormat("Valve VTA", "vta");
      TSShapeLoader::addFormat("Starcraft II M3", "m3");
      TSShapeLoader::addFormat("Unreal", "3d");
      TSShapeLoader::addFormat("BlitzBasic 3D", "b3d" );
      TSShapeLoader::addFormat("Quick3D Q3D", "q3d");
      TSShapeLoader::addFormat("Quick3D Q3S", "q3s");
      TSShapeLoader::addFormat("Neutral File Format", "nff");
      TSShapeLoader::addFormat("Object File Format", "off");
      TSShapeLoader::addFormat("PovRAY Raw", "raw");
      TSShapeLoader::addFormat("Terragen Terrain", "ter");
      TSShapeLoader::addFormat("3D GameStudio (3DGS)", "mdl");
      TSShapeLoader::addFormat("3D GameStudio (3DGS) Terrain", "hmp");
      TSShapeLoader::addFormat("Izware Nendo", "ndo");
   }
MODULE_END;

//-----------------------------------------------------------------------------

AssimpShapeLoader::AssimpShapeLoader() 
{
   mScene = NULL;
}

AssimpShapeLoader::~AssimpShapeLoader()
{

}

void AssimpShapeLoader::releaseImport()
{
   aiReleaseImport(mScene);
}

void AssimpShapeLoader::enumerateScene()
{
   Con::printf("[ASSIMP] Attempting to load file: %s", shapePath.getFullPath().c_str());

   // This is where assimp does all it's work.  
   // The flags correct the data output to work 
   // with torque's method of handling things.
   mScene = aiImportFile(shapePath.getFullPath().c_str(), 
      aiProcessPreset_TargetRealtime_MaxQuality |
      aiProcess_ConvertToLeftHanded |
      aiProcess_JoinIdenticalVertices |
      aiProcess_Triangulate |
      aiProcess_GenUVCoords
      
      /* 
      //aiProcess_MakeLeftHanded | 
      //aiProcess_FlipUVs |
      aiProcess_FlipWindingOrder | 
      aiProcess_CalcTangentSpace | // calculate tangents and bitangents if possible
		    | // join identical vertices/ optimize indexing
		aiProcess_ValidateDataStructure    | // perform a full validation of the loader's output
		aiProcess_ImproveCacheLocality     | // improve the cache locality of the output vertices
		aiProcess_RemoveRedundantMaterials | // remove redundant materials
		aiProcess_FindDegenerates          | // remove degenerated polygons from the import
		aiProcess_FindInvalidData          | // detect invalid model data, such as invalid normal vectors
		aiProcess_GenUVCoords              | // convert spherical, cylindrical, box and planar mapping to proper UVs
		aiProcess_TransformUVCoords        | // preprocess UV transformations (scaling, translation ...)
		aiProcess_FindInstances            | // search for instanced meshes and remove them by references to one master
		aiProcess_LimitBoneWeights         | // limit bone weights to 4 per vertex
      aiProcess_Triangulate			     | // triangulate polygons with more than 3 edges
		aiProcess_ConvertToLeftHanded*/
      );

   if ( mScene )
   {
      Con::printf("[ASSIMP] Mesh Count: %d", mScene->mNumMeshes);
      Con::printf("[ASSIMP] Material Count: %d", mScene->mNumMaterials);

      // Load all the materials.
      for ( U32 i = 0; i < mScene->mNumMaterials; i++ )
         AppMesh::appMaterials.push_back(new AssimpAppMaterial(mScene->mMaterials[i]));

      // Define the root node, and process down the chain.
      AssimpAppNode* node = NULL;
      node = new AssimpAppNode(mScene, mScene->mRootNode, 0);
      
      if (!processNode(node))
         delete node;

      // Check for animations and process those.
      processAnimations();
   } else {
      Con::printf("[ASSIMP] Import Error: %s", aiGetErrorString());
   }
}

void AssimpShapeLoader::processAnimations()
{
   for(U32 n = 0; n < mScene->mNumAnimations; ++n)
   {
      Con::printf("[ASSIMP] Animation Found: %s", mScene->mAnimations[n]->mName.C_Str());
   }
}

void AssimpShapeLoader::updateMaterialsScript(const Torque::Path &path)
{
   Torque::Path scriptPath(path);
   scriptPath.setFileName("materials");
   scriptPath.setExtension("cs");

   // First see what materials we need to update
   PersistenceManager persistMgr;
   for ( U32 iMat = 0; iMat < AppMesh::appMaterials.size(); iMat++ )
   {
      AssimpAppMaterial *mat = dynamic_cast<AssimpAppMaterial*>( AppMesh::appMaterials[iMat] );
      if ( mat )
      {
         Material *mappedMat;
         if ( Sim::findObject( MATMGR->getMapEntry( mat->getName() ), mappedMat ) )
         {
            // Only update existing materials if forced to
            if ( ColladaUtils::getOptions().forceUpdateMaterials )
               persistMgr.setDirty( mappedMat );
         }
         else
         {
            // Create a new material definition
            persistMgr.setDirty( mat->createMaterial( scriptPath ), scriptPath.getFullPath() );
         }
      }
   }

   if ( persistMgr.getDirtyList().empty() )
      return;

   persistMgr.saveDirty();
}

/// Check if an up-to-date cached DTS is available for this DAE file
bool AssimpShapeLoader::canLoadCachedDTS(const Torque::Path& path)
{
   // Generate the cached filename
   Torque::Path cachedPath(path);
   cachedPath.setExtension("cached.dts");

   // Check if a cached DTS newer than this file is available
   FileTime cachedModifyTime;
   if (Platform::getFileTimes(cachedPath.getFullPath(), NULL, &cachedModifyTime))
   {
      bool forceLoadDAE = Con::getBoolVariable("$assimp::forceLoad", false);

      FileTime daeModifyTime;
      if (!Platform::getFileTimes(path.getFullPath(), NULL, &daeModifyTime) ||
         (!forceLoadDAE && (Platform::compareFileTimes(cachedModifyTime, daeModifyTime) >= 0) ))
      {
         // DAE not found, or cached DTS is newer
         return true;
      }
   }

   return false;
}

//-----------------------------------------------------------------------------
/// This function is invoked by the resource manager based on file extension.
TSShape* assimpLoadShape(const Torque::Path &path)
{
   // TODO: add .cached.dts generation.
   // Generate the cached filename
   Torque::Path cachedPath(path);
   cachedPath.setExtension("cached.dts");

   // Check if an up-to-date cached DTS version of this file exists, and
   // if so, use that instead.
   if (AssimpShapeLoader::canLoadCachedDTS(path))
   {
      FileStream cachedStream;
      cachedStream.open(cachedPath.getFullPath(), Torque::FS::File::Read);
      if (cachedStream.getStatus() == Stream::Ok)
      {
         TSShape *shape = new TSShape;
         bool readSuccess = shape->read(&cachedStream);
         cachedStream.close();

         if (readSuccess)
         {
         #ifdef TORQUE_DEBUG
            Con::printf("Loaded cached shape from %s", cachedPath.getFullPath().c_str());
         #endif
            return shape;
         }
         else
            delete shape;
      }

      Con::warnf("Failed to load cached shape from %s", cachedPath.getFullPath().c_str());
   }

   if (!Torque::FS::IsFile(path))
   {
      // File does not exist, bail.
      return NULL;
   }

   AssimpShapeLoader loader;
   TSShape* tss = loader.generateShape(path);
   if (tss)
   {
      Con::printf("[ASSIMP] Shape created successfully.");

      // Cache the model to a DTS file for faster loading next time.
      FileStream dtsStream;
      if (dtsStream.open(cachedPath.getFullPath(), Torque::FS::File::Write))
      {
         Con::printf("Writing cached shape to %s", cachedPath.getFullPath().c_str());
         tss->write(&dtsStream);
      }

      loader.updateMaterialsScript(path);
   }
   loader.releaseImport();
   return tss;
}

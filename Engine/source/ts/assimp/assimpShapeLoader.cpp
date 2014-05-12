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

   // This is where assimp does all it's work. The flipUVs and
   // flipWindingOrder flags correct the data output to work 
   // with torque's method of handling things.
   mScene = aiImportFile(shapePath.getFullPath().c_str(), 
      aiProcessPreset_TargetRealtime_MaxQuality | 
      aiProcess_FlipUVs |
      aiProcess_FlipWindingOrder
      );

   if ( mScene )
   {
      Con::printf("[ASSIMP] Mesh Count: %d", mScene->mNumMeshes);
      Con::printf("[ASSIMP] Material Count: %d", mScene->mNumMaterials);

      // Load all the materials.
      for ( U32 i = 0; i < mScene->mNumMaterials; i++ )
         AppMesh::appMaterials.push_back(new AssimpAppMaterial(mScene->mMaterials[i]));

      // Define the root node, and process down the chain.
      AssimpAppNode* node = new AssimpAppNode(mScene, mScene->mRootNode, 0);
      if (!processNode(node))
         delete node;
   } else {
      Con::printf("[ASSIMP] Failed to load file.");
   }
}

//-----------------------------------------------------------------------------
/// This function is invoked by the resource manager based on file extension.
TSShape* assimpLoadShape(const Torque::Path &path)
{
   // TODO: add .cached.dts generation.

   AssimpShapeLoader loader;
   TSShape* tss = loader.generateShape(path);
   if (tss)
   {
      Con::printf("[ASSIMP] Shape created successfully.");
   }
   loader.releaseImport();
   return tss;
}

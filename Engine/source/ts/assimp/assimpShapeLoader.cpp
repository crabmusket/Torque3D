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

// assimp include files. These three are usually needed.
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

//-----------------------------------------------------------------------------

AssimpShapeLoader::AssimpShapeLoader() 
{

}

AssimpShapeLoader::~AssimpShapeLoader()
{

}

void AssimpShapeLoader::enumerateScene()
{

}

//-----------------------------------------------------------------------------
/// This function is invoked by the resource manager based on file extension.
TSShape* assimpLoadShape(const Torque::Path &path)
{
   Con::printf("[ASSIMP] Attempting to load file: %s", path.getFullPath().c_str());
   const struct aiScene* scene = NULL;
   scene = aiImportFile(path.getFullPath().c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
   if ( scene )
   {
      Con::printf("[ASSIMP] File loaded successfully.");
      Con::printf("[ASSIMP] Mesh Count: %d", scene->mNumMeshes);
      Con::printf("[ASSIMP] Material Count: %d", scene->mNumMaterials);
      Con::printf("[ASSIMP] Texture Count: %d", scene->mNumTextures);
      aiReleaseImport(scene);
   } else {
      Con::printf("[ASSIMP] Failed to load file.");
   }
   return NULL;
}

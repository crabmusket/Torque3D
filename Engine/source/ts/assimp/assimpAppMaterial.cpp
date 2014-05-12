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

#include "platform/platform.h"
#include "ts/loader/appSequence.h"
#include "ts/assimp/assimpAppMaterial.h"
#include "ts/assimp/assimpAppMesh.h"
#include "materials/materialManager.h"
#include "ts/tsMaterialList.h"

// assimp include files. 
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/types.h>

AssimpAppMaterial::AssimpAppMaterial(const char* matName)
{
   name = matName;

   // Set some defaults
   flags |= TSMaterialList::S_Wrap;
   flags |= TSMaterialList::T_Wrap;
}

AssimpAppMaterial::AssimpAppMaterial(const struct aiMaterial* mtl)
{
   aiString matName;
   mtl->Get(AI_MATKEY_NAME, matName);
   Con::printf("[ASSIMP] Loaded Material: %s", matName.C_Str());
   name = matName.C_Str();

   // Set some defaults
   flags |= TSMaterialList::S_Wrap;
   flags |= TSMaterialList::T_Wrap;
}
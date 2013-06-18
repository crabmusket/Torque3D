//
// Copyright (c) 2009-2010 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
// Changes, Additions and Refactoring : Copyright (c) 2010-2011 Lethal Concept, LLC
// Changes, Additions and Refactoring Author: Simon Wittenberg (MD)
// Minor modifications: Daniel Buckmaster, 2011
// The above license is fully inherited.
//

#ifndef _NAV_MESHLOADER_H_
#define _NAV_MESHLOADER_H_

#include "T3D/objectTypes.h"
#include "terrain/terrCell.h"
#include "T3D/groundplane.h"

namespace Nav {

   /***** built against Recast & Detour Version 1.4 ********/

   /** Contains vertices and triangles in recast coords. */
   struct NavModelData
   {
      NavModelData()
      {
         verts = 0;
         vert_ct = 0;
         vert_cap = 0;
         normals = 0;
         tri_ct= 0;
         tri_cap = 0;
         tris = 0;
      }
      void clear(bool del = true)
      {
         vert_ct = 0;
         tri_ct = 0;
         if(verts)
            if(del)
               delete [] verts;
         verts = 0;
         vert_cap = 0;
         vert_ct = 0;
         if(tris)
            if(del)
               delete [] tris;
         tris = 0;
         tri_cap = 0;
         tri_ct = 0;
         if(normals)
            if(del)
               delete [] normals;
         normals = 0;
      }
      F32*  verts;
      U32 vert_cap;
      U32 vert_ct;
      F32*  normals;
      U32 tri_ct;
      U32 tri_cap;
      S32*    tris;
      /** @return The raw vertex array. */
      inline const F32* getVerts() const { return verts; }
      /** @return The raw normal array. */
      inline const F32* getNormals() const { return normals; }
      /** @return The raw face array. */
      inline const S32* getTris() const { return tris; }
      /** @return The number of vertices in this data set. */
      inline U32 getVertCount() const { return vert_ct; }
      /** @return The number of faces in this data set. */
      inline U32 getTriCount() const { return tri_ct; }
   };

   enum MESH_DETAIL_LEVEL{
      DETAIL_ABSOLUTE,
      DETAIL_HIGH,
      DETAIL_MEDIUM,
      DETAIL_LOW,
      DETAIL_BOUNDINGBOX
   };

   class NavMeshLoader
   {
   public:
      NavMeshLoader();
      ~NavMeshLoader();

      static NavModelData loadMeshFile(const char* fileName);
      static NavModelData mergeModels(NavModelData a, NavModelData b, bool delOriginals = false);
      static bool saveModelData(NavModelData data, const char* filename, const char* mission = NULL);
      static NavModelData parseTerrainData(Box3F box, SimSet* set = NULL, S32* count = NULL);
      static NavModelData parseStaticObjects(Box3F box, SimSet* set = NULL, S32* count = NULL);
      static NavModelData parseDynamicObjects(Box3F box, SimSet* set = NULL, S32* count = NULL);
   private:
      static NavModelData parse(Box3F box, U32 mask, MESH_DETAIL_LEVEL level, SimSet* set = NULL, S32* count = NULL);
      static void addVertex(NavModelData* modelData, F32 x, F32 y, F32 z);
      static void addTriangle(NavModelData* modelData, S32 a, S32 b, S32 c);
      static char* parseRow(char* buf, char* bufEnd, char* row, S32 len);
      static S32 parseFace(char* row, S32* data, S32 n, S32 vcnt);
   };
};// end namespace Nav
#endif // _NAV_MESHLOADER_H_

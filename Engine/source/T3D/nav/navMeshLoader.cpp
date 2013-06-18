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

#include "navMeshLoader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <iostream>
#include <fstream>
#include <map>
#include "collision/clippedPolyList.h"
#include "terrain/terrData.h"
#include "core/volume.h"

//#include "T3D\AI\aiLog.h"
namespace Nav {

   NavMeshLoader::NavMeshLoader()
   {
   }

   NavMeshLoader::~NavMeshLoader()
   {
   }

   char* NavMeshLoader::parseRow(char* buf, char* bufEnd, char* row, int len)
   {
      bool cont = false;
      bool start = true;
      bool done = false;
      int n = 0;
      while (!done && buf < bufEnd)
      {
         char c = *buf;
         buf++;
         // multirow
         switch (c)
         {
         case '\\':
            cont = true; // multirow
            break;
         case '\n':
            if (start) break;
            done = true;
            break;
         case '\r':
            break;
         case '\t':
         case ' ':
            if (start) break;
         default:
            start = false;
            cont = false;
            row[n++] = c;
            if (n >= len-1)
               done = true;
            break;
         }
      }
      row[n] = '\0';
      return buf;
   }

   int NavMeshLoader::parseFace(char* row, int* data, int n, int vcnt)
   {
      int j = 0;
      while (*row != '\0')
      {
         // Skip initial white space
         while (*row != '\0' && (*row == ' ' || *row == '\t'))
            row++;
         char* s = row;
         // Find vertex delimiter and terminated the string there for conversion.
         while (*row != '\0' && *row != ' ' && *row != '\t')
         {
            if (*row == '/') *row = '\0';
            row++;
         }
         if (*s == '\0')
            continue;
         int vi = atoi(s);
         data[j++] = vi < 0 ? vi+vcnt : vi-1;
         if (j >= n) return j;
      }
      return j;
   }

   NavModelData NavMeshLoader::loadMeshFile(const char* filename)
   {
      NavModelData modelData;

      char* buf = 0;
      FILE* fp = fopen(filename, "rb");
      if (!fp)
         return modelData;
      fseek(fp, 0, SEEK_END);
      int bufSize = ftell(fp);
      fseek(fp, 0, SEEK_SET);
      buf = new char[bufSize];
      if (!buf)
      {
         fclose(fp);
         return modelData;
      }
      fread(buf, bufSize, 1, fp);
      fclose(fp);

      char* src = buf;
      char* srcEnd = buf + bufSize;
      char row[512];
      int face[32];
      F32 x,y,z;
      int nv;
      //int vcap = 0;
      //int tcap = 0;

      while (src < srcEnd)
      {
         // Parse one row
         row[0] = '\0';
         src = parseRow(src, srcEnd, row, sizeof(row)/sizeof(char));
         // Skip comments
         if (row[0] == '#') continue;
         if (row[0] == 'v' && row[1] != 'n' && row[1] != 't')
         {
            // Vertex pos
            sscanf(row+1, "%f %f %f", &x, &y, &z);
            addVertex(&modelData, x, y, z);
         }
         if (row[0] == 'f')
         {
            // Faces
            nv = parseFace(row+1, face, 32, modelData.vert_ct);
            for (int i = 2; i < nv; ++i)
            {
               const int a = face[0];
               const int b = face[i-1];
               const int c = face[i];
               if (a < 0 || a >= modelData.vert_ct || b < 0 || b >= modelData.vert_ct || c < 0 || c >= modelData.vert_ct)
                  continue;
               addTriangle(&modelData, a, b, c);
            }
         }
      }

      delete [] buf;

      // Calculate normals.
      modelData.normals = new F32[modelData.tri_ct*3];
      for (int i = 0; i < modelData.tri_ct*3; i += 3)
      {
         const F32* v0 = &modelData.verts[modelData.tris[i]*3];
         const F32* v1 = &modelData.verts[modelData.tris[i+1]*3];
         const F32* v2 = &modelData.verts[modelData.tris[i+2]*3];
         F32 e0[3], e1[3];
         for (int j = 0; j < 3; ++j)
         {
            e0[j] = v1[j] - v0[j];
            e1[j] = v2[j] - v0[j];
         }
         F32* n = &modelData.normals[i];
         n[0] = e0[1]*e1[2] - e0[2]*e1[1];
         n[1] = e0[2]*e1[0] - e0[0]*e1[2];
         n[2] = e0[0]*e1[1] - e0[1]*e1[0];
         F32 d = sqrtf(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
         if (d > 0)
         {
            d = 1.0f/d;
            n[0] *= d;
            n[1] *= d;
            n[2] *= d;
         }
      }

      //strncpy(m_filename, filename, sizeof(m_filename));
      //m_filename[sizeof(m_filename)-1] = '\0';

      return modelData;
   }

   NavModelData NavMeshLoader::mergeModels(NavModelData a, NavModelData b, bool delOriginals/* = false*/)
   {
      NavModelData mergedData;
      if(!a.verts)
         return b;
      else if (!b.verts)
         return a;
      else
      {
         mergedData.clear();

         S32 totalVertCt = (a.vert_ct + b.vert_ct);
         S32 newCap = 8;
         while(newCap < totalVertCt)
            newCap *= 2;
         mergedData.verts = new F32[newCap*3];
         mergedData.vert_cap = newCap;
         mergedData.vert_ct = totalVertCt;
         memcpy(mergedData.verts,               a.verts, a.vert_ct*3*sizeof(F32));
         memcpy(mergedData.verts + a.vert_ct*3, b.verts, b.vert_ct*3*sizeof(F32));

         S32 totalTriCt = (a.tri_ct + b.tri_ct);
         newCap = 8;
         while(newCap < totalTriCt)
            newCap *= 2;
         mergedData.tris = new S32[newCap*3];
         mergedData.tri_cap = newCap;
         mergedData.tri_ct = totalTriCt;
         S32 aFaceSize = a.tri_ct * 3;
         memcpy(mergedData.tris,                a.tris, aFaceSize*sizeof(S32));

         S32 bFaceSize = b.tri_ct * 3;
         S32* bFacePt = mergedData.tris + a.tri_ct * 3;// i like pointing at faces
         memcpy(bFacePt, b.tris, bFaceSize*sizeof(S32));


         for(U32 i = 0; i < bFaceSize;i++)
            *(bFacePt + i) += a.vert_ct;

         if(mergedData.vert_ct > 0)
         {
            if(delOriginals)
            {
               a.clear();
               b.clear();
            }
         }
         else
            mergedData.clear();
      }
      return mergedData;
   }

   bool NavMeshLoader::saveModelData(NavModelData data, const char* filename, const char* mission/* = NULL*/)
   {
      if( ! data.getVertCount() || ! data.getTriCount())
         return false;

      String path("levels/");
      String filenamestring(filename);
      //CreatePath //IsDirectory

      if(mission)
      {
         path += String(mission);
         if(!Torque::FS::IsDirectory(Torque::Path(path)))
            Torque::FS::CreateDirectory(Torque::Path(path));

      }

      path += String("/") + filenamestring + String(".obj");

      std::ofstream myfile;
      myfile.open(path.c_str());
      if(!myfile.is_open())
         return false;

      F32* vstart = data.verts;
      int* tstart = data.tris;
      for(U32 i = 0; i < data.getVertCount(); i++)
      {
         F32* vp = vstart + i*3;
         myfile << "v " << (*(vp)) << " " << *(vp+1) << " " << (*(vp+2)) << "\n";
      }
      for(U32 i = 0; i < data.getTriCount(); i++)
      {
         int* tp = tstart + i*3;
         myfile << "f " << *(tp) << " " << *(tp+1) << " " << *(tp+2) << "\n";
      }
      myfile.close();
      return true;
   }

   NavModelData NavMeshLoader::parseTerrainData(Box3F box, SimSet* set/* = NULL*/, S32* count/* = NULL*/)
   {
      return parse(box, TerrainObjectType, DETAIL_ABSOLUTE, set, count);
   }
   NavModelData NavMeshLoader::parseStaticObjects(Box3F box, SimSet* set/* = NULL*/, S32* count/* = NULL*/)
   {
      return parse(box, (StaticShapeObjectType), DETAIL_ABSOLUTE, set, count);
   }
   NavModelData NavMeshLoader::parseDynamicObjects(Box3F box, SimSet* set/* = NULL*/, S32* count/* = NULL*/)
   {
      return parse(box, (DynamicShapeObjectType), DETAIL_BOUNDINGBOX, set, count);
   }


   static const Point3F cubePoints[8] = 
   {
      Point3F(-1.0f, -1.0f, -1.0f), Point3F(-1.0f, -1.0f,  1.0f), Point3F(-1.0f,  1.0f, -1.0f), Point3F(-1.0f,  1.0f,  1.0f),
      Point3F( 1.0f, -1.0f, -1.0f), Point3F( 1.0f, -1.0f,  1.0f), Point3F( 1.0f,  1.0f, -1.0f), Point3F( 1.0f,  1.0f,  1.0f)
   };
   static const U32 cubeFaces[6][4] = 
   {
      { 0, 4, 6, 2 }, { 0, 2, 3, 1 }, { 0, 1, 5, 4 },
      { 3, 2, 6, 7 }, { 7, 6, 4, 5 }, { 3, 7, 5, 1 }
   };

   NavModelData NavMeshLoader::parse(Box3F box, U32 mask, MESH_DETAIL_LEVEL level, SimSet* set/* = NULL*/, S32* count /*= NULL*/)
   {
      if(count)
         *count = 0;

      NavModelData data;

      if(!set)
      {
         // Don't know if the lookup succeeds if we'd use SimSet here
         SimGroup* serverGroup = NULL;
         Sim::findObject("ServerGroup",serverGroup);
         if(!serverGroup)
            return data;
         // the implicit cast shouldn't harm
         set = serverGroup; 
      }

      data.clear(false);
      U32 numVert = 1;

      //Polys of this object
      std::map<U32, Point3F> globalPointIdxMap;
      std::map<U32, U32> translationMap;
      /*SimTime startTime = Sim::getCurrentTime();*/
      Vector<Point3F> vertexVector;
      Vector<Point3I> faceVector;
      for(SimSetIterator obj(set); *obj; ++obj)
      {
         SceneObject* sceneObj = dynamic_cast<SceneObject*>(*obj);
         TerrainBlock* terrain = dynamic_cast<TerrainBlock*>(*obj);
         GroundPlane* groundplane = dynamic_cast<GroundPlane*>(*obj);

         if(! sceneObj)
            continue;
         Box3F worldBox = sceneObj->getWorldBox();
         if(! box.isOverlapped(worldBox))
            continue;

         if(groundplane)
         {
            if(!(mask & groundplane->getTypeMask()))
               continue;
            F32 z = groundplane->getPosition().z;
            addVertex(&data, box.minExtents.x, z, -box.minExtents.y);
            addVertex(&data, box.maxExtents.x, z, -box.maxExtents.y);
            addVertex(&data, box.minExtents.x, z, -box.maxExtents.y);
            addTriangle(&data, data.vert_ct-2, data.vert_ct-1, data.vert_ct);
            addVertex(&data, box.minExtents.x, z, -box.minExtents.y);
            addVertex(&data, box.maxExtents.x, z, -box.minExtents.y);
            addVertex(&data, box.maxExtents.x, z, -box.maxExtents.y);
            addTriangle(&data, data.vert_ct-2, data.vert_ct-1, data.vert_ct);
            numVert += 6;
            continue;
         }

         U32 curNumVert = numVert;

         if(terrain)
         {
            if(mask & TerrainObjectType)
            {
               /*Log::write(LogEntry(LOG_OUTPUT, 
               String("Parsing TerrainBlock."),
               String("Nav::NavMeshLoader")));*/
            }
            else
            {
               //Log::write(LogEntry(LOG_OUTPUT, 
               //                        String("Skipping TerrainBlock."),
               //                        String("Nav::NavMeshLoader")));
               continue;
            }
         }

         if(sceneObj && (sceneObj->getTypeMask() & mask)) 
         {
            if(level == DETAIL_ABSOLUTE)
            {
               globalPointIdxMap.clear();
               translationMap.clear();
               //ConcretePolyList* list = new ConcretePolyList();
               ClippedPolyList *list = new ClippedPolyList();
               list->mPlaneList.clear();
               list->mNormal.set(0.0f, 0.0f, 0.0f);
               list->mPlaneList.setSize(6);
               list->mPlaneList[0].set(box.minExtents,VectorF(-1.0f, 0.0f, 0.0f));
               list->mPlaneList[1].set(box.maxExtents,VectorF(0.0f, 1.0f, 0.0f));
               list->mPlaneList[2].set(box.minExtents,VectorF(0.0f, -1.0f, 0.0f));
               list->mPlaneList[3].set(box.maxExtents,VectorF(1.0f, 0.0f, 0.0f));
               list->mPlaneList[4].set(box.minExtents,VectorF(0.0f, 0.0f, -1.0f));
               list->mPlaneList[5].set(box.maxExtents,VectorF(0.0f, 0.0f, 1.0f));
               // only use the Terrains worldbox instead of the given box
               // if we exclusively parse terrain
               //if(terrain && mask == TerrainObjectType) 
               //sceneObj->buildPolyList(PLC_Collision, list,terrain->getWorldBox(),SphereF());
               //else
               sceneObj->buildPolyList(PLC_Collision, list,box,SphereF());

               //For all the polygons in this mesh
               Vector<ClippedPolyList::Poly>::iterator itP;
               for(itP = list->mPolyList.begin(); itP != list->mPolyList.end(); ++itP)
               {
                  ClippedPolyList::Poly p = *itP;
                  U32 vertexStart = p.vertexStart;
                  U32 vertexCount = p.vertexCount;

                  //Get the vertices in a map (I need that there are no vertices repeated!)
                  for ( U32 i = 0; i < vertexCount; ++i )
                  {
                     Point3F pnt = list->mVertexList[list->mIndexList[vertexStart + i]].point;
                     U32 localIdx = list->mIndexList[vertexStart + i];

                     // the following block ensures that we dont add the same point only once
                     if(globalPointIdxMap.find(localIdx) == globalPointIdxMap.end())
                     {
                        globalPointIdxMap[localIdx] = pnt; 
                        translationMap[localIdx] = numVert;
                        numVert++;
                        addVertex(&data,pnt.x, pnt.z, -pnt.y);
                        // Note: we convert from torque to recast coordinates here
                        vertexVector.push_back(Point3F(pnt.x, pnt.z, -pnt.y));
                     }
                  }


                  U32 *verts = new U32[vertexCount];
                  for ( U32 i = 0; i < vertexCount; ++i )
                  {
                     U32 localIdx = list->mIndexList[vertexStart + i];
                     U32 globalIdx = translationMap[localIdx];
                     verts[i] = globalIdx;
                  }

                  if(vertexCount == 4)
                  {
                     for (int  i = 2; i < 4; ++i)
                     {
                        // Note: We reverse normal on the polygons to prevent things from going inside out
                        addTriangle(&data, verts[0],  verts[i] ,verts[i-1]);
                     }
                  }else if(vertexCount == 3)
                  {
                     // Note: We reverse normal on the polygons to prevent things from going inside out
                     addTriangle(&data, verts[0], verts[2], verts[1]);
                  }
                  else
                  {
                     //AssertFatal(false, "Polygons with more than 4 or less than 3 vertices aren't supported.");
                     Con::warnf("Polygons with more than 4 or less than 3 vertices aren't supported.");
                  }
                  delete[] verts;
               }
               delete list;
               if(count)
                  (*count)++;
            }
            else if( level == DETAIL_MEDIUM || level == DETAIL_HIGH)
            {
               /*Log::write(LogEntry(LOG_OUTPUT, 
               String("MEDIUM and HIGH Detail Levels arent supported. Skipping Object."),
               String("Nav::NavMeshLoader")));*/
               Con::errorf("Nav::NavMeshLoader MEDIUM and HIGH Detail Levels arent supported. Skipping Object.");
            }else if( level == DETAIL_LOW || level == DETAIL_BOUNDINGBOX )
            {
               Box3F bBox = sceneObj->getWorldBox();
               if(bBox.len_x() < 0.05f || bBox.len_y() < 0.05f || bBox.len_z() < 0.05f)
                  continue;

               Vector<Point3F> verts;
               Point3F boxCenter = bBox.getCenter();
               Point3F halfSize = bBox.getExtents() / 2.0f;

               for(U32 i = 0; i < 8; i++)
               {
                  verts.push_back(cubePoints[i]);
               }

               MatrixF transform = sceneObj->getTransform();

               for(U32 i = 0; i < 8; i++)
               {
                  Point3F tmp = verts[i] * halfSize;
                  transform.mulV(tmp);
                  tmp += boxCenter; 
                  numVert++;
                  addVertex(&data, tmp.x, tmp.z, -tmp.y);
               }

               for(U32 f = 0; f < 6; f++)
                  for(U32 v = 2; v < 4; v++)
                  {
                     // Note: We reverse normal on the polygons to prevent things from going inside out
                     addTriangle(&data, curNumVert+cubeFaces[f][0]+1, curNumVert+cubeFaces[f][v]+1, curNumVert+cubeFaces[f][v-1]+1);
                  }

                  if(count)
                     (*count)++;
            }
            else
            {
               /*Log::write(LogEntry(LOG_OUTPUT, 
               String("UNKOWN Detail level not supported. Skipping Object."),
               String("Nav::NavMeshLoader")));*/
            }
         }
         if(terrain)
         {
            /*Log::write(LogEntry(LOG_OUTPUT, 
            String::ToString("Finished Parsing of TerrainBlock with %d vertices.",(numVert - curNumVert)),
            String("Nav::NavMeshLoader")));*/

         }
         else
         {
            //if(numVert > 1)
            //    Log::write(LogEntry(LOG_OUTPUT, 
            //        String::ToString("Parsed Object with %d vertices.",(numVert -  ( curNumVert + 1 ))),
            //        String("Nav::NavMeshLoader")));
         }
      }
      //Log::write(LogEntry(LOG_OUTPUT, 
      //    String::ToString("Object parsing done for %d vertices of %s objects total.",  numVert, 
      //    count ? String::ToString(*count).c_str() : "?"),
      //    String("Nav::NavMeshLoader")));

      globalPointIdxMap.clear();
      translationMap.clear();

      return data;
   }

   void NavMeshLoader::addVertex(NavModelData* modelData, F32 x, F32 y, F32 z)
   {
      if (modelData->vert_ct+1 > modelData->vert_cap)
      {
         modelData->vert_cap = ! modelData->vert_cap ? 8 : modelData->vert_cap*2;
         F32* nv = new F32[modelData->vert_cap*3];
         if (modelData->vert_ct)
            memcpy(nv, modelData->verts, modelData->vert_ct*3*sizeof(F32));
         if( modelData->verts )
            delete [] modelData->verts;
         modelData->verts = nv;
      }
      F32* dst = &modelData->verts[modelData->vert_ct*3];
      *dst++ = x;
      *dst++ = y;
      *dst++ = z;
      modelData->vert_ct++;
   }

   void NavMeshLoader::addTriangle(NavModelData* modelData,int a, int b, int c)
   {
      if (modelData->tri_ct+1 > modelData->tri_cap)
      {
         modelData->tri_cap = !modelData->tri_cap ? 8 : modelData->tri_cap*2;
         int* nv = new int[modelData->tri_cap*3];
         if (modelData->tri_ct)
            memcpy(nv, modelData->tris, modelData->tri_ct*3*sizeof(int));
         if( modelData->tris )
            delete [] modelData->tris;
         modelData->tris = nv;
      }
      int* dst = &modelData->tris[modelData->tri_ct*3];
      *dst++ = a;
      *dst++ = b;
      *dst++ = c;
      modelData->tri_ct++;
   }

}; // end of namespace Nav

// returns number of parsed objects and negates that number if the saving failed
// in case of 0, theres something wrong anyway
ConsoleFunction(dumpTerrainToFile, S32, 4, 6, ", (minx miny minz), (maxx, maxy, maxz), ( filename ),[optional](simset), [optional]( missionname )")
{
   S32 count = 0;
   String filename(argv[3]);
   String missionname;
   SimSet* set = NULL;
   if(argc > 4)
      Sim::findObject(argv[4], set);
   if(argc > 5)
      missionname = String(argv[5]);
   Point3F max,min;
   dSscanf(argv[1], "%g %g %g", &min.x, &min.y, &min.z); 
   dSscanf(argv[2], "%g %g %g", &max.x, &max.y, &max.z); 
   Box3F box(min, max);
   Nav::NavModelData data = Nav::NavMeshLoader::parseTerrainData(box, set, &count);
   bool result = Nav::NavMeshLoader::saveModelData(data, filename.c_str(), (missionname.length() > 0) ? missionname.c_str() : 0);
   data.clear(true);
   if(!result)
      count = -count;

   return count;
};

ConsoleFunction(dumpStaticsToFile, S32, 4, 6, ", (minx miny minz), (maxx, maxy, maxz), ( filename ),[optional](simset), [optional]( missionname )")
{
   S32 count = 0;
   String filename(argv[3]);
   String missionname;
   SimSet* set = NULL;
   if(argc > 4)
      Sim::findObject(argv[4], set);
   if(argc > 5)
      missionname = String(argv[5]);
   Point3F max,min;
   dSscanf(argv[1], "%g %g %g", &min.x, &min.y, &min.z); 
   dSscanf(argv[2], "%g %g %g", &max.x, &max.y, &max.z); 
   Box3F box(min, max);
   Nav::NavModelData data = Nav::NavMeshLoader::parseStaticObjects(box, set, &count);
   bool result = Nav::NavMeshLoader::saveModelData(data, filename.c_str(), (missionname.length() > 0) ? missionname.c_str() : 0);
   data.clear(true);
   if(!result)
      count = -count;

   return count;
};

ConsoleFunction(dumpStaticsAndTerrainToFile, S32, 4, 6, ", (minx miny minz), (maxx, maxy, maxz), ( filename ),[optional](simset), [optional]( missionname )")
{
   S32 count = 0;
   String filename(argv[3]);
   String missionname;
   SimSet* set = NULL;
   if(argc > 4)
      Sim::findObject(argv[4], set);
   if(argc > 5)
      missionname = String(argv[5]);
   Point3F max,min;
   dSscanf(argv[1], "%g %g %g", &min.x, &min.y, &min.z); 
   dSscanf(argv[2], "%g %g %g", &max.x, &max.y, &max.z); 
   Box3F box(min, max);
   Nav::NavModelData data = Nav::NavMeshLoader::mergeModels(Nav::NavMeshLoader::parseTerrainData(box, set), Nav::NavMeshLoader::parseStaticObjects(box, set, &count),true);
   bool result = Nav::NavMeshLoader::saveModelData(data, filename.c_str(), (missionname.length() > 0) ? missionname.c_str() : 0);
   data.clear(true);
   if(!result)
      count = -count;

   return count;
};

ConsoleFunction(dumpDynamicsToFile, S32, 4, 6, ", (minx miny minz), (maxx, maxy, maxz), ( filename ),[optional](simset), [optional]( missionname )")
{
   S32 count = 0;
   String filename(argv[3]);
   String missionname;
   SimSet* set = NULL;
   if(argc > 4)
      Sim::findObject(argv[4], set);
   if(argc > 5)
      missionname = String(argv[5]);
   Point3F max,min;
   dSscanf(argv[1], "%g %g %g", &min.x, &min.y, &min.z); 
   dSscanf(argv[2], "%g %g %g", &max.x, &max.y, &max.z); 
   Box3F box(min, max);
   Nav::NavModelData data = Nav::NavMeshLoader::parseDynamicObjects(box, set, &count);
   bool result = Nav::NavMeshLoader::saveModelData(data, filename.c_str(), (missionname.length() > 0) ? missionname.c_str() : 0);
   data.clear(true);
   if(!result)
      count = -count;

   return count;
};

ConsoleFunction(dumpALLToFile, S32, 4, 6, ", (minx miny minz), (maxx, maxy, maxz), ( filename ),[optional](simset), [optional]( missionname )")
{
   S32 count = 0;
   S32 terrCount = 0, statCount = 0, dynCount = 0;
   String filename(argv[3]);
   String missionname;
   SimSet* set = NULL;
   if(argc > 4)
      Sim::findObject(argv[4], set);
   if(argc > 5)
      missionname = String(argv[5]);
   Point3F max,min;
   dSscanf(argv[1], "%g %g %g", &min.x, &min.y, &min.z); 
   dSscanf(argv[2], "%g %g %g", &max.x, &max.y, &max.z); 
   Box3F box(min, max);
   Nav::NavModelData data = Nav::NavMeshLoader::mergeModels
      ( 
      Nav::NavMeshLoader::mergeModels(
      Nav::NavMeshLoader::parseTerrainData(box, set, &terrCount), 
      Nav::NavMeshLoader::parseStaticObjects(box, set,&statCount),
      true),
      Nav::NavMeshLoader::parseDynamicObjects(box, set, &dynCount), 
      true);
   bool result = Nav::NavMeshLoader::saveModelData(data, filename.c_str(), (missionname.length() > 0) ? missionname.c_str() : 0);
   data.clear(true);
   count = statCount + dynCount + terrCount;
   if(!result)
      count = -count;

   return count;
};

ConsoleFunction(navMeshDump, S32, 3, 5, ", (bounds object), ( filename ),[optional](simset), [optional]( missionname )")
{
   S32 count = 0;
   String filename(argv[2]);
   String missionname;
   SimSet* set = NULL;
   if(argc > 3)
      Sim::findObject(argv[3], set);
   if(argc > 4)
      missionname = String(argv[4]);
   SceneObject* obj = dynamic_cast<SceneObject*>(Sim::findObject(argv[1]));
   if(obj)
   {
      Box3F box(obj->getWorldBox());
      Nav::NavModelData data = Nav::NavMeshLoader::mergeModels(Nav::NavMeshLoader::parseTerrainData(box, set), Nav::NavMeshLoader::parseStaticObjects(box, set, &count),true);
      bool result = Nav::NavMeshLoader::saveModelData(data, filename.c_str(), (missionname.length() > 0) ? missionname.c_str() : 0);
      data.clear(true);
      if(!result)
         count = -count;
   }

   return count;
};
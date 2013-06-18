//
// T3D 1.1 implementation of Recast's DebugUtils.
// Daniel Buckmaster, 2011
// With huge thanks to:
//    Mikko Mononen http://groups.google.com/group/recastnavigation
//

#include "navMeshDraw.h"

#include "gfx/gfxDevice.h"
#include "gfx/primBuilder.h"
#include "gfx/gfxStateBlock.h"

namespace Nav {

   duDebugDrawTorque::duDebugDrawTorque()
   {
      mOverrideColour = 0;
      mOverride = false;
   }

   duDebugDrawTorque::~duDebugDrawTorque()
   {
   }

   void duDebugDrawTorque::depthMask(bool state)
   {
      mDesc.setZReadWrite(true, state);
   }

   void duDebugDrawTorque::texture(bool state)
   {
   }

   /// Begin drawing primitives.
   /// @param prim [in] primitive type to draw, one of rcDebugDrawPrimitives.
   /// @param size [in] size of a primitive, applies to point size and line width only.
   void duDebugDrawTorque::begin(duDebugDrawPrimitives prim, float size)
   {
      mQuadsMode = false;
      mVertCount = 0;
      mPrimType = 0;
      switch(prim)
      {
      case DU_DRAW_POINTS: mPrimType = GFXPointList; break;
      case DU_DRAW_LINES:  mPrimType = GFXLineList; break;
      case DU_DRAW_TRIS:   mPrimType = GFXTriangleList; break;
      case DU_DRAW_QUADS:  mPrimType = GFXTriangleList; mQuadsMode = true; break;
      }
      mDesc.setCullMode(GFXCullNone);
      mDesc.setBlend(true);
      GFXStateBlockRef sb = GFX->createStateBlock(mDesc);
      GFX->setStateBlock(sb);
      PrimBuild::begin((GFXPrimitiveType)mPrimType, 128);
   }

   /// Submit a vertex
   /// @param pos [in] position of the verts.
   /// @param color [in] color of the verts.
   void duDebugDrawTorque::vertex(const float* pos, unsigned int color)
   {
      vertex(pos[0], pos[1], pos[2], color);
   }

   /// Submit a vertex
   /// @param x,y,z [in] position of the verts.
   /// @param color [in] color of the verts.
   void duDebugDrawTorque::vertex(const float x, const float y, const float z, unsigned int color)
   {
      if(mOverride)
         color = mOverrideColour;
      U8 r, g, b, a;
      rcCol(color, r, g, b, a);
      PrimBuild::color4i(r, g, b, a);
      if(mQuadsMode)
      {
         mVertCount++;
         // Handle quads
         switch(mVertCount)
         {
         case 1:
            mStore[0][0] = x;
            mStore[0][1] = -z;
            mStore[0][2] = y;
            break;
         case 3:
            mStore[1][0] = x;
            mStore[1][1] = -z;
            mStore[1][2] = y;
            break;
         case 4:
            PrimBuild::vertex3f(x, -z, y);
            PrimBuild::vertex3f(mStore[0][0], mStore[0][1], mStore[0][2]);
            PrimBuild::vertex3f(mStore[1][0], mStore[1][1], mStore[1][2]);
            mVertCount = 0;
            break;
         }
      }
      PrimBuild::vertex3f(x, -z, y);
   }

   /// Submit a vertex
   /// @param pos [in] position of the verts.
   /// @param color [in] color of the verts.
   void duDebugDrawTorque::vertex(const float* pos, unsigned int color, const float* uv)
   {
      vertex(pos[0], pos[1], pos[2], color);
   }

   /// Submit a vertex
   /// @param x,y,z [in] position of the verts.
   /// @param color [in] color of the verts.
   void duDebugDrawTorque::vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v)
   {
      vertex(x, y, z, color);
   }

   /// End drawing primitives.
   void duDebugDrawTorque::end()
   {
      PrimBuild::end();
   }

   void duDebugDrawTorque::overrideColour(unsigned int col)
   {
      mOverride = true;
      mOverrideColour = col;
   }

   void duDebugDrawTorque::cancelOverride()
   {
      mOverride = false;
   }
};

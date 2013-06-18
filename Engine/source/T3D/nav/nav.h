//
// Header for Recast integration with Torque 3D 1.1 Final in Nav namespace.
// Daniel Buckmaster, 2011
// With huge thanks to:
//    Lethal Concept http://www.lethalconcept.com/
//    Mikko Mononen http://code.google.com/p/recastnavigation/
//

#ifndef _NAV_H_
#define _NAV_H_

#include "console/simSet.h"
#include "math/mPoint3.h"

/// @namespace Nav
/// Groups common functions and classes relating to AI navigation. Specifically,
/// the integration of Recast/Detour with Torque.
namespace Nav {
   inline Point3F DTStoRC(F32 x, F32 y, F32 z) {return Point3F(x, z, -y);};
   inline Point3F DTStoRC(Point3F point)       {return Point3F(point.x, point.z, -point.y);};
   inline Point3F RCtoDTS(const F32* xyz)      {return Point3F(xyz[0], -xyz[2], xyz[1]);};
   inline Point3F RCtoDTS(F32 x, F32 y, F32 z) {return Point3F(x, -z, y);};
   inline Point3F RCtoDTS(Point3F point)       {return Point3F(point.x, -point.z, point.y);};

   /// Convert a Rcast colour integer to RGBA components.
   inline void rcCol(unsigned int col, U8 &r, U8 &g, U8 &b, U8 &a)
   {
      r = col % 256; col /= 256;
      g = col % 256; col /= 256;
      b = col % 256; col /= 256;
      a = col % 256;
   }
};

#endif

//-----------------------------------------------------------------------------
// Copyright (c) 2012 Daniel Buckmaster
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

#include "gui/core/guiControl.h"
#include "gui/3d/guiTSControl.h"
#include "console/consoleTypes.h"
#include "scene/sceneManager.h"
#include "T3D/gameBase/gameConnection.h"
#include "T3D/shapeBase.h"
#include "gfx/gfxDrawUtil.h"
#include "console/engineAPI.h"

//----------------------------------------------------------------------------
/// Displays name & damage above selectable objects.
///
/// This control displays the name and damage value of all named
/// ShapeBase objects on the client.  The name and damage of objects
/// within the control's display area are overlayed above the object.
///
/// This GUI control must be a child of a TSControl, and a server connection
/// and control object must be present.
///
/// This is a stand-alone control and relies only on the standard base GuiControl.
class GuiSelectHud : public GuiControl {
   typedef GuiControl Parent;

   // field data
   ColorF mTextColor;
   F32    mVerticalOffset;
   F32    mCutoff;

   S32 mCurrentObject;

protected:
   void drawName( Point2I offset, const char *buf, F32 opacity);

public:
   GuiSelectHud();

   // GuiControl
   virtual void onRender(Point2I offset, const RectI &updateRect);

   static void initPersistFields();
   DECLARE_CONOBJECT(GuiSelectHud);
   DECLARE_CATEGORY("Gui Game");
   DECLARE_DESCRIPTION("Basic client-side object selection HUD.");
};

//-----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(GuiSelectHud);

ConsoleDocClass(GuiSelectHud,
   "@brief Displays name and damage of ShapeBase objects in its bounds. Must be a child of a GuiTSCtrl and a server connection must be present.\n\n"
   "This control displays the name and damage value of all named ShapeBase objects on the client. "
   "The name and damage of objects within the control's display area are overlayed above the object.\n\n"
   "This GUI control must be a child of a TSControl, and a server connection and control object must be present. "
   "This is a stand-alone control and relies only on the standard base GuiControl.\n\n"
   
   "@tsexample\n"
		"\n new GuiSelectHud()"
		"{\n"
		"	textColor = \"1.0 1.0 1.0 1.0\"; // Solid white text Color\n"
		"	verticalOffset = \"0.15\";\n"
      "  cutoff = \"0.3\";\n"
		"};\n"
   "@endtsexample\n\n"
   
   "@ingroup GuiGame\n"
);

/// Default distance for object's information to be displayed.
static const F32 SelectDistance = 3.0f;

/// Object types we are allowed to select.
static const U32 SelectMask = PlayerObjectType | VehicleObjectType | ItemObjectType;

GuiSelectHud::GuiSelectHud()
{
   mTextColor.set( 0, 1, 0, 1 );
   mVerticalOffset = 0.0f;
   mCutoff = 0.85f;
   mCurrentObject = -1;
}

void GuiSelectHud::initPersistFields()
{
   addGroup("Colors");
   addField( "textColor",  TypeColorF, Offset( mTextColor, GuiSelectHud ), "Color for the text on this control." );
   endGroup("Colors");

   addGroup("Misc");
   addField( "verticalOffset", TypeF32, Offset( mVerticalOffset, GuiSelectHud ), "Amount to vertically offset the control in relation to the ShapeBase object in focus." );
   addField( "cutoff",         TypeF32, Offset( mCutoff,         GuiSelectHud ), "Cosine of angle above which objects are not selected." );
   addField( "currentObject",  TypeS32, Offset( mCurrentObject,  GuiSelectHud ), "ID of the currently selected object." );
   endGroup("Misc");
   Parent::initPersistFields();
}

//----------------------------------------------------------------------------
/// Core rendering method for this control.
///
/// @param   updateRect   Extents of control.
void GuiSelectHud::onRender( Point2I, const RectI &updateRect)
{
   // Must be in a TS Control
   GuiTSCtrl *parent = dynamic_cast<GuiTSCtrl*>(getParent());
   if (!parent) return;

   // Must have a connection and control object
   GameConnection* conn = GameConnection::getConnectionToServer();
   if (!conn) return;
   ShapeBase * control = dynamic_cast<ShapeBase*>(conn->getControlObject());
   if (!control) return;

   // Get control camera info
   MatrixF cam;
   Point3F camPos;
   VectorF camDir;
   control->getRenderEyeTransform(&cam);
   cam.getColumn(3, &camPos);
   cam.getColumn(1, &camDir);

   F32 camFov;
   conn->getControlCameraFov(&camFov);
   camFov = mDegToRad(camFov) / 2;

   // Collision info. We're going to be running LOS tests and we
   // don't want to collide with the control object.
   static U32 losMask = TerrainObjectType | InteriorObjectType | ShapeBaseObjectType;
   control->disableCollision();

   // Perform a container radius search for objects to display.
   ShapeBase *best = NULL;
   F32 bestDot = mCutoff;
   gClientContainer.initRadiusSearch(camPos, SelectDistance, SelectMask);
   while(SceneObject *sc = gClientContainer.containerSearchNextObject()) {
      ShapeBase* shape = dynamic_cast<ShapeBase*>(sc);
      if ( shape ) {
         if (shape != control) 
         {
            // Target pos to test, if it's a player run the LOS to his eye
            // point, otherwise we'll grab the generic box center.
            Point3F shapePos;
            shape->getRenderTransform().getColumn(3, &shapePos);
            VectorF shapeDir = shapePos - camPos;

            // Test to see if it's within our viewcone, this test doesn't
            // actually match the viewport very well, should consider
            // projection and box test.
            shapeDir.normalize();
            F32 dot = mDot(shapeDir, camDir);
            if (dot < camFov)
               continue;

            // Test to see if it's behind something, and we want to
            // ignore anything it's mounted on when we run the LOS.
            RayInfo info;
            shape->disableCollision();
            SceneObject *mount = shape->getObjectMount();
            if (mount)
               mount->disableCollision();
            bool los = !gClientContainer.castRay(camPos, shapePos,losMask, &info);
            shape->enableCollision();
            if (mount)
               mount->enableCollision();
            if (!los)
               continue;

            // Is it the closest object we've seen so far?
            if(dot > bestDot)
            {
               best = shape;
               bestDot = dot;
            }
         }
      }
   }

   if(best)
   {
      // Project the shape pos into screen space and calculate
      // the distance opacity used to fade the labels into the
      // distance.
      Point3F shapePos = best->getPosition();
      Point3F projPnt;
      shapePos.z += mVerticalOffset;
      if (parent->project(shapePos, &projPnt))
      {
         F32 opacity = 1.0;
         // Render the shape's name
         drawName(Point2I((S32)projPnt.x, (S32)projPnt.y), "Interact", opacity);
      }
      // Update current object and handle callbacks.
      S32 id = best->getId();
      if(mCurrentObject != -1)
      {
         if(mCurrentObject != id)
         {
            mCurrentObject = id;
            //callback? object deselected, object selected
         }
      }
      else
      {
         mCurrentObject = id;
         //callback? object selected
      }
   }
   else
   {
      if(mCurrentObject != -1)
      {
         mCurrentObject = -1;
         //callback?
      }
   }

   // Restore control object collision
   control->enableCollision();
}

//----------------------------------------------------------------------------
/// Render object names.
///
/// Helper function for GuiSelectHud::onRender
///
/// @param   offset  Screen coordinates to render name label. (Text is centered
///                  horizontally about this location, with bottom of text at
///                  specified y position.)
/// @param   name    String name to display.
/// @param   opacity Opacity of name (a fraction).
void GuiSelectHud::drawName(Point2I offset, const char *name, F32 opacity)
{
   // Center the name
   offset.x -= mProfile->mFont->getStrWidth((const UTF8 *)name) / 2;
   offset.y -= mProfile->mFont->getHeight();

   // Deal with opacity and draw.
   mTextColor.alpha = opacity;
   GFX->getDrawUtil()->setBitmapModulation(mTextColor);
   GFX->getDrawUtil()->drawText(mProfile->mFont, offset, name);
   GFX->getDrawUtil()->clearBitmapModulation();
}

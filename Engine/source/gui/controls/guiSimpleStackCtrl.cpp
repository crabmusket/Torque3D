#include "gui/controls/guiSimpleStackCtrl.h"

IMPLEMENT_CONOBJECT(GuiSimpleStackCtrl);

GuiSimpleStackCtrl::GuiSimpleStackCtrl()
{
	mExtendParent = true;
	mPadding = 5;
   mBestExtent = -1;
}

void GuiSimpleStackCtrl::initPersistFields()
{
	Parent::initPersistFields();

	addField("extendParent", TypeBool, Offset(mExtendParent, GuiSimpleStackCtrl), "Controls if this control force the parent control to expand to match");
	addField("padding", TypeS32, Offset(mPadding, GuiSimpleStackCtrl), "Controls how much padding space is there between child elements");
}

void GuiSimpleStackCtrl::addObject(SimObject *obj)
{
	Parent::addObject(obj);
   
	setExtent(updateStack());
}

void GuiSimpleStackCtrl::removeObject(SimObject *obj)
{
	Parent::removeObject(obj);
   
	setExtent(updateStack());
}

Point2I GuiSimpleStackCtrl::updateStack()
{
   if(mExtendParent && !getParent())
      return mMinExtent;

   S32 ypos = 0;
   S32 extent = 0;

   //resize the control and position it in the stack
   U32 children = size();
   for(U32 i=0; i < children; i++)
   {
      GuiControl *ctrl = dynamic_cast<GuiControl*>(at(i));
      
      if(ctrl->getParent()->getId() == getId())
      {      
         Point2I ext = ctrl->getExtent();
         
         //we want to keep it aligned to the side
         //so keep x to 0
         Point2I pos = Point2I(0,0);
         
         if(i == 0)
            pos.y = 0;
         else
            pos.y = ypos + mPadding;  

         ctrl->setPosition(pos);
         
         ypos = pos.y + ctrl->getExtent().y;
                  
         extent = ypos;
      }
   }

   if(mBestExtent != extent)
   {
      //setExtent(Point2I(getParent()->getExtent().x, extent));
      mBestExtent = extent;
   }

   return Point2I(getParent()->getExtent().x, mBestExtent);

   /*if(extent != 0)
   {
      Point2I curExt = getExtent();
      if(curExt.y != extent)
         curExt.y = extent;

      setExtent(curExt);

      if(mExtendParent)
      {
         Point2I parentExt = getParent()->getExtent();
         if(parentExt.y < curExt.y)
         {
            S32 extentDif = curExt.y - parentExt.y;

            parentExt.y += extentDif;

            getParent()->setExtent(parentExt);
         }
      }
   }*/
}

bool GuiSimpleStackCtrl::resize(const Point2I &newPosition, const Point2I &newExtent)
{
   Point2I stackSize = updateStack();

   Point2I modExtent = Point2I(newExtent.x, stackSize.y);

   //if(stackSize.y != newExtent.y)
   //   modExtent.y = stackSize.y;

   return Parent::resize(newPosition, modExtent);
}

ConsoleMethod(GuiSimpleStackCtrl, updateStack, void, 2, 2, "() - Get the template name of this behavior\n"
																	 "@return (string name) The name of the template this behaivor was created from")
{
   Point2I stackSize = object->updateStack();
   object->setExtent(stackSize);
}
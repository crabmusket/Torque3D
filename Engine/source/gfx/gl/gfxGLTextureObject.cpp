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

#include "console/console.h"
#include "gfx/gl/tGL/tGL.h"
#include "math/mRect.h"
#include "gfx/gl/gfxGLTextureObject.h"
#include "gfx/gfxDevice.h"
#include "gfx/gl/gfxGLEnumTranslate.h"
#include "gfx/gl/gfxGLTextureManager.h"
#include "gfx/gl/gfxGLUtils.h"
#include "gfx/gfxCardProfile.h"


GFXGLTextureObject::GFXGLTextureObject(GFXDevice * aDevice, GFXTextureProfile *profile) :
   GFXTextureObject(aDevice, profile),
   mBinding(GL_TEXTURE_2D),
   mBytesPerTexel(4),
   mLockedRectRect(0, 0, 0, 0),
   mGLDevice(static_cast<GFXGLDevice*>(mDevice)),
   mZombieCache(NULL),
   mNeedInitSamplerState(true)
{
   AssertFatal(dynamic_cast<GFXGLDevice*>(mDevice), "GFXGLTextureObject::GFXGLTextureObject - Invalid device type, expected GFXGLDevice!");
   glGenTextures(1, &mHandle);
   glGenBuffers(1, &mBuffer);
}

GFXGLTextureObject::~GFXGLTextureObject() 
{ 
   glDeleteTextures(1, &mHandle);
   glDeleteBuffers(1, &mBuffer);
   delete[] mZombieCache;
   kill();
}

GFXLockedRect* GFXGLTextureObject::lock(U32 mipLevel, RectI *inRect)
{
   AssertFatal(mBinding != GL_TEXTURE_3D, "GFXGLTextureObject::lock - We don't support locking 3D textures yet");
   U32 width = mTextureSize.x >> mipLevel;
   U32 height = mTextureSize.y >> mipLevel;

   if(inRect)
   {
      if((inRect->point.x + inRect->extent.x > width) || (inRect->point.y + inRect->extent.y > height))
         AssertFatal(false, "GFXGLTextureObject::lock - Rectangle too big!");

      mLockedRectRect = *inRect;
   }
   else
   {
      mLockedRectRect = RectI(0, 0, width, height);
   }
   
   mLockedRect.pitch = mLockedRectRect.extent.x * mBytesPerTexel;
   
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, mBuffer);
   // CodeReview [ags 12/19/07] This one texel boundary is necessary to keep the clipmap code from crashing.  Figure out why.
   glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, (mLockedRectRect.extent.x + 1) * (mLockedRectRect.extent.y + 1) * mBytesPerTexel, NULL, GL_STREAM_DRAW);
   mLockedRect.bits = (U8*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY);
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
   
   if( !mLockedRect.bits )
      return NULL;

   return &mLockedRect;
}

void GFXGLTextureObject::unlock(U32 mipLevel)
{
   if(!mLockedRect.bits)
      return;

   PRESERVE_TEXTURE(mBinding);

   glActiveTexture(GL_TEXTURE0);
   

   glBindTexture(mBinding, mHandle);
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, mBuffer);
   glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);
   if(mBinding == GL_TEXTURE_2D)
	   glTexSubImage2D(mBinding, mipLevel, mLockedRectRect.point.x, mLockedRectRect.point.y, 
		  mLockedRectRect.extent.x, mLockedRectRect.extent.y, GFXGLTextureFormat[mFormat], GFXGLTextureType[mFormat], NULL);
   else if(mBinding == GL_TEXTURE_1D)
		glTexSubImage1D(mBinding, mipLevel, (mLockedRectRect.point.x > 1 ? mLockedRectRect.point.x : mLockedRectRect.point.y), 
		  (mLockedRectRect.extent.x > 1 ? mLockedRectRect.extent.x : mLockedRectRect.extent.y), GFXGLTextureFormat[mFormat], GFXGLTextureType[mFormat], NULL);
   
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

   mLockedRect.bits = NULL;
}

void GFXGLTextureObject::release()
{
   glDeleteTextures(1, &mHandle);
   glDeleteBuffers(1, &mBuffer);
   
   mHandle = 0;
   mBuffer = 0;
}

void GFXGLTextureObject::reInit()
{
   AssertFatal(!mHandle && !mBuffer,"Must release before reInit");
   glGenTextures(1, &mHandle);
   glGenBuffers(1, &mBuffer);
}

bool GFXGLTextureObject::copyToBmp(GBitmap * bmp)
{
   PRESERVE_TEXTURE(mBinding);
   glBindTexture(mBinding, mHandle);
   
   GLint textureFormat = GFXGLTextureFormat[bmp->getFormat()];
   GLint textureType = GFXGLTextureType[bmp->getFormat()];
   
   glGetTexImage(mBinding, 0, textureFormat, textureType, bmp->getWritableBits());
   return true;
}

void GFXGLTextureObject::initSamplerState(const GFXSamplerStateDesc &ssd)
{
   glTexParameteri(mBinding, GL_TEXTURE_MIN_FILTER, minificationFilter(ssd.minFilter, ssd.mipFilter, mMipLevels));
   glTexParameteri(mBinding, GL_TEXTURE_MAG_FILTER, GFXGLTextureFilter[ssd.magFilter]);
   glTexParameteri(mBinding, GL_TEXTURE_WRAP_S, !mIsNPoT2 ? GFXGLTextureAddress[ssd.addressModeU] : GL_CLAMP_TO_EDGE);
   glTexParameteri(mBinding, GL_TEXTURE_WRAP_T, !mIsNPoT2 ? GFXGLTextureAddress[ssd.addressModeV] : GL_CLAMP_TO_EDGE);
   if(mBinding == GL_TEXTURE_3D)
      glTexParameteri(mBinding, GL_TEXTURE_WRAP_R, GFXGLTextureAddress[ssd.addressModeW]);
   if(static_cast< GFXGLDevice* >( GFX )->supportsAnisotropic() )
      glTexParameterf(mBinding, GL_TEXTURE_MAX_ANISOTROPY_EXT, ssd.maxAnisotropy);

   mNeedInitSamplerState = false;
   mSampler = ssd;
}

void GFXGLTextureObject::bind(U32 textureUnit)
{
   glActiveTexture(GL_TEXTURE0 + textureUnit);
   glBindTexture(mBinding, mHandle);
  
   GFXGLStateBlockRef sb = mGLDevice->getCurrentStateBlock();
   AssertFatal(sb, "GFXGLTextureObject::bind - No active stateblock!");
   if (!sb)
      return;
         
   const GFXSamplerStateDesc ssd = sb->getDesc().samplers[textureUnit];

   if(mNeedInitSamplerState)
   {
      initSamplerState(ssd);
      return;
   }

   if(mSampler.minFilter != ssd.minFilter || mSampler.mipFilter != ssd.mipFilter)
      glTexParameteri(mBinding, GL_TEXTURE_MIN_FILTER, minificationFilter(ssd.minFilter, ssd.mipFilter, mMipLevels));
   if(mSampler.magFilter != ssd.magFilter)
      glTexParameteri(mBinding, GL_TEXTURE_MAG_FILTER, GFXGLTextureFilter[ssd.magFilter]);
   if(mSampler.addressModeU != ssd.addressModeU)
      glTexParameteri(mBinding, GL_TEXTURE_WRAP_S, !mIsNPoT2 ? GFXGLTextureAddress[ssd.addressModeU] : GL_CLAMP_TO_EDGE);
   if(mSampler.addressModeV != ssd.addressModeV)
      glTexParameteri(mBinding, GL_TEXTURE_WRAP_T, !mIsNPoT2 ? GFXGLTextureAddress[ssd.addressModeV] : GL_CLAMP_TO_EDGE);
   if(mBinding == GL_TEXTURE_3D && mSampler.addressModeW != ssd.addressModeW )
      glTexParameteri(mBinding, GL_TEXTURE_WRAP_R, GFXGLTextureAddress[ssd.addressModeW]);
   if(mSampler.maxAnisotropy != ssd.maxAnisotropy  && static_cast< GFXGLDevice* >( GFX )->supportsAnisotropic() )
      glTexParameterf(mBinding, GL_TEXTURE_MAX_ANISOTROPY_EXT, ssd.maxAnisotropy);

   mSampler = ssd;
}

U8* GFXGLTextureObject::getTextureData()
{
   U8* data = new U8[mTextureSize.x * mTextureSize.y * mBytesPerTexel];
   glBindTexture(mBinding, mHandle);
   glGetTexImage(mBinding, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data);
   return data;
}

void GFXGLTextureObject::copyIntoCache()
{
   glBindTexture(mBinding, mHandle);
   U32 cacheSize = mTextureSize.x * mTextureSize.y;
   if(mBinding == GL_TEXTURE_3D)
      cacheSize *= mTextureSize.z;
      
   cacheSize *= mBytesPerTexel;
   mZombieCache = new U8[cacheSize];
   
   glGetTexImage(mBinding, 0, GFXGLTextureFormat[mFormat], GFXGLTextureType[mFormat], mZombieCache);
   glBindTexture(mBinding, 0);
}

void GFXGLTextureObject::reloadFromCache()
{
   if(!mZombieCache)
      return;
      
   if(mBinding == GL_TEXTURE_3D)
   {
      static_cast<GFXGLTextureManager*>(TEXMGR)->_loadTexture(this, mZombieCache);
      delete[] mZombieCache;
      mZombieCache = NULL;
      return;
   }
   
   glBindTexture(mBinding, mHandle);

   if(mBinding == GL_TEXTURE_2D)
		glTexSubImage2D(mBinding, 0, 0, 0, mTextureSize.x, mTextureSize.y, GFXGLTextureFormat[mFormat], GFXGLTextureType[mFormat], mZombieCache);
   else if(mBinding == GL_TEXTURE_1D)
		glTexSubImage1D(mBinding, 0, 0, (mTextureSize.x > 1 ? mTextureSize.x : mTextureSize.y), GFXGLTextureFormat[mFormat], GFXGLTextureType[mFormat], mZombieCache);
   
   if(GFX->getCardProfiler()->queryProfile("GL::Workaround::needsExplicitGenerateMipmap") && mMipLevels != 1)
      glGenerateMipmapEXT(mBinding);
      
   delete[] mZombieCache;
   mZombieCache = NULL;
   mIsZombie = false;
}

void GFXGLTextureObject::zombify()
{
   if(mIsZombie)
      return;
      
   mIsZombie = true;
   if(!mProfile->doStoreBitmap() && !mProfile->isRenderTarget() && !mProfile->isDynamic() && !mProfile->isZTarget())
      copyIntoCache();
      
   release();
}

void GFXGLTextureObject::resurrect()
{
   if(!mIsZombie)
      return;
      
   glGenTextures(1, &mHandle);
   glGenBuffers(1, &mBuffer);
}

F32 GFXGLTextureObject::getMaxUCoord() const
{
   return mBinding == GL_TEXTURE_2D ? 1.0f : (F32)getWidth();
}

F32 GFXGLTextureObject::getMaxVCoord() const
{
   return mBinding == GL_TEXTURE_2D ? 1.0f : (F32)getHeight();
}

const String GFXGLTextureObject::describeSelf() const
{
   String ret = Parent::describeSelf();
   ret += String::ToString("   GL Handle: %i", mHandle);
   
   return ret;
}

//-----------------------------------------------------------------------------
// Copyright (c) 2014 GarageGames, LLC
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

#include <iostream>
#include <gtest/gtest.h>

#if defined(TORQUE_OS_WIN)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

namespace testing
{
   // Original author: Stephan Brenner
   // https://github.com/ymx/gtest_mem
   class MemoryLeakDetector : public EmptyTestEventListener
   {
   public:
      virtual void OnTestStart(const TestInfo&)
      {
#if defined(TORQUE_OS_WIN)
         _CrtMemCheckpoint(&memState_);
#endif
      }

      virtual void OnTestEnd(const TestInfo& test_info)
      {
         if(test_info.result()->Passed())
         {
#if defined(TORQUE_OS_WIN)
            _CrtMemState stateNow, stateDiff;
            _CrtMemCheckpoint(&stateNow);
            int diffResult = _CrtMemDifference(&stateDiff, &memState_, &stateNow);
            if (diffResult)
            {
               FAIL() << "Memory leak of " << stateDiff.lSizes[1] << " byte(s) detected.";
            }
#endif
         }
      }

   private:
#if defined(TORQUE_OS_WIN)
      _CrtMemState memState_;
#endif
   };
}
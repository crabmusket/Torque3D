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

#ifdef TORQUE_TESTS_ENABLED
#include "testing/unitTesting.h"

#include "core/stringTable.h"
#include "core/crc.h"
#include "core/strings/stringFunctions.h"
#include "core/util/zip/zipArchive.h"

// The filename of the zip file that we create for writing to
#define WRITE_FILE    "zipUnitTest.zip"
// The filename of the zip file created in a standard zip application
#define BASELINE_FILE "zipUnitTestBaseline.zip"
// The filename of our working copy of the above so that we don't destroy the svn copy
#define WORKING_FILE  "zipUnitTestWorking.zip"

using namespace Zip;

TEST(Zip, Misc)
{
   FileStream source, dest;

   ASSERT_TRUE(source.open(BASELINE_FILE, Torque::FS::File::Read))
      << "Failed to open baseline zip for read";

   ASSERT_TRUE(dest.open(WORKING_FILE, Torque::FS::File::Write))
      << "Failed to open working zip for write";

   ASSERT_TRUE(dest.copyFrom(&source))
      << "Failed to copy baseline zip";

   dest.close();
   source.close();

   ZipArchive *zip = new ZipArchive;

   if(!zip->openArchive(WORKING_FILE, ZipArchive::ReadWrite))
   {
      delete zip;
      ASSERT_TRUE(false) << "Unable to open zip file";
   }

   // Opening a file in the zip as ReadWrite (should fail)
   Stream *stream;
   if((stream = zip->openFile("readWriteTest.txt", ZipArchive::ReadWrite)) != NULL)
   {
      zip->closeFile(stream);
      ASSERT_TRUE(false) << "File opened successfully as read/write";
   }

   // Enumerating, Extracting and Deleting files
   if(zip->numEntries() > 0)
   {
      // Find the biggest file in the zip
      const CentralDir *del = NULL;
      for(S32 i = 0;i < zip->numEntries();++i)
      {
         const CentralDir &cd = (*zip)[i];

         if(del == NULL || (del && cd.mUncompressedSize > del->mUncompressedSize))
            del = &cd;
      }

      if(del)
      {
         // Extract the file
         const char *ptr = dStrrchr(del->mFilename, '/');
         if(ptr)
            ++ptr;
         else
            ptr = del->mFilename;
         StringTableEntry fn = makeTestPath(ptr);

         EXPECT_TRUE(zip->extractFile(del->mFilename, fn))
            << "Failed to extract file.";

         // Then delete it
         EXPECT_TRUE(zip->deleteFile(del->mFilename))
            << "ZipArchive::deleteFile() failed?!";

         // Close and reopen the zip to force it to rebuild
         zip->closeArchive();

         if(!zip->openArchive(WORKING_FILE, ZipArchive::ReadWrite))
         {
            delete zip;
            ASSERT_TRUE(false) << "Unable to re-open zip file?!";
         }
      }
      else
      {
         ASSERT_TRUE(false) << "Couldn't find a file to delete?!";
      }
   }
   else
   {
      ASSERT_TRUE(false) << "Baseline zip has no files.";
   }

   zip->closeArchive();
   delete zip;

   if(Platform::isFile(WRITE_FILE))
      dFileDelete(WRITE_FILE);
   if(Platform::isFile(WORKING_FILE))
      dFileDelete(WORKING_FILE);
}

TEST(Zip, Read)
{
private:
   StringTableEntry WRITE_FILE;
   StringTableEntry BASELINE_FILE;
   StringTableEntry WORKING_FILE;

public:
   ZipTestRead()
   {
      WRITE_FILE = makeTestPath(ZIPTEST_WRITE_FILENAME);
      BASELINE_FILE = makeTestPath(ZIPTEST_BASELINE_FILENAME);
      WORKING_FILE = makeTestPath(ZIPTEST_WORKING_FILENAME);
   }

   void run()
   {
      MemoryTester m;
      m.mark();

      // Clean up from a previous run
      cleanup();

      // Read mode tests with the baseline zip
      readTest(BASELINE_FILE);

      // Read mode tests with a zip created by us
      test(makeTestZip(), "Failed to make test zip");
      readTest(WORKING_FILE);

      // Do read/write mode tests
      readWriteTest(WRITE_FILE);

      test(m.check(), "Zip read test leaked memory!");
   }

private:

   //-----------------------------------------------------------------------------

   void cleanup()
   {
      if(Platform::isFile(WRITE_FILE))
         dFileDelete(WRITE_FILE);
      if(Platform::isFile(WORKING_FILE))
         dFileDelete(WORKING_FILE);
   }

   //-----------------------------------------------------------------------------

   bool writeFile(ZipArchive *zip, const char *filename)
   {
      Stream * stream = zip->openFile(filename, ZipArchive::Write);
      if(stream != NULL)
      {
         stream->writeLine((U8 *)"This is a test file for reading from a zip created with the zip code.\r\nIt is pointless by itself, but needs to be long enough that it gets compressed.\r\n");
         zip->closeFile(stream);
         return true;
      }

      return false;
   }

   bool makeTestZip()
   {
      ZipArchive *zip = new ZipArchive;

      if(!zip->openArchive(WORKING_FILE, ZipArchive::Write))
      {
         delete zip;
         fail("Unable to open zip file");

         return false;
      }

      test(writeFile(zip, "test.txt"), "Failed to write file into test zip");
      test(writeFile(zip, "console.log"), "Failed to write file into test zip");
      test(writeFile(zip, "folder/OpenAL32.dll"), "Failed to write file into test zip");

      zip->closeArchive();
      delete zip;

      return true;
   }

   //-----------------------------------------------------------------------------

   bool testReadFile(ZipArchive *zip, const char *filename)
   {
      // [tom, 2/5/2007] Try using openFile() first for fairest real-world test
      Stream *stream;
      if((stream = zip->openFile(filename, ZipArchive::Read)) == NULL)
         return false;

      const CentralDir *cd = zip->findFileInfo(filename);
      if(cd == NULL)
      {
         // [tom, 2/5/2007] This shouldn't happen
         zip->closeFile(stream);

         fail("testReadFile - File opened successfully, but couldn't find central directory. This is bad.");

         return false;
      }

      U32 crc = CRC::INITIAL_CRC_VALUE;
      
      bool ret = true;
      U8 buffer[1024];
      U32 numBytes = stream->getStreamSize() - stream->getPosition();
      while((stream->getStatus() != Stream::EOS) && numBytes > 0)
      {
         U32 numRead = numBytes > sizeof(buffer) ? sizeof(buffer) : numBytes;
         if(! stream->read(numRead, buffer))
         {
            ret = false;
            fail("testReadFile - Couldn't read from stream");
            break;
         }

         crc = CRC::calculateCRC(buffer, numRead, crc);

         numBytes -= numRead;
      }

      if(ret)
      {
         // CRC Check
         crc ^= CRC::CRC_POSTCOND_VALUE;
         if(crc != cd->mCRC32)
         {
            fail("testReadFile - File failed CRC check");
            ret = false;
         }
      }

      zip->closeFile(stream);
      return ret;
   }

   //-----------------------------------------------------------------------------

   bool readTest(const char *zipfile)
   {
      ZipArchive *zip = new ZipArchive;

      if(!zip->openArchive(zipfile, ZipArchive::Read))
      {
         delete zip;
         fail("Unable to open zip file");

         return false;
      }

      // Try reading files that exist in various formats
      testReadFile(zip, "test.txt");
      testReadFile(zip, "console.log");
      testReadFile(zip, "folder/OpenAL32.dll");

      // Try reading a file that doesn't exist
      if(testReadFile(zip, "hopefullyDoesntExist.bla.bla.foo"))
         fail("Opening a file the didn't exist succeeded");

      zip->closeArchive();
      delete zip;

      return true;
   }

   bool readWriteTest(const char *zipfile)
   {
      ZipArchive *zip = new ZipArchive;
      bool ret = true;

      if(!zip->openArchive(zipfile, ZipArchive::ReadWrite))
      {
         delete zip;
         fail("Unable to open zip file");

         return false;
      }

      // Test writing a file and then reading it back before the zip is rebuilt
      test(writeFile(zip, "test.txt"), "Failed to write file to test zip");
      test(testReadFile(zip, "test.txt"), "Failed to read file back from test zip");
      
      // Read from file that is already open for write (should fail)
      Stream *wrStream = zip->openFile("writeTest.txt", ZipArchive::Write);
      if(wrStream != NULL)
      {
         wrStream->writeLine((U8 *)"Test text to make a valid file");

         // This next open should fail
         Stream * rdStream = zip->openFile("writeTest.txt", ZipArchive::Read);
         if(rdStream != NULL)
         {
            fail("Succeeded in opening a file for read that's already open for write");
            ret = false;

            zip->closeFile(rdStream);
         }

         zip->closeFile(wrStream);
      }
      else
      {
         fail("Could not open file for write");
         ret = false;
      }

      zip->closeArchive();
      delete zip;

      return ret;
   }
}

#endif
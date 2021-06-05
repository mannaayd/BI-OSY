#ifndef __PROGTEST__
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <functional>
using namespace std;


/* Filesystem size: min 8MiB, max 1GiB
 * Filename length: min 1B, max 28B
 * Sector size: 512B
 * Max open files: 8 at a time
 * At most one filesystem mounted at a time.
 * Max file size: < 1GiB
 * Max files in the filesystem: 128
 */

#define FILENAME_LEN_MAX    28
#define DIR_ENTRIES_MAX     128
#define OPEN_FILES_MAX      8
#define SECTOR_SIZE         512
#define DEVICE_SIZE_MAX     ( 1024 * 1024 * 1024 )
#define DEVICE_SIZE_MIN     ( 8 * 1024 * 1024 )

struct TFile
{
  char            m_FileName[FILENAME_LEN_MAX + 1];
  size_t          m_FileSize;
};

struct TBlkDev
{
  size_t           m_Sectors;
  function<size_t(size_t, void *, size_t )> m_Read;
  function<size_t(size_t, const void *, size_t )> m_Write;
};
#endif /* __PROGTEST__ */

#define myEOF (uint32_t)(-1)
#define HEADERFS 0.07

class CFileSystem
{
  public:
    ~CFileSystem()
    {
        free (files);
        free (linkedList);
        free (descriptors);
    }

    CFileSystem()
    {

    }

    struct CFile
    {
        char fileName[29];
        uint32_t size;
        uint32_t fileBlock;
    };

    struct CFileDescriptor
    {
        CFile *file;
        bool isFree = true;
        bool writeMode;
        uint32_t currentSector;
    };

    static void      printBuffer                                (void* data, size_t countSectors)
    {
        uint8_t * dataInt = (uint8_t*)data;
        for(int i = 0; i < 512 * countSectors; i++)
        {
            printf("%d ", dataInt[i]);
            if(i % 128 == 0 && i > 0) printf("\n");
            //if(i > 1024) break;
        }
        printf("\n\n\n");
    }

    static bool    CreateFs                                ( const TBlkDev   & dev )
    {
        uint32_t sectorsFAT = ((sizeof(CFile) * DIR_ENTRIES_MAX) / SECTOR_SIZE);
        sectorsFAT += (double)((sizeof(CFile) * DIR_ENTRIES_MAX) / SECTOR_SIZE) - sectorsFAT > 0.0000001 ?  1 : 0;
        uint32_t sectorsLinkedList = dev.m_Sectors * HEADERFS - 1 - sectorsFAT;

        void        *headerFAT = calloc(sectorsFAT * SECTOR_SIZE, sizeof(uint8_t));
        uint32_t    *headerFirst = (uint32_t*)calloc(SECTOR_SIZE / 4, sizeof(uint32_t));
        void        *headerLinkedList = calloc(sectorsLinkedList * SECTOR_SIZE, sizeof(uint8_t));

        headerFirst[0] = sectorsFAT + sectorsLinkedList + 1;

        bool retFAT = sectorsFAT == dev.m_Write(0, headerFAT, sectorsFAT);
        bool retFirst = 1 == dev.m_Write(sectorsFAT, headerFirst, 1);
        bool retLinkedList = sectorsLinkedList == dev.m_Write(sectorsFAT + 1, headerLinkedList, sectorsLinkedList);
        free(headerFAT);
        free(headerFirst);
        free(headerLinkedList);
        return retFAT && retFirst && retLinkedList;
    }

    uint32_t getValueFromSector(uint32_t sectorPos, uint32_t valuePos, uint32_t valueSize) // valueSize must be from 1 to 4
    {
        uint32_t value;

        return value;
    }

    static CFileSystem * Mount                             ( const TBlkDev   & dev )
    {

        CFileSystem * fs = new CFileSystem();
        fs->SetDevice(dev);
        return fs;
    }

    void SetDevice( const TBlkDev   & dev )
    {
        device = &dev;
        sectorsFAT = ((40 * DIR_ENTRIES_MAX) / SECTOR_SIZE);
        sectorsLinkedList = dev.m_Sectors * HEADERFS - 1 - sectorsFAT;
    }
    bool           Umount                                  ( void )
    {
        if(!device) return false;

        return true;
    }
    size_t         FileSize                                ( const char      * fileName )
    {

    }

    int GetFileBlock(const char      * fileName)
    {

        uint8_t *bufferFAT = (uint8_t*)calloc(sectorsFAT * SECTOR_SIZE, sizeof(uint8_t));
        device->m_Read(0, bufferFAT, sectorsFAT);
        char bufferName[32];
        uint32_t bufferSize[1];
        uint32_t bufferFileBlock[1];
        for(std::size_t i = 0; i < DIR_ENTRIES_MAX; i+=40)
        {
            memcpy(bufferName, bufferFAT + i, 32);
            memcpy(bufferSize, bufferFAT + i + 32, 4);
            memcpy(bufferFileBlock, bufferFAT + i + 32 + 4, 4);
            if(bufferFileBlock[0] == 0) break;
            if(!strcmp(fileName, bufferName))
            {
                free(bufferFAT);
                return bufferFileBlock[0];
            }
        }
        free(bufferFAT);
        return -1;
    }



    uint32_t GetRef(uint32_t addr)
    {

    }

    void SetRef(uint32_t dest, uint32_t src)
    {

    }

    int CreateFile(const char      * fileName)
    {
        uint8_t *bufferFAT = (uint8_t*)calloc(sectorsFAT * SECTOR_SIZE, sizeof(uint8_t));
        device->m_Read(0, bufferFAT, sectorsFAT);
        char bufferName[32];
        uint32_t bufferSize[1];
        uint32_t bufferFileBlock[1];
        for(std::size_t i = 0; i < DIR_ENTRIES_MAX; i+=40)
        {
            memcpy(bufferFileBlock, bufferFAT + i + 32 + 4, 4);
            if(bufferFileBlock[0] == 0)
            {
                free(bufferFAT);
                device->m_Read(sectorsFAT, bufferFileBlock, 1);
                bufferSize[0] = 0;
                if(bufferFileBlock[0] == myEOF) return -1;
                memcpy(bufferFAT + i, fileName, 32);
                memcpy(bufferFAT + i + 32, bufferSize, 4);
                memcpy(bufferFAT + i + 32 + 4, bufferFileBlock, 4);
                return bufferFileBlock[0];
            }
        }
        free(bufferFAT);
        return -1;
    }

    int            OpenFile                                ( const char      * fileName,
                                                             bool              writeMode )
    {
        int res = GetFileBlock(fileName);
        if(res == -1 && writeMode)
        {
            res = CreateFile(fileName);
        }
        if(res == -1) return -1;
        for(std::size_t i = 0; i < OPEN_FILES_MAX; i++)
        {
            if(openedFiles[i] == -1)
            {
                 if(!writeMode) res *= -1;
                openedFiles[i] = res;
                return res;
            }
        }
        return -1;
    }


    bool           CloseFile                               ( int               fd )
    {
        for(std::size_t i = 0; i < OPEN_FILES_MAX; i++)
        {
            if(openedFiles[i] == fd)
            {
                openedFiles[i] = -1;
                return true;
            }
        }
        return false;
    }
    size_t         ReadFile                                ( int               fd,
                                                             void            * data,
                                                             size_t            len )
    {

    }
    size_t         WriteFile                               ( int               fd,
                                                             const void      * data,
                                                             size_t            len )
    {

    }
    bool           DeleteFile                              ( const char      * fileName )
    {

    }
    bool           FindFirst                               ( TFile           & file )
    {

    }
    bool           FindNext                                ( TFile           & file )
    {

    }
  private:
    TBlkDev dev;
    CFile * files;
    uint32_t * nodes;
    CFileDescriptor * descriptors;
};


#ifndef __PROGTEST__
#include "simple_test.inc"
#endif /* __PROGTEST__ */

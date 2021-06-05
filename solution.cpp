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
        //printBuffer(fileTable, 2);
        if(fileTable)   free (fileTable);
        if(linkedList)  free (linkedList);
        if(descriptors) free (descriptors);
    }

    CFileSystem(const TBlkDev   & dev)
    {
        device = dev;
        sectorsFAT = ((sizeof(CFile) * DIR_ENTRIES_MAX) / SECTOR_SIZE);
        sectorsFAT += (double)((sizeof(CFile) * DIR_ENTRIES_MAX) / SECTOR_SIZE) - sectorsFAT > 0.0000001 ?  1 : 0;
        sectorsLinkedList = dev.m_Sectors * HEADERFS - 1 - sectorsFAT;

        fileTable = (CFile*)calloc(sectorsFAT * SECTOR_SIZE, sizeof(uint8_t));
        linkedList = (uint32_t*)calloc(sectorsLinkedList * SECTOR_SIZE / 4, sizeof(uint32_t));
        uint32_t *buffer = (uint32_t*)calloc(SECTOR_SIZE / 4, sizeof(uint32_t));

        device.m_Read(0, fileTable, sectorsFAT);
        device.m_Read(sectorsFAT, buffer, 1);
        device.m_Read(sectorsFAT + 1, linkedList, sectorsLinkedList);
        firstFree = buffer[0];

        descriptors = (CFileDescriptor*)calloc(OPEN_FILES_MAX, sizeof(CFileDescriptor));
        for(size_t i = 0; i < OPEN_FILES_MAX; i++) descriptors[i] = CFileDescriptor(this);

        free(buffer);
    }

    struct CFile
    {
        char fileName[29];
        uint32_t size;
        uint32_t fileBlock;
    };

    struct CFileDescriptor
    {
        CFileDescriptor(CFileSystem * fs)
        {
            fileSystem = fs;
            file = NULL;
            writeMode = false;
            bytes = 0;
            currentSector = 0;
            shift = fileSystem->sectorsLinkedList - 1 - fileSystem->sectorsFAT;
        }
        bool open(CFile *file, bool writeMode)
        {
            if(this->file) return false;
            this->file = file;
            this->bytes = 0;
            this->writeMode = writeMode;
            if(writeMode)
            {
                file->size = 0;
                if(file->fileBlock != fileSystem->GetTail(file->fileBlock)) // deleting file
                {
                    fileSystem->linkedList[fileSystem->GetTail(file->fileBlock) - shift] = fileSystem->firstFree;
                    fileSystem->firstFree = fileSystem->linkedList[file->fileBlock - shift];
                    fileSystem->linkedList[file->fileBlock - shift] = myEOF;
                }
            }
            currentSector = file->fileBlock;
            return true;
        }

        bool close()
        {
            if(!file) return false;
            file = NULL;
            if(writeMode) file->size = bytes;
            return true;
        }

        size_t write(const uint8_t * data, size_t len)
        {
            uint32_t positionSector = bytes % SECTOR_SIZE;
            size_t res = 0;
            if(len == 0 || !file || !writeMode) return 0;
            uint8_t *sector = (uint8_t*)calloc(SECTOR_SIZE, sizeof(uint8_t));
            if(fileSystem->device.m_Read(currentSector, sector, 1) != 1)
            {
                free(sector);
                return res;
            }
            while(len > 0)
            {
                uint32_t writen = (uint32_t)(len) < (uint32_t)(SECTOR_SIZE - positionSector) ? len : SECTOR_SIZE - positionSector;
                memcpy(sector + positionSector, data, writen);
                if(fileSystem->device.m_Read(currentSector, sector, 1) != 1)
                {
                    free(sector);
                    return res;
                }
                len -= writen;
                data += writen;
                bytes += writen;
                res += writen;
                positionSector = (positionSector + writen)%SECTOR_SIZE;
                if (!positionSector) // adding next sector to file
                {
                    uint32_t add = fileSystem->GetFreeSector();
                    if (add == myEOF)
                    {
                        free(sector);
                        return res;
                    }
                    fileSystem->linkedList[currentSector - shift] = add;
                    currentSector = add;
                }
            }
            free(sector);
            return res;
        }

        void read()
        {

        }

        CFile *file;
        bool writeMode;
        uint32_t bytes;
        uint32_t currentSector;
        uint32_t shift;
        CFileSystem * fileSystem;
    };

    static void         printBuffer                                 (const void* data, size_t countSectors)
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

    static void         printArray                                  (uint32_t *data, std::size_t len)
    {
        for(std::size_t i = 0; i < len; i++)
        {
            printf("%d ", data[i]);
            if(i % 64 == 0 && i > 0) printf("\n");
        }
        printf("\n\n\n");
    }

    static bool    CreateFs                                ( const TBlkDev   & dev )
    {
        uint32_t sectorsFAT = ((sizeof(CFile) * DIR_ENTRIES_MAX) / SECTOR_SIZE);
        sectorsFAT += (double)((sizeof(CFile) * DIR_ENTRIES_MAX) / SECTOR_SIZE) - sectorsFAT > 0.0000001 ?  1 : 0;
        uint32_t sectorsLinkedList = dev.m_Sectors * HEADERFS - 1 - sectorsFAT;
        uint32_t fileSectors = dev.m_Sectors - sectorsFAT - sectorsLinkedList - 1;

        void        *headerFAT = calloc(sectorsFAT * SECTOR_SIZE, sizeof(uint8_t));
        uint32_t    *headerFirst = (uint32_t*)calloc(SECTOR_SIZE / 4, sizeof(uint32_t));
        uint32_t    *headerLinkedList = (uint32_t*)calloc(sectorsLinkedList * SECTOR_SIZE / 4, sizeof(uint32_t));

        headerFirst[0] = sectorsFAT + sectorsLinkedList + 1;

        size_t i;
        for(i = 0; i < fileSectors; i++) headerLinkedList[i] = sectorsFAT + sectorsLinkedList + i + 2;
        headerLinkedList[i] = myEOF;

       // printBuffer(headerFAT, 2);
       // printBuffer(headerLinkedList, sectorsLinkedList);
       // printBuffer(headerFirst, 1);
        //printArray(headerLinkedList, sectorsLinkedList * SECTOR_SIZE / 4);
        bool retFAT = sectorsFAT == dev.m_Write(0, headerFAT, sectorsFAT);
        bool retFirst = 1 == dev.m_Write(sectorsFAT, headerFirst, 1);
        bool retLinkedList = sectorsLinkedList == dev.m_Write(sectorsFAT + 1, headerLinkedList, sectorsLinkedList);

        free(headerFAT);
        free(headerFirst);
        free(headerLinkedList);

        printf(retFAT && retFirst && retLinkedList ? "CreateFS: Success\n" : "CreateFS: Fail\n");
        return retFAT && retFirst && retLinkedList;
    }

    static CFileSystem * Mount                             ( const TBlkDev   & dev )
    {
        CFileSystem * fs = new CFileSystem(dev);
       // printArray(fs->linkedList, fs->sectorsLinkedList * SECTOR_SIZE / 4);
        return fs;
    }


    bool           Umount                                  ( void )
    {


        return true;
    }

    CFile * FindFile(const char* fileName)
    {
        for(std::size_t i = 0; i < DIR_ENTRIES_MAX; i++)
            if(fileTable[i].size != 0 && !strncmp(fileTable[i].fileName, fileName, FILENAME_LEN_MAX)) return fileTable + i;
        return NULL;
    }

    uint32_t GetFreeSector()
    {

        uint32_t res = firstFree;
        firstFree = linkedList[res - sectorsFAT - 1 - sectorsLinkedList];
        linkedList[res - sectorsFAT - 1 - sectorsLinkedList] = myEOF;
        return res;
    }

    CFile * CreateFile(const char *fileName)
    {
        CFile *file = NULL;
        for(size_t i = 0; i < DIR_ENTRIES_MAX; i++)
        {
            if(fileTable[i].fileBlock == 0)
            {
                strncpy(fileTable[i].fileName, fileName, FILENAME_LEN_MAX + 1);
                fileTable[i].fileName[FILENAME_LEN_MAX] = '\0';
                fileTable[i].size = 0;
                uint32_t next = GetFreeSector();
                if(next == myEOF) return NULL;
                fileTable[i].fileBlock = next;
                file = fileTable + i;
                break;
            }
        }
        return file;
    }

    uint32_t GetTail(uint32_t start)
    {
        uint32_t tail = start - sectorsLinkedList - sectorsFAT - 1;
        while(linkedList[tail] != myEOF)
            tail = linkedList[tail] - sectorsLinkedList - sectorsFAT - 1;
        return tail + sectorsLinkedList + sectorsFAT + 1;
    }


    size_t         FileSize                                ( const char      * fileName )
    {
        CFile *file = FindFile(fileName);
        if(file)
            return file->size;
        return SIZE_MAX;
    }

    int            OpenFile                                ( const char      * fileName,
                                                             bool              writeMode )
    {
        size_t fd = 0;

        for(fd = 0; fd < OPEN_FILES_MAX && descriptors[fd].file; fd++); // finding free descriptor
        if(fd >= OPEN_FILES_MAX)
            return -1;
        CFile * file = FindFile(fileName); // finding file
        //printArray(linkedList, sectorsLinkedList * SECTOR_SIZE / 4);
        if(writeMode && !file)
            file = CreateFile(fileName);
        //printf("%d\n", firstFree);
        if(!file)
            return -1;
        if(!descriptors[fd].open(file, writeMode))
            return -1;
        return fd;
    }


    bool           CloseFile                               ( int               fd )
    {
        return fd >= 0 && fd < OPEN_FILES_MAX && descriptors[fd].close();
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
        //printBuffer(data, 1);
        if(fd < 0 || fd >= OPEN_FILES_MAX)
            return 0;
        size_t writen = descriptors[fd].write((const uint8_t*)data, len);
        descriptors[fd].file->size = descriptors[fd].bytes;
        return writen;
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
    uint32_t sectorsFAT;
    uint32_t sectorsLinkedList;
    TBlkDev device;
    uint32_t firstFree;
    CFile * fileTable;
    uint32_t * linkedList;
    CFileDescriptor * descriptors;
};


#ifndef __PROGTEST__
#include "simple_test.inc"
#endif /* __PROGTEST__ */

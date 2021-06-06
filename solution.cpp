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
#define FREE_BLOCK 0
#define HEADERFS 0.07

class CFileSystem
{
public:
    struct CFile
    {
        char fileName[FILENAME_LEN_MAX + 1];
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
            shift = fileSystem->sectorsLinkedList + fileSystem->sectorsFAT;
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
                fileSystem->FreeSequence(file->fileBlock); // deleting sequence
            }
            currentSector = file->fileBlock;
            return true;
        }

        bool close()
        {
            if(!file) return false;
            if(writeMode) file->size = bytes;
            file = NULL;
            return true;
        }

        size_t write(const uint8_t * data, size_t len)
        {
            uint32_t positionSector = bytes % SECTOR_SIZE;
            size_t res = 0;
            if(len == 0 || !file || !writeMode) return 0;
            uint8_t *sector = (uint8_t*)calloc(SECTOR_SIZE, sizeof(uint8_t));
            if(fileSystem->device.m_Read(currentSector + shift, sector, 1) != 1)
            {
                free(sector);
                return res;
            }
            while(len > 0)
            {
                uint32_t writen = (uint32_t)len < (uint32_t)(SECTOR_SIZE - positionSector) ? len : SECTOR_SIZE - positionSector;
                memcpy(sector + positionSector, data, writen);
                if(fileSystem->device.m_Write(currentSector + shift, sector, 1) != 1)
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
                    fileSystem->linkedList[currentSector] = add;
                    currentSector = add;
                    fileSystem->linkedList[add] = myEOF;
                }
            }
            free(sector);
            return res;
        }

        size_t read(uint8_t *data, size_t len)
        {
            len = len < file->size - bytes ? len : file->size - bytes;
            uint32_t positionSector = bytes % SECTOR_SIZE;
            size_t res = 0;
            if(!len || writeMode || file->fileBlock == 0) return res;
            uint8_t *sector = (uint8_t*)calloc(SECTOR_SIZE, sizeof(uint8_t));
            while(len > 0)
            {
                if(currentSector == myEOF || fileSystem->device.m_Read(currentSector + shift, sector, 1) != 1)
                {
                    free(sector);
                    return res;
                }
                size_t read = (uint32_t)len < (uint32_t)(SECTOR_SIZE - positionSector) ? len : SECTOR_SIZE - positionSector;
                memcpy(data, sector+positionSector, read);
                data += read;
                bytes += read;
                len -= read;
                res += read;
                positionSector = (positionSector + read)%SECTOR_SIZE;
                if(!positionSector)
                    currentSector = fileSystem->linkedList[currentSector];
            }
            free(sector);
            return res;
        }

        CFile *file;
        bool writeMode;
        uint32_t bytes;
        uint32_t currentSector;
        uint32_t shift;
        CFileSystem * fileSystem;
    };

    ~CFileSystem()
    {
        if(fileTable)   free (fileTable);
        if(linkedList)  free (linkedList);
        if(descriptors) free (descriptors);
    }

    CFileSystem(const TBlkDev   & dev)
    {
        device = dev;
        sectorsFAT = ((sizeof(CFile) * DIR_ENTRIES_MAX) / SECTOR_SIZE);
        sectorsFAT += (double)((sizeof(CFile) * DIR_ENTRIES_MAX) / SECTOR_SIZE) - sectorsFAT > 0.0000001 ?  1 : 0;
        sectorsLinkedList = dev.m_Sectors * HEADERFS - sectorsFAT;
        fileSectors = dev.m_Sectors - sectorsLinkedList - sectorsFAT + 1;

        fileTable = (CFile*)calloc(sectorsFAT * SECTOR_SIZE, sizeof(uint8_t));
        linkedList = (uint32_t*)calloc(sectorsLinkedList * SECTOR_SIZE / 4, sizeof(uint32_t));

        device.m_Read(0, fileTable, sectorsFAT);
        device.m_Read(sectorsFAT, linkedList, sectorsLinkedList);

        descriptors = (CFileDescriptor*)calloc(OPEN_FILES_MAX, sizeof(CFileDescriptor));
        for(size_t i = 0; i < OPEN_FILES_MAX; i++) descriptors[i] = CFileDescriptor(this);
    }

    static bool    CreateFs                                ( const TBlkDev   & dev )
    {
        uint32_t sectorsFAT = ((sizeof(CFile) * DIR_ENTRIES_MAX) / SECTOR_SIZE);
        sectorsFAT += (double)((sizeof(CFile) * DIR_ENTRIES_MAX) / SECTOR_SIZE) - sectorsFAT > 0.0000001 ?  1 : 0;
        uint32_t sectorsLinkedList = dev.m_Sectors * HEADERFS - sectorsFAT;
        uint32_t fileSectors = dev.m_Sectors - sectorsFAT - sectorsLinkedList;

        void        *headerFAT = calloc(sectorsFAT * SECTOR_SIZE, sizeof(uint8_t));
        uint32_t    *headerLinkedList = (uint32_t*)calloc(sectorsLinkedList * SECTOR_SIZE / 4, sizeof(uint32_t));

        headerLinkedList[fileSectors] = myEOF;

        bool retFAT = sectorsFAT == dev.m_Write(0, headerFAT, sectorsFAT);
        bool retLinkedList = sectorsLinkedList == dev.m_Write(sectorsFAT + 1, headerLinkedList, sectorsLinkedList);

        free(headerFAT);
        free(headerLinkedList);

        return retFAT && retLinkedList;
    }

    static CFileSystem * Mount                             ( const TBlkDev   & dev )
    {
        CFileSystem * fs = new CFileSystem(dev);
        return fs;
    }


    bool           Umount                                  ( void )
    {
        bool retFAT = sectorsFAT == device.m_Write(0, fileTable, sectorsFAT);
        bool retLinkedList = sectorsLinkedList == device.m_Write(sectorsFAT, linkedList, sectorsLinkedList);
        for (size_t i = 0; i < OPEN_FILES_MAX; i++) CloseFile(i);
        free(fileTable);
        free(descriptors);
        free(linkedList);
        fileTable = NULL;
        linkedList = NULL;
        descriptors = NULL;
        return retFAT && retLinkedList;
    }

    CFile * FindFile(const char* fileName)
    {
        for(std::size_t i = 0; i < DIR_ENTRIES_MAX; i++)
            if(fileTable[i].size != 0 && !strncmp(fileTable[i].fileName, fileName, FILENAME_LEN_MAX)) return fileTable + i;
        return NULL;
    }

    uint32_t GetFreeSector()
    {
        for(size_t i = 1; i < fileSectors; i++)
            if(linkedList[i] == FREE_BLOCK) return i;
        return myEOF;
    }

    CFile * CreateFile(const char *fileName)
    {
        CFile *file = NULL;
        for(size_t i = 0; i < DIR_ENTRIES_MAX; i++)
        {
            if(fileTable[i].fileBlock == 0)
            {
                strcpy(fileTable[i].fileName, fileName);
                fileTable[i].fileName[FILENAME_LEN_MAX] = '\0';
                fileTable[i].size = 0;
                uint32_t next = GetFreeSector();
                if(next == myEOF) return NULL;
                fileTable[i].fileBlock = next;
                linkedList[next] = myEOF;
                file = fileTable + i;
                break;
            }
        }
        return file;
    }

    void FreeSequence(uint32_t start)
    {
        uint32_t tmp;
        uint32_t act = start;
        while(linkedList[act] != myEOF)
        {
            tmp = act;
            act = linkedList[act];
            linkedList[tmp] = FREE_BLOCK;
        }
        linkedList[act] = FREE_BLOCK;
        linkedList[start] = myEOF;
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
        size_t fd;
        for(fd = 0; fd < OPEN_FILES_MAX && descriptors[fd].file; fd++); // finding free descriptor
        if(fd >= OPEN_FILES_MAX)
            return -1;
        CFile * file = FindFile(fileName);      // finding file
        if(writeMode && !file)                  // need to create file
            file = CreateFile(fileName);
        if(!file)                               // can't create or find file
            return -1;
        if(!descriptors[fd].open(file, writeMode))      // overwriting file
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
        if(fd < 0 || fd >= OPEN_FILES_MAX) return 0;
        size_t read = descriptors[fd].read((uint8_t*)data, len);
        return read;
    }
    size_t         WriteFile                               ( int               fd,
                                                             const void      * data,
                                                             size_t            len )
    {
        if(fd < 0 || fd >= OPEN_FILES_MAX) return 0;
        size_t writen = descriptors[fd].write((const uint8_t*)data, len);
        descriptors[fd].file->size = descriptors[fd].bytes;
        return writen;
    }
    bool           DeleteFile                              ( const char      * fileName )
    {
        CFile *file = FindFile(fileName);
        if(!file) return false;
        for(size_t i = 0; i < OPEN_FILES_MAX; i++) // checking for functional descriptors
            if(descriptors[i].file == file) return false;
        FreeSequence(file->fileBlock); // deleting sequence
        linkedList[file->fileBlock] = 0;
        file->fileBlock = 0;
        file->size = 0;
        return true;
    }
    bool           FindFirst                               ( TFile           & file )
    {
        for (size_t i = 0; i < DIR_ENTRIES_MAX; i++)
        {
            if(fileTable[i].fileBlock != 0)
            {
                strcpy(file.m_FileName, fileTable[i].fileName);
                file.m_FileSize = fileTable[i].size;
                return true;
            }
        }
        return false;
    }
    bool           FindNext                                ( TFile           & file )
    {
        CFile * find = FindFile(file.m_FileName);
        if(!find) return false;
        for(; ++find < fileTable + DIR_ENTRIES_MAX;)
        {
            if(find->fileBlock != 0)
            {
                strcpy(file.m_FileName, find->fileName);
                file.m_FileSize = find->size;
                return true;
            }
        }
        return false;
    }
private:
    uint32_t sectorsFAT;
    uint32_t sectorsLinkedList;
    TBlkDev device;
    uint32_t fileSectors;
    CFile * fileTable;
    uint32_t * linkedList;
    CFileDescriptor * descriptors;
};


#ifndef __PROGTEST__
    #include "simple_test.inc"
#endif /* __PROGTEST__ */
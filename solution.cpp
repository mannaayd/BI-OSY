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
#define HEADERFS 0.1

class CFileSystem
{
  public:
    ~CFileSystem()
    {

    }

    CFileSystem()
    {

    }

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

        // Define variables for blocks (FAT, FIRST_FREE, LINKED_LIST, FILES)
        uint32_t sectorsFAT = ((40 * DIR_ENTRIES_MAX) / SECTOR_SIZE);                                                                           // count of sectors for FAT block
        sectorsFAT += (double)((40 * DIR_ENTRIES_MAX) / SECTOR_SIZE) - sectorsFAT > 0.0000001 ?  1 : 0;
        uint8_t *bufferFAT = (uint8_t*)calloc(sectorsFAT * SECTOR_SIZE, sizeof(uint8_t));                                                                  // buffer for FAT block
        uint32_t *buffer = (uint32_t*)calloc(SECTOR_SIZE / 4, sizeof(uint32_t));
        uint32_t sectorsLinkedList = dev.m_Sectors * HEADERFS - 1 - sectorsFAT;                                                                 // count of sectors for linked list
        uint32_t * bufferLinkedList = (uint32_t*)calloc(sectorsLinkedList * SECTOR_SIZE / 4, sizeof(uint32_t));                                                  // buffer for linked list
        uint32_t firstFree = sectorsFAT + sectorsLinkedList + 1;
        // Printing info about file system
        printf("Total number of sectors: %d\n", dev.m_Sectors + 1);
        printf("Sectors for file allocation table: 0 - %d\n", sectorsFAT - 1);
        printf("Sector for pointer to first free sector: %d\n", sectorsFAT);
        printf("Sectors for linked list: %d - %d\n", sectorsFAT + 1, sectorsFAT + sectorsLinkedList);
        printf("Number of sectors for file data: %d\n", dev.m_Sectors - sectorsFAT - sectorsLinkedList);
        printf("First free sector: %d\n", firstFree);
        // Writing first FAT block
        //printBuffer(bufferFAT, sectorsFAT);
        dev.m_Write(0, bufferFAT, sectorsFAT);

        // Writing Pointer to first free sector
        buffer[0] = firstFree;
        //printBuffer(buffer, 1);
        dev.m_Write(sectorsFAT, buffer, 1);

        // Writing linked list block
        uint32_t fileSectors = dev.m_Sectors - sectorsFAT - sectorsLinkedList;
        for(uint32_t i = 0; i < fileSectors; i++)
            bufferLinkedList[i] = i + firstFree;

        bufferLinkedList[fileSectors] = myEOF;
        printBuffer(bufferLinkedList, sectorsLinkedList);

        dev.m_Write(sectorsFAT + 1, bufferLinkedList, sectorsLinkedList);
        free(bufferLinkedList);
        free(bufferFAT);
        free(buffer);
        return true;
    }

    uint32_t getValueFromSector(uint32_t sectorPos, uint32_t valuePos, uint32_t valueSize) // value size must be from 1 to 4
    {
        uint32_t value;

        return value;
    }

    static CFileSystem * Mount                             ( const TBlkDev   & dev )
    {

    }
    bool           Umount                                  ( void )
    {

    }
    size_t         FileSize                                ( const char      * fileName )
    {

    }
    int            OpenFile                                ( const char      * fileName,
                                                             bool              writeMode )
    {

    }
    bool           CloseFile                               ( int               fd )
    {

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

};


#ifndef __PROGTEST__
#include "simple_test.inc"
#endif /* __PROGTEST__ */

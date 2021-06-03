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


struct CFile
{
    char        fileName[32];
    uint32_t    fileSize;
    uint32_t    fileBlock;
};

class CFileSystem
{
  public:
    ~CFileSystem()
    {

    }

    CFileSystem()
    {

    }

    static bool    CreateFs                                ( const TBlkDev   & dev )
    {

        // Define variables for first three blocks (FAT, FIRST_FREE, LINKED_LIST)
        size_t i = 0;
        size_t sectorsFAT = ((40 * DIR_ENTRIES_MAX) / SECTOR_SIZE);                                                             // count of sectors for FAT block
        sectorsFAT += (double)((40 * DIR_ENTRIES_MAX) / SECTOR_SIZE) - sectorsFAT > 0.0000001 ?  1 : 0;
        size_t bufferFATSize = sectorsFAT * (SECTOR_SIZE / 4) * sizeof(char);                                                   // size of FAT block in bytes
        char *bufferFAT = (char*)malloc(bufferFATSize);                                                                         // buffer for FAT block
        uint32_t *buffer = (uint32_t*)malloc(SECTOR_SIZE);
        size_t sectorsLinkedList = (dev.m_Sectors - sectorsFAT - 1) / SECTOR_SIZE;                                              // count of sectors for linked list
        sectorsLinkedList += (double)((dev.m_Sectors - sectorsFAT - 1) / SECTOR_SIZE) - sectorsLinkedList > 0.0000001 ?  1 : 0;
        size_t bufferLinkedListSize = sectorsLinkedList * SECTOR_SIZE;                                                          // buffer size for linked list
        uint32_t * bufferLinkedList = (uint32_t*)malloc(bufferLinkedListSize);                                                  // buffer for linked list
        size_t sectorsFiles = dev.m_Sectors - sectorsFAT - sectorsLinkedList;                                   // count of sectors for files data

        // Printing info about file system
        printf("Total number of sectors: %d\n", dev.m_Sectors + 1);
       /* printf("Sectors for file allocation table: 0 - %d\n", sectorsFAT - 1);
        printf("Sector for pointer to first free sector: %d\n", sectorsFAT);
        printf("Sectors for linked list: %d - %d\n", sectorsFAT + 1, sectorsFAT + sectorsLinkedList);
        printf("Number of sectors for file data: %d\n", sectorsFiles);*/

        // Writing first FAT block
        for(i = 0; i < bufferFATSize; i++) bufferFAT[i] = 0;
        dev.m_Write(0, bufferFAT, sectorsFAT);
        free(bufferFAT);
        // Writing Pointer to first free sector
        for(i = 0; i < SECTOR_SIZE; i++) buffer[i] = 0;
       // buffer[0] =
        dev.m_Write(sectorsFAT, buffer, 1);
        free(buffer);
        // Writing linked list block
        for(i = 0; i < sectorsFiles; i++) bufferLinkedList[i] = i + 1;
        dev.m_Write(sectorsFAT + 1, bufferLinkedList, sectorsLinkedList);
        //free(bufferLinkedList);

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

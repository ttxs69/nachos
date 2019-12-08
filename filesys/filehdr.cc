// filehdr.cc 
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the 
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the 
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect 
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector, 
//
//      Unlike in a real system, we do not keep track of file permissions, 
//	ownership, last modification date, etc., in the file header. 
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "filehdr.h"

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool
FileHeader::Allocate(BitMap *freeMap, int fileSize)
{
    if (fileSize > MaxFileSize)
      return FALSE;

    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);
    if (freeMap->NumClear() < numSectors)
	return FALSE;		// not enough space

    if (fileSize > ((NumDirect-1)*SectorSize)) {
      for (int i=0; i<NumDirect-1; i++)       // allocate the direct sectors
        dataSectors[i] = freeMap->Find();

      indirect = new FileHeader;            // allocate the indirect sectors
      if(!indirect->Allocate(freeMap, fileSize-((NumDirect-1)*SectorSize)))
        return FALSE;
      dataSectors[NumDirect-1] = freeMap->Find();
      indirect->WriteBack(dataSectors[NumDirect-1]);
    }
    else
      {
        for (int i = 0; i < numSectors; i++)
	    dataSectors[i] = freeMap->Find();
      }
    return TRUE;
}

//---------------------------------------------------------------------
// FileHeader::Extend
//
//--------------------------------------------------------------------
bool
FileHeader::Extend(BitMap *freeMap, int fileSize)
{
  int newNumBytes = fileSize;
  int newNumSectors = divRoundUp(fileSize, SectorSize);
  if (newNumSectors > MaxFileSize)
    return FALSE;              // file is too big

  if (freeMap->NumClear() < newNumSectors)
    return FALSE;              // not enough sapce
  if (fileSize > ((NumDirect-1)*SectorSize)) { // from direct to indirect
    if (numBytes <= (NumDirect-1)*SectorSize) {
      for (int i = numSectors;i<NumDirect-1;i++)       // allocate the direct sectors
        dataSectors[i] = freeMap->Find();
      indirect = new FileHeader;            // allocate the indirect sectors
      indirect->Allocate(freeMap, fileSize-((NumDirect-1)*SectorSize));
      dataSectors[NumDirect-1] = freeMap->Find();
      indirect->WriteBack(dataSectors[NumDirect-1]);
    }
    else
      {
        // from indirect to indirect
        indirect->FetchFrom(dataSectors[NumDirect-1]);
        if (!indirect->Extend(freeMap,fileSize-(NumDirect-1)*SectorSize))
          return FALSE;
        indirect->WriteBack(dataSectors[NumDirect-1]);
      }
  }
  else
    { // from direct to direct
      for (int i = numSectors; i< newNumSectors;i++)
        dataSectors[i] = freeMap->Find();
    }
  numBytes = newNumBytes;
  numSectors = newNumSectors;
  return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void 
FileHeader::Deallocate(BitMap *freeMap)
{
  if (numSectors <= NumDirect) // if only has direct
    for (int i = 0; i < numSectors; i++) {
	ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
	freeMap->Clear((int) dataSectors[i]);
    }
  else { // if has both direct and indirect
    for (int i = 0; i < NumDirect-1; i++) { // clear the direct
      ASSERT(freeMap->Test((int) dataSectors[i]));
      freeMap->Clear((int) dataSectors[i]);
    }
    indirect->FetchFrom(dataSectors[NumDirect-1]); // get indirect
    indirect->Deallocate(freeMap); // free indirect
    ASSERT(freeMap->Test((int) dataSectors[NumDirect-1]));
    freeMap->Clear((int) dataSectors[NumDirect-1]); // free the sector of indirect
  }
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk. 
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    synchDisk->ReadSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    synchDisk->WriteSector(sector, (char *)this); 
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int
FileHeader::ByteToSector(int offset)
{
  if (numSectors <= NumDirect-1)
    return(dataSectors[offset / SectorSize]);
  else {
    indirect->FetchFrom(dataSectors[NumDirect-1]);
    return indirect->ByteToSector(offset-(NumDirect-1)*SectorSize);
  }
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{
    int i, j, k;
    char *data = new char[SectorSize];

    printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
    if (numSectors <= NumDirect-1) {
      for (i = 0; i < numSectors; i++)
	printf("%d ", dataSectors[i]);
      printf("\nFile contents:\n");
      for (i = k = 0; i < numSectors; i++) {
	synchDisk->ReadSector(dataSectors[i], data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
	    if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
		printf("%c", data[j]);
            else
		printf("\\%x", (unsigned char)data[j]);
	}
        printf("\n");
      }
    }
    else {
      for (i = 0; i < NumDirect-1; i++)
	printf("%d ", dataSectors[i]);
      printf("\nFile contents:\n");
      for (i = k = 0; i < NumDirect-1; i++) {
	synchDisk->ReadSector(dataSectors[i], data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
	    if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
		printf("%c", data[j]);
            else
		printf("\\%x", (unsigned char)data[j]);
	}
        printf("\n");
      }
      indirect->FetchFrom(dataSectors[NumDirect-1]);
      indirect->Print();
    }
    delete [] data;
}

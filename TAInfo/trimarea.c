#include <stdio.h>

#include "trimarea.h"
#include "hashing.h"

//2 partition types (see trimarea.h)
//TA img is 2MB / TA_PARTITION_MAX_SIZE = 16
static struct TAPartitionHdr* headers[2][16] = { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } };

static struct TAUnitHdr* getNextTAUnit(struct TAUnitHdr* hdr)
{
	int unitLen;
	if(hdr == NULL)
		return NULL;
	//rare case if hdr is actually a TAPartitionHdr
	if(hdr->unitNumber == TA_MAGIC)
		return (struct TAUnitHdr*)((unsigned char*)hdr + sizeof(struct TAPartitionHdr));

	//reached end
	if(hdr->magic == 0xFFFFFFFF)
		return NULL;
	if(hdr->magic != TA_MAGIC)
	{
		//fprintf(stderr, "Unknown magic %d in TAUnit %d\n", hdr->magic, hdr->unitNumber);
		return NULL;
	}

	//aligned to 4 bytes
	unitLen = hdr->length;
	if(unitLen % 4)
		unitLen = unitLen - (unitLen % 4) + 4;
	return (struct TAUnitHdr*)((unsigned char*)hdr + sizeof(struct TAUnitHdr) + unitLen);
}

unsigned int getTAPartitionPartSize(struct TAPartitionHdr* hdr)
{
	unsigned char blocks = hdr->numblocks;
	unsigned int size;
	if(blocks == (unsigned char)0xFF)
		return TA_PARTITION_MAX_SIZE;

	size = (unsigned int)(blocks + 1) * TA_PARTITION_BLOCK_SIZE;
	return (size >= TA_PARTITION_MAX_SIZE) ? TA_PARTITION_MAX_SIZE : size;
}

int getPartitionFromUnit(struct TAUnitHdr* hdr)
{
	int partition;
	int part;
	if(hdr == NULL)
		return -1;

	for(partition = TRIMAREA_PARTITION_TRIM; partition < TRIMAREA_PARTITION_ENDMARKER; partition++)
	{
		for(part = 0;part < 16;part++)
		{
			if(headers[partition - 1][part] == NULL)
				break;
			if((unsigned int)hdr - ((unsigned int)headers[partition - 1][part]) < TA_PARTITION_MAX_SIZE)
				return partition;
		}
	}

	return -1;
}

unsigned int calcTAPartitionHash(struct TAPartitionHdr* hdr)
{
	unsigned int psize = getTAPartitionPartSize(hdr) - sizeof(hdr->magic) - sizeof(hdr->hash);
	return CalcAdler32(((unsigned char*)&hdr->hash + sizeof(hdr->hash)), psize);
}

int getTAUnitCount(struct TAPartitionHdr* hdr)
{
	int ucount = 0;
	struct TAUnitHdr* uhdr = (struct TAUnitHdr*)hdr;

	if(hdr == NULL)
		return -1;
	//is this enough or should we loop over the whole partition part?
	//do we also need to check the 'unknown' field in the hdr?
	while(uhdr = getNextTAUnit(uhdr))
		ucount++;

	return ucount;
}

int getTAPartitionPartCount(int partitionType)
{
	int pcount = 0;
	switch(partitionType)
	{
	case TRIMAREA_PARTITION_TRIM:
	case TRIMAREA_PARTITION_MISC:
		while(headers[partitionType-1][pcount] != NULL)
			pcount++;
		return pcount;

	default:
		fprintf(stderr, "Unknown partition type: %d\n", partitionType);
		return -1;
	}
}

struct TAPartitionHdr* getTAPartitionHeader(int partitionType, int partcount)
{
	switch(partitionType)
	{
	case TRIMAREA_PARTITION_TRIM:
	case TRIMAREA_PARTITION_MISC:
		if(partcount > 15)
			return NULL;
		return headers[partitionType-1][partcount];

	default:
		fprintf(stderr, "Unknown partition type: %d\n", partitionType);
		return NULL;
	}
}

struct TAUnitHdr* findTAUnit(unsigned int unitNum, int partition)
{
	struct TAUnitHdr* hdr = NULL;
	int curPartition = (partition == -1 ? TRIMAREA_PARTITION_TRIM : partition);

	do
	{
		int partCount = getTAPartitionPartCount(curPartition);
		for( ; partCount--; )
		{
			struct TAPartitionHdr* phdr = getTAPartitionHeader(curPartition, partCount);
			hdr = getNextTAUnit((struct TAUnitHdr*)phdr);
			while(hdr != NULL)
			{
				if(hdr->unitNumber == unitNum)
				{
					//printf("found in part: %d\n", curPartition);
					return hdr;
				}
				hdr = getNextTAUnit(hdr);
			}
		}
		curPartition++;
	} while(partition == -1 && curPartition < TRIMAREA_PARTITION_ENDMARKER);


	return NULL;
}

int ParseTAImage(unsigned char* ptr, unsigned int len)
{
	struct TAPartitionHdr* curhdr = (struct TAPartitionHdr*)ptr;
	if(len != 2097152) //2MB
	{
		fprintf(stderr, "Unexpected file size: %d\n", len);
		return 1;
	}

	for( ; len; curhdr = (struct TAPartitionHdr*)((char*)curhdr + TA_PARTITION_MAX_SIZE), len -= TA_PARTITION_MAX_SIZE)
	{
		if(curhdr->magic != TA_MAGIC)
			continue;
		if(curhdr->unknown != curhdr->partitionnumber)
		{
			fprintf(stderr, "Unexpected partitionNumber: %d / %d\n", curhdr->unknown, curhdr->partitionnumber);
			continue;
		}

		headers[curhdr->partitionnumber - 1][curhdr->partnumber] = curhdr;
	}

	return 0;
}
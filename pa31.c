#define ALIGNMENT 8 // must be a power of 2
#define ALIGNED(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1)) //http://moss.cs.iit.edu/cs351/slides/slides-malloc.pdf
#define rdtsc(x)      __asm__ __volatile__("rdtsc \n\t" : "=A" (*(x)))
#define DEFAULT_MEM_SIZE 1<<20

typedef char* addrs_t; //char pointer is 8 bytes
typedef void* any_t;

// prototypes below
void Init(size_t);
addrs_t Malloc(size_t);
void Free(addrs_t);
addrs_t Put(any_t, size_t);
void Get(any_t, addrs_t, size_t);
void heapChecker(void);
void PrintAddrs(void);
int test_stability(int, unsigned long*, unsigned long*);
void test_ff(void);


static addrs_t basePointer;//starting address of our heap space
static addrs_t curPointer;// current end address of allotted memory
static size_t memSize;

/*static variables needed for heapChecker */
static long int mallocCount = 0; //variable to count the number of malloc requests
static long int freeCount = 0; //variable to count the number of free requests
static long int reqfailCount; //variable to count the failed requests
static long int rawTotalAllocated; //variable to count the raw total memory requested
static long int paddedTotalAllocated; //variable to count the padded total allocated
static long int allocatedBlocks = 0;
static unsigned long tot_alloc_time;
static unsigned long tot_free_time;

static long int freeBlocks;
static long int rawFreeBytes;



void main(int argc, char **argv){
	 int i, n;
	 char s[80];
	 char data[80];
	 int mem_size = DEFAULT_MEM_SIZE;

	 if (argc>2){
	 	fprintf(stderr,"Usage %s [memory area size in bytes]\n",argv[0]);
	 	exit(1);
	 }
	 
	 else if(argc==2)
	 	mem_size = atoi(argv[1]);

	 Init(mem_size);

  	 int err = 0;
  // Round 1 - 2 consequtive allocations should be allocated after one another
 	
  	 test_ff();
  	 printf("\n");
	 PrintAddrs();
	 printf("\n");
  	 int numIterations = 1000000;
  	// Initialize the heap
  	 PrintAddrs();
  	 printf("\n");
 	 heapChecker();
 	 printf("\n\n");
	 test_stability(numIterations,&tot_alloc_time,&tot_free_time);
	 heapChecker();
}	

 

void PrintAddrs(void){
	printf("Base Pointer is %p\n",basePointer);
	printf("CurPointer is %p\n",curPointer);

}


void Init(size_t size){
	/*
	use the system malloc() routine (new in C++) only to allocate size bytes for the initial 
	memory area, M1. baseptr is the starting address of M1. 
	*/
	/* add other initializations as needed */
	basePointer = (addrs_t) malloc (size);//baseptr; //set the static basePointer variable to track the virtual address to the start of the heap
	curPointer = basePointer + 4; // set the curPointer to be the start of the list.7
	*basePointer = (unsigned int) size; // set the initial header to be the size of the entire thing.
	memSize = size;	 // set the static memsize variable to track when the heap is full.
	rawFreeBytes = memSize-4; //subtract 4 from memSize in order to account for the 4 bytes included in the header.
	freeBlocks = 1;
}

addrs_t Malloc (size_t size){
	/* implement your own memory allocation routine here.
	This should allocate the first contiguous size bytes available
	in M1. Since some machine architectures are 64-bit, it should be safe to allocate 
	space starting at the first address divisible by 8. Hence align all addresses on
	8-byte boundaries.

	If enough space exists, allocate space and return the base address of the memory. 
	If insufficient space exists, return NULL.
	*/
	mallocCount++;
	unsigned int alignedSize = ALIGNED(size); //align size by 8 - originally had size_t
	rawTotalAllocated+=alignedSize;
	paddedTotalAllocated+=(alignedSize+8);

	if (alignedSize > memSize)
	{
		reqfailCount++;
		return NULL;
	}

	addrs_t memBlock;
	addrs_t searchPtr = basePointer + 4;
	while ((searchPtr != curPointer) && ((*searchPtr & 1) || ( (*searchPtr & -2) < alignedSize))){ // traverse the entire memory heap until searchPtr meets cur pointer.
		searchPtr = searchPtr + (*(unsigned int *)searchPtr & -2) + 8;
	}
	if (searchPtr >= (basePointer + memSize))
	{ //if searchPtr ends up being the end of the heap
		reqfailCount++;
		return NULL;
	} 

	rawFreeBytes-=alignedSize;


	if ((searchPtr == curPointer)){ // first call of Malloc or extending from the end of the allocated block.
		memBlock = curPointer; // set the memBlock return address to be the address of the start of the heap
		*(unsigned int*)memBlock = (unsigned int) alignedSize + 1; //set the first 4 bytes of memBlock to be the size word. ** Add 1 to size to denote that it is an allocted block
		*(unsigned int*)(memBlock + alignedSize + 4) = (unsigned int) alignedSize + 1; //set the footer of the block to also be the size.
		curPointer = memBlock + (*(unsigned int *)memBlock & -2) + 8; // set the curPointer to be the byte following the allocated block (accounting for the 4 byte footer)
		allocatedBlocks++;
		return memBlock + 4;
	}
	//otherwise searchPointer is an internal block and needs to be potentially split
	unsigned int oldSize = *(unsigned int *) searchPtr & -2; //type cast to be a 4 byte word
	*(unsigned int *)searchPtr = alignedSize | 1; // marks that it is now an allocated block.
	*(searchPtr + alignedSize + 4) = alignedSize | 1;

	if (alignedSize == oldSize){
		allocatedBlocks++;
		freeBlocks--;

	}

	if (alignedSize < oldSize){
		unsigned int sizeDif = oldSize - alignedSize;	
		*(searchPtr + alignedSize + 8) = sizeDif; //set the rest of the un-allocated internal block to have the new size that it needs.
		*(searchPtr + alignedSize + 12 + sizeDif) = sizeDif; //set the footer of the split block to hold the size.		
		allocatedBlocks++;
		//freeBlocks++;
	}



	return searchPtr + 4;
}

void Free(addrs_t addr){

	freeCount++;

	addrs_t footer, header;
	header = (addr - 4);
	size_t size = (*(unsigned int *) header) & ~0x7;
	rawTotalAllocated -= size;
	rawFreeBytes+=size;
	paddedTotalAllocated -= (size+8);
	freeBlocks++;
	footer = (header + size+4);
	(*(unsigned int *)header) = (size & -2);
	(*(unsigned int*)footer) = (size & -2);
	int colnext = 0;
	addrs_t next, nxthdr, nxtftr;
	next = (header + size + 8);
	if (next < curPointer)
	{
		next += 4;
		nxthdr = (next - 4);
		size_t nextsize = (*(unsigned int *) nxthdr)& ~0x7;
		nxtftr = (next + nextsize);

		//Checks to see if next needs to be coalesced
		if( !(*(unsigned int *)nxthdr & 1))
		{
			size += nextsize+8;
			(*(unsigned int *)header) = (size & -2);
			(*(unsigned int *)nxtftr) = (size & -2);
			colnext = 1;
			freeBlocks--; //one less freeblock since two blocks will be coalesced 
		}		
	}

	else 
	{
		curPointer = header;		
	}
	

	if (addr-4 != basePointer + 4){
		addrs_t prev, prvhdr, prvftr;
		prvftr = (header - 4);
		size_t prevsize = (*(unsigned int *) prvftr)& ~0x7;
		prev = (header - prevsize - 4);
		prvhdr = (prev - 4);
	
		//Checks to see if block before needs to be coalesced
		if (!(*(unsigned int *) prvhdr & 1)){
			if (curPointer == header){
				curPointer = prvhdr; 
			}
			freeBlocks--; //one more less freeblock since the prior block is coalesced as well.
			size+=prevsize+8;		
			(*(unsigned int *)prvhdr)=   (size & -2);
		}

			if (colnext)
			{
				(*(unsigned int *)nxtftr) =   (size & -2);
			}
			else{
				(*(unsigned int *)footer)=   (size & -2);
			}	
	}
	
	if (curPointer == basePointer+4){
		freeBlocks = 1;
	}

	allocatedBlocks--;
}


addrs_t Put(any_t data, size_t size){
	/*allocate size bytes from M1 using Malloc(). Copy size bytes of data into Malloc'd memory.
	You can assume data is a storage area outside M1. Return starting address of data in Malloc'd memory.
	*/
	addrs_t baseAddress = Malloc(size); // returns starting address of the header of the block so must put data at this address +4 (int)
	if (baseAddress == NULL){
		return NULL;
	}
	
	memcpy(( (void*) (baseAddress)),data,size);
	return baseAddress;
    
}

void Get(any_t return_data, addrs_t addr, size_t size){
	/* copy size bytes from addr in the memory area, M1, to data address.
	As with Put(), you can assume data is a storage area outside M1. De-allocate size
	bytes of memory starting from addr using Free().
	*/
	
	*((unsigned int * )return_data) =  *((unsigned int *)addr);
	
	size_t cursize = (*(unsigned int *) (addr - 4)) & ~0x7;
	addrs_t next = (addr + cursize + 4);
	Free(addr);
	while (size > cursize && next < curPointer){
		size -= cursize;
		*((unsigned int * )return_data) +=  *((unsigned int *)next+ 4);
		cursize = (*(unsigned int *) (next)) & ~0x7;
		Free(next + 4);
		next = (next + cursize + 8);
	}
	

}

void test_ff(void){
	addrs_t v1;
  	addrs_t v2;
  	addrs_t v3;
    addrs_t v4;
  	v1 = Malloc(8);
    v2 = Malloc(4);
  	
  	if ((size_t)(v1) >= (size_t)(v2))
    	printf("not FF\n");
  	if (((size_t)(v1) & (8-1)) || ((size_t)(v2) & (8-1)))
    	printf("not aligned\n");
    //Round 2 - New allocation should be placed in a free block at top if fits
 	Free(v1);
 	v3 = Malloc(64);
  	v4 = Malloc(5);
  	if ((size_t)(v4) != (size_t)(v1) ||(size_t)(v3) < (size_t)(v2))
  		printf("not aligned\n");
  	if (((size_t)(v3) & (8-1)) || ((size_t)(v4) & (8-1)))
    		 printf("not aligned\n");
    //Round 3 - Correct merge
    Free(v4);
    Free(v2);
 	v4 = Malloc(10);
  	if ((size_t)(v4) != (size_t)(v1))
    		 printf("not ff\n");
    //Round 4 - Correct Merge 2
 	Free(v4);
  	Free(v3);

 	v4 = Malloc(256);
  	if ((size_t)(v4) != (size_t)(v1))
    	printf("not ff\n");
	Free(v4);
}

int test_stability(int numIterations, unsigned long* tot_alloc_time, unsigned long* tot_free_time){
  int i, n, res = 0;
  char s[80];
  addrs_t addr1;
  addrs_t addr2;
  char data[80];
  char data2[80];

  unsigned long start, finish;
  *tot_alloc_time = 0;
  *tot_free_time = 0;
  for (i = 0; i < numIterations; i++) {
    n = sprintf (s, "String 1, the current count is %d\n", i);
    rdtsc(&start);
    addr1 = Put(s, n+1);
    rdtsc(&finish);
    *tot_alloc_time += finish - start;
    addr2 = Put(s, n+1);
    // Check for heap overflow
    if (!addr1 || !addr2){
      printf("Out of memory.\n");
      break;
    }
    // Check aligment
    if (((uint64_t)addr1 & (8-1)) || ((uint64_t)addr2 & (8-1)))
      printf("Error not aligned.\n");
    // Check for data consistency
    rdtsc(&start);
    Get((any_t)data2, addr2, n+1);
    rdtsc(&finish);
    *tot_free_time += finish - start;
    if (!strcmp(data,data2))
	    printf("Inconsistent data.\n");
     Get((any_t)data2, addr1, n+1);
    if (!strcmp(data,data2))
	    printf("Inconsistent data.\n");
  }
  return res;
}


/* heapChecker() makes use of static variables that are altered within varied areas of program execution in order to assess programs efficiency*/
void heapChecker(){ 

	printf("Number of allocated blocks: %ld\n",allocatedBlocks); //

	printf("Number of free blocks: %ld\n",freeBlocks); //FREE BLOCKS discounting padding bytes

	printf("Raw total number of bytes allocated: %ld\n",rawTotalAllocated); //RAW TOTAL ALLOCATED which is the actual total bytes requested

	printf("Padded total number of bytes allocated: %ld\n",paddedTotalAllocated); //PADDED TOTAL ALLOCATED which is the total bytes requested plus internally fragmented blocks wasted due to padding/alignment

	printf("Raw total number of bytes free: %ld\n",rawFreeBytes); //RAW TOTAL FREE

	printf("Aligned total number of bytes free: %ld\n",memSize-paddedTotalAllocated); //ALIGNED TOTAL FREE which is sizeof(M1) minus the padded total number of bytes allocated. 

	printf("Total number of Malloc requests: %ld\n",mallocCount); //TOTAL MALLOC requests

	printf("Total number of Free requests: %ld\n",freeCount); //TOTAL FREE REQUESTS

	printf("Total number of request failures: %ld\n",reqfailCount); //TOTAL which were unable to satisfy the allocation or de-allocation requests

	printf("Average clock cycles for a Malloc request: %ld\n",tot_alloc_time); //tot_alloc_time and below is allocated based on different program calls.

	printf("Average clock cycles for a Free request: %ld\n",tot_free_time);

	printf("Total clock cycles for all requests: %ld\n",tot_free_time+tot_alloc_time);


}

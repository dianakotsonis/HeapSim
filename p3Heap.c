////////////////////////////////////////////////////////////////////////////////
// Main File:        p3Heap.c
// This File:        p3Heap.c
// Other Files:      p3Heap.o, p3Heap.h, Makefile, libheap.so
// Semester:         CS 354 Lecture 01      SPRING 2024
// Grade Group:      gg01  (See canvas.wisc.edu/groups for your gg#)
// Instructor:       deppeler
// 
// Author:           Diana Kotsonis
// Email:            dakotsonis@wisc.edu
// CS Login:         kotsonis
//
/////////////////////////// SEARCH LOG //////////////////////////////// 
// Online sources: do not rely web searches to solve your problems, 
// but if you do search for help on a topic, include Query and URLs here.
// IF YOU FIND CODED SOLUTIONS, IT IS ACADEMIC MISCONDUCT TO USE THEM
//                               (even for "just for reference")
// Date:   Query:                      URL:
// --------------------------------------------------------------------- 
// N/A
// 
// 
// 
// 
// AI chats: save a transcript.  No need to submit, just have available 
// if asked.
/////////////////////////// COMMIT LOG  ////////////////////////////// 
//  Date and label your work sessions, include date and brief description
//  Date:   Commit Message: 
//  -------------------------------------------------------------------
// 2/28     Read through all of the instructions, came up with pseudocode
// 2/29     Wrote a balloc that worked for test101 and test102
// 3/1      Fixed a bug that got all tests to work, added comments
// 3/2      Read over all code and instructions to ensure everything looked
//          correct. Added footer and p-bit correction
// 3/4      Read over my notes and instructions again, ensure commenting, along
//          along with implementation is correct. Submitted for p3A
// 3/5      Started P3B implementation of bfree() by reading instructions and
//          adding comments of what I plan to do
// 3/7      Fixed bugs in balloc and bfree, got my part B tests to pass
// 3/9      Started implementing immediate coalescing (part C/D), got those 
//          tests to pass
// 3/14     Read through instructions and code, added comments/style, tested
//          further to confirm there were no bugs. Submitted
///////////////////////// OTHER SOURCES OF HELP ////////////////////////////// 
// Persons:          Identify persons by name, relationship to you, and email.
//                   Describe in detail the the ideas and help they provided.
// Date:   Name (email):   Helped with: (brief description)
// ---------------------------------------------------------------------------
// N/A
//////////////////////////// 80 columns wide ///////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Copyright 2020-2024 Deb Deppeler based on work by Jim Skrentny
// Posting or sharing this file is prohibited, including any changes.
// Used by permission SPRING 2024, CS354-deppeler
//
/////////////////////////////////////////////////////////////////////////////

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include "p3Heap.h"

/*
 * This structure serves as the header for each allocated and free block.
 * It also serves as the footer for each free block.
 */
typedef struct blockHeader {           

    /*
     * 1) The size of each heap block must be a multiple of 8
     * 2) heap blocks have blockHeaders that contain size and status bits
     * 3) free heap block contain a footer, but we can use the blockHeader 
     *.
     * All heap blocks have a blockHeader with size and status
     * Free heap blocks have a blockHeader as its footer with size only
     *
     * Status is stored using the two least significant bits.
     *   Bit0 => least significant bit, last bit
     *   Bit0 == 0 => free block
     *   Bit0 == 1 => allocated block
     *
     *   Bit1 => second last bit 
     *   Bit1 == 0 => previous block is free
     *   Bit1 == 1 => previous block is allocated
     * 
     * Start Heap: 
     *  The blockHeader for the first block of the heap is after skip 4 bytes.
     *  This ensures alignment requirements can be met.
     * 
     * End Mark: 
     *  The end of the available memory is indicated using a size_status of 1.
     * 
     * Examples:
     * 
     * 1. Allocated block of size 24 bytes:
     *    Allocated Block Header:
     *      If the previous block is free      p-bit=0 size_status would be 25
     *      If the previous block is allocated p-bit=1 size_status would be 27
     * 
     * 2. Free block of size 24 bytes:
     *    Free Block Header:
     *      If the previous block is free      p-bit=0 size_status would be 24
     *      If the previous block is allocated p-bit=1 size_status would be 26
     *    Free Block Footer:
     *      size_status should be 24
     */
    int size_status;

} blockHeader;         

/* Global variable - DO NOT CHANGE NAME or TYPE. 
 * It must point to the first block in the heap and is set by init_heap()
 * i.e., the block at the lowest address.
 */
blockHeader *heap_start = NULL;     

/* Size of heap allocation padded to round to nearest page size.
 */
int alloc_size;

/*
 * Additional global variables may be added as needed below
 * add global variables needed by your function
 */




/* 
 * Function for allocating 'size' bytes of heap memory.
 * Argument size: requested size for the payload
 * Returns address of allocated block (payload) on success.
 * Returns NULL on failure.
 *
 * This function must:
 * - Check size - Return NULL if size < 1 
 * - Determine block size rounding up to a multiple of 8 
 *   and possibly adding padding as a result.
 *
 * - Use BEST-FIT PLACEMENT POLICY to chose a free block
 *
 * - If the BEST-FIT block that is found is exact size match
 *   - 1. Update all heap blocks as needed for any affected blocks
 *   - 2. Return the address of the allocated block payload
 *
 * - If the BEST-FIT block that is found is large enough to split 
 *   - 1. SPLIT the free block into two valid heap blocks:
 *         1. an allocated block
 *         2. a free block
 *         NOTE: both blocks must meet heap block requirements 
 *       - Update all heap block header(s) and footer(s) 
 *              as needed for any affected blocks.
 *   - 2. Return the address of the allocated block payload
 *
 *   Return if NULL unable to find and allocate block for required size
 *
 * Note: payload address that is returned is NOT the address of the
 *       block header.  It is the address of the start of the 
 *       available memory for the requesterr.
 *
 * Tips: Be careful with pointer arithmetic and scale factors.
 */
void* balloc(int size) {     
    // Confirm that the desired size is at least 1. If not, return NULL
    if (size < 1) {
        return NULL;
    }
    // Initialize a void pointer that will store the address of allocated block
    // on success, otherwise it will remain NULL
    void* pointerToReturn = NULL;

    // Obtain the correct allocation size by adding header, payload, and
    // padding, if required
    int allocSize = size + 4;
    if (allocSize % 8 != 0) {
        allocSize = allocSize + (8 - (allocSize % 8));
    }

    // If there isn't a free block exactly equal to the allocSize, store a
    // pointer to the free block that is greater than allocSize. 
    void* pointerGreaterThan = NULL;
    int x = 1;
    // Store the current location when traversing through the heap block
    void* currentPoint = heap_start;

    while (x == 1) {
        // Cast the current location pointer to an int, extract the header value
        int* headerPoint = (int *) currentPoint;
        int headerVal = (int) *headerPoint;
        // If the headerVal is 1, we have reached the end of the heap block.
        if (headerVal == 1) {
            x = 0;
            break;
        }
        // If the last bit is 1, the block is allocated, so jump to next block
        if ((headerVal & 1) == 1) {
            int valToJump = headerVal - 1;
            // If the second last bit is set, subtract two to get the correct 
            // jump amount
            if ((headerVal & 2) == 2) {
                valToJump = valToJump - 2;
            }
            currentPoint = (void *) currentPoint + valToJump;
            continue;
        }
        // Check the p bit value. If it is set, subtract two to get the
        // correct size of the block
        if ((headerVal & 2) == 2) {
            headerVal = headerVal - 2;
        }
        // If the current size and allocated size are equal, save a pointer to
        // the start of the block. Update the header to show it's allocated
        if (headerVal == allocSize) {
            pointerToReturn = (void *) currentPoint + sizeof(blockHeader);
            // Update the a bit in the header
            *headerPoint = (*headerPoint) + 1;
            x = 0;
            break;
        }
        // If the headerVal is greater than alloc size, store a pointer to this
        // header if it is a better fit
        if (headerVal > allocSize) {
            if (pointerGreaterThan != NULL) {
                int* greaterThanInt = (int *) pointerGreaterThan;
                // currentSize represents the current size of the block that is
                // saved as pointerGreaterThan
                int currentSize = (int) *greaterThanInt;
                if ((currentSize & 2) == 2) {
                    currentSize = currentSize - 2;
                }
                // If the current free block is smaller than the already stored
                // free block, change pointerGreaterThan's reference to the
                // current free block
                if (headerVal < currentSize) {
                    pointerGreaterThan = currentPoint;
                }
                currentPoint = (void *) currentPoint + headerVal;
            }
            else {
                pointerGreaterThan = currentPoint;
                currentPoint = (void *) currentPoint + headerVal;
            }
            continue;
        } 
        // Update the current point by jumping to the next head and trying again
        currentPoint = (void *) currentPoint + headerVal;
    }

    // If pointerToGreaterThan = something and pointerToReturn is uninitialized,
    // then must split the free block
    if (pointerGreaterThan != NULL && pointerToReturn == NULL ) {
        // Cast the pointerGreaterThan to an int, obtain the size of the free
        // block from it
        int* greaterThanInt = (int *) pointerGreaterThan;
        int totalSize = (int) *greaterThanInt;

        // Check the second last bit. If it is set, subtract two to get the
        // correct size of the new block
        int totalSizeCurrent = totalSize;
        if ((totalSize & 2) == 2) {
            totalSizeCurrent = totalSize - 2;
        }
        int freeSize = totalSizeCurrent - allocSize;

        // Update pointerToReturn to point to the start of the returned block
        pointerToReturn = (void *) pointerGreaterThan + sizeof(blockHeader);
        // Change the headers a bit to 1 to show it is allocated
        *greaterThanInt = allocSize + 1;
        if ((totalSize & 2) == 2) {
            *greaterThanInt = (*greaterThanInt) + 2;
        }

        // Jump to new free block header and add its size 
        void* freePointer = (void *) pointerGreaterThan + allocSize;
        int* freePointerInt = (int *) freePointer;
        *freePointerInt = freeSize; 

        //  Update the free block's footer to store the block size
        void* footerPointer = (void *) freePointer + freeSize - 4;
        int* footerPointerInt = (int *) footerPointer;
        *footerPointerInt = freeSize;
    }

    // If pointerToReturn is still null, a free block big enough wasn't found
    // So just return NULL
    if (pointerToReturn == NULL) {
        return pointerToReturn;
    }

    // Jump to the next block, set its p bit to 1 to show that the previous
    // block is allocated
    void* pointerNextBlock = (void *) pointerToReturn - 4 + allocSize;
    int* intNextBlock = (int *) pointerNextBlock;
    int nextBlockVal = (int) *intNextBlock;

    // If the next block is the end of the heap, don't update the value
    if (nextBlockVal == 1) {
        return pointerToReturn;
    }
    // Otherwise, update the p bit by adding 2
    *intNextBlock = nextBlockVal + 2;

    return pointerToReturn;
} 

/* 
 * Function for freeing up a previously allocated block.
 * Argument ptr: address of the block to be freed up.
 * Returns 0 on success.
 * Returns -1 on failure.
 * This function should:
 * - Return -1 if ptr is NULL.
 * - Return -1 if ptr is not a multiple of 8.
 * - Return -1 if ptr is outside of the heap space.
 * - Return -1 if ptr block is already freed.
 * - Update header(s) and footer as needed.
 *
 * If free results in two or more adjacent free blocks,
 * they will be immediately coalesced into one larger free block.
 * so free blocks require a footer (blockHeader works) to store the size
 *
 * TIP: work on getting immediate coalescing to work after your code 
 *      can pass the tests in partA and partB of tests/ directory.
 *      Submit code that passes partA and partB to Canvas before continuing.
 */                    
int bfree(void *ptr) {    
    // If pointer is null, return -1
    if (ptr == NULL) {
        return -1;
    }
    int* ptrInt = (int *) ptr; 

    // If pointer is not a mulitple of 8, return -1
    if (((unsigned int) ptrInt & 7) != 0) {
        return -1;
    }
    // If the pointer is outside of the heap space, return -1
    void* endOfHeap = (void *) heap_start + alloc_size;
    int* endIntPtr = (int *) endOfHeap;
    if (ptrInt > endIntPtr) {
        return -1;
    }

    // If the pointer is already freed, return -1
    void* ptrHeader = (void *) ptr - 4;
    int* ptrHeaderInt = (int *) ptrHeader;
    if ((*ptrHeaderInt & 1) != 1) {
        return -1;
    }
    // Update the header's a bit to 0
    *ptrHeaderInt = (*ptrHeaderInt) - 1;

    // Set footer value to the block's size
    int size = *ptrHeaderInt;
    if ((size & 2) == 2) {
        size = size - 2;
    }
    void* footerPtr = (void *) ptrHeaderInt + size - 4;
    int* footerPtrInt = (int *) footerPtr;
    *footerPtrInt = size;

    // Create a reference to the next block in the heap
    void* nextBlock = (void *) footerPtr + sizeof(blockHeader);
    int* intNextBlock = (int *) nextBlock;

    // Update the next block's p bits, as long as it isn't the end of the heap
    if (((*intNextBlock) & 2) == 2) {
        *intNextBlock = (*intNextBlock) - 2;
    }

    // Check the next block's a bit (unless the end of the heap). If it is
    // free, update the nextBlock size
    int nextBlockSize = 0;
    if (((*intNextBlock) & 1) != 1) {
        if ((*intNextBlock) == 1) {
            nextBlockSize = 0;
        }
        else {
            nextBlockSize = *intNextBlock;
        }
    }
    int prevBlockSize = 0;

    // The newFreeBlock will eventually represent the final free block after
    // implementing immediate coalescing
    void* newFreeBlock = (void *) ptrHeader;

    // Check the p bit of the original free block header. If 0, the previous
    // block is free, so obtain its size from its footer and save this value
    if (((*ptrHeaderInt) & 2) != 2) {
        void* prevBlockFooter = (void *) ptrHeader - 4;
        int* prevBlockFooterInt = (int *) prevBlockFooter;
        if (((*prevBlockFooterInt) & 2) == 2) {
            prevBlockSize = *prevBlockFooterInt - 2;
        }
        else {
            prevBlockSize = *prevBlockFooterInt;
        }
        // Update the pointer of the new free block header to the start of the
        // previous block
        newFreeBlock = (void *) ptrHeader - prevBlockSize;
    }

    // Update the size of the newFree Block using current, previous, and next
    // block sizes
    int* newFreeBlockInt = (int *) newFreeBlock;
    if (((*newFreeBlockInt) & 2) == 2) {
        *newFreeBlockInt = prevBlockSize + size + nextBlockSize + 2;
    }
    else {
        *newFreeBlockInt = prevBlockSize + size + nextBlockSize;
    }

    // Update the footer of this new block by first attaining the size from
    // header
    int newSize = 0;
    if (((*newFreeBlockInt) & 2) == 2) {
        newSize = *newFreeBlockInt - 2;
    }
    else {
        newSize = *newFreeBlockInt;
    }

    // newBlockFooter represents the footer value of the freed block after
    // immediate coalescing
    void* newBlockFooter = (void *) newFreeBlockInt + newSize - 4;
    int* newBlockFooterInt = (int *) newBlockFooter;
    *newBlockFooterInt = newSize;

    // Update the next block's p bit to 0 (unless the end of the heap)
    void* newNextBlock = (void *) newBlockFooter + sizeof(blockHeader);
    int* newNextBlockInt = (int *) newNextBlock;
    if (*newNextBlockInt == 1) {
        ptr = NULL;
        return 0;
    }
    if (((*newNextBlockInt) & 2) == 2) {
        *newNextBlockInt = (*newNextBlockInt) - 2;
    }
    ptr = NULL;
    return 0;
}


/* 
 * Initializes the memory allocator.
 * Called ONLY once by a program.
 * Argument sizeOfRegion: the size of the heap space to be allocated.
 * Returns 0 on success.
 * Returns -1 on failure.
 */                    
int init_heap(int sizeOfRegion) {    

    static int allocated_once = 0; //prevent multiple myInit calls

    int   pagesize; // page size
    int   padsize;  // size of padding when heap size not a multiple of page size
    void* mmap_ptr; // pointer to memory mapped area
    int   fd;

    blockHeader* end_mark;

    if (0 != allocated_once) {
        fprintf(stderr, 
                "Error:mem.c: InitHeap has allocated space during a previous call\n");
        return -1;
    }

    if (sizeOfRegion <= 0) {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize from O.S. 
    pagesize = getpagesize();

    // Calculate padsize as the padding required to round up sizeOfRegion 
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    alloc_size = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd) {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    mmap_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == mmap_ptr) {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }

    allocated_once = 1;

    // for double word alignment and end mark
    alloc_size -= 8;

    // Initially there is only one big free block in the heap.
    // Skip first 4 bytes for double word alignment requirement.
    heap_start = (blockHeader*) mmap_ptr + 1;

    // Set the end mark
    end_mark = (blockHeader*)((void*)heap_start + alloc_size);
    end_mark->size_status = 1;

    // Set size in header
    heap_start->size_status = alloc_size;

    // Set p-bit as allocated in header
    // note a-bit left at 0 for free
    heap_start->size_status += 2;

    // Set the footer
    blockHeader *footer = (blockHeader*) ((void*)heap_start + alloc_size - 4);
    footer->size_status = alloc_size;

    return 0;
} 

/* STUDENTS MAY EDIT THIS FUNCTION, but do not change function header.
 * TIP: review this implementation to see one way to traverse through
 *      the blocks in the heap.
 *
 * Can be used for DEBUGGING to help you visualize your heap structure.
 * It traverses heap blocks and prints info about each block found.
 * 
 * Prints out a list of all the blocks including this information:
 * No.      : serial number of the block 
 * Status   : free/used (allocated)
 * Prev     : status of previous block free/used (allocated)
 * t_Begin  : address of the first byte in the block (where the header starts) 
 * t_End    : address of the last byte in the block 
 * t_Size   : size of the block as stored in the block header
 */                     
void disp_heap() {     

    int    counter;
    char   status[6];
    char   p_status[6];
    char * t_begin = NULL;
    char * t_end   = NULL;
    int    t_size;

    blockHeader *current = heap_start;
    counter = 1;

    int used_size =  0;
    int free_size =  0;
    int is_used   = -1;

    fprintf(stdout, 
            "********************************** HEAP: Block List ****************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\n");
    fprintf(stdout, 
            "--------------------------------------------------------------------------------\n");

    while (current->size_status != 1) {
        t_begin = (char*)current;
        t_size = current->size_status;

        if (t_size & 1) {
            // LSB = 1 => used block
            strcpy(status, "alloc");
            is_used = 1;
            t_size = t_size - 1;
        } else {
            strcpy(status, "FREE ");
            is_used = 0;
        }

        if (t_size & 2) {
            strcpy(p_status, "alloc");
            t_size = t_size - 2;
        } else {
            strcpy(p_status, "FREE ");
        }

        if (is_used) 
            used_size += t_size;
        else 
            free_size += t_size;

        t_end = t_begin + t_size - 1;

        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%4i\n", counter, status, 
                p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size);

        current = (blockHeader*)((char*)current + t_size);
        counter = counter + 1;
    }

    fprintf(stdout, 
            "--------------------------------------------------------------------------------\n");
    fprintf(stdout, 
            "********************************************************************************\n");
    fprintf(stdout, "Total used size = %4d\n", used_size);
    fprintf(stdout, "Total free size = %4d\n", free_size);
    fprintf(stdout, "Total size      = %4d\n", used_size + free_size);
    fprintf(stdout, 
            "********************************************************************************\n");
    fflush(stdout);

    return;  
} 


//		p3Heap.c (SP24)                     
                                       

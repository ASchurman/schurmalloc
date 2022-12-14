#pragma once
#include <cstddef>
#include <vector>

// Simulates malloc and free by using a fixed block of memory as if it's the entire heap.
class Schurmalloc
{
public:
    Schurmalloc() = delete;

    // mem is the block of memory in which malloc will be simulated.
    // size is the size of that block in bytes.
    Schurmalloc(void* mem, std::size_t size);

    void* malloc(std::size_t size);
    void* realloc(void* ptr, std::size_t newSize);
    void free(void* ptr);

    // Run a suite of tests on Schurmalloc
    static void test();
    
private:
    // The block of memory in which we simulate malloc. Creator of Schurmalloc is responsible
    // for freeing this memory!
    void* memory;
    std::size_t memorySize;

    // Each block of memory in the free list has a header and a footer. From these headers, we
    // form a linked list of free blocks.
    // size: The size of the block following this header. Doesn't include the size of the footer.
    // free: Indicates whether this block is free and reservable
    // prev: Forms the linked list of free blocks. NULL if this is the first block in the free list.
    // next: Forms the linked list of free blocks. NULL if this is the last block in the free list.
    struct Header
    {
        std::size_t size;
        bool free;
        Header* prev;
        Header* next;
    };

    // size: The size of the block preceding this footer. Doesn't include the size of the header.
    // free: Indicates whether the preceding block is free and reservable
    struct Footer
    {
        std::size_t size;
        bool free;
    };
    
    // The linked list of free blocks
    Header* freeList;

    // Is this the first or the last block in the whole block of available memory?
    bool isFirstBlock(Header* header);
    bool isLastBlock(Footer* footer);

    static Footer* getFooter(Header* header);
    static Header* getHeader(void* payload);
    static Header* getHeader(Footer* footer);
    static Header* getPrevHeader(Header* header);
    static Footer* getPrevFooter(Header* header);
    static Header* getNextHeader(Header* header);
    static Header* getNextHeader(Footer* footer);
    static void* getPayload(Header* header);

    // Splits a block into 2 blocks, the first of which is size bytes.
    // If block isn't large enough to split (or if the remainder is deemed too small), then don't split.
    // If the block is free, then keeps both blocks in the free list; if the block is reserved, then
    //   inserts the residual block into the free list.
    // Returns whether we split the block.
    bool trySplitBlock(Header* block, std::size_t size);

    // Coalesces 2 free blocks and returns a pointer to the coalesced block.
    // Both blocks must be free. And blocks must be adjacent, with first coming
    // before second.
    Header* coalesce(Header* first, Header* second);

    // Reserves a free block by marking it as reserved and removing it from the free list
    void reserve(Header* block);

    //////////////////////////////
    /////// Test resources ///////
    //////////////////////////////

    // Test block. Used to represent memory in tests.
    struct TB
    {
        bool free;
        size_t size;
        TB(bool f, size_t s) : free(f), size(s) {}
    };

    void verifyMemory(const std::vector<TB>& expectedMemory, const std::vector<size_t>& expectedFreeList);
};

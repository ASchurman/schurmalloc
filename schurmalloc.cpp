#include "schurmalloc.h"
#include <iostream>
#include <cstddef>
#include <cassert>

Schurmalloc::Footer* Schurmalloc::getFooter(Header* header)
{
    return reinterpret_cast<Footer*>(reinterpret_cast<char*>(header) + sizeof(Header) + header->size);
}

Schurmalloc::Header* Schurmalloc::getHeader(void* payload)
{
    return reinterpret_cast<Header*>(static_cast<char*>(payload) - sizeof(Header));
}

Schurmalloc::Header* Schurmalloc::getPrevHeader(Header* header)
{
    Footer* prevFooter = getPrevFooter(header);
    return reinterpret_cast<Header*>(reinterpret_cast<char*>(prevFooter) - prevFooter->size - sizeof(Header));
}

Schurmalloc::Footer* Schurmalloc::getPrevFooter(Header* header)
{
    return reinterpret_cast<Footer*>(reinterpret_cast<char*>(header) - sizeof(Footer));
}

Schurmalloc::Header* Schurmalloc::getNextHeader(Header* header)
{
    return getNextHeader(getFooter(header));
}

Schurmalloc::Header* Schurmalloc::getNextHeader(Footer* footer)
{
    return reinterpret_cast<Header*>(reinterpret_cast<char*>(footer) + sizeof(Footer));
}

bool Schurmalloc::isFirstBlock(Header* header)
{
    return reinterpret_cast<void*>(header) == memory;
}

bool Schurmalloc::isLastBlock(Footer* footer)
{
    return reinterpret_cast<char*>(footer) + sizeof(Footer) >= static_cast<char*>(memory) + memorySize;
}

Schurmalloc::Schurmalloc(void* mem, std::size_t size)
{
    memory = mem;
    memorySize = size;

    // Initially, all of memory is a free block.
    // Initialize the header
    freeList = static_cast<Header*>(memory);
    freeList->size = size - sizeof(Header) - sizeof(Footer);
    freeList->free = true;
    freeList->prev = NULL;
    freeList->next = NULL;

    // Find and initialize the footer
    Footer* footer = getFooter(freeList);
    footer->size = freeList->size;
    footer->free = true;

    // Sanity checks...
    assert(freeList->free);
    assert(getFooter(freeList)->free);
    assert(freeList->prev == NULL);
    assert(freeList->next == NULL);
    assert(freeList->size == size - sizeof(Header) - sizeof(Footer));
    assert(freeList->size == getFooter(freeList)->size);
}

void* Schurmalloc::malloc(std::size_t size)
{
    if (size == 0 || size >= memorySize)
    {
        return NULL;
    }

    // Find the first free block that's large enough
    Header* block = freeList;
    for (;;)
    {
        if (block->size >= size)
        {
            // We found the block to reserve!
            // First, create a new free block out of the remainder, if there's enough remainder.
            trySplitBlock(block, size);

            // Then, connect prev block to next block.
            if (block->prev)
            {
                block->prev->next = block->next;
            }
            else
            {
                // There is no previous block. That means that we're reserving the head of the
                // free list. The free list needs to get a new head.
                freeList = block->next;
            }

            if (block->next)
            {
                block->next->prev = block->prev;
            }

            // Finally, mark block as reserved and return a pointer to the addr after the header
            block->free = getFooter(block)->free = false;
            block->prev = block->next = NULL;
            return static_cast<void*>(reinterpret_cast<char*>(block) + sizeof(Header));
        }
        else if (block->next)
        {
            block = block->next;
        }
        else
        {
            return NULL;
        }
    }
}

/*
void* Schurmalloc::realloc(void* ptr, std::size_t newSize)
{
    if (newSize == 0)
    {
        free(ptr);
        return NULL;
    }

    // TODO Implement realloc
}
*/

void Schurmalloc::free(void* ptr)
{
    Header* block = getHeader(ptr);
    Footer* footer = getFooter(block);

    // Sanity checks...
    assert(!block->free);
    assert(!footer->free);
    assert(block->size == footer->size);

    block->free = true;
    footer->free = true;

    // Insert this new free block into the free list
    if (freeList == NULL) // block is the only free block
    {
        std::cout << "Freelist is NULL, so making block-to-free the only free block.\n";
        freeList = block;
        block->prev = NULL;
        block->next = NULL;
        return; // quit before coalescing, because we know there's no other blocks to coalesce with
    }
    else if (block < freeList) // block is to be the first element in the free list
    {
        std::cout << "Block to free is prior to other free blocks.\n";
        block->prev = NULL;
        block->next = freeList;
        freeList->prev = block;
        freeList = block;
    }
    else // traverse the free list to find where block belongs
    {
        std::cout << "Traversing freelist to find where to put block-to-free.\n";
        Header* prev = NULL;
        Header* next = freeList;
        while (next && block > next)
        {
            prev = next;
            next = next->next;
        }
        prev->next = block;
        block->prev = prev;
        block->next = next;
        if (next) next->prev = block;
    }

    // See if we can coalesce with the previous block
    if (!isFirstBlock(block) && // If this is the first block, don't look at prev block!
        getPrevFooter(block)->free)
    {
        std::cout << "Coalescing newly freed block with previous block...\n";
        block = coalesce(getPrevHeader(block), block);
    }

    // See if we can coalesce with the next block
    if (!isLastBlock(footer) &&
        getNextHeader(footer)->free)
    {
        std::cout << "Coalescing newly freed block with subsequent block...\n";
        block = coalesce(block, getNextHeader(block));
    }
}

bool Schurmalloc::trySplitBlock(Header* block, std::size_t size)
{
    if (block->size <= size || !block->free)
    {
        return false;
    }

    // Sanity checks...
    assert(block->size == getFooter(block)->size);
    assert(block->free);
    assert(getFooter(block)->free);

    /* When the block is split, this will be what happens:
    |------------------------------------------------------------------------|
    | Header |                     block->size bytes                | Footer | ==> 
    |------------------------------------------------------------------------|

    |------------------------------------------------------------------------|
    | Header | size bytes | NewFooter | NewHeader | remainder bytes | Footer |
    |------------------------------------------------------------------------| */

    // Knowing that this is how the block will split, we can calculate the remainder:
    std::size_t remainder = block->size - size - sizeof(Footer) - sizeof(Header);

    if (remainder == 0)
    {
        // The residual block isn't large enough for both metadata and data.
        return false;
    }

    Header* remainderHeader = reinterpret_cast<Header*>(reinterpret_cast<char*>(block) +
                                                        sizeof(Header) +
                                                        size +
                                                        sizeof(Footer));
    Footer* remainderFooter = getFooter(block);
    Footer* thisFooter = reinterpret_cast<Footer*>(reinterpret_cast<char*>(block) +
                                                   sizeof(Header) +
                                                   size);
    remainderHeader->size = remainder;
    remainderFooter->size = remainder;
    remainderHeader->free = true;
    remainderFooter->free = true;
    remainderHeader->prev = block;
    remainderHeader->next = block->next;
    
    block->size = size;
    thisFooter->size = size;
    thisFooter->free = true;
    block->next = remainderHeader;

    // Sanity checks...
    assert(block->size == getFooter(block)->size);
    assert(block->free);
    assert(getFooter(block)->free);
    assert(block->next == getNextHeader(block));
    assert(getNextHeader(block)->prev == block);
    assert(getNextHeader(block)->size == getFooter(getNextHeader(block))->size);
    assert(getNextHeader(block)->free);
    assert(getFooter(getNextHeader(block))->free);

    return true;
}

Schurmalloc::Header* Schurmalloc::coalesce(Header* first, Header* second)
{
    // Sanity checks...
    Footer* firstFooter = getFooter(first);
    Footer* secondFooter = getFooter(second);
    assert(first->free);
    assert(firstFooter->free);
    assert(second->free);
    assert(secondFooter->free);
    assert(first->size == firstFooter->size);
    assert(second->size == secondFooter->size);
    assert(first->next == second);
    assert(second->prev == first);
    assert(getNextHeader(first) == second);

    /* When the blocks are coalesced, this is what will happen:
    |---------------------------------------------------------|
    | Header1 | Block1 | Footer1 | Header2 | Block2 | Footer2 | ==>
    |---------------------------------------------------------|

    |---------------------------------------------------------|
    | Header1 |                Block                | Footer2 |
    |---------------------------------------------------------| */

    first->size += sizeof(Footer) + sizeof(Header) + second->size;
    secondFooter->size = first->size;
    first->next = second->next;

    if (first->next)
    {
        first->next->prev = first;
    }

    // Sanity checks...
    assert(first->size == getFooter(first)->size);
    assert(first->free);
    assert(getFooter(first)->free);

    return first;
}

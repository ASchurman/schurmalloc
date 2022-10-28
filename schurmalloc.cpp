#include "schurmalloc.h"
#include <iostream>
#include <cstddef>
#include <cstring>
#include <cassert>

void* Schurmalloc::getPayload(Header* header)
{
    return static_cast<void*>(reinterpret_cast<char*>(header) + sizeof(Header));
}

Schurmalloc::Footer* Schurmalloc::getFooter(Header* header)
{
    return reinterpret_cast<Footer*>(reinterpret_cast<char*>(header) + sizeof(Header) + header->size);
}

Schurmalloc::Header* Schurmalloc::getHeader(void* payload)
{
    return reinterpret_cast<Header*>(static_cast<char*>(payload) - sizeof(Header));
}

Schurmalloc::Header* Schurmalloc::getHeader(Footer* footer)
{
    return reinterpret_cast<Header*>(reinterpret_cast<char*>(footer) - footer->size - sizeof(Header));
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
    if (size == 0 || size >= memorySize || freeList == NULL)
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

            // Then, reserve the block...
            reserve(block);

            // Finally, return a pointer to the address after the header
            return getPayload(block);
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

void Schurmalloc::reserve(Header* block)
{
    assert(block);
    assert(block->free);
    assert(getFooter(block)->free);
    assert(block->size == getFooter(block)->size);

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

    block->free = false;
    getFooter(block)->free = false;
    block->prev = NULL;
    block->next = NULL;
}

void* Schurmalloc::realloc(void* ptr, std::size_t newSize)
{
    if (ptr == NULL)
    {
        return malloc(newSize);
    }
    if (newSize == 0)
    {
        this->free(ptr);
        return NULL;
    }

    Header* block = getHeader(ptr);

    if (newSize < block->size)
    {
        std::cout << "\trealloc: Shrinking block...\n";
        // Split block into a reserved block of newSize and a free block of size - newSize.
        // The reserved block stays where it is, so don't touch ptr.
        bool split = trySplitBlock(block, newSize);

        // Sanity checks...
        if (split)
        {
            std::cout << "\trealloc: We've split the block in order to shrink\n";
            assert(block->size == newSize);
        }
        else
        {
            std::cout << "\trealloc: Actually didn't shrink block, because the shrink is < metadata size\n";
            // If split didn't happen, it's because diff between size and newSize isn't enough for metadata.
            assert(block->size - newSize <= sizeof(Header) + sizeof(Footer));
            assert(block->size > newSize);
        }
    }
    else if (newSize > block->size)
    {
        std::cout << "\trealloc: Expanding block...\n";
        if (!isLastBlock(getFooter(block)) && // Can we expand block into the following block?
            getNextHeader(block)->free &&
            block->size + sizeof(Footer) + sizeof(Header) + getNextHeader(block)->size >= newSize)
        {
            // Expand block into following block
            std::cout << "\trealloc: Expanding block into the following block...\n";
            Header* freeHeader = getNextHeader(block);
            Footer* freeFooter = getFooter(freeHeader);
            assert(freeHeader->free);
            assert(freeFooter->free);
            assert(freeHeader->size == freeFooter->size);
            size_t availableSize = block->size + sizeof(Footer) + sizeof(Header) + freeHeader->size;
            if (availableSize == newSize)
            {
                // We have exactly enough bytes in the subsequent block to hold our realloc'd data.
                // This means that the subsequent block needs to be taken out of the free list, and
                // the footer of the subsequent block will become our new footer.
                reserve(freeHeader);
                block->size = newSize;
                assert(getFooter(block) == freeFooter);
                freeFooter->size = newSize;
            }
            else
            {
                // The subsequent block will be shrunk, but it will maintain its place in the free list.
                assert(availableSize > newSize);
                size_t remainderSize = freeHeader->size - (newSize - block->size);
                Header* prev = freeHeader->prev;
                Header* next = freeHeader->next;
                freeFooter->size = remainderSize;
                assert(getHeader(freeFooter) == reinterpret_cast<Header*>(reinterpret_cast<char*>(freeHeader) + (newSize - block->size)));
                getHeader(freeFooter)->size = remainderSize;
                getHeader(freeFooter)->free = true;
                getHeader(freeFooter)->prev = prev;
                getHeader(freeFooter)->next = next;
                if (getHeader(freeFooter)->prev == NULL)
                {
                    freeList = getHeader(freeFooter);
                }
                
                block->size = newSize;
                assert(getFooter(block) == getPrevFooter(getHeader(freeFooter)));
                getFooter(block)->size = newSize;
                getFooter(block)->free = false;
            }
        }
        else if (!isFirstBlock(block) && // Can we expand block into the preceding block?
                 getPrevFooter(block)->free &&
                 getPrevFooter(block)->size + sizeof(Footer) + sizeof(Header) + block->size >= newSize)
        {
            // Expand block into preceding block
            std::cout << "\trealloc: Expanding block into preceding block...\n";
            Header* prevHeader = getPrevHeader(block);
            Footer* prevFooter = getFooter(prevHeader);
            Footer* blockFooter = getFooter(block);
            assert(prevHeader->free);
            assert(prevFooter->free);
            assert(prevHeader->size == prevFooter->size);
            assert(!blockFooter->free);
            assert(block->size == blockFooter->size);
            size_t availableSize = prevHeader->size + sizeof(Footer) + sizeof(Header) + block->size;
            if (availableSize == newSize)
            {
                // We have exactly enough bytes in the preceding block to hold our realloc'd data.
                // This means that the preceding block needs to be taken out of the free list, and
                // the header of the preceding block becomes our new header.
                reserve(prevHeader);
                prevHeader->size = newSize;
                assert(getFooter(prevHeader) == blockFooter);
                getFooter(prevHeader)->size = newSize;
                std::memmove(getPayload(prevHeader), ptr, newSize);
                ptr = getPayload(prevHeader);
            }
            else
            {
                // The preceding block will be shrunk, but it will maintain its place in the free list.
                assert(availableSize > newSize);
                size_t remainderSize = prevHeader->size - (newSize - block->size);

                prevHeader->size = remainderSize;
                getFooter(prevHeader)->size = remainderSize;
                getFooter(prevHeader)->free = true;
                
                blockFooter->size = newSize;
                getHeader(blockFooter)->size = newSize;
                getHeader(blockFooter)->free = false;
                getHeader(blockFooter)->prev = NULL;
                getHeader(blockFooter)->next = NULL;

                std::memmove(getPayload(getHeader(blockFooter)), ptr, newSize);
                ptr = getPayload(getHeader(blockFooter));
            }
        }
        else
        {
            // We need to try to malloc a new block, since we can't expand in place.
            std::cout << "\trealloc: Can't expand in place. Need to malloc new block\n";
            ptr = this->malloc(newSize);
            if (ptr)
            {
                // Copy the old block's data into the new one.
                // (We can use memcpy because we know for sure the blocks don't overlap.)
                std::memcpy(ptr, getPayload(block), block->size);

                // Now that we're done with the old block, free it.
                free(getPayload(block));
            }
        }
    }
    // Do nothing to the block if newSize == block->size

    // Sanity checks...
    if (ptr)
    {
        Header* b = getHeader(ptr);
        assert(b->size >= newSize);
        assert(b->size == getFooter(b)->size);
        assert(!b->free);
        assert(!getFooter(b)->free);
    }
    return ptr;
}

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
        std::cout << "\tfree: Freelist is NULL, so making block-to-free the only free block.\n";
        freeList = block;
        block->prev = NULL;
        block->next = NULL;
        return; // quit before coalescing, because we know there's no other blocks to coalesce with
    }
    else if (block < freeList) // block is to be the first element in the free list
    {
        std::cout << "\tfree: Block to free is prior to other free blocks.\n";
        block->prev = NULL;
        block->next = freeList;
        freeList->prev = block;
        freeList = block;
    }
    else // traverse the free list to find where block belongs
    {
        std::cout << "\tfree: Traversing freelist to find where to put block-to-free.\n";
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
        std::cout << "\tfree: Coalescing newly freed block with previous block...\n";
        block = coalesce(getPrevHeader(block), block);
    }

    // See if we can coalesce with the next block
    if (!isLastBlock(footer) &&
        getNextHeader(footer)->free)
    {
        std::cout << "\tfree: Coalescing newly freed block with subsequent block...\n";
        block = coalesce(block, getNextHeader(block));
    }
}

bool Schurmalloc::trySplitBlock(Header* block, std::size_t size)
{
    // Sanity checks...
    assert(block);
    assert(block->size == getFooter(block)->size);
    assert(block->free == getFooter(block)->free);

    /* When the block is split, this will be what happens:
    |------------------------------------------------------------------------|
    | Header |                     block->size bytes                | Footer | ==> 
    |------------------------------------------------------------------------|

    |------------------------------------------------------------------------|
    | Header | size bytes | NewFooter | NewHeader | remainder bytes | Footer |
    |------------------------------------------------------------------------| */

    // Make sure there's enough remainder bytes for us to be able to split.
    // (Do this check before calculating remainder, because size_t is unsigned, which could lead to
    // some underflow funkiness if, e.g., block->size == size.)
    if (size + sizeof(Footer) + sizeof(Header) >= block->size)
    {
        // The remainder is <= 0. The residual block isn't large enough for both metadata and data.
        return false;
    }
    std::size_t remainder = block->size - size - sizeof(Footer) - sizeof(Header);

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
    block->size = size;
    thisFooter->size = size;
    thisFooter->free = block->free;

    if (block->free)
    {
        remainderHeader->free = true;
        remainderFooter->free = true;
        remainderHeader->prev = block;
        remainderHeader->next = block->next;
        block->next = remainderHeader;

        // Sanity checks...
        assert(block->next == getNextHeader(block));
        assert(getNextHeader(block)->prev == block);
    }
    else // Block isn't free, but the remainder will be made free
    {
        remainderHeader->free = false;
        remainderFooter->free = false;
        remainderHeader->prev = NULL;
        remainderHeader->next = NULL;
        this->free(getPayload(remainderHeader));
    }

    // Sanity checks...
    assert(block->size == getFooter(block)->size);
    assert(block->free == getFooter(block)->free);
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

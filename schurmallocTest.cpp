#include "schurmalloc.h"
#include <iostream>
#include <cstddef>
#include <cassert>
#include <cstdlib>

using std::cout;
using std::vector;

// Run a suite of tests. A fair bit of sanity checking happens in assertions in Schurmalloc.
void Schurmalloc::test()
{
    const size_t h = sizeof(Schurmalloc::Header);
    const size_t f = sizeof(Schurmalloc::Footer);
    const size_t meta = h+f;
    cout << "Header size: " << h << "\n";
    cout << "Footer size: " << f << "\n\n";

    // These tests, being quick and dirty, aren't portable...
    assert(meta == 24);

    size_t m = 1000; // total size of memory
    size_t rem = m-meta;  // how much remaining memory at the end of the block can be allocated
    cout << "Allocating " << m << " bytes and passing it to Schurmalloc...\n";
    void* memory = std::malloc(m);
    Schurmalloc schurm(memory, m);
    cout << "Schurmalloc is initialized.\n\n";

    // A container to hold the memory blocks we allocate
    vector<void*> mem;
    void* ptr; void* ptr2;

    schurm.verifyMemory(vector<TB> {TB(true, rem)},
                        vector<size_t> {rem});

    cout << "malloc(0)\n";
    ptr = schurm.malloc(0);
    assert(!ptr);
    schurm.verifyMemory(vector<TB> {TB(true, rem)},
                        vector<size_t> {rem});

    cout << "malloc(300)\n";
    ptr = schurm.malloc(300);
    assert(ptr);
    mem.push_back(ptr);
    rem -= 300+meta;
    schurm.verifyMemory(vector<TB> {TB(false, 300), TB(true, rem)},
                        vector<size_t> {rem});
    cout << "malloc(300)\n";
    ptr = schurm.malloc(300);
    assert(ptr);
    mem.push_back(ptr);
    rem -= 300+meta;
    schurm.verifyMemory(vector<TB> {TB(false, 300), TB(false, 300), TB(true, rem)},
                        vector<size_t> {rem});
    cout << "malloc(350) (should fail)\n";
    assert(schurm.malloc(350) == NULL);

    cout << "free second block\n";
    schurm.free(mem.back());
    mem.pop_back();
    rem += 300+meta;
    schurm.verifyMemory(vector<TB> {TB(false, 300), TB(true, rem)},
                        vector<size_t> {rem});
    cout << "free remaining block\n";
    schurm.free(mem.back());
    mem.pop_back();
    rem += 300+meta;
    assert(rem == m - meta);
    schurm.verifyMemory(vector<TB> {TB(true, rem)},
                        vector<size_t> {rem});
    
    cout << "malloc(1000) should fail\n";
    assert(schurm.malloc(1000) == NULL);
    schurm.verifyMemory(vector<TB> {TB(true, rem)},
                        vector<size_t> {rem});
    
    cout << "malloc just more than available (should fail)\n";
    assert(schurm.malloc(rem+1) == NULL);
    schurm.verifyMemory(vector<TB> {TB(true, rem)},
                        vector<size_t> {rem});
    
    cout << "malloc exactly what's available\n";
    ptr = schurm.malloc(rem);
    assert(ptr);
    mem.push_back(ptr);
    schurm.verifyMemory(vector<TB> {TB(false, rem)},
                        vector<size_t> {});
    cout << "now free it\n";
    schurm.free(mem.back());
    mem.pop_back();
    rem = m-meta;
    schurm.verifyMemory(vector<TB> {TB(true, rem)},
                        vector<size_t> {rem});
    
    cout << "malloc(10)\n";
    ptr = schurm.malloc(10);
    assert(ptr);
    mem.push_back(ptr);
    rem -= 10 + meta;
    schurm.verifyMemory(vector<TB> {TB(false, 10), TB(true, rem)},
                        vector<size_t> {rem});
    cout << "malloc(11)\n";
    ptr = schurm.malloc(11);
    assert(ptr);
    mem.push_back(ptr);
    rem -= 11 + meta;
    schurm.verifyMemory(vector<TB> {TB(false, 10), TB(false, 11), TB(true, rem)},
                        vector<size_t> {rem});
    cout << "malloc(12)\n";
    ptr = schurm.malloc(12);
    assert(ptr);
    mem.push_back(ptr);
    rem -= 12 + meta;
    schurm.verifyMemory(vector<TB> {TB(false,10), TB(false,11), TB(false,12), TB(true,rem)},
                        vector<size_t> {rem});
    cout << "malloc(13)\n";
    ptr = schurm.malloc(13);
    assert(ptr);
    mem.push_back(ptr);
    rem -= 13 + meta;
    schurm.verifyMemory(vector<TB> {TB(false,10), TB(false,11), TB(false,12), TB(false,13), TB(true,rem)},
                        vector<size_t> {rem});
    
    cout << "free the 11 block\n";
    schurm.free(mem.at(1));
    schurm.verifyMemory(vector<TB> {TB(false,10), TB(true,11), TB(false,12), TB(false,13), TB(true,rem)},
                        vector<size_t> {11, rem});

    cout << "malloc(11) again to make sure it fits back into the 11 slot\n";
    ptr = schurm.malloc(11);
    assert(ptr);
    assert(ptr == mem.at(1));
    schurm.verifyMemory(vector<TB> {TB(false,10), TB(false,11), TB(false,12), TB(false,13), TB(true,rem)},
                        vector<size_t> {rem});
    cout << "free the 11 block again\n";
    schurm.free(ptr);
    schurm.verifyMemory(vector<TB> {TB(false,10), TB(true,11), TB(false,12), TB(false,13), TB(true,rem)},
                        vector<size_t> {11, rem});
    
    cout << "malloc(14) to make sure it doesn't go into the 11 slot\n";
    ptr = schurm.malloc(14);
    assert(ptr);
    assert(ptr > mem.at(1));
    rem -= 14 + meta;
    schurm.verifyMemory(vector<TB> {TB(false,10), TB(true,11), TB(false,12), TB(false,13), TB(false,14), TB(true,rem)},
                        vector<size_t> {11, rem});
    cout << "free the 14 block to continue the coalescing test\n";
    schurm.free(ptr);
    rem += 14 + meta;
    schurm.verifyMemory(vector<TB> {TB(false,10), TB(true,11), TB(false,12), TB(false,13), TB(true,rem)},
                        vector<size_t> {11, rem});
    
    cout << "free the 13 block\n";
    schurm.free(mem.at(3));
    rem += 13 + meta;
    schurm.verifyMemory(vector<TB> {TB(false,10), TB(true,11), TB(false,12), TB(true,rem)},
                        vector<size_t> {11, rem});
    
    cout << "free the 12 block\n";
    schurm.free(mem.at(2));
    rem += 11 + 12 + 2*meta;
    schurm.verifyMemory(vector<TB> {TB(false,10), TB(true,rem)},
                        vector<size_t> {rem});
    
    cout << "free the 10 block\n";
    schurm.free(mem.at(0));
    rem += 10 + meta;
    assert(rem == m - meta);
    schurm.verifyMemory(vector<TB> {TB(true,rem)},
                        vector<size_t> {rem});

    mem.clear();

    cout << "\nrealloc(NULL, 255)\n";
    ptr = schurm.realloc(NULL, 255);
    assert(ptr);
    assert(ptr == static_cast<void*>(static_cast<char*>(memory) + h));
    rem -= 255 + meta;
    schurm.verifyMemory(vector<TB> {TB(false,255), TB(true,rem)},
                        vector<size_t> {rem});

    cout << "realloc(ptr, 355). Expand into subsequent block\n";
    ptr2 = schurm.realloc(ptr, 355);
    assert(ptr2 == ptr);
    rem -= 100;
    schurm.verifyMemory(vector<TB> {TB(false,355), TB(true,rem)},
                        vector<size_t> {rem});
    
    cout << "realloc(ptr, 255). Shrink block\n";
    ptr2 = schurm.realloc(ptr, 255);
    assert(ptr2 == ptr);
    rem += 100;
    schurm.verifyMemory(vector<TB> {TB(false,255), TB(true,rem)},
                        vector<size_t> {rem});
    
    cout << "malloc(200) to make a second block\n";
    mem.push_back(ptr);
    ptr = schurm.malloc(200);
    assert(ptr);
    mem.push_back(ptr);
    rem -= 200 + meta;
    schurm.verifyMemory(vector<TB> {TB(false,255), TB(false,200), TB(true,rem)},
                        vector<size_t> {rem});

    cout << "remalloc(block1, 350). It can't expand, so it'll move\n";
    unsigned char* c = static_cast<unsigned char*>(mem.at(0));
    for (unsigned char i = 0; i < 255; i++)
    {
        c[i] = i;
    }
    ptr = schurm.realloc(mem.at(0), 350);
    assert(ptr);
    assert(ptr > mem.at(0));
    mem.erase(mem.begin());
    mem.push_back(ptr);
    rem -= 350 + meta;
    schurm.verifyMemory(vector<TB> {TB(true,255), TB(false,200), TB(false,350), TB(true,rem)},
                        vector<size_t> {255, rem});
    c = static_cast<unsigned char*>(mem.at(1));
    for (unsigned char i = 0; i < 255; i++)
    {
        assert(c[i] == i);
    }

    cout << "remalloc(block2, 250). Expand into preceding block\n";
    c = static_cast<unsigned char*>(mem.at(0));
    for (unsigned char i = 0; i < 200; i++)
    {
        c[i] = i;
    }
    ptr = schurm.realloc(mem.at(0), 250);
    assert(ptr);
    assert(ptr < mem.at(0));
    mem[0] = ptr;
    schurm.verifyMemory(vector<TB> {TB(true,205), TB(false,250), TB(false,350), TB(true,rem)},
                        vector<size_t> {205, rem});
    c = static_cast<unsigned char*>(mem.at(0));
    for (unsigned char i = 0; i < 200; i++)
    {
        assert(c[i] == i);
    }

    cout << "realloc(block2, 0) to free it\n";
    ptr = schurm.realloc(mem.at(0), 0);
    assert(ptr == NULL);
    schurm.verifyMemory(vector<TB> {TB(true,479), TB(false,350), TB(true,rem)},
                        vector<size_t> {479, rem});

    cout << "realloc(block3, 0) to free it\n";
    ptr = schurm.realloc(mem.at(1), 0);
    assert(ptr == NULL);
    rem = m - meta;
    schurm.verifyMemory(vector<TB> {TB(true,rem)},
                        vector<size_t> {rem});

    mem.clear();
    cout << "\nmalloc(50) 4 times to set up for another realloc test\n";
    for (int i = 0; i < 4; i++) mem.push_back(schurm.malloc(50));
    rem -= 200 + 4*meta;
    schurm.verifyMemory(vector<TB> {TB(false,50), TB(false,50), TB(false,50), TB(false,50), TB(true,rem)},
                        vector<size_t> {rem});
    
    cout << "free blocks 0 and 2\n";
    schurm.free(mem.at(0));
    schurm.verifyMemory(vector<TB> {TB(true,50), TB(false,50), TB(false,50), TB(false,50), TB(true,rem)},
                        vector<size_t> {50, rem});
    schurm.free(mem.at(2));
    schurm.verifyMemory(vector<TB> {TB(true,50), TB(false,50), TB(true,50), TB(false,50), TB(true,rem)},
                        vector<size_t> {50, 50, rem});

    cout << "realloc(block1, 124) to swallow block 2 whole\n";
    ptr = schurm.realloc(mem.at(1), 124);
    assert(ptr == mem.at(1));
    schurm.verifyMemory(vector<TB> {TB(true,50), TB(false,124), TB(false,50), TB(true,rem)},
                        vector<size_t> {50, rem});

    cout << "realloc(block1, 198) to swallow block 0 whole\n";
    ptr = schurm.realloc(mem.at(1), 198);
    assert(ptr < mem.at(1));
    assert(ptr == mem.at(0));
    schurm.verifyMemory(vector<TB> {TB(false,198), TB(false,50), TB(true,rem)},
                        vector<size_t> {rem});

    cout << "free blocks 1 and 3 to clean up\n";
    schurm.free(ptr);
    schurm.verifyMemory(vector<TB> {TB(true,198), TB(false,50), TB(true,rem)},
                        vector<size_t> {198, rem});
    schurm.free(mem.at(3));
    rem = m - meta;
    schurm.verifyMemory(vector<TB> {TB(true,rem)},
                        vector<size_t> {rem});

    mem.clear();

    cout << "\nDone with Schurmalloc tests!\n";
    std::free(memory);
}

void Schurmalloc::verifyMemory(const vector<TB>& expMem, const vector<size_t>& expFreelist)
{
    // Verify memory...
    void* ptr = memory;
    int i = 0;
    while (static_cast<char*>(ptr) < static_cast<char*>(memory) + memorySize)
    {
        Header* header = static_cast<Header*>(ptr);
        assert(header);
        assert(header->free == expMem.at(i).free);
        assert(header->size == expMem.at(i).size);

        Footer* footer = getFooter(header);
        assert(footer);
        assert(footer->free == expMem.at(i).free);
        assert(footer->size == expMem.at(i).size);

        ptr = static_cast<void*>(getNextHeader(footer));
        i++;
    }
    assert(i == expMem.size());

    // Verify free list...
    Header* f = freeList;
    i = 0;
    if (f)
    {
        assert(f->prev == NULL);
    }
    while (f)
    {
        assert(f->size == expFreelist.at(i));
        if (i > 0)
        {
            assert(f->prev);
            assert(f->prev->size == expFreelist.at(i-1));
        }
        f = f->next;
        i++;
    }
    assert(i == expFreelist.size());
}

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

    size_t m = 100; // total size of memory
    size_t rem = m-meta;  // how much remaining memory at the end of the block can be allocated
    cout << "Allocating " << m << " bytes and passing it to Schurmalloc...\n";
    void* memory = std::malloc(m);
    Schurmalloc schurm(memory, m);
    cout << "Schurmalloc is initialized.\n\n";

    // A container to hold the memory blocks we allocate
    vector<void*> mem;
    void* ptr;

    cout << "malloc(10)\n";
    ptr = schurm.malloc(10);
    assert(ptr);
    mem.push_back(ptr);
    rem -= 10+meta;
    schurm.verifyMemory(vector<TB> {TB(false, 10), TB(true, rem)},
                        vector<size_t> {rem});
    cout << "malloc(10)\n";
    ptr = schurm.malloc(10);
    assert(ptr);
    mem.push_back(ptr);
    rem -= 10+meta;
    schurm.verifyMemory(vector<TB> {TB(false, 10), TB(false, 10), TB(true, rem)},
                        vector<size_t> {rem});
    cout << "malloc(10) (should fail)\n";
    assert(schurm.malloc(10) == NULL);

    cout << "free second block\n";
    schurm.free(mem.back());
    mem.pop_back();
    rem += 10+meta;
    schurm.verifyMemory(vector<TB> {TB(false, 10), TB(true, rem)},
                        vector<size_t> {rem});
    cout << "free remaining block\n";
    schurm.free(mem.back());
    mem.pop_back();
    rem += 10+meta;
    assert(rem == m - meta);
    schurm.verifyMemory(vector<TB> {TB(true, rem)},
                        vector<size_t> {rem});
    
    cout << "malloc(100) should fail\n";
    assert(schurm.malloc(100) == NULL);
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

    cout << "Done with Schurmalloc tests!\n";
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

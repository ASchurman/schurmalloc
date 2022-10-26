#include "schurmalloc.h"
#include <iostream>
#include <cassert>

using std::cout;

int main(int argc, char** argv)
{
    size_t memorySize = 4096;
    cout << "Allocating " << memorySize << " bytes and passing it to Schurmalloc...\n";
    void* memory = malloc(memorySize);
    Schurmalloc schurm(memory, memorySize);
    cout << "Schurmalloc is initialized.\n";

    size_t headerSize = sizeof(Schurmalloc::Header);
    size_t footerSize = sizeof(Schurmalloc::Footer);

    cout << "Header size: " << headerSize << "\n";
    cout << "Footer size: " << footerSize << "\n";

    // Do stuff with schurm
    cout << "Trying to malloc too much memory...\n";
    void* mem1 = schurm.malloc(memorySize);
    assert(mem1 == NULL);
    cout << "Trying to malloc(5)...\n";
    mem1 = schurm.malloc(5);
    assert(mem1);
    assert(mem1 == static_cast<char*>(memory) + headerSize);
    cout << "Trying to free...\n";
    schurm.free(mem1);

    cout << "Trying to malloc(5)...\n";
    mem1 = schurm.malloc(5);
    assert(mem1 == static_cast<char*>(memory) + headerSize);
    cout << "Again (mem2)\n";
    void* mem2 = schurm.malloc(5);
    assert(mem2 == static_cast<char*>(mem1) + 5 + footerSize + headerSize);
    cout << "Again (mem3)\n";
    void* mem3 = schurm.malloc(5);
    assert(mem3 == static_cast<char*>(mem2) + 5 + footerSize + headerSize);
    cout << "Again (mem4)\n";
    void* mem4 = schurm.malloc(5);
    assert(mem4 == static_cast<char*>(mem3) + 5 + footerSize + headerSize);

    cout << "Freeing mem2\n";
    schurm.free(mem2);
    cout << "Allocating into mem2's slot again\n";
    void* mem2Again = schurm.malloc(5);
    assert(mem2 == mem2Again);
    cout << "Freeing mem2Again\n";
    schurm.free(mem2Again);
    cout << "Allocating something too large for mem2's slot\n";
    void* mem2Large = schurm.malloc(10);
    assert(mem2Large == static_cast<char*>(mem4) + 5 + footerSize + headerSize);
    cout << "Freeing mem2Large\n";
    schurm.free(mem2Large);
    cout << "Freeing mem4\n";
    schurm.free(mem4);
    cout << "Freeing mem3\n";
    schurm.free(mem3);
    cout << "freeing mem1\n";
    schurm.free(mem1);

    cout << "Done with Schurmalloc!\n";
    free(memory);
    return 0;
}

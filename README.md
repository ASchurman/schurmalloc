Simulates an implementation of malloc for fun! This isn't a real implementation of malloc, since I don't use any system calls in my malloc. Instead, I initialize my malloc with a block of memory, and then that block is treated as the entire heap.

Currently, there are only a few extremely rudimentary and disorganized tests. As my leisure time permits, I plan to make more comprehensive tests.

## Compiling
The supplied makefile is for the Windows NMAKE utility. Run `nmake` to compile.

## Running
Run `schurmalloc.exe` from the command line in order to run a suite of tests.

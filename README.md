# Parallel Systems:Connected Components of an undirected graph

- serial.cpp – Sequential version
- openmp.cpp – OpenMP version
- opencilk.cpp – OpenCilk version
- pthreads.cpp – Pthreads version
- Makefile – Build rules for each implementation

I recommend running and testing everything in WSL or Linux, because the OpenCilk version requires clang++ and may not compile correctly on native Windows.

# Dependencies
Before compiling the project, the following software must be installed:

- make
- g++ (supports OpenMP and pthreads)
- clang++ (used for the OpenCilk version)
- Standard C++ libraries



CXX= g++
CXXCILK= clang++
CXXFLAGS= -O3
OMPFLAGS= -fopenmp 
PTHREADFLAGS= -pthread 
CILKFLAGS= -fopencilk

SERIAL = serial
OPENMP = openmp
OPENCILK = opencilk
PTHREADS = pthreads

SERIAL_SRC = serial.cpp
OPENMP_SRC = openmp.cpp
OPENCILK_SRC = opencilk.cpp
PTHREADS_SRC = pthreads.cpp

all: $(SERIAL) $(OPENMP) $(OPENCILK) $(PTHREADS)

$(SERIAL): $(SERIAL_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(OPENMP): $(OPENMP_SRC)
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) -o $@ $<

$(OPENCILK): $(OPENCILK_SRC)
	$(CXXCILK) $(CXXFLAGS) $(CILKFLAGS) -o $@ $<

$(PTHREADS): $(PTHREADS_SRC)
	$(CXX) $(CXXFLAGS) $(PTHREADFLAGS) -o $@ $<

clean:m
	rm -f $(SERIAL) $(OPENMP) $(OPENCILK) $(PTHREADS)

.PHONY: all clean



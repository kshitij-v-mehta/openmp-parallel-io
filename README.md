This is a prototype implementation of a parallel I/O API for multi-threaded programming models such as OpenMP.  
It was developed as part of a PhD thesis in 2013.

---

Pcollio is an API for performing Parallel I/O in multi-threaded applications using OpenMP.

#### Installation:
Type 'make' in the top-level directory to compile the library.

#### Example:  
The test/ directory contains an example that uses one of the library routines. Type 'make' in the test directory to compile the example. 
Note that running the example requires a configuration file 'fs.config' as given. 

#### Bugs:  
Some of the asynchronous and list I/O interfaces have not been tested.

#### Related publications:  
[1] "Specification and performance evaluation of parallel I/O interfaces for OpenMP", IWOMP 2012  
Kshitij Mehta, Edgar Gabriel, Barbara Chapman  
[https://doi.org/10.1007/978-3-642-30961-8_1](https://doi.org/10.1007/978-3-642-30961-8_1)

[2] "Multi-Threaded Parallel I/O for OpenMP Applications", International Journal of Parallel Programming, 2014  
Kshitij Mehta and Edgar Gabriel  
[https://doi.org/10.1007/s10766-014-0306-9](https://doi.org/10.1007/s10766-014-0306-9)  

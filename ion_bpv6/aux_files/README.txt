This directory contains all auxiliary files taken from ION 4.0.0 and modified a little bit.
These files maintain their original copyright and license.
You have to replace these files with the original ones contained in ION.

A little explanation about cgr.h, ipnfw.c and libcgr.c: at present these files contain some modifications made 
by University of Bologna but most (or all) of these modifications concern only 
ION's official CGR implementation where initially some Unibo-CGR feature was developed in an early stage.

The following relative paths ("." is ION's root directory) are the paths to the directory 
where the files should be replaced/inserted.

bpextensions.c:    ./bpv6/library/ext/
bpv6_Makefile:     ./bpv6/
cgr.h:             ./bpv6/library/
cgrfetch.c:        ./bpv6/utils/
configure.ac:      ./
ipnfw.c            ./bpv6/ipn/
libcgr.c:          ./bpv6/cgr/
Makefile.noBP:     ./

The Unibo-CGR's root directory must be inserted under ./bpv6/cgr/ (next to libcgr.c).

In ION 4.0.0, by default, dependency tracking in the Makefile is disabled. 
The result is that changes made in the header files (i.e. some Unibo-CGR's feature switched from on to off) will not be intercepted 
by the "make" command. We have therefore decided to use the solution proposed to us by Patricia Lindner 
and for this reason you fine in this directory also the following files: Makefile.noBP, bpv6_Makefile, configure.ac.
In particular, bpv7_Makefile contains the modifications to compile Unibo-CGR with the RGR and CGRR extensions.
So, you can compile (and install) with this sequence of commands: 
autoreconf -fi && ./configure --enable-bpv6 && make && make install
Note that "autoreconf -fi" is necessary the first time.

This directory contains a few auxiliary files taken from ION 4.0.0 and marginally modified to support Unibo-CGR.
These files maintain their original copyright and license.
They are intended to replace the homonimous original files contained in ION, thus they should be copyed in the following directories ("." refers to ION's root directory):

bpextensions.c:    ./bpv6/library/ext/
bpv6_Makefile:     ./bpv6/
cgr.h:             ./bpv6/library/
cgrfetch.c:        ./bpv6/utils/
configure.ac:      ./
ipnfw.c            ./bpv6/ipn/
libcgr.c:          ./bpv6/cgr/
Makefile.am:       ./
Makefile.noBP:     ./

The Unibo-CGR's root directory must be inserted under ./bpv6/cgr/ (next to libcgr.c).

In ION 4.0.0, by default, dependency tracking in the Makefile is disabled, which prevents the "make" command from
intercepting changes made in the header files (e.g. to switch on/off Unibo-CGR's optional features), which is inconvenient and error prone. 
Therefore, we have inserted here the solution provided to us by Patricia Lindner, of Ohio University, which consists of the following files: Makefile.am, Makefile.noBP, bpv6_Makefile, configure.ac. 
Note that bpv6_Makefile compiles Unibo-CGR with the RGR and CGRR extensions.

After having copyed the auxiliary files and the unibo-CGR directory, you can compile (and install) with this sequence of commands: 
autoreconf -fi && ./configure --enable-bpv6 && make && make install
Note that "autoreconf -fi" is necessary only the first time.

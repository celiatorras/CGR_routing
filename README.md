# Unibo-CGR

Modular implementation of CGR/SABR routing algorithm for DTN space networks. It consist of a core, independent of the Bundle Protocol implementation in use, and of multiple BP specific interfaces.  Compatible with both ION and DTN2.

Although it is fully compliant with CCSDS standard on SABR, it also contains a series of experimental features that can be optionally enabled or disabled. A comprehensive description can be found in:

C. Caini, G. M. De Cola, L. Persampieri, “Scheduled Aware Bundle Routing: Analysis and Enhancements”, accepted for publication on Wiley International Journal of Satellite Communications and Networking, Sept. 2020.

Unibo-CGR has been designed in a modular way, to be independent of the specific bundle protocol implementation in use. It consists of a core module, written in C, and multiple interfaces, one for each BP implementation supported. At present, we have developed two interfaces to ION (one for bpv6 and another for bpv7, as in ION-3.7.1 and 4.0.0), and one for DTN2 (DTNME, the NASA MSFC version).

Some advanced features of Unibo-CGR rely on two experimental bundle extensions, developed to this end: RGR (Record Geographical Route) for anti-loop mechanisms, and CGRR (CGR route), for MSR (Moderate Source Routing). At present these two extensions have been developed only for ION.

**Unibo-CGR Directory Structure**

    Unibo-CGR               _the root directory (README, etc.)_

        core                _core files independent of the BP implementation_
        
            bundles
            
            cgr
            
            contact_plan
            
                contacts
                 
                nodes
                
                ranges
                
            library
                
                log
                    
                list
                    
            library_from_ion
                    
                rbt
                    
                scalar
                    
            msr
                
            routes

        ion_bpv6            _interface interface code core for ION BP v6_
        
            interface       _interface code core for ION BP v6_

                utility_functions_from_ion

            extensions      _CGRR and RGR code (BP v6)_

            aux_files       _auxiliary files for building in ION BP v6 (ipnfw.c, etc.)_

        ion_bpv7            _the same as above, but for BP v7_

        dtn2

            interface        _interface code for DTN2_

            aux_files        _auxiliary files for building in DTN2 (Makefile, Bundle.Router.cc, etc.)_

        docs                 _doxygen html documentation_

Packages for a specific implementation will include only the interface(s) involved.

**Copyright**

Copyright (c) 2020, University of Bologna. Auxiliary files maintain their original copyright.

**License**

Unibo-CGR is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.  
Unibo-CGR is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.  
You should have received a copy of the GNU General Public License along with Unibo-CGR. If not, see <http://www.gnu.org/licenses/>.

Auxiliary files maintain their original licences.

**Additional information**

_Including Unibo-CGR in a pre-existent BP release._

Although Unibo-CGR has been designed to be as much independent as possible from the BP implementation to the modification of few files in pre-existing BP releases that do not include Unibo-CGR is obviously necessary. The files to be replaced or added outside the Unibo-CGR root are included in BP-specific aux_files directories.  
_Note (ION)_: After the download rename the root directory to "Unibo-CGR" and put it under (ION's root directory)/bpv*/cgr/ then read the files Unibo-CGR/ion_bpv*/aux_files/README.txt and Unibo-CGR/ion_bpv*/extensions/README.txt.  
_Note (DTN2)_: After the download rename the root directory to "uniboCGR" and put it under (DTN2's root directory)/servlib/routing/ then read the file uniboCGR/dtn2/aux_files/README.txt.

_Compilation Switches_

The exact behavior of UniboCGR depends on configuration switch settings. Researchers proficient in CGR routing can override defaults by editing the Unibo-CGR/core/config.h file. Some experimental features are not available on specific BP implementations.

_Use in ION_

The use of Unibo-CGR in ION does not differ from the use of previous implementation of CGR/SABR, a part experimental features that can be optionally enabled/disabled by overriding defaults (see above).

_Use in DTN2_

Contact plan: The use of Unibo-CGR requires that the contact plan be provided as an external file (contact_plan.txt, to be placed in the directory from which the dtnd daemon is launched) at dtnd daemon start-up. The syntax is exactly the same as that used in ION, including the possible bidirectional interpretation of single “range” instructions, for the sake of commonality. A simple example file is provided for user convenience.

Configuration file: The Unibo-cgr bundle router must be selected in the DTN2 configuration file (usually in /etc/dtn.conf). As CGR support only the “ipn” scheme, only bundles with an “ipn” destination will be processed by Unibo-CGR. Others will be processed by static routing. The generalized use and advertisement of ipn scheme also for DTN2 nodes is therefore recommended (compulsory in space by CCSDS rules). It requires the declaration of an “ipn” node and its advertising in TCPCL links. The advertising is necessary to let CGR know the ipn identity of the previous hop.

_DTN2 Limitations_

1. DTN2 does not support, at present, scheduled contacts. As a result, bundle enqueued by CGR to a neighbor are immediately sent if the link is active, independently of what reported in the contact plan. In other words, in DTN2, by contrast to ION, the contact plan is used only by CGR, not to enforce link activity.  
2. contacts cannot be updated/changed once the dtnd daemon has started. This limitation could be partially removed, should CGR be used in production on DTN2 nodes, by allowing subsequent reading of the contact plan file.  
3. As said before, at present DTN2 interface lacks RGR and CGRR. As a result, MSR and anti-loop mechanism cannot be enabled.  
4. The previous hop mechanism, based on the addition of a specific extension block to each bundle (see RFC 6259), seems to rely on a different mechanism in DTN2. This prevents ION nodes from knowing the EID of the previous hop in mixed environments, whenever the previous node is a DTN2 node (even if declared and advertised with an ipn EID). As a result, ping-pong events may occur. In tests we had to disable reverse contact to prevent them.

_Logs_

As Unibo-CGR was designed with research in mind, logs are one of the most distinctive features. Logs are organized in two layers. They are put in cgr_log directory, created at first launch. At the higher layer we have a file listing all CGR calls, with a brief summary of each call. At the lower layer, call-specific information, (extremely detailed, for CGR proficient researchers only), is provided in call-specific files.

**Credits**

Lorenzo Persampieri (main author), lorenzo.persampieri@studio.unibo.it

Giacomo Gori (DTN2 interface), giacomo.gori3@studio.unibo.it

Carlo Caini (supervisor), carlo.caini@unibo.it

Acknowledgments: Gian Marco De Cola, Federico Marchetti, Laura Mazzuca.
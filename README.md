# Unibo-CGR

Modular implementation of CGR/SABR routing algorithm for DTN space networks. It consist of a core, independent of the Bundle Protocol implementation in use, and of multiple BP specific interfaces.  Compatible with both ION and DTNME.

Although it is fully compliant with CCSDS standard on SABR, it also contains a series of experimental features that can be optionally enabled or disabled. A comprehensive description can be found in:

C. Caini, G. M. De Cola, L. Persampieri, “Schedule-Aware Bundle Routing: Analysis and Enhancements”, accepted for publication on Wiley International Journal of Satellite Communications and Networking, Sept. 2020. https://doi.org/10.1002/sat.1384

Unibo-CGR has been designed in a modular way, to be independent of the specific bundle protocol implementation in use. It consists of a core module, written in C, and multiple interfaces, one for each BP implementation supported. At present, we have developed two interfaces to ION (one for bpv6 and another for bpv7, as in ION-3.7.2 and 4.0.1), and one for DTNME (DTNME, the NASA MSFC version).

Some advanced features of Unibo-CGR rely on two experimental bundle extensions, developed to this end: RGR (Record Geographical Route) for anti-loop mechanisms and CGRR (CGR Route), for MSR (Moderate Source Routing). At present these two extensions have been developed only for ION.

#### Unibo-CGR Directory Structure

    Unibo-CGR               _the root directory (README, etc.)_

        core                _core files independent of the BP implementation_
        
            bundles         _implemnetation of internal bundle structures_
            
            cgr             _implementation of the 3 CGR phases_
            
            contact_plan    _implementation of the (internal to Unibo-CGR) contact plan_
            
                contacts
                 
                nodes
                
                ranges
                
            library
                
                log
                    
                list
                    
            library_from_ion 
                    
                rbt
                    
                scalar
                    
            msr              _implementation of Moderate Source Routing_
                
            routes           _implementation of Unibo-CGR routes structures_

        ion_bpv6            _interface interface code core for ION BP v6_
        
            interface       _interface code core for ION BP v6_

                utility_functions_from_ion

            extensions      _CGRR and RGR code (BP v6)_

            aux_files       _auxiliary files for building in ION BP v6 (ipnfw.c, etc.)_

        ion_bpv7            _the same as above, but for BP v7_

        dtnme

            interface        _interface code for DTNME_

            aux_files        _auxiliary files for building in DTNME (Makefile, Bundle.Router.cc, etc.)_

        docs                 _doxygen html documentation_

Packages for a specific implementation will include only the interface(s) involved.

#### Copyright

Copyright (c) 2020, University of Bologna. Auxiliary files maintain their original copyright.

#### License

Unibo-CGR is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.  
Unibo-CGR is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.  
You should have received a copy of the GNU General Public License along with Unibo-CGR. If not, see <http://www.gnu.org/licenses/>.

Auxiliary files maintain their original licences.

#### Including Unibo-CGR and its dependencies in DTNME
The new version of the Unibo-CGR interface for DTNME requires the use of the Contact Plan Manager (CPM) classes provided by the UNIBO-DTNME-CPM project. 
The steps for automatic installation of both Unibo-CGR and Unibo-DTNME-CPM are the following
- Download Unibo-CGR
- use the script "fetch_unibo_cgr_dependencies.sh" in the Unibo-CGR directory; launch it without paramaters to print the help. This script will download the files of Unibo-DTNMECPM directly from Gitlab to your Unibo-CGR directory
- use the script "mv_unibo_cgr.sh"; launch it without paramaters to print the help. This script will copy both Unibo-DTNME-CPM and Unibo-CGR files to DTNME and it will also update the DTNME/servlib Makefile.
- compile DTNME the usual way 

#### Including Unibo-CGR in ION
Unibo-CGR is already included in recent ION versions. Thus ION users can normally skip this section and go directly to Additional information.
To install Unibo-CGR on older ION versions or to overwrite an existent installation:
- download Unibo-CGR
- use the script mv_unibo_cgr.sh; launch the script without parameters to print the help
- see Additional information for compilation 

Alternatively, for manual installation, rename the Unibo-CGR root directory as "Unibo-CGR" and copy it to (ION's root directory)/bpv*/cgr/; then follow the instructions given in the files Unibo-CGR/ion_bpv*/aux_files/README.txt and Unibo-CGR/ion_bpv*/extensions/README.txt.  


#### Additional information

_Compilation Switches_

The exact behavior of Unibo-CGR depends on the settings of configuration switches. Researchers proficient in CGR routing can override defaults by editing the Unibo-CGR/core/config.h file. Some experimental features are not available on specific BP implementations.

_Use in ION_

The use of Unibo-CGR in ION does not differ from the use of previous implementation of CGR/SABR, a part experimental features that can be optionally enabled/disabled by overriding defaults (see above).  
Launch the configure script with the following sintax (from ION's root directory):  
Only Unibo-CGR: ./configure --enable-unibo-cgr  
Unibo-CGR + RGR Ext. Block: ./configure --enable-unibo-cgr CPPFLAGS='-DRGR=1 -DRGREB=1'  
Unibo-CGR + CGRR Ext. Block: ./configure --enable-unibo-cgr CPPFLAGS='-DCGRR=1 -DCGRREB=1'  
Unibo-CGR + RGR and CGRR Ext. Block: ./configure --enable-unibo-cgr CPPFLAGS='-DRGR=1 -DRGREB=1 -DCGRR=1 -DCGRREB=1'  
Unibo-CGR disabled: ./configure

_Use in DTNME_

Support to DTNME contacts and ranges is given by means of Unibo-DTNME-CPM (https://gitlab.com/ccaini/unibo-dtnme); the user is referred to its README for further necessary explanation about contact plan management.

_DTNME Limitations_

1. At present DTNME interface lacks RGR and CGRR. As a result, MSR and anti-loop mechanism must be disabled in Unibo-CGR/core/config.h. This switches are automatically disabled if the inclusion is carried out by means of the script mv_unibo_cgr.sh.  
2. The previous hop mechanism, based on the addition of a specific extension block to each bundle (see RFC 6259), relies on a different mechanism in DTNME 1.0.1Beta. This prevents ION nodes from knowing the EID of the previous hop in mixed environments, whenever the previous node is a DTNME node (even if declared and advertised with an ipn EID). As a result, ping-pong events may occur. In tests we had to disable reverse contact to prevent them.
3. Bundle reforwarding after forfait time has not been implemented yet.

_Logs_

As Unibo-CGR was designed with research in mind, logs are one of the most distinctive features. Logs are organized in two layers. They are put in cgr_log directory, created at first launch. At the higher layer we have a file listing all CGR calls, with a brief summary of each call. At the lower layer, call-specific information, (extremely detailed, for CGR proficient researchers only), is provided in call-specific files.

#### Credits

Lorenzo Persampieri (main author), lorenzo.persampieri@studio.unibo.it

Giacomo Gori and Federico Le Pera (DTNME interface) giacomo.gori3@studio.unibo.it

Carlo Caini (supervisor), carlo.caini@unibo.it

Acknowledgments: Gian Marco De Cola, Federico Marchetti, Laura Mazzuca.

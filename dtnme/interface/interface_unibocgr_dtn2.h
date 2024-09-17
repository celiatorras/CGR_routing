/** \file interface_unibocgr_dtn2.h
 *
 *  \brief This file provides the definitions of the functions
 *         that make UniboCGR's implementation compatible with DTN2,
 *         included in interface_unibocgr_dtn2.c
 *
 ** \copyright Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
 **
 ** \par License
 **
 **    This file is part of Unibo-CGR.                                            <br>
 **                                                                               <br>
 **    Unibo-CGR is free software: you can redistribute it and/or modify
 **    it under the terms of the GNU General Public License as published by
 **    the Free Software Foundation, either version 3 of the License, or
 **    (at your option) any later version.                                        <br>
 **    Unibo-CGR is distributed in the hope that it will be useful,
 **    but WITHOUT ANY WARRANTY; without even the implied warranty of
 **    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **    GNU General Public License for more details.                               <br>
 **                                                                               <br>
 **    You should have received a copy of the GNU General Public License
 **    along with Unibo-CGR.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  \author Giacomo Gori, giacomo.gori3@studio.unibo.it
 *
 *  \par Supervisor
 *       Carlo Caini, carlo.caini@unibo.it
 */


#ifndef SOURCES_INTERFACE_UNIBO_CGR_DTN2_H_
#define SOURCES_INTERFACE_UNIBO_CGR_DTN2_H_

//include from dtn2
#include "../../../../bundling/Bundle.h"
#include "../../../UniboCGRBundleRouter.h"
#include <cstdint>
#include <ctime>
#include <string>

extern int callUniboCGR(time_t time, dtn::Bundle *bundle, std::string *res);
extern void destroy_contact_graph_routing(time_t time);
extern int initialize_contact_graph_routing(uint64_t ownNode, time_t time, dtn::UniboCGRBundleRouter* router);

#endif /* SOURCES_INTERFACE_UNIBO_CGR_DTN2_H_ */

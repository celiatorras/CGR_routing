/** \file msr.h
 *
 *  \brief  This file provides the declaration of the function used
 *          to start, call and stop the Moderate Source Routing.
 *          You find also the macro to enable MSR in this CGR implementation,
 *          and other macros to change MSR behavior.
 *
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
 *  \author Lorenzo Persampieri, lorenzo.persampieri@studio.unibo.it
 *
 *  \par Supervisor
 *          Carlo Caini, carlo.caini@unibo.it
 */

#ifndef CGR_UNIBO_MSR_MSR_H_
#define CGR_UNIBO_MSR_MSR_H_

#include <stdlib.h>

#include "../UniboCGRSAP.h"
#include "../library/commonDefines.h"
#include "../library/list/list.h"
#include "../bundles/bundles.h"
#include "../routes/routes.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int tryMSR(UniboCGRSAP* uniboCgrSap, CgrBundle *bundle, List excludedNeighbors, FILE *file_call, List *bestRoutes);
extern int MSRSAP_open(UniboCGRSAP* uniboCgrSap);
extern void MSRSAP_close(UniboCGRSAP* uniboCgrSap);

#ifdef __cplusplus
}
#endif

#endif /* CGR_UNIBO_MSR_MSR_H_ */

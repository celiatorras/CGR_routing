/** \file feature-config.h
 *
 *  \brief  This file contains the most important macros to configure Unibo-CGR.
 *
 *
 *  \details You can configure dynamic memory, logging, debugging and other features of Unibo-CGR
 *           (i.e. SABR enhancements or Moderate Source Routing).
 *           Unless otherwise specified features are enabled when the corresponding value is set 
 *           to 1 and disabled when set to 0.
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

#ifndef UNIBO_CGR_FEATURE_CONFIG_H
#define UNIBO_CGR_FEATURE_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Enable/disable usage of ION's reference time. Must be 0 in production code. */
#ifndef UNIBO_CGR_RELATIVE_TIME
#define UNIBO_CGR_RELATIVE_TIME 0
#endif

/* Enable/disable Unibo-CGR logger. */
#ifndef UNIBO_CGR_FEATURE_LOG
#define UNIBO_CGR_FEATURE_LOG 0
#endif

/* Enable/disable One Route Per Neighbor algorithm. */
#ifndef UNIBO_CGR_FEATURE_ONE_ROUTE_PER_NEIGHBOR
#define UNIBO_CGR_FEATURE_ONE_ROUTE_PER_NEIGHBOR 0
#endif

/* Set a value (K) > 0 if you want to limit the one route per neighbor algorithm to K neighbors. */
#ifndef UNIBO_CGR_FEATURE_ONE_ROUTE_PER_NEIGHBOR_LIMIT
#define UNIBO_CGR_FEATURE_ONE_ROUTE_PER_NEIGHBOR_LIMIT 0
#endif

/* Enable/disable Queue Delay algorithm. (ETO on all hops) */
#ifndef UNIBO_CGR_FEATURE_QUEUE_DELAY
#define UNIBO_CGR_FEATURE_QUEUE_DELAY 0
#endif

/* Enable/disable Reactive Anti Loop algorithm. (depends on RGR Ext. Block) */
#ifndef UNIBO_CGR_FEATURE_REACTIVE_ANTI_LOOP
#define UNIBO_CGR_FEATURE_REACTIVE_ANTI_LOOP 0
#endif

/* Enable/disable Proactive Anti Loop algorithm. (depends on RGR Ext. Block) */
#ifndef UNIBO_CGR_FEATURE_PROACTIVE_ANTI_LOOP
#define UNIBO_CGR_FEATURE_PROACTIVE_ANTI_LOOP 0
#endif

/* Enable/disable Moderate Source Routing algorithm. (depends on CGRR Ext. Block) */
#ifndef UNIBO_CGR_FEATURE_MODERATE_SOURCE_ROUTING
#define UNIBO_CGR_FEATURE_MODERATE_SOURCE_ROUTING 0
#endif

/* Enable/disable CGRR Ext. Block update. */
#ifndef UNIBO_CGR_FEATURE_MSR_WISE_NODE
#define UNIBO_CGR_FEATURE_MSR_WISE_NODE 1
#endif

/* TODO description. */
#ifndef UNIBO_CGR_FEATURE_MSR_UNWISE_NODE_LOWER_BOUND
#define UNIBO_CGR_FEATURE_MSR_UNWISE_NODE_LOWER_BOUND 1
#endif

/* Start time tolerance (in seconds) for contacts stored into CGRR Ext. Block. */
#ifndef UNIBO_CGR_FEATURE_MSR_TIME_TOLERANCE
#define UNIBO_CGR_FEATURE_MSR_TIME_TOLERANCE 2
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* neighbor limit must be 1 when one route per neighbor is disabled. */
#if (UNIBO_CGR_FEATURE_ONE_ROUTE_PER_NEIGHBOR == 0)
#undef UNIBO_CGR_FEATURE_ONE_ROUTE_PER_NEIGHBOR_LIMIT
#define UNIBO_CGR_FEATURE_ONE_ROUTE_PER_NEIGHBOR_LIMIT 1
#endif

#ifdef __cplusplus
}
#endif

#endif //UNIBO_CGR_FEATURE_CONFIG_H

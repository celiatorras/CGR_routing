/** \file ranges.h
 *
 * \brief  This file provides the definition of the Range type,
 *         with all the declarations of the functions to manage the range tree.
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
 *       Carlo Caini, carlo.caini@unibo.it
 */

#ifndef SOURCES_CONTACTS_PLAN_RANGES_RANGES_H_
#define SOURCES_CONTACTS_PLAN_RANGES_RANGES_H_

#include <sys/time.h>

#include "../../UniboCGRSAP.h"
#include "../../library/commonDefines.h"
#include "../../library_from_ion/rbt/rbt_type.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
	/**
	 * \brief Start time
	 */
	time_t fromTime;
	/**
	 * \brief End time
	 */
	time_t toTime;
	/**
	 * \brief Sender ipn node
	 */
	uint64_t fromNode;
	/**
	 * \brief Receiver ipn node
	 */
	uint64_t toNode;
	/**
	 * \brief One-Way-Light-Time
	 */
	uint64_t owlt;
} Range;

extern int compare_ranges(void *first, void *second);
extern void free_range(void*);

extern int RangeSAP_open(UniboCGRSAP* uniboCgrSap);
extern void RangeSAP_close(UniboCGRSAP* uniboCgrSap);
extern void reset_RangesGraph(UniboCGRSAP* uniboCgrSap);

extern void removeExpiredRanges(UniboCGRSAP* uniboCgrSap);

extern int add_range_to_graph(UniboCGRSAP* uniboCgrSap, uint64_t fromNode, uint64_t toNode, time_t fromTime, time_t toTime, uint64_t owlt);
extern void remove_range_from_graph(UniboCGRSAP* uniboCgrSap, time_t fromTime, uint64_t fromNode, uint64_t toNode);
extern void remove_range_elt_from_graph(UniboCGRSAP* uniboCgrSap, Range *range);

extern Range* get_range(UniboCGRSAP* uniboCgrSap, uint64_t fromNode, uint64_t toNode, time_t fromTime, RbtNode **node);
extern Range* get_first_range(UniboCGRSAP* uniboCgrSap, RbtNode **node);
extern Range* get_first_range_from_node(UniboCGRSAP* uniboCgrSap, uint64_t fromNodeNbr, RbtNode **node);
extern Range* get_first_range_from_node_to_node(UniboCGRSAP* uniboCgrSap, uint64_t fromNodeNbr, uint64_t toNodeNbr, RbtNode **node);
extern Range* get_next_range(RbtNode **node);
extern Range* get_prev_range(RbtNode **node);
extern int get_applicable_range(UniboCGRSAP* uniboCgrSap, uint64_t fromNode, uint64_t toNode, time_t targetTime, uint64_t *owltResult);

extern int revise_range_start_time(UniboCGRSAP* uniboCgrSap, uint64_t fromNode, uint64_t toNode, time_t fromTime, time_t newFromTime);
extern int revise_range_end_time(UniboCGRSAP* uniboCgrSap, uint64_t fromNode, uint64_t toNode, time_t fromTime, time_t newEndTime);
extern int revise_owlt(UniboCGRSAP* uniboCgrSap, uint64_t fromNode, uint64_t toNode, time_t fromTime, uint64_t owlt);

extern int printRangesGraph(UniboCGRSAP* uniboCgrSap, FILE *file);

#ifdef __cplusplus
}
#endif

#endif /* SOURCES_CONTACTS_PLAN_RANGES_RANGES_H_ */

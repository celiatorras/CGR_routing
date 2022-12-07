/** \file ranges.c
 *
 *  \brief  This file provides the implementation of the functions
 *          to manage the range tree.
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

#include "ranges.h"

#include <stdlib.h>

#include "../../library_from_ion/rbt/rbt.h"

static void erase_range(Range*);
static Range* create_range(uint64_t fromNode, uint64_t toNode, time_t fromTime,
		time_t toTime, uint64_t owlt);

/**
 * \brief This struct is used to keep in one place all the data used by
 *        the range graph library.
 */
struct RangeSAP {
	/**
	 * \brief The range graph.
	 */
	Rbt *ranges;
	/**
	 * \brief The time when the next Range expires.
	 */
	time_t timeRangeToRemove;
};

/******************************************************************************
 *
 * \par Function Name:
 *      compare_ranges
 *
 * \brief Compare two ranges
 *
 *
 * \par Date Written:
 *      19/01/20
 *
 * \return int
 *
 * \retval   0  The ranges are equals (even if they has the same pointed address)
 * \retval  -1  The first range is less than the second range
 * \retval   1  The first range is greater than the second range
 *
 * \param[in]	*first   The first range
 * \param[in]	*second  The second range
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int compare_ranges(void *first, void *second)
{
	Range *a, *b;
	int result = 0;

	if (first == second) //if they point to the same address
	{
		result = 0;
	}
	else if (first != NULL && second != NULL)
	{

		a = (Range*) first;
		b = (Range*) second;

		if (a->fromNode < b->fromNode)
		{
			result = -1;
		}

		else if (a->fromNode > b->fromNode)
		{
			result = 1;
		}
		else if (a->toNode < b->toNode)
		{
			result = -1;
		}
		else if (a->toNode > b->toNode)
		{
			result = 1;
		}
		else if (a->fromTime < b->fromTime)
		{
			result = -1;
		}
		else if (a->fromTime > b->fromTime)
		{
			result = 1;
		}
		else
		{
			result = 0;
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      erase_range
 *
 * \brief  Reset all fields for a Range type
 *
 *
 * \par Date Written:
 *      19/01/20
 *
 * \return void
 *
 * \param[in]  *range  The Range for which we want to reset all fields
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static void erase_range(Range *range)
{
	range->fromNode = 0;
	range->toNode = 0;
	range->fromTime = 0;
	range->toTime = 0;
	range->owlt = 0;
}

/******************************************************************************
 *
 * \par Function Name:
 *      free_range
 *
 * \brief  Deallocate memory for a Range type
 *
 *
 * \par Date Written:
 *      19/01/20
 *
 * \return void
 *
 * \param[in]  *range  The Range that we want to deallocate
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR          |  DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void free_range(void *range)
{
	Range *temp;

	if (range != NULL)
	{
		temp = (Range*) range;
		erase_range(temp);
		MDEPOSIT(temp);
	}
}

/******************************************************************************
 *
 * \par Function Name:
 *      create_range
 *
 * \brief  Allocate memory for a Range type
 *
 *
 * \par Date Written:
 *      19/01/20
 *
 * \return Range*
 *
 * \retval Range*  The new allocated Range
 * \retval NULL    MWITHDRAW error
 *
 * \param[in]	fromNode   The ipn node number of the sender node
 * \param[in]	toNode     The ipn node number of the receiver node
 * \param[in]	fromTime   The range's start time
 * \param[in]	toTime     The range's end time
 * \param[in]	owlt       The distance from the sender node to the receiver node in light time
 *
 * \par Notes:
 *             1.  You must check that the return value of this function is not NULL.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static Range* create_range(uint64_t fromNode, uint64_t toNode, time_t fromTime,
		time_t toTime, uint64_t owlt)
{
	Range *range = (Range*) MWITHDRAW(sizeof(Range));

	if (range != NULL)
	{
		range->fromNode = fromNode;
		range->toNode = toNode;
		range->fromTime = fromTime;
		range->toTime = toTime;
		range->owlt = owlt;
	}

	return range;
}

/******************************************************************************
 *
 * \par Function Name:
 *      RangeSAP_open
 *
 * \brief  Allocate memory for the ranges graph
 *
 *
 * \par Date Written:
 *      19/01/20
 *
 * \return int
 *
 * \retval   0  Success case: the ranges graph now exists
 * \retval  -2  MWITHDRAW error
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *  21/10/22 | L. Persampieri  |  Renamed function.
 *****************************************************************************/
int RangeSAP_open(UniboCGRSAP* uniboCgrSap)
{
    if (UniboCGRSAP_get_RangeSAP(uniboCgrSap)) return 0;
    RangeSAP* sap = MWITHDRAW(sizeof(RangeSAP));
    if (!sap) return -2;
    UniboCGRSAP_set_RangeSAP(uniboCgrSap, sap);
    memset(sap, 0, sizeof(RangeSAP));
    sap->timeRangeToRemove = MAX_POSIX_TIME;
    sap->ranges = rbt_create(free_range, compare_ranges);
	if (!sap->ranges) {
        RangeSAP_close(uniboCgrSap);
        return -2;
    }

	return 0;
}

/******************************************************************************
 *
 * \par Function Name:
 *      removeExpiredRanges
 *
 * \brief Remove from the ranges graph all ranges where Range's toTime field is less than
 *        time passed as argument.
 *
 *
 * \par Date Written:
 *      19/01/20
 *
 * \return void
 *
 * \par Notes:
 *             1.  The timeRangeToRemove will be redefined.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void removeExpiredRanges(UniboCGRSAP* uniboCgrSap)
{
	time_t min = MAX_POSIX_TIME;
	Range *range;
    const time_t time = UniboCGRSAP_get_current_time(uniboCgrSap);
	RbtNode *node, *next;
#if (DEBUG_CGR)
	uint32_t tot = 0;
#endif
	RangeSAP *sap = UniboCGRSAP_get_RangeSAP(uniboCgrSap);

	if (time >= sap->timeRangeToRemove)
	{
		debug_printf("Remove the expired ranges.");

		node = rbt_first(sap->ranges);
		while (node != NULL)
		{
			next = rbt_next(node);
			if (node->data != NULL)
			{
				range = (Range*) node->data;

				if (range->toTime <= time)
				{
					rbt_delete(sap->ranges, range);
#if (DEBUG_CGR)
					tot++;
#endif
				}
				else if (range->toTime < min)
				{
					min = range->toTime;
				}
			}
			node = next;
		}

		sap->timeRangeToRemove = min;
		debug_printf("Removed %" PRIu32 " ranges, next remove ranges time: %ld", tot,
				(long int ) sap->timeRangeToRemove);
	}

}
/**
 * \brief modify range start time
 * \retval 0 success
 * \retval -1 range not found
 * \retval -2 arguments error
 * \retval -3 cannot insert overlapping ranges
 */
int revise_range_start_time(UniboCGRSAP* uniboCgrSap, uint64_t fromNode, uint64_t toNode, time_t fromTime, time_t newFromTime) {
    RbtNode* range_node;
    Range *range = get_range(uniboCgrSap, fromNode, toNode, fromTime, &range_node);
    if (range == NULL) {
        return -1; // range not found
    }
    if (newFromTime > range->toTime) {
        return -2;
    }

    // note that since we are changing start time we may overlap
    // only with a range the compares lower than the current range.
    // so we need only to check if the previous range has the same sender
    // and receiver and if its end time is greater than the new start time.

    RbtNode* prev_range_node = rbt_prev(range_node);
    if (!prev_range_node || !prev_range_node->data) {
        // ok no overlapping
    } else {
        Range* prev_range = prev_range_node->data;

        if (prev_range->fromNode == range->fromNode
            && prev_range->toNode == range->toNode
            && prev_range->toTime > newFromTime) {
            // will overlap
            return -3;
        }
    }

    range->fromTime = newFromTime;
    return 0;
}
/**
 * \brief modify range end time
 * \retval 0 success
 * \retval -1 range not found
 * \retval -2 arguments error
 * \retval -3 cannot insert overlapping ranges
 */
int revise_range_end_time(UniboCGRSAP* uniboCgrSap, uint64_t fromNode, uint64_t toNode, time_t fromTime, time_t newEndTime) {
    RbtNode* range_node;
    Range *range = get_range(uniboCgrSap, fromNode, toNode, fromTime, &range_node);
    if (range == NULL) {
        return -1; // contact not found
    }
    if (newEndTime < range->fromTime) {
        return -2;
    }

    // note that since we are changing end time we may overlap
    // only with a range the compares greater than the current range.
    // so we need only to check if the next range has the same sender
    // and receiver and if its start time is lower than the new end time.

    RbtNode* next_range_node = rbt_next(range_node);
    if (!next_range_node || !next_range_node->data) {
        // ok no overlapping
    } else {
        Range* next_range = next_range_node->data;

        if (next_range->fromNode == range->fromNode
            && next_range->toNode == range->toNode
            && next_range->fromTime < newEndTime) {
            // will overlap
            return -3;
        }
    }
    range->toTime = newEndTime;
    return 0;
}


/******************************************************************************
 *
 * \par Function Name:
 *      revise_range
 *
 * \brief  Revise the range's OWLT
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return int
 *
 * \retval   0  Contact revised
 * \retval  -1  Contact not found
 * \retval  -2  Arguments error
 *
 * \param[in]	   fromNode     The contact's sender node
 * \param[in]        toNode     The contact's receiver node
 * \param[in]      fromTime     The contact's start time
 * \param[in]          owlt     The revised owlt
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int revise_owlt(UniboCGRSAP* uniboCgrSap, uint64_t fromNode, uint64_t toNode, time_t fromTime, uint64_t owlt)
{
	int result = -2;
	Range *range = NULL;

	if (fromNode != 0 && toNode != 0 && fromTime >= 0)
	{
		range = get_range(uniboCgrSap, fromNode, toNode, fromTime, NULL);
		result = -1;
		if(range != NULL)
		{
			range->owlt = owlt;
			result = 0;
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      add_range_to_graph
 *
 * \brief  Add a Range to the ranges graph
 *
 *
 * \par Date Written:
 *      19/01/20
 *
 * \return int
 *
 * \retval   2  Success case: Revised range's owlt
 * \retval   1  Success case: the range now is inside the ranges graph
 * \retval   0  Arguments error
 * \retval  -1  The range cannot be inserted because it overlaps some other ranges.
 * \retval  -2  MWITHDRAW error
 *
 * \param[in]	fromNode    The ipn node number of the sender node
 * \param[in]	toNode      The ipn node number of the receiver node
 * \param[in]	fromTime    The range's start time
 * \param[in]	toTime      The range's end time
 * \param[in]	owlt        The distance from the sender node to the receiver node in light time
 *
 * \par Notes:
 *              1. This function will change timeRangeToRemove if the range->toTime of the
 *                 new range is less than the current timeRangeToRemove, in that case
 *                  timeRangeToRemove will be equals to range->toTime
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int add_range_to_graph(UniboCGRSAP* uniboCgrSap, uint64_t fromNode, uint64_t toNode, time_t fromTime,
                       time_t toTime, uint64_t owlt)
{
	int result, overlapped;
	Range *range = NULL, *foundRange = NULL;
	RbtNode *elt = NULL;
	RangeSAP *sap = UniboCGRSAP_get_RangeSAP(uniboCgrSap);

	result = -1;

	if (toTime == 0)
	{
		toTime = MAX_POSIX_TIME;
	}

	if (toTime < 0 || fromTime < 0 || toTime < fromTime || fromNode == 0 || toNode == 0)
	{
		result = 0;
	}
	else
	{
		result = -1;
		overlapped = 0;
		foundRange = get_first_range_from_node_to_node(uniboCgrSap, fromNode, toNode, &elt);
		while (foundRange != NULL)
		{
			if (foundRange->fromNode == fromNode && foundRange->toNode == toNode)
			{
				if(foundRange->fromTime == fromTime && foundRange->toTime == toTime)
				{
					// Range exists in the ranges graph
					overlapped = 1;
					foundRange = NULL; //I leave the loop
				}
				else if (foundRange->fromTime <= fromTime && fromTime < foundRange->toTime)
				{
					overlapped = 1;
					foundRange = NULL; //I leave the loop
				}
				else if (foundRange->fromTime < toTime && toTime <= foundRange->toTime)
				{
					overlapped = 1;
					foundRange = NULL; //I leave the loop
				}
				else if (toTime <= foundRange->fromTime)
				{
					//Ranges are ordered by fromTime
					foundRange = NULL; //I leave the loop
				}
				else
				{
					foundRange = get_next_range(&elt);
				}
			}
			else
			{
				foundRange = NULL; //I leave the loop
			}
		}

		if (overlapped == 0)
		{
			range = create_range(fromNode, toNode, fromTime, toTime, owlt);
			elt = rbt_insert(sap->ranges, range);

			result = ((elt != NULL) ? 1 : -2);

			if (result == -2)
			{
				free_range(range);
			}
			else if (sap->timeRangeToRemove > toTime)
			{
				sap->timeRangeToRemove = toTime;
			}
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      remove_range_elt_from_graph
 *
 * \brief  Remove from the ranges graph a Range with the same {fromNode, toNode, fromTime}
 *         of the Range passed as argument.
 *
 *
 * \par Date Written:
 *      19/01/20
 *
 * \return void
 *
 * \param[in]	range    The range to remove from graph
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void remove_range_elt_from_graph(UniboCGRSAP* uniboCgrSap, Range *range)
{
	RangeSAP *sap = UniboCGRSAP_get_RangeSAP(uniboCgrSap);
	if (range != NULL)
	{
		rbt_delete(sap->ranges, range);
	}
}

/******************************************************************************
 *
 * \par Function Name:
 *      remove_range_from_graph
 *
 * \brief Remove the Range with the fields {fromNode, toNode, *fromTime}.
 *        If fromTime points to NULL we remove all ranges with the fields {fromNode, toNode}.
 *
 *
 * \par Date Written:
 *      19/01/20
 *
 * \return void
 *
 * \param[in]	*fromTime   The Range's start time, if it points to NULL we
 *                          remove all ranges with the fields {fromNode, toNode}.
 * \param[in]	fromNode    The ipn node number of the sender node
 * \param[in]	toNode      The ipn node number of the receiver node
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void remove_range_from_graph(UniboCGRSAP* uniboCgrSap, time_t fromTime, uint64_t fromNode, uint64_t toNode)
{
	Range arg;
	RangeSAP *sap = UniboCGRSAP_get_RangeSAP(uniboCgrSap);

    arg.fromNode = fromNode;
    arg.toNode = toNode;
    arg.fromTime = fromTime;
    arg.toTime = 0; //compare function doesn't use it
    arg.owlt = 0; //compare function doesn't use it
    rbt_delete(sap->ranges, &arg);
}

/******************************************************************************
 *
 * \par Function Name:
 *      reset_RangesGraph
 *
 * \brief Remove all ranges from the ranges graph, but not the graph itself.
 *
 *
 * \par Date Written:
 *      19/01/20
 *
 * \return void
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void reset_RangesGraph(UniboCGRSAP* uniboCgrSap)
{
	RangeSAP *sap = UniboCGRSAP_get_RangeSAP(uniboCgrSap);
	rbt_clear(sap->ranges);
	sap->timeRangeToRemove = MAX_POSIX_TIME;
}

/******************************************************************************
 *
 * \par Function Name:
 *      RangeSAP_close
 *
 * \brief  Remove all ranges from the ranges graph, and the graph itself.
 *
 *
 * \par Date Written:
 *      19/01/20
 *
 * \return void
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *  21/10/22 | L. Persampieri  |  Renamed function
 *****************************************************************************/
void RangeSAP_close(UniboCGRSAP* uniboCgrSap)
{
    if (!UniboCGRSAP_get_RangeSAP(uniboCgrSap)) return;

	RangeSAP *sap = UniboCGRSAP_get_RangeSAP(uniboCgrSap);
	rbt_destroy(sap->ranges);

    memset(sap, 0, sizeof(RangeSAP));
    MDEPOSIT(sap);
    UniboCGRSAP_set_RangeSAP(uniboCgrSap, NULL);
}

/*
 * Functions to search a Range
 */

/******************************************************************************
 *
 * \par Function Name:
 *      get_range
 *
 * \brief  Get the Range that matches with the {fromNode, toNode, fromTime}
 *
 *
 * \par Date Written:
 *      19/01/20
 *
 * \return Range*
 *
 * \retval Range*  The range found 
 * \retval NULL    There isn't a range with this characteristics
 *
 * \param[in]      fromNode   The range's sender node
 * \param[in]      toNode     The range's receiver node
 * \param[in]      fromTime   The range's start time
 * \param[out]     **node     If this argument isn't NULL, at the end it will
 *                            contains the RbtNode that points to the range returned by the function
 *
 * \par Notes:
 *             1. You must check that the return value of this function is not NULL.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
Range* get_range(UniboCGRSAP* uniboCgrSap, uint64_t fromNode, uint64_t toNode, time_t fromTime,
                 RbtNode **node)
{
	Range arg, *result;
	RbtNode *elt;
	RangeSAP *sap = UniboCGRSAP_get_RangeSAP(uniboCgrSap);

	result = NULL;
	if (fromNode != 0 && toNode != 0 && fromTime >= 0)
	{
		arg.fromNode = fromNode;
		arg.toNode = toNode;
		arg.fromTime = fromTime;
		arg.toTime = 0;
		arg.owlt = 0;

		elt = rbt_search(sap->ranges, &arg, NULL);
		if (elt != NULL)
		{
			if (elt->data != NULL)
			{
				result = (Range*) elt->data;

				if (node != NULL) {
				    *node = elt;
				}
			}
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      get_first_range
 *
 * \brief  Get the first range of the ranges graph
 *
 *
 * \par Date Written:
 *      19/01/20
 *
 * \return Range*
 *
 * \retval Range*   The range found
 * \retval NULL     The ranges graph is empty.
 *
 * \param[out]	**node  If node isn't NULL, at the end it will points to the
 *                      RbtNode that points to the Range returned by the function (or NULL).
 *
 * \par Notes:
 *              1. You must check that the return value of this function is not NULL.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
Range* get_first_range(UniboCGRSAP* uniboCgrSap, RbtNode **node)
{
	Range *result = NULL;
	RbtNode *currentRange = NULL;
	RangeSAP *sap = UniboCGRSAP_get_RangeSAP(uniboCgrSap);

	currentRange = rbt_first(sap->ranges);
	if (currentRange != NULL)
	{
		result = (Range*) currentRange->data;
		if (node != NULL)
		{
			*node = currentRange;
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      get_first_range_from_node
 *
 * \brief  Get the first range of the ranges graph that has the sender node
 *         passed as argument.
 *
 *
 * \par Date Written: 
 *      19/01/20
 *
 * \return Range*
 *
 * \retval Range*  The range found
 * \retval NULL    There isn't a Range that has this fromNode field.
 *
 * \param[in]      fromNodeNbr   The Range's sender node (ipn node number)
 * \param[out]     **node        If node isn't NULL, at the end it will points to the
 *                               RbtNode that points to the Range returned by the function (or NULL).
 *
 * \par Notes:
 *              1. You must check that the return value of this function is not NULL.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
Range* get_first_range_from_node(UniboCGRSAP* uniboCgrSap, uint64_t fromNodeNbr, RbtNode **node)
{
	Range arg, *result = NULL;
	RbtNode *currentRange;
	RangeSAP *sap = UniboCGRSAP_get_RangeSAP(uniboCgrSap);

	arg.fromNode = fromNodeNbr;
	arg.toNode = 0;
	arg.fromTime = -1;
	arg.toTime = -1;
	arg.owlt = 0;

	rbt_search(sap->ranges, &arg, &currentRange);

	if (currentRange != NULL)
	{
		result = (Range*) currentRange->data;

		if (result->fromNode != fromNodeNbr)
		{
			result = NULL;
		}
		else if (node != NULL)
		{
			*node = currentRange;
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      get_first_range_from_node_to_node
 *
 * \brief Get the first range of the ranges graph that has the sender node and the
 *        receiver node passed as arguments.
 *
 *
 * \par Date Written:
 *      19/01/20
 *
 * \return Range*
 *
 * \retval Range*  The range found 
 * \retval NULL    There isn't a Range that has this {fromNode, toNode} fields.
 *
 * \param[in]    fromNodeNbr   The Range's sender node (ipn node number)
 * \param[in]    toNodeNbr   The Range's receiver node (ipn node number)
 * \param[out]   **node        If node isn't NULL, at the end it will points to the
 *                             RbtNode that points to the Range returned by the function (or NULL).
 *
 * \par Notes:
 *             1. You must check that the return value of this function is not NULL.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
Range* get_first_range_from_node_to_node(UniboCGRSAP* uniboCgrSap, uint64_t fromNodeNbr, uint64_t toNodeNbr, RbtNode **node)
{
	Range arg;
	Range *result = NULL;
	RbtNode *currentRange = NULL;
	RangeSAP *sap = UniboCGRSAP_get_RangeSAP(uniboCgrSap);

	arg.fromNode = fromNodeNbr;
	arg.toNode = toNodeNbr;
	arg.fromTime = -1;
	arg.toTime = -1;
	arg.owlt = 0;

	rbt_search(sap->ranges, &arg, &currentRange);
	if (currentRange != NULL)
	{
		result = (Range*) currentRange->data;
		if (result->fromNode != fromNodeNbr || result->toNode != toNodeNbr)
		{
			result = NULL;
		}
		else if (node != NULL)
		{
			*node = currentRange;
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      get_next_range
 *
 * \brief Get the next range referring to the current range pointed by the argument "node"
 *
 *
 * \par Date Written:
 *      19/01/20
 *
 * \return Range*
 *
 * \retval Range*  The range found 
 * \retval NULL    There isn't the next range
 *
 * \param[in,out] **node  If this arguments isn't NULL, at the end it will
 *                        contains the RbtNode that points to the range returned by the function
 *
 * \par Notes:
 *             1. You must check that the return value of this function is not NULL.
 *             2. If we find the next range this function update the RbtNode pointed by node.
 *             3. You can use this function in a delete loop (referring to the current
 *                implementation of "rbt_delete").
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
Range* get_next_range(RbtNode **node)
{
	Range *result = NULL;
	RbtNode *temp = NULL;

	if (node != NULL)
	{
		temp = rbt_next(*node);
		if (temp != NULL)
		{
			result = (Range*) temp->data;
		}

		*node = temp;
	}
	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      get_prev_range
 *
 * \brief  Get the prev range referring to the current range pointed by the argument "node"
 *
 *
 * \par Date Written:
 *      19/01/20
 *
 * \return Range*
 *
 * \retval Range*  The range found
 * \retval NULL    There isn't the previous range
 *
 * \param[in,out]  **node   If this arguments isn't NULL, at the end it will
 *                          contains the RbtNode that points to the range returned by the function
 *
 * \par Notes:
 *              1. You must check that the return value of this function is not NULL.
 *              2. If we find the next range this function update the RbtNode pointed by node.
 *              3. Never use this function in a delete loop (referring to the current
 *                 implementation of "rbt_delete").
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
Range* get_prev_range(RbtNode **node)
{
	Range *result = NULL;
	RbtNode *temp = NULL;

	if (node != NULL)
	{
		temp = rbt_prev(*node);
		if (temp != NULL)
		{
			result = (Range*) temp->data;
		}

		*node = temp;
	}
	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      get_applicable_range
 *
 * \brief  Get the owlt from the sender node to the receiver node at the time "targetTime".
 *
 *
 * \par Date Written:
 *      19/01/20
 *
 * \return int
 *
 * \retval   0  Success case: found the owlt
 * \retval  -1  Error case: Range not found
 * \retval  -2  Arguments error case: owltResult points to NULL.
 *
 * \param[in]	fromNode      The ipn node number of the sender node
 * \param[in]	toNode        The ipn node number of the receiver node
 * \param[in]	targetTime    The time that has to been between the Range's {fromTime, toTime}.
 * \param[out]	*owltResult   The distance from the sender node to the receiver node
 *                            in light time.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int get_applicable_range(UniboCGRSAP* uniboCgrSap, uint64_t fromNode, uint64_t toNode, time_t targetTime, uint64_t *owltResult)
{
	RbtNode *temp = NULL;
	int result = -1;
	Range *current;

	if (owltResult != NULL)
	{
		current = get_first_range_from_node_to_node(uniboCgrSap, fromNode, toNode, &temp);

		while (current != NULL)
		{
			if (current->fromNode == fromNode && current->toNode == toNode)
			{
				if (current->fromTime <= targetTime && targetTime < current->toTime)
				{
					*owltResult = current->owlt;
					current = NULL; //I leave the loop
					result = 0;
				}
				else if (current->toTime < targetTime)
				{
					current = get_next_range(&temp);
				}
				else
				{
					current = NULL;
				}
			}
			else
			{
				current = NULL; //I leave the loop
			}
		}
	}
	else
	{
		result = -2;
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      printRange
 *
 * \brief Print the range pointed by data to a buffer and at the end to_Add points to that buffer
 *
 *
 * \par Date Written:
 *      19/01/20
 *
 * \return int
 *
 * \retval ">= 0"  The number of characters printed to the buffer
 * \retval    -1   Some errors occurred
 *
 * \param[out]	*file      The file where we print the range
 * \param[in]	*data      The pointer to the range
 *
 *  \par Notes:
 *              1. This function assumes that "to_add" is not NULL.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int printRange(FILE *file, void *data)
{
	Range *range;
	int result = -1;

	if (data != NULL && file != NULL)
	{
		result = 0;
		range = (Range*) data;
		fprintf(file, "%-15" PRIu64 " %-15" PRIu64 " %-15ld %-15ld %" PRIu64 "\n", range->fromNode, range->toNode,
				(long int) range->fromTime, (long int) range->toTime, range->owlt);
	}
	else
	{
		fprintf(file, "RANGE: NULL\n");
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      printRangesGraph
 *
 * \brief  Print the ranges graph
 *
 *
 * \par Date Written:
 *      19/01/20
 *
 * \return int
 * 
 * \retval   1  Success case, ranges graph printed
 * \retval   0  The file is NULL
 *
 * \param[in]	file          The file where we want to print the ranges graph
 * \param[in]	currentTime   The time to print together at the ranges graph to keep trace of the
 *                            modification's history
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int printRangesGraph(UniboCGRSAP* uniboCgrSap, FILE *file)
{
	int result = 0;
	RangeSAP *sap = UniboCGRSAP_get_RangeSAP(uniboCgrSap);

	if (file != NULL)
	{
		result = 1;
		fprintf(file, "\n---------------------------- RANGES GRAPH ----------------------------\n");

		fprintf(file, "Time: %ld\n%-15s %-15s %-15s %-15s %s\n", (long int) UniboCGRSAP_get_current_time(uniboCgrSap), "FromNode",
				"ToNode", "FromTime", "ToTime", "OWLT");
		result = printTreeInOrder(sap->ranges, file, printRange);

		if (result == 1)
		{
			fprintf(file,
					"\n----------------------------------------------------------------------\n");

		}
		else
		{
			fprintf(file, "\n---------- RANGES GRAPH ERROR ----------\n");
		}
	}

	return result;
}
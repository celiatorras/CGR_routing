/** \file cgr.c
 *
 *  \brief  This file provides the implementation of the functions
 *          used to start, call and stop the CGR.
 *
 *  \details The main function are executeCGR and getBestRoutes, which call
 *	     the functions included in phase_one.c, phase_two.c and phase_three.c
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

#include "cgr.h"

#include <stdlib.h>
#include "../contact_plan/nodes/nodes.h"
#include <stdio.h>
#include <sys/time.h>

#include "cgr_phases.h"
#include "../contact_plan/contacts/contacts.h"
#include "../contact_plan/ranges/ranges.h"
#include "../library/list/list.h"
#include "../msr/msr.h"
#include "../routes/routes.h"
#include "../time_analysis/time.h"
#include "../library/log/log.h"

/**
 * \brief Used to keep in one place the data used by Unibo-CGR during a call.
 */
struct UniboCgrCurrentCallSAP {
	/**
	 * \brief The file for the logs of the current call
	 */
	FILE *file_call;
	/**
	 * \brief The destination node for the current Bundle.
	 */
	Node *destinationNode;
	/**
	 * \brief The algorithm used (with success) for the current call (i.e. CGR or MSR).
	 */
	RoutingAlgorithm algorithm;

};



/******************************************************************************
 *
 * \par Function Name:
 *  	print_result_cgr
 *
 * \brief  Print some logs for the CGR's output
 *
 *
 * \par Date Written:
 *  	15/02/20
 *
 * \return void
 *
 * \param[in]   result         The getBestRoutes function result
 * \param[in]   bestRoutes      The list of best routes
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  15/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static void print_result_cgr(UniboCGRSAP* uniboCgrSap, int result, List bestRoutes)
{
	ListElt *elt;
	Route *route;

    if (!LogSAP_is_enabled(uniboCgrSap)) return;

	if (result >= 0)
	{
		writeLog(uniboCgrSap, "Best routes found: %d.", result);

		if (bestRoutes != NULL)
		{
			for (elt = bestRoutes->first; elt != NULL; elt = elt->next)
			{
				route = (Route*) elt->data;
				writeLog(uniboCgrSap, "Used route to neighbor %llu.", route->neighbor);
			}
		}
	}
	else if (result == -1)
	{
		writeLog(uniboCgrSap, "0 routes found to the destination.");
	}

}


/******************************************************************************
 *
 * \par Function Name:
 * 		get_computed_routes_number
 *
 * \brief Get the number of computed routes (phase one) by Unibo-CGR to a specific destination.
 *
 *
 * \par Date Written:
 * 	    01/08/20
 *
 * \return long unsigned int
 *
 * \retval ">= 0"  The number of computed routes to reach destination
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR          |  DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  01/08/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int64_t get_computed_routes_number(UniboCGRSAP *uniboCgrSap, uint64_t destination) {
    const UniboCgrCurrentCallSAP *sap = UniboCGRSAP_get_UniboCgrCurrentCallSAP(uniboCgrSap);
	Node *destinationNode = NULL;
	int64_t result = 0;

	if(sap->destinationNode != NULL)
	{
		if(sap->destinationNode->nodeNbr == destination)
		{
			destinationNode = sap->destinationNode;
		}
	}

	if(destinationNode == NULL)
	{
		destinationNode = get_node(uniboCgrSap, destination);
	}

	if(destinationNode != NULL) {
		if(destinationNode->routingObject != NULL &&
				destinationNode->routingObject->selectedRoutes != NULL &&
				destinationNode->routingObject->knownRoutes != NULL)
		{
			result = (int64_t) destinationNode->routingObject->selectedRoutes->length
                    + (int64_t) destinationNode->routingObject->knownRoutes->length;
		}
	}

	return result;

}

/******************************************************************************
 *
 * \par Function Name:
 * 		get_file_call
 *
 * \brief Get the pointer to the FILE used to print the detailed log of the current call.
 *
 *
 * \par Date Written:
 * 	    02/07/20
 *
 * \return FILE*
 *
 * \retval   FILE*  A reference to the FILE used to print the detailed logs during the current call.
 * \retval   NULL   FILE not yet opened.
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR          |  DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  02/07/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static FILE * get_file_call(UniboCGRSAP* uniboCgrSap) {
	const UniboCgrCurrentCallSAP *sap = UniboCGRSAP_get_UniboCgrCurrentCallSAP(uniboCgrSap);
	return sap->file_call;
}

/******************************************************************************
 *
 * \par Function Name:
 * 		get_last_call_routing_algorithm
 *
 * \brief Get the routing algorithm used for the previous call.
 *
 *
 * \par Date Written:
 * 	    15/09/20
 *
 * \return RoutingAlgorithm
 *
 * \retval   unknown    Routing algorithm unknown (or Unibo-CGR failure/routes not found)
 * \retval   cgr        Contact Graph Routing used with success
 * \retval   msr        Moderate Source Routing used with success
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR          |  DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  15/09/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
RoutingAlgorithm get_last_call_routing_algorithm(UniboCGRSAP* uniboCgrSap) {
	UniboCgrCurrentCallSAP *sap = UniboCGRSAP_get_UniboCgrCurrentCallSAP(uniboCgrSap);
	return sap->algorithm;
}

/******************************************************************************
 *
 * \par Function Name:
 * 		UniboCgrCurrentCallSAP_open
 *
 * \brief Initialize UniboCgrCurrentCallSAP
 *
 *
 * \par Date Written:
 *  	15/02/20
 *
 * \return int
 *
 * \retval          1	Success case: CGR initialized
 * \retval         -2	MWITHDRAW error
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  15/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int UniboCgrCurrentCallSAP_open(UniboCGRSAP *uniboCgrSap)
{
    if (UniboCGRSAP_get_UniboCgrCurrentCallSAP(uniboCgrSap)) return 0; // already initialized

    UniboCgrCurrentCallSAP* sap = MWITHDRAW(sizeof(UniboCgrCurrentCallSAP));
    if (!sap) { return -2; }
    UniboCGRSAP_set_UniboCgrCurrentCallSAP(uniboCgrSap, sap);
    memset(sap, 0, sizeof(UniboCgrCurrentCallSAP));

    return 0;
}

/******************************************************************************
 *
 * \par Function Name:
 * 		UniboCgrCurrentCallSAP_close
 *
 * \brief Deallocate UniboCgrCurrentCallSAP
 *
 *
 * \par Date Written:
 *  	15/02/20
 *
 * \return void
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  15/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void UniboCgrCurrentCallSAP_close(UniboCGRSAP* uniboCgrSap)
{
    if (!UniboCGRSAP_get_UniboCgrCurrentCallSAP(uniboCgrSap)) return;

	UniboCgrCurrentCallSAP *currentCallSap = UniboCGRSAP_get_UniboCgrCurrentCallSAP(uniboCgrSap);

    if (currentCallSap->file_call) {
        closeBundleFile(&currentCallSap->file_call);
    }

    memset(currentCallSap, 0, sizeof(UniboCgrCurrentCallSAP));
    MDEPOSIT(currentCallSap);
    UniboCGRSAP_set_UniboCgrCurrentCallSAP(uniboCgrSap, NULL);
}

/******************************************************************************
 *
 * \par Function Name:
 * 		parse_excluded_nodes
 *
 * \brief Remove from the list all duplicate nodes
 *
 *
 * \par Date Written:
 *  	30/04/20
 *
 * \return void
 *
 * \param[in]  excludedNodes    The list with the excluded neighbors
 *
 * \par Notes:
 *             1.  In CGR Unibo a neighbor is a node for which the local node
 *                 has at least one opportunity of transmission (contact).
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  30/04/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static void parse_excluded_nodes(List excludedNodes)
{
	ListElt *main_elt;
	ListElt *current_elt, *next_current;
	uint64_t *main_node;
	uint64_t *current_node;


	main_elt = excludedNodes->first;
	while(main_elt != NULL)
	{
		if(main_elt->data != NULL)
		{
			main_node = (uint64_t *) main_elt->data;

			current_elt = main_elt->next;

			while (current_elt != NULL)
			{
				next_current = current_elt->next;

				if (current_elt->data != NULL)
				{
					current_node = (uint64_t*) current_elt->data;

					if (*current_node == *main_node)
					{
						list_remove_elt(current_elt);
					}

				}

				current_elt = next_current;
			}

            main_elt = main_elt->next;
		}
		else
		{
            current_elt = main_elt->next;
			list_remove_elt(main_elt);
            main_elt = current_elt;
		}
	}
}

/******************************************************************************
 *
 * \par Function Name:
 * 		reset_cgr
 *
 * \brief  Reset the CGR: all the structures used by the 3 phases
 *         will be cleaned but not the contact plan.
 *
 * \details   This function should be used every time you receive
 *            a new call for the CGR.
 *
 *
 * \par Date Written:
 *      15/02/20
 *
 * \return void
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  15/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static void reset_cgr(UniboCGRSAP* uniboCgrSap)
{
	reset_phase_one(uniboCgrSap);
	reset_phase_two(uniboCgrSap);
	reset_neighbors_temporary_fields(uniboCgrSap);
}

/******************************************************************************
 *
 * \par Function Name:
 * 		clear_rtg_object
 *
 * \brief  This function cleans the RtgObject from the temporary
 *         values used in this call.
 *
 *
 * \par Date Written:
 * 		15/02/20
 *
 * \return void
 *
 * \param[in]	*rtgObj  The RtgObject to clean
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  15/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static void clear_rtg_object(RtgObject *rtgObj)
{
	ListElt *elt;
	Route *route;

	if (rtgObj != NULL)
	{
		for (elt = rtgObj->selectedRoutes->first; elt != NULL; elt = elt->next)
		{
			route = (Route*) elt->data;
			route->checkValue = 0;
			route->num = 0;
		}

	}
}

/******************************************************************************
 *
 * \par Function Name:
 * 		is_initialized_terminus_node
 *
 * \brief Boolean, to know if the Node has some field that points to NULL
 *
 *
 * \par Date Written:
 * 		30/01/20
 *
 * \return int
 *
 * \retval  1	The Node is initialized and can be used
 * \retval  0	The Node isn't initialized correctly
 *
 * \param[in]   *terminusNode   The Node for which we want to know if is initialized
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR          |   DESCRIPTION
 *  -------- | --------------- |  -----------------------------------------------
 *  30/01/20 | L. Persampieri  |   Initial Implementation and documentation.
 *****************************************************************************/
static int is_initialized_terminus_node(Node *terminusNode)
{
	int result = 0;
	RtgObject *rtgObj;
	if (terminusNode != NULL)
	{
		rtgObj = terminusNode->routingObject;

		if (rtgObj != NULL)
		{
			if (rtgObj->knownRoutes != NULL && rtgObj->selectedRoutes != NULL && rtgObj->citations != NULL)
			{
				result = 1;
			}
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 * 		excludeNeighbor
 *
 * \brief Insert a neighbor in the excluded neighbors list
 *
 *
 * \par Date Written:
 * 		15/02/20
 *
 * \return int
 *
 * \retval    0   Success case: neighbor inserted in the excluded neighbors list
 * \retval   -2   MWITHDRAW error
 *
 * \param[in]	excludedNeighbors   The excluded neighbors list
 * \param[in]	neighbor            The ipn node of the neighbor that we want to exclude
 *
 * \warning excludedNeighbors doesn't have to be NULL
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  15/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int excludeNeighbor(List excludedNeighbors, uint64_t neighbor)
{
	uint64_t *temp = (uint64_t*) MWITHDRAW(sizeof(uint64_t));
	int result = -2;
	if (temp != NULL)
	{
		*temp = neighbor;

		if (neighbor == 0 || list_insert_last(excludedNeighbors, temp) != NULL)
		{
			result = 0;
		}
		else
		{
			MDEPOSIT(temp);
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name: executeCGR
 *
 * \brief Implementation of the CGR, call the 3 phases to choose
 *        the best routes for the forwarding of the bundle.
 *
 *
 * \par Date Written:
 * 		15/02/20
 *
 * \return int
 *
 * \retval      ">= 0"  Success case: number of best routes found
 * \retval         -1   There aren't routes to reach the destination.
 * \retval         -2   MWITHDRAW error
 * \retval         -3   Arguments error (phase one error)
 *
 * \param[in]   *bundle            The bundle that has to be forwarded
 * \param[in]   *terminusNode      The destination Node of the bundel
 * \param[in]   excludedNeighbors  The excluded neighbors list, the nodes to which
 *                                 the bundle hasn't to be forwarded as "first hop"
 * \param[out]  *bestRoutes        If result > 0: the list of best routes, NULL otherwise
 *
 * \warning bundle doesn't have to be NULL
 * \warning terminusNode doesn't have to be NULL
 * \warning excludedNeighbors doesn't have to be NULL
 * \warning excludedNeighbor doesn't have to be NULL
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  15/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int executeCGR(UniboCGRSAP* uniboCgrSap, CgrBundle *bundle, Node *terminusNode, List excludedNeighbors,
                      List *bestRoutes)
{
	int result = 0, stop = 0;
	uint32_t missingNeighbors = 0;
	List candidateRoutes = NULL;
	List subsetComputedRoutes = NULL;
	RtgObject *rtgObj = terminusNode->routingObject;

	if(get_local_node_neighbors_count(uniboCgrSap) == 0)
	{
		// 0 neighbors to reach destination...
		stop = 1;
	}

	while (!stop)
	{
		result = getCandidateRoutes(uniboCgrSap, terminusNode, bundle, excludedNeighbors, rtgObj->selectedRoutes, &subsetComputedRoutes, &missingNeighbors, &candidateRoutes); //phase two

		if (result != 0 || missingNeighbors == 0)
		{
			stop = 1;
		}
		if (!stop)
		{
			result = computeRoutes(uniboCgrSap, terminusNode, subsetComputedRoutes, missingNeighbors); //phase one

			stop = (result <= 0) ? 1 : 0;
		}
	}

	print_phase_one_routes(get_file_call(uniboCgrSap), rtgObj->selectedRoutes);
	print_phase_two_routes(uniboCgrSap, get_file_call(uniboCgrSap), candidateRoutes);
	*bestRoutes = NULL;

	if (result >= 0 && candidateRoutes != NULL && candidateRoutes->length > 0)
	{
		record_phases_start_time(uniboCgrSap, phaseThree);

		result = chooseBestRoutes(uniboCgrSap, bundle, candidateRoutes); //phase three

		record_phases_stop_time(uniboCgrSap, phaseThree);

		*bestRoutes = candidateRoutes;
	}

	print_phase_three_routes(get_file_call(uniboCgrSap), *bestRoutes);

	clear_rtg_object(rtgObj); //clear the temporary values

	debug_printf("result -> %d", result);

	return result;

}

/******************************************************************************
 *
 * \par Function Name:
 *  	getBestRoutes
 *
 * \brief	 Implementation of the CGR, get the best routes list.
 *
 *
 * \par Date Written:
 *  	15/02/20
 *
 * \return int
 *
 * \retval      ">= 0"  Success case: number of best routes found
 * \retval         -1   There aren't routes to reach the destination.
 * \retval         -2   MWITHDRAW error
 * \retval         -3   Phase one error (phase one's arguments error)
 * \retval         -4   Arguments error
 *
 * \param[in]   *bundle              The bundle that has to be forwarded
 * \param[in]   excludedNeighbors    The excluded neighbors list, the nodes to which
 *                                   the bundle hasn't to be forwarded as "first hop"
 * \param[out]  *bestRoutes          If result > 0: the list of best routes, NULL otherwise
 *
 * \par Notes:
 *          1.  If the contact plan changed this function discards
 *              all the routes computed by the previous calls.
 *          2.  You must initialize the CGR before to call this function.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  15/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int getBestRoutes(UniboCGRSAP* uniboCgrSap, CgrBundle *bundle, List excludedNeighbors, List *bestRoutes)
{
	int result = -4;
	Node *terminusNode;
	UniboCgrCurrentCallSAP *currentCallSap = UniboCGRSAP_get_UniboCgrCurrentCallSAP(uniboCgrSap);
    const time_t time = UniboCGRSAP_get_current_time(uniboCgrSap);

	record_total_core_start_time(uniboCgrSap);

	currentCallSap->algorithm = unknown_algorithm;

	if (bundle != NULL && excludedNeighbors != NULL && bestRoutes != NULL)
	{
		*bestRoutes = NULL;
		debug_printf("Call n.: %" PRIu32 "", UniboCGRSAP_get_bundle_count(uniboCgrSap));
		writeLog(uniboCgrSap, "Bundle - Destination node number: %" PRIu64 ".", bundle->terminus_node);
		if (check_bundle(bundle) != 0)
		{
			writeLog(uniboCgrSap, "Bundle bad formed.");
			result = -4;
		}
		else if (bundle->expiration_time < time) //bundle expired
		{
			result = 0;
			writeLog(uniboCgrSap, "Bundle expired.");
		}
		else
		{

			result = 0;

            reset_cgr(uniboCgrSap);

			bool originalOneRoutePerNeighborFeatureFlag = false;
			uint32_t originalOneRoutePerNeighborLimit = 1;

			if (IS_CRITICAL(bundle))
			{
				originalOneRoutePerNeighborFeatureFlag = UniboCGRSAP_check_one_route_per_neighbor(uniboCgrSap, &originalOneRoutePerNeighborLimit);
				UniboCGRSAP_tweak_one_route_per_neighbor(uniboCgrSap, true, 0U);
			}

            if (UniboCGRSAP_handle_updates(uniboCgrSap) < 0) {
                //writeLog(uniboCgrSap, "Need to discard all routes.");
                verbose_debug_printf("Error...");
                result = -2;
            }

			if(result == 0)
			{
                removeExpiredContacts(uniboCgrSap);
                removeExpiredRanges(uniboCgrSap);
                removeOldNeighbors(uniboCgrSap);

				terminusNode = add_node(uniboCgrSap, bundle->terminus_node);

				currentCallSap->destinationNode = terminusNode;

				if(!is_initialized_terminus_node(terminusNode))
				{
					// Some error in the "Node graph" management
					terminusNode = NULL;
				}

				result = 0;

                if (UniboCGRSAP_check_reactive_anti_loop(uniboCgrSap)) {
                    result = set_failed_neighbors_list(bundle, UniboCGRSAP_get_local_node(uniboCgrSap));
                }
				if (result >= 0 && !(RETURN_TO_SENDER(bundle)) && bundle->sender_node != 0)
				{
					result = excludeNeighbor(excludedNeighbors, bundle->sender_node);
				}
				parse_excluded_nodes(excludedNeighbors);

				currentCallSap->file_call = openBundleFile(uniboCgrSap);
				print_bundle(uniboCgrSap, currentCallSap->file_call, bundle, excludedNeighbors, UniboCGRSAP_get_current_time(uniboCgrSap));

				if (terminusNode != NULL && result >= 0)
				{
                    if (UniboCGRSAP_check_moderate_source_routing(uniboCgrSap)) {
                        result = tryMSR(uniboCgrSap, bundle, excludedNeighbors, currentCallSap->file_call, bestRoutes);
                        if(result > 0) {
                            currentCallSap->algorithm = msr;
                        }
                        if (result <= 0 && result != -2) {
                            result = executeCGR(uniboCgrSap, bundle, terminusNode, excludedNeighbors, bestRoutes);
							if(result > 0) {
                                currentCallSap->algorithm = cgr;
                            }
                        }
                    } else {
                        result = executeCGR(uniboCgrSap, bundle, terminusNode, excludedNeighbors, bestRoutes);
						if(result > 0) {
                            currentCallSap->algorithm = cgr;
                        }
                    }
				}
				else
				{
					result = -2;
				}

				closeBundleFile(&(currentCallSap->file_call));
			}

			if (IS_CRITICAL(bundle))
			{
				UniboCGRSAP_tweak_one_route_per_neighbor(uniboCgrSap, originalOneRoutePerNeighborFeatureFlag, originalOneRoutePerNeighborLimit);
			}
		}

		if(result == -1)
		{
			writeLog(uniboCgrSap, "0 routes found to destination.");
		}
		else if(result == 0)
		{
			writeLog(uniboCgrSap, "Best routes found: 0.");
		}
		else if(result > 0)
		{
			print_result_cgr(uniboCgrSap, result, *bestRoutes);
		}
	}

	debug_printf("result -> %d", result);

    UniboCGRSAP_increase_bundle_count(uniboCgrSap);

	record_total_core_stop_time(uniboCgrSap);

	return result;
}

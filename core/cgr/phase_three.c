/** \file phase_three.c
 *
 *  \brief  This file provides the implementation of the CGR's phase three:
 *          the choose of the best routes from the candidate routes list (phase two's output).
 *  
 *  \details The output of this phase is the best routes list.
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

#include <stdlib.h>
#include <stdio.h>

#include "cgr_phases.h"
#include "../contact_plan/contacts/contacts.h"
#include "../library/list/list.h"
#include "../routes/routes.h"

typedef int (*getBestRouteFn)(
        UniboCGRSAP* uniboCgrSap,
        void* firstRoute,
        void* secondRoute);

/*  ... declare cost functions between here ... */
static int best_route_cost_function(UniboCGRSAP* uniboCgrSap, void *first, void *second);
/* ... and here ... */


/**
 * \brief Private data for phase three.
 */
struct PhaseThreeSAP {
    getBestRouteFn costFunction;
};

/******************************************************************************
 *
 * \par Function Name:
 * 		PhaseThreeSAP_open
 *
 * \brief Initialize all the data used by the phase three
 *
 *
 * \par Date Written:  21/10/22
 *
 * \return int
 *
 * \retval   0   Success case: initialized
 * \retval  -2   MWITHDRAW error
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR          |   DESCRIPTION
 *  -------- | --------------- |  -----------------------------------------------
 *  21/10/22 | L. Persampieri  |   Initial Implementation and documentation.
 *****************************************************************************/
int PhaseThreeSAP_open(UniboCGRSAP* uniboCgrSap) {
    if (UniboCGRSAP_get_PhaseThreeSAP(uniboCgrSap)) return 0;
    PhaseThreeSAP* sap = MWITHDRAW(sizeof(PhaseThreeSAP));
    if (!sap) return -2;

    UniboCGRSAP_set_PhaseThreeSAP(uniboCgrSap, sap);
    memset(sap, 0, sizeof(PhaseThreeSAP));
    sap->costFunction = best_route_cost_function;
    return 0;
}

/******************************************************************************
 *
 * \par Function Name:
 * 		PhaseThreeSAP_close
 *
 * \brief Deallocate all the memory allocated by the phase two.
 *
 *
 * \par Date Written:  21/10/22
 *
 * \return void
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR          |   DESCRIPTION
 *  -------- | --------------- |  -----------------------------------------------
 *  21/10/22 | L. Persampieri  |   Initial Implementation and documentation.
 *****************************************************************************/
void PhaseThreeSAP_close(UniboCGRSAP* uniboCgrSap) {
    PhaseThreeSAP* sap = UniboCGRSAP_get_PhaseThreeSAP(uniboCgrSap);
    if (!sap) return;
    memset(sap, 0, sizeof(PhaseThreeSAP));
    MDEPOSIT(sap);
    UniboCGRSAP_set_PhaseThreeSAP(uniboCgrSap, NULL);
}

/******************************************************************************
 *
 * \par Function Name:
 * 		PhaseThreeSAP_set_cost_function_default
 *
 * \brief Set the "default" cost function used to compare routes in phase three.
 *
 *
 * \par Date Written:  21/10/22
 *
 * \return void
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR          |   DESCRIPTION
 *  -------- | --------------- |  -----------------------------------------------
 *  21/10/22 | L. Persampieri  |   Initial Implementation and documentation.
 *****************************************************************************/
void PhaseThreeSAP_set_cost_function_default(UniboCGRSAP* uniboCgrSap) {
    PhaseThreeSAP* sap = UniboCGRSAP_get_PhaseThreeSAP(uniboCgrSap);
    sap->costFunction = best_route_cost_function;
}

/******************************************************************************
 *
 * \par Function Name:
 * 		best_route_cost_function
 *
 * \brief Compare two routes with some "cost" values, to know who is the best
 *
 *
 * \par Date Written:
 * 		13/02/20
 *
 * \return int
 *
 * \retval  -1    The first Route is better than the second Route
 * \retval   0    The first Route has the same cost of the second Route
 * \retval   1    The second Route is better than the first Route
 *
 * \param[in]	*first     The first Route
 * \param[in]	*second    The second Route
 *
 * \warning first doesn't have to be NULL.
 * \warning second doesn't have to be NULL.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int best_route_cost_function(UniboCGRSAP* uniboCgrSap, void *first, void *second)
{
	int result = -1;
    Route* firstRoute = first;
    Route* secondRoute = second;


    if (UniboCGRSAP_check_reactive_anti_loop(uniboCgrSap) || UniboCGRSAP_check_proactive_anti_loop(uniboCgrSap)) {
        if (firstRoute->checkValue > secondRoute->checkValue) {
            return 1;
        }
        else if (firstRoute->checkValue < secondRoute->checkValue)
        {
            return -1;
        }
    } else {
		if (firstRoute->pbat > secondRoute->pbat) //SABR 3.2.8.1.4 a) 1)
		{
			result = 1;
		}
		else if (firstRoute->pbat == secondRoute->pbat)
		{
			if (firstRoute->hops->length > secondRoute->hops->length) //SABR 3.2.8.1.4 a) 2)
			{
				result = 1;
			}
			else if (firstRoute->hops->length == secondRoute->hops->length)
			{
				if (firstRoute->toTime < secondRoute->toTime) //SABR 3.2.8.1.4 a) 3)
				{
					result = 1;
				}
				else if (firstRoute->toTime == secondRoute->toTime)
				{
					if (firstRoute->owltSum > secondRoute->owltSum)
					{
						result = 1;
					}
					else if (firstRoute->owltSum == secondRoute->owltSum)
					{
						//SABR 3.2.8.1.4 a) 4)
						if (firstRoute->neighbor > secondRoute->neighbor)
						{
							result = 1;
						}
						else if (firstRoute->neighbor == secondRoute->neighbor)
						{
							result = 0;
						}
					}
				}
			}
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 * 		getOneBestRoutePerNeighbor
 *
 * \brief For each neighbor choose the best route and removes from candidateRoutes the other routes.
 *
 *
 * \par Date Written:
 * 		13/02/20
 *
 * \return void
 *
 * \param[in,out]   *candidateRoutes     Initially it contains all the candidateRoutes,
 *                                       at the end only the best route per neighbor remains in this list.
 *
 *\warning candidateRoutes doesn't have to be NULL.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static void getOneBestRoutePerNeighbor(UniboCGRSAP* uniboCgrSap, List candidateRoutes)
{
	ListElt *elt, *next, *temp;
	Route *currentNeighborFirstRoute, *currentRoute;
	unsigned long long viaNeighbor;

	for (elt = candidateRoutes->first; elt != NULL; elt = elt->next)
	{
		temp = elt->next;
		currentNeighborFirstRoute = (Route*) elt->data;
		viaNeighbor = currentNeighborFirstRoute->neighbor;
		while (temp != NULL)
		{
			currentRoute = (Route*) temp->data;
			next = temp->next;
			if (viaNeighbor == currentRoute->neighbor)
			{
				if (best_route_cost_function(uniboCgrSap, (Route*) currentRoute, (Route*) elt->data) < 0)
				{
					elt->data = currentRoute;
				}

				list_remove_elt(temp);
			}

			temp = next;
		}
	}
}

/******************************************************************************
 *
 * \par Function Name:
 * 		getBestRoute
 *
 * \brief Choose the best route and removes all the other routes from candidateRoutes.
 * 		 At the end only the best route will remains in candidateRoutes.
 *
 *
 * \par Date Written:
 * 		13/02/20
 *
 * \return void
 *
 * \param[in,out]  *candidateRoutes     Initially it contains all the candidateRoutes,
 *                                      at the end only the best route will remains in this list.
 *
 * \warning candidateRoutes doesn't have to be NULL.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static void getBestRoute(UniboCGRSAP* uniboCgrSap, List candidateRoutes)
{
	ListElt *elt, *next, *bestElt = NULL;

	elt = candidateRoutes->first;

	while (elt != NULL)
	{
		next = elt->next;

		if (bestElt == NULL)
		{
			bestElt = elt;
		}
		else
		{
			if (best_route_cost_function(uniboCgrSap, (Route*) elt->data, (Route*) bestElt->data) < 0)
			{
				bestElt->data = elt->data;
			}

			list_remove_elt(elt);
		}

		elt = next;
	}
}

/******************************************************************************
 *
 * \par Function Name:
 * 		update_volumes
 *
 * \brief For each contact in the hops list of a best route
 *        this function will decrease the mtv field for all level of priority
 *        less than or equal to the bundle's priority. The volume will be decreased
 *        by the bundle's evc.
 *
 *
 * \par Date Written:
 * 		13/02/20
 *
 * \return void
 *
 * \param[in]   *bundle         The bundle from we get the priority level and the EVC
 * \param[in]   bestRoutes      The list of best routes
 *
 * \warning bundle doesn't have to be NULL.
 * \warning bestRoutes doesn't have to be NULL.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static void update_volumes(CgrBundle *bundle, List bestRoutes)
{
	ListElt *routeElt, *hopElt;
	Contact *contact;
	Route *route;
	int i, priority = bundle->priority_level;

	for (routeElt = bestRoutes->first; routeElt != NULL; routeElt = routeElt->next)
	{
		route = (Route*) routeElt->data;

		for (hopElt = route->hops->first; hopElt != NULL; hopElt = hopElt->next)
		{
			contact = (Contact*) hopElt->data;

			for (i = 0; i <= priority; i++)
			{
				contact->mtv[i] -= (double) bundle->evc;
			}
		}
	}
}

/******************************************************************************
 *
 * \par Function Name:
 * 		chooseBestRoutes
 *
 * \brief Get the best routes. At the end in candidateRoutes will remain only
 * 		  the best routes choosed (subset).
 *
 *
 * \par Date Written:
 * 		13/02/20
 *
 * \return int
 *
 * \retval  ">= 0"	The number of the best routes choosed.
 * \retval     -1	Arguments error: NULL pointer.
 *
 * \param[in]       *bundle             The bundle that has to be forwarded
 * \param[in,out]   candidateRoutes     Initially it contains the candidate routes,
 *                                      at the end it will contains only the best routes. The other
 *                                      routes will be discarded for the forwarding of the bundle.
 *
 * \par Notes:
 *          1. If the bundle is not critical at the end we get at most one route.
 *          2. If the bundle is critical at the end we get at most one route per neighbor.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int chooseBestRoutes(UniboCGRSAP* uniboCgrSap, CgrBundle *bundle, List candidateRoutes)
{
    (void) uniboCgrSap;
	int result = -1;

	debug_printf("Entry point phase three.");

	if (candidateRoutes != NULL && bundle != NULL)
	{
		if (IS_CRITICAL(bundle))
		{
			getOneBestRoutePerNeighbor(uniboCgrSap, candidateRoutes);
		}
		else
		{
			getBestRoute(uniboCgrSap, candidateRoutes);
		}

		update_volumes(bundle, candidateRoutes);

		result = (int) candidateRoutes->length;
	}

	debug_printf("Best routes chosen: %d", result);

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 * 		print_phase_three_route
 *
 * \brief Print the route in the "call" file in phase three format
 *
 *
 * \par Date Written:
 * 		13/02/20
 *
 * \return int
 *
 * \retval      0   Success case
 * \retval     -1   Arguments error
 *
 * \param[in]   file    The file where we want to print the route
 * \param[in]   *route  The Route that we want to print.
 *
 * \par Notes:
 *          1. For phase three best routes we print only the number of the route and the neighbor.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int print_phase_three_route(FILE *file, Route *route)
{
	int len, result = -1;
	char num[20];

	if (file != NULL && route != NULL)
	{
		result = 0;
		num[0] = '\0';
		len = sprintf(num, "%u)", route->num);
		if (len >= 0)
		{
			fprintf(file, "%-15s %" PRIu64 "\n", num, route->neighbor);
		}

	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 * 		print_phase_three_routes
 *
 * \brief Print the routes in the file in phase three format
 *
 *
 * \par Date Written:
 * 		13/02/20
 *
 * \return void
 *
 * \param[in]   file         The file where we want to print the routes
 * \param[in]   bestRoutes   The routes that we want to print
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void print_phase_three_routes(FILE *file, List bestRoutes)
{
	ListElt *elt;

	if (file != NULL)
	{
		fprintf(file, "\n------------ PHASE THREE: BEST ROUTES ------------\n");

		if (bestRoutes != NULL && bestRoutes->length > 0)
		{
			fprintf(file, "\n%-15s %s\n", "Route n.", "Neighbor");
			for (elt = bestRoutes->first; elt != NULL; elt = elt->next)
			{
				print_phase_three_route(file, (Route*) elt->data);
			}
		}
		else
		{
			fprintf(file, "\n0 best routes.\n");
		}
		fprintf(file, "\n--------------------------------------------------\n");

		debug_fflush(file);
	}
}


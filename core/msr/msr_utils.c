/** \file msr_utils.c
 *
 *  \brief  This file provides the implementation of some utility function to manage routes
 *          get from CGRR Extension Block and attach them to CgrBundle struct.
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
#include "msr_utils.h"

/******************************************************************************
 *
 * \par Function Name:
 * 		populate_msr_route
 *
 * \brief Build a Route from the last contact to the first.
 *
 * \details This function should be used only to build routes that will be used by MSR.
 *
 * \par Date Written:
 * 		23/04/20
 *
 * \return int
 *
 * \retval  0    Success case: Route built correctly
 * \retval -1    Arguments error
 * \retval -2    MWITHDRAW error
 *
 * \param[in]     current_time   The current (differential from interface) time from the start of Unibo-CGR
 * \param[in]     finalContact   The last contact of the route
 * \param[out]    resultRoute    The Route just built in success case. This field must be
 *                               allocated by the caller.
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR          |   DESCRIPTION
 *  -------- | --------------- |  -----------------------------------------------
 *  23/04/20 | L. Persampieri  |   Initial Implementation and documentation.
 *****************************************************************************/
int populate_msr_route(time_t current_time, Contact *finalContact, Route *resultRoute)
{
	int result = -1;
	time_t earliestEndTime;
	Contact *contact, *firstContact = NULL;
	ContactNote *current_work;
	ListElt *elt;

	if(finalContact != NULL && resultRoute != NULL)
	{
		result = 0;

		resultRoute->arrivalConfidence = finalContact->routingObject->arrivalConfidence;
		resultRoute->computedAtTime = current_time;

		earliestEndTime = MAX_POSIX_TIME;
		contact = finalContact;

		while (contact != NULL)
		{
			current_work = contact->routingObject;
			if (contact->toTime < earliestEndTime)
			{
				earliestEndTime = contact->toTime;
			}

			elt = list_insert_first(resultRoute->hops, contact);

			if (elt == NULL)
			{
				result = -2;
				contact = NULL; //I leave the loop
			}
			else
			{
				firstContact = contact;
				contact = current_work->predecessor;
			}
		}

		if (result == 0)
		{
			resultRoute->neighbor = firstContact->toNode;
			resultRoute->fromTime = firstContact->fromTime;
			resultRoute->toTime = earliestEndTime;
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 * 		delete_msr_route
 *
 * \brief Delete a route previously built by populate_msr_route() function.
 *
 * \par Date Written:
 * 		23/04/20
 *
 * \return void
 *
 * \param[in]     route   The route to destroy
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR          |   DESCRIPTION
 *  -------- | --------------- |  -----------------------------------------------
 *  23/04/20 | L. Persampieri  |   Initial Implementation and documentation.
 *****************************************************************************/
void delete_msr_route(Route *route)
{
	if(route != NULL)
	{
		if(route->hops != NULL)
		{
			route->hops->delete_data_elt = NULL;
			route->hops->delete_userData = NULL;
			free_list(route->hops);
		}
		if(route->children != NULL)
		{
			route->children->delete_data_elt = NULL;
			route->children->delete_userData = NULL;
			free_list(route->children);
		}

		memset(route,0,sizeof(Route));
		MDEPOSIT(route);
	}
}
Route* create_msr_route() {
    Route* route = MWITHDRAW(sizeof(Route));
    if (!route) {
        return NULL;
    }
    memset(route, 0, sizeof(Route));
    route->hops = list_create(NULL, NULL, NULL, NULL);
    if (!route->hops) {
        MDEPOSIT(route);
        return NULL;
    }
    return route;
}
void reset_msr_route(Route *route) {
    if (!route) return;
    List hops = route->hops;
    free_list_elts(route->hops);
    memset(route, 0, sizeof(Route));
    route->hops = hops;
}


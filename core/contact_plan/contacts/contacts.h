/** \file contacts.h
 *
 *  \brief  This file provides the definition of the Contact type,
 *          of the ContactNode type and of the CtType, with all the declarations
 *          of the functions to manage the contact graph
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

#ifndef SOURCES_CONTACTS_PLAN_CONTACTS_CONTACTS_H_
#define SOURCES_CONTACTS_PLAN_CONTACTS_CONTACTS_H_

#include <sys/time.h>

#include "../../UniboCGRSAP.h"
#include "../../library/commonDefines.h"
#include "../../library/list/list_type.h"
#include "../../library_from_ion/rbt/rbt_type.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct cgrContactNote ContactNote;

// support to new types must be carefully designed -- at present only scheduled type is supported
typedef enum
{
    //TypeRegistration = 1,
    TypeScheduled,
    //TypeSuppressed,
    //TypePredicted,
    //TypeHypothetical,
    //TypeDiscovered
} CtType;

typedef struct
{
	/**
	 * \brief Sender node (ipn node number)
	 */
	uint64_t fromNode;
	/**
	 * \brief Receiver node (ipn node number)
	 */
	uint64_t toNode;
	/**
	 * \brief Start transit time
	 */
	time_t fromTime;
	/**
	 * \brief Stop transmit time
	 */
	time_t toTime;
	/**
	 * \brief In bytes per second
	 */
	uint64_t xmitRate;
	/**
	 * \brief Confidence that the contact will materialize
	 */
	float confidence;
	/**
	 * \brief Registration or Scheduled
	 */
	CtType type;
	/**
	 * \brief Remaining volume (for each level of priority)
	 */
	double mtv[3];
	/**
	 * \brief Used by Dijkstra's search
	 */
	ContactNote *routingObject;
	/**
	 * \brief List of ListElt data.
	 *
	 * \details Each citation is a pointer to the element
	 * of the hops list of the Route where the contact appears,
	 * and this element of the hops list points to this contact.
	 */
	List citations;
} Contact;

struct cgrContactNote
{
	/**
	 * \brief Previous contact in the route, used to reconstruct the route at the end of
	 * the Dijkstra's search
	 */
	Contact *predecessor;
	/**
	 * \brief Best case arrival time to the toNode of the contact
	 */
	time_t arrivalTime;
	/**
	 * \brief Boolean used to identify each contact that belongs to the visited set
	 *
	 * \details Values
	 *          -  1  Contact already visited
	 *          -  0  Contact not visited
	 */
	int visited;
	/**
	 * \brief Flag used to identify each contact that belongs to the excluded set
	 */
	int suppressed;
	/**
	 * \brief Ranges sum to reach the toNode
	 */
	uint64_t owltSum;
	/**
	 * \brief Number of hops to reach this contact during Dijkstra's search.
	 */
	uint32_t hopCount;
	/**
	 * \brief Product of the confidence of each contacts in the path to
	 * reach this contact and of the contact's confidence itself
	 */
	float arrivalConfidence;
	/**
	 * \brief Flag to known if we already get the range for this contact during the Dijkstra's search.
	 *
	 * \details Values:
	 *          -  1  Range found at contact's start time
	 *          -  0  Range has yet to be searched
	 *          - -1  Range not found at the contact's start time
	 *
	 * \par Notes:
	 *          1.  Phase one always looks for the range at contact's start time.
	 */
	int rangeFlag;
	/**
	 * \brief The owlt of the range found.
	 */
	uint32_t owlt;
	/**
	 * \brief
	 */
	Contact *nextContactInDijkstraQueue;
};

extern int compare_contacts(void *first, void *second);
extern Contact* create_contact(uint64_t fromNode, uint64_t toNode,
		time_t fromTime, time_t toTime, uint64_t xmitRate, float confidence, CtType type);
extern void free_contact(void*);

extern int ContactSAP_open(UniboCGRSAP* uniboCgrSap);
extern void ContactSAP_close(UniboCGRSAP* uniboCgrSap);
extern void reset_ContactsGraph(UniboCGRSAP* uniboCgrSap);

extern void ContactSAP_decrease_time(UniboCGRSAP* uniboCgrSap, time_t diff);

extern void removeExpiredContacts(UniboCGRSAP* uniboCgrSap);
extern void remove_contact_from_graph(UniboCGRSAP* uniboCgrSap, time_t fromTime, uint64_t fromNode, uint64_t toNode);
extern void remove_contact_elt_from_graph(UniboCGRSAP* uniboCgrSap, Contact *elt);
int add_contact_to_graph(UniboCGRSAP* uniboCgrSap, uint64_t fromNode, uint64_t toNode, time_t fromTime,
                         time_t toTime, uint64_t xmitRate, float confidence, int copyMTV, const double mtv[]);
extern void discardAllRoutesFromContactsGraph(UniboCGRSAP* uniboCgrSap);

extern Contact* get_contact(UniboCGRSAP* uniboCgrSap, uint64_t fromNode, uint64_t toNode, time_t fromTime,
                            RbtNode **node);
extern Contact* get_first_contact(UniboCGRSAP* uniboCgrSap, RbtNode **node);
extern Contact* get_first_contact_from_node(UniboCGRSAP* uniboCgrSap, uint64_t fromNodeNbr, RbtNode **node);
extern Contact* get_first_contact_from_node_to_node(UniboCGRSAP* uniboCgrSap, uint64_t fromNodeNbr,
                                                    uint64_t toNodeNbr, RbtNode **node);
extern Contact* get_next_contact(RbtNode **node);
extern Contact* get_prev_contact(RbtNode **node);
extern Contact * get_contact_with_time_tolerance(UniboCGRSAP* uniboCgrSap, uint64_t fromNode, uint64_t toNode, time_t fromTime, uint32_t tolerance);


extern int revise_contact_start_time(UniboCGRSAP* uniboCgrSap, uint64_t fromNode, uint64_t toNode, time_t fromTime, time_t newFromTime);
extern int revise_contact_end_time(UniboCGRSAP* uniboCgrSap, uint64_t fromNode, uint64_t toNode, time_t fromTime, time_t newEndTime);
extern int revise_confidence(UniboCGRSAP* uniboCgrSap, uint64_t fromNode, uint64_t toNode, time_t fromTime, float newConfidence);
extern int revise_xmit_rate(UniboCGRSAP* uniboCgrSap, uint64_t fromNode, uint64_t toNode, time_t fromTime, uint64_t xmitRate);

extern int printContactsGraph(UniboCGRSAP* uniboCgrSap, FILE *file);

#ifdef __cplusplus
}
#endif

#endif /* SOURCES_CONTACTS_PLAN_CONTACTS_CONTACTS_H_ */

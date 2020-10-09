/*
 	cgr.h:	definitions supporting the utilization of Contact
		Graph Routing in forwarding infrastructure.

	Author: Scott Burleigh, JPL

	Modification History:
	Date      Who   What

	Copyright (c) 2011, California Institute of Technology.
	ALL RIGHTS RESERVED.  U.S. Government Sponsorship
	acknowledged.
 									*/
#ifndef _CGR_H_
#define _CGR_H_

/******************************************************************************
 * Added by Gian Marco De Cola gianmarco.decola@studio.unibo.it
 *
 * MODERATE_SOURCE_ROUTING and WISE_NODE added by Federico Marchetti and
 * ported to ion-3.7.0 by Gian Marco De Cola
 *
 * Supervisor Carlo Caini carlo.caini@unibo.it
 *
 * FLAGS explanation (at 07/12/2019)
 *
 * DIJKSTRA_ENHANCED: Dijkstra and computeSpurRoutes enhancements
 * ONE_ROUTE_PER_NEIGHBOR: it always saves at least one route per each neighbor,
 * 						   as for critical bundles (but it still replicates only critical bundles)
 * QUEUE_DELAY: queueDelay extension (i.e. ETO on hops following the first) on computePBAT
 * MODERATE_SOURCE_ROUTING: Moderate source routing (alias “Path Encoding” in 2015 IEEE Comm.Mag)”.
 * 						    It requires CGRREB extension on
 * WISE_NODE: it sets the level of control in moderate source routing (1->full, 0->min)
 * 			  It has no effect if moderate source routing is off. Wise nodes are the
 * 			  nodes aware of all contacts, unwise nodes are nodes aware of only the contacts to proximate nodes.
 *
 *
 *****************************************************************************/

//Added by L. Persampieri
#ifndef CGR_UNIBO
#define CGR_UNIBO 1
#endif

#if (CGR_UNIBO == 0)

#ifndef ONE_ROUTE_PER_NEIGHBOR
#define ONE_ROUTE_PER_NEIGHBOR 0
#endif

//Added by A. Stramiglio 19/12/19
#ifndef AVOID_LOOP_ENTRY_NODE
#define AVOID_LOOP_ENTRY_NODE 0
#endif

#ifndef QUEUE_DELAY
#define QUEUE_DELAY 1
#endif

#ifndef MODERATE_SOURCE_ROUTING
#define MODERATE_SOURCE_ROUTING 0
#endif

#ifndef	WISE_NODE
#define	WISE_NODE	0
#endif

#ifndef DIJKSTRA_ENHANCED
#define DIJKSTRA_ENHANCED 1
#endif

#endif

#include "bpP.h"


#ifdef __cplusplus
extern "C" {
#endif

// A CGR tracepoint
typedef enum
{
	// CgrBuildRoutes(uvast stationNode, unsigned int payloadLength,
	//                unsigned int atTime)
	CgrBuildRoutes,
	// CgrInvalidTerminusNode(void)
	CgrInvalidTerminusNode,

	// CgrBeginRoute(int payloadClass)
	CgrBeginRoute,
	// CgrConsiderRoot(uvast fromNode, uvast toNode)
	CgrConsiderRoot,
	// CgrConsiderContact(uvast fromNode, uvast toNode)
	CgrConsiderContact,
	// CgrIgnoreContact(CgrReason reason)
	CgrIgnoreContact,

	// CgrCost(unsigned int transmitTime, unsigned int owlt,
	//		unsigned int arrivalTime)
	CgrCost,
	// CgrHop(uvast fromNode, uvast toNode)
	CgrHop,

	// CgrProposeRoute(uvast firstHop, unsigned int fromTime,
	//		unsigned int deliveryTime, uvast maxVolume,
	//		int payloadClass)
	CgrProposeRoute,
	// CgrDiscardRoute(void)
	CgrDiscardRoute,

	// CgrIdentifyRoutes(unsigned int deadline)
	CgrIdentifyRoutes,
	// CgrFirstRoute(void)
	CgrFirstRoute,
	// CgrNoMoreRoutes(void)
	CgrNoMoreRoutes,
	// CgrCheckRoute(int payloadClass, uvast firstHop,
	// 		unsigned int fromTime, unsigned int deliveryTime)
	CgrCheckRoute,
	// CgrExpiredRoute(void)
	CgrExpiredRoute,
	// CgrExcludeRoute(CgrReason reason)
	CgrExcludeRoute,
	// CgrUncertainEntry(CgrReason reason)
	CgrUncertainEntry,
	// CgrWrongViaNode(void)
	CgrWrongViaNode,
	// CgrAddRoute(void)
	CgrAddRoute,
	// CgrUpdateRoute(CgrReason reason)
	CgrUpdateRoute,

	// CgrSelectRoutes(void)
	CgrSelectRoutes,
	// CgrUseAllRoutes(void)
	CgrUseAllRoutes,
	// CgrConsiderRoute(uvast proxNode)
	CgrConsiderRoute,
	// CgrSelectRoute(void)
	CgrSelectRoute,
	// CgrSkipRoute(CgrReason reason)
	CgrSkipRoute,
	// CgrUseRoute(uvast proxNode)
	CgrUseRoute,
	// CgrNoRoute(void)
	CgrNoRoute,
	// CgrFullOverbooking(double overbooking)
	CgrFullOverbooking,
	// CgrPartialOverbooking(double overbooking)
	CgrPartialOverbooking,

	// End of valid trace types
	CgrTraceTypeMax,
} CgrTraceType;

// Describes the reason CGR made a certain decision
typedef enum
{
	// Reasons to ignore a contact (CgrIgnoreContact)
	CgrContactEndsEarly,
	CgrSuppressed,
	CgrVisited,
	CgrNoRange,

	// Reasons to ignore a route (CgrExcludeRoute)
	CgrRouteViaSelf,
	CgrRouteVolumeTooSmall,
	CgrInitialContactExcluded,
	CgrRouteTooSlow,
	CgrRouteCongested,
	CgrNoPlan,
	CgrBlockedPlan,
	CgrMaxPayloadTooSmall,
	CgrNoResidualVolume,
	CgrResidualVolumeTooSmall,

	// Reasons to ignore a route (CgrExcludeRoute,
	// CgrSkipRoute) or reasons a previously-selected
	// route was ignored (CgrUpdateRoute)
	CgrMoreHops,
	CgrEarlierTermination,
	CgrNoHelp,
	CgrLowerVolume,
	CgrLaterArrivalTime,
	CgrLargerNodeNbr,

	CgrReasonMax,
} CgrReason;

typedef void		(*CgrTraceFn)(void *data, unsigned int lineNbr,
				CgrTraceType traceType, ...);
typedef struct
{
	CgrTraceFn	fn;	/*	Function to call at tracepoint.	*/
	void		*data;	/*	Data to pass to the function.	*/
} CgrTrace;

#define TRACE(...) do \
{ \
	if (trace) \
	{ \
		trace->fn(trace->data, __LINE__, __VA_ARGS__); \
	} \
} while (0)

/*		IPN-specific RFX data structures.			*/

typedef void *CgrSAP;

typedef struct
{
	PsmAddress	rootOfSpur;	/*	Within *prior* route.	*/
	int		spursComputed;	/*	Boolean.		*/

	/*	Address of list element referencing this route, in
		either a knownRoutes list or a selectedRoutes list.	*/

	PsmAddress	referenceElt;

	/*	Contact that forms the initial hop of the route.	*/

	uvast		toNodeNbr;	/*	Initial-hop neighbor.	*/
	time_t		fromTime;	/*	As from time(2).	*/

	/*	Time at which route shuts down: earliest contact
	 *	end time among all contacts in the end-to-end path.	*/

	time_t		toTime;		/*	As from time(2).	*/

	/*	Details of the route.					*/

	float		arrivalConfidence;
	time_t		arrivalTime;	/*	Earliest arrival time.	*/
	PsmAddress	hops;		/*	SM list: IonCXref addr.	*/

	/*	Transient values, valid only for the routing of the
	 *	current bundle.  Note that, to reduce possible
	 *	confusion, we refer to "projected bundle arrival time"
	 *	as "pbat", reserving the term "arrivalTime"
	 *	to mean "earliest arrival time" - which is a property
	 *	of the route's sequence of contacts, without reference
	 *	to any particular bundle.				*/

	Scalar		overbooked;	/*	Bytes needing reforward.*/
	Scalar		committed;	/*	Bytes not overbooked.	*/
	double		maxVolumeAvbl;
	size_t		bundleECCC;
	time_t		eto;		/*	Earliest xmit oppor'ty.	*/
	time_t		pbat;		/*	Proj. bundle arr. time.	*/

	/*	NOTE: initial transmission on the "spur" portion of
	 *	this route is from the contact identified by rootOfSpur
	 *	to the contact identified by the first entry in the
	 *	hops list.  For a route that is not a branch off of
	 *	any other route, rootOfSpur is zero indicating that the
	 *	initial transmission on the "spur" portion of this
	 *	route (which is the entire route) is from the root
	 *	of the contact graph to the first contact in "hops".	*/
} CgrRoute;

typedef struct
{
	/*	ION supports the concept that different endpoints in
	 *	the network might be reachable using different routing
	 *	systems, so "routing object" is an abstraction.  The
	 *	particular structure of the routing objects used in
	 *	forwarding to "ipn"-scheme endpoints is defined here.
 	 *
	 *	Routing objects are properties of nodes, which are
	 *	realized in IonNode objects.  Each CgrRtgObject
	 *	structure containing the addresses of two SmLists
	 *	that are the lists of routes required for Yen's
	 *	algorithm: selectedRoutes is the A list and knownRoutes
	 *	is the B list.
	 *
	 *	The *list user data* of each of these SmList objects
	 *	is the address of the IonNode structure for the 
	 *	destination node of which the routing object is a
	 *	property.
	 *
	 *	The *entries* of each of these SmList structures
	 *	contain the addresses of CgrRoute structures.
	 *
	 *	The CgrRtgObject also contains a list of the local
	 *	node's neighbors (proximateNodes) within each
	 *	region in which the local node resides.  This is
	 *	for use in forwarding Critical bundles.
	 *
	 *	The CgrRtgObject also contains a list of all of the
	 *	passageway nodes (ultimately just one) through which
	 *	a bundle destined for this remote node should be
	 *	forwarded in the event that the node is in some
	 *	foreign region.						*/

	PsmAddress	nodeAddr;	/*	Back-reference.		*/
	PsmAddress	selectedRoutes;	/*	SmList of CgrRoute.	*/
	PsmAddress	knownRoutes;	/*	SmList of CgrRoute.	*/
	PsmAddress	proximateNodes;	/*	SmList of uvast node#s.	*/
	PsmAddress	viaPassageways;	/*	SmList of uvast node#s.	*/
} CgrRtgObject;	/*	IonNode's routingObject is one of these.	*/

/*		Data structure for the CGR volatile database.		*/

typedef struct
{
	struct timeval	lastLoadTime;	/*	Add/del contacts/ranges	*/

	/*	Since routing objects are properties of nodes, the
	 *	normal way to access a routing object is via the
	 *	routing object pointer in the IonNode object.  But
	 *	for some purposes we need a listing of all of the
	 *	routing objects in the database; the CgrVdb provides
	 *	this listing.
	 *
	 *	There is one entry in the routingObjects list for each
	 *	remote destination node that has got a routing object.	*/

	PsmAddress	routingObjects;	/*	SmList of CgrRtgObject.	*/
} CgrVdb;

#include "ext/cgrr/cgrr.h"

//Added by L. Persampieri
#if (CGR_UNIBO == 1)
#include "../cgr/Unibo-CGR/ion_bpv7/interface/interface_cgr_ion.h"
#endif

extern void		cgr_start();
extern CgrVdb		*cgr_get_vdb();
extern void		cgr_clear_vdb(CgrVdb *);
extern int		cgr_create_routing_object(IonNode *node);
extern int		cgr_start_SAP(uvast, time_t, CgrSAP *sap);
extern int		cgr_identify_best_routes(IonNode *terminusNode,
				Bundle *bundle, Lyst excludedNodes, time_t currentTime,
				CgrSAP sap, CgrTrace *trace, Lyst bestRoutes);
extern void		cgr_stop_SAP(CgrSAP sap);
extern float		cgr_get_dlv_confidence(Bundle *bundle, CgrRoute *route);
extern int		cgr_preview_forward(uvast terminusNodeNbr,
				Bundle *bundle, time_t atTime, CgrSAP sap, CgrTrace *trace);
extern int		cgr_prospect(uvast terminusNode, unsigned int deadline);
extern const char	*cgr_tracepoint_text(CgrTraceType traceType);
extern const char	*cgr_reason_text(CgrReason reason);
extern void		cgr_stop();


/************ Added by G.M. De Cola *************/
#if ONE_ROUTE_PER_NEIGHBOR
extern int tryNeighboringBestRoutes(Bundle *bundle, Lyst bestRoutes, uvast loopNode);
#endif

/************* Added by F. Marchetti *************/
/************ Ported by G.M. De Cola *************/
#if MODERATE_SOURCE_ROUTING
extern int trySourceRouting(Bundle *bundle, CgrTrace *trace,uvast terminusNodeNbr, Object bundleObj);
extern int computeNewRoute(CGRRoute* extensionRoute, CGRRoute* newRoute, uvast terminusNodeNbr);
extern int getTerminusNode(uvast terminusNodeNbr, IonNode** terminusNode);
extern int findExistingIonRoute(IonNode* terminusNode, time_t currentTime, CGRRoute* newRoute, CgrRoute** route);
extern int computeRouteFromContacts(Bundle* bundle, CGRRoute *extensionRoute, CgrRoute** route, time_t currentTime);
extern int trySRRoute(CgrRoute* route, Bundle* bundle, CgrTrace* trace,	time_t currentTime);

//constant definition
#define MSR_SUCCESS 0
#define MSR_UNSPECIFIED_ERROR -1
#define MSR_EXTENSION_NOT_FOUND -2
#define MSR_CANNOT_COMPUTE_NEW_ROUTE -3
#define MSR_UNABLE_TO_COMPUTE_EQIV_ROUTE -4
#define MSR_LAST_COMP_ROUTE_NOT_VIABLE -5
#define MSR_UNABLE_TO_QUEUE -6
#define MSR_UNKNOWN_TERMINUS -7
#define MSR_CANNOT_RETRIEVE_EXT_BLOCK -8
#define MSR_NODE_NOT_FOUND_IN_ROUTE -9
#define MSR_UNABLE_TO_TRY_ROUTE -10
#define MSR_NO_KNOWN_ROUTES -11
#define MSR_NO_PRIOR_ROUTE_VIABLE -12
#define MSR_DIFFERENT_NO_HOPS -13
#define MSR_ROUTES_ARE_DIFFERENT -14
#define MSR_NO_MATCHING_CONTACT -15
#define MSR_CANNOT_GET_TRANSIT_TIME -16

#endif
/*************************************************/

#ifdef __cplusplus
}
#endif

#endif  /* _CGR_H_ */

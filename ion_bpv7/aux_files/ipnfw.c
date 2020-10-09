/*
	ipnfw.c:	scheme-specific forwarder for the "ipn"
			scheme, used for Interplanetary Internet.

	Author: Scott Burleigh, JPL

	Copyright (c) 2006, California Institute of Technology.
	ALL RIGHTS RESERVED.  U.S. Government Sponsorship
	acknowledged.
									*/
#include <stdarg.h>

#include "ipnfw.h"

#if(CGR_UNIBO == 0)
//Added by A. Stramiglio 19/11/19
#include "../library/ext/rgr/rgr.h"
//Added by A. Stramiglio 21/11/19
#include "../library/ext/rgr/rgr_utils.h"
//Added by A.Stramiglio 28/11/19
#include <strings.h>

#include "ext/cgrr/cgrr.h"
#endif

#ifdef	ION_BANDWIDTH_RESERVED
#define	MANAGE_OVERBOOKING	0
#endif

#ifndef	MANAGE_OVERBOOKING
#define	MANAGE_OVERBOOKING	1
#endif

#ifndef	MIN_PROSPECT
#define	MIN_PROSPECT	(0.0)
#endif

#ifndef CGR_DEBUG
#define CGR_DEBUG	1
#endif

static CgrSAP	cgrSap(CgrSAP *newSap)
{
	static CgrSAP	sap;

	if (newSap)
	{
		sap = *newSap;
	}

	return sap;
}

#if CGR_DEBUG == 1
static void	printCgrTraceLine(void *data, unsigned int lineNbr,
			CgrTraceType traceType, ...)
{
	va_list args;
	const char *text;

	va_start(args, traceType);
	text = cgr_tracepoint_text(traceType);
	vprintf(text, args);
	switch (traceType)
	{
	case CgrIgnoreContact:
	case CgrExcludeRoute:
	case CgrSkipRoute:
		fputc(' ', stdout);
		fputs(cgr_reason_text(va_arg(args, CgrReason)), stdout);
	default:
		break;
	}

	putchar('\n');
	//Added by G.M. De Cola
	fflush(stdout);
	va_end(args);
}
#endif

static sm_SemId		_ipnfwSemaphore(sm_SemId *newValue)
{
	uaddr		temp;
	void		*value;
	sm_SemId	sem;

	if (newValue)			/*	Add task variable.	*/
	{
		temp = *newValue;
		value = (void *) temp;
		value = sm_TaskVar(&value);
	}
	else				/*	Retrieve task variable.	*/
	{
		value = sm_TaskVar(NULL);
	}

	temp = (uaddr) value;
	sem = temp;
	return sem;
}

static void	shutDown(int signum)
{
	isignal(SIGTERM, shutDown);
	sm_SemEnd(_ipnfwSemaphore(NULL));
}

/*		CGR override functions.					*/

static int	applyRoutingOverride(Bundle *bundle, Object bundleObj,
			uvast nodeNbr)
{
	Sdr		sdr = getIonsdr();
	Object		addr;
			OBJ_POINTER(IpnOverride, ovrd);
	char		eid[MAX_EID_LEN + 1];
	VPlan		*vplan;
	PsmAddress	vplanElt;
	BpPlan		plan;

	if (ipn_lookupOvrd(bundle->ancillaryData.dataLabel,
			bundle->id.source.ssp.ipn.nodeNbr,
			bundle->destination.ssp.ipn.nodeNbr, &addr) == 0)
	{
		/*	No applicable routing override.			*/

		return 0;
	}

	/*	Routing override found.					*/

	GET_OBJ_POINTER(sdr, IpnOverride, ovrd, addr);
	if (ovrd->neighbor == 0)
	{
		/*	Override neighbor not yet determined.		*/

		bundle->ovrdPending = 1;
		sdr_write(sdr, bundleObj, (char *) bundle, sizeof(Bundle));
		return 0;
	}

	/*	Must forward to override neighbor.			*/

	isprintf(eid, sizeof eid, "ipn:" UVAST_FIELDSPEC ".0", ovrd->neighbor);
	findPlan(eid, &vplan, &vplanElt);
	if (vplanElt == 0)	/*	Not a usable override.		*/
	{
		return 0;
	}

	sdr_read(sdr, (char *) &plan, sdr_list_data(sdr, vplan->planElt),
			sizeof(BpPlan));
	if (plan.blocked)	/*	Maybe later.			*/
	{
		if (enqueueToLimbo(bundle, bundleObj) < 0)
		{
			putErrmsg("Can't put bundle in limbo.", NULL);
			return -1;
		}

		return 0;
	}

	/*	Invoke the routing override.				*/

	if (bpEnqueue(vplan, bundle, bundleObj) < 0)
	{
		putErrmsg("Can't enqueue bundle.", NULL);
		return -1;
	}

	return 0;
}

static void	bindOverride(Bundle *bundle, Object bundleObj, uvast nodeNbr)
{
	Sdr		sdr = getIonsdr();
	Object		ovrdAddr;
	IpnOverride	ovrd;

	bundle->ovrdPending = 0;
	sdr_write(sdr, bundleObj, (char *) bundle, sizeof(Bundle));
	if (ipn_lookupOvrd(bundle->ancillaryData.dataLabel,
			bundle->id.source.ssp.ipn.nodeNbr,
			bundle->destination.ssp.ipn.nodeNbr, &ovrdAddr) == 0)
	{
		return;		/*	No pending override to bind.	*/
	}

	sdr_stage(sdr, (char *) &ovrd, ovrdAddr, sizeof(IpnOverride));
	if (ovrd.neighbor == 0)
	{
		ovrd.neighbor = nodeNbr;
		sdr_write(sdr, ovrdAddr, (char *) &ovrd, sizeof(IpnOverride));
	}
}

/*		HIRR invocation functions.				*/

static int	initializeHIRR(CgrRtgObject *routingObj)
{
	Sdr		sdr = getIonsdr();
	PsmPartition	ionwm = getIonwm();
	IonDB		iondb;
	int		i;
	Object		elt;
	Object		addr;
			OBJ_POINTER(RegionMember, member);

	routingObj->viaPassageways = sm_list_create(ionwm);
	if (routingObj->viaPassageways == 0)
	{
		putErrmsg("Can't initialize HIRR routing.", NULL);
		return -1;
	}

	sdr_read(sdr, (char *) &iondb, getIonDbObject(), sizeof(IonDB));

	/*	For each region in which the local node resides, add
	 *	to the viaPassageways list for this remote node one
	 *	entry for every passageway residing in that region.	*/

	for (i = 0; i < 2; i++)
	{
		for (elt = sdr_list_first(sdr, iondb.regions[i].members); elt;
					elt = sdr_list_next(sdr, elt))
		{
			addr = sdr_list_data(sdr, elt);
			GET_OBJ_POINTER(sdr, RegionMember, member, addr);
			if (member->nodeNbr == getOwnNodeNbr())
			{
				continue;	/*	Safety check.	*/
			}

			if (member->outerRegionNbr != -1)
			{
				/*	Node is a passageway.		*/

				if (sdr_list_insert_last(sdr,
						routingObj->viaPassageways,
						member->nodeNbr) == 0)
				{
					putErrmsg("Can't note passageway.",
							NULL);
					return -1;
				}
			}
		}
	}

	return 0;
}

static int 	tryHIRR(Bundle *bundle, Object bundleObj, IonNode *terminusNode,
			time_t atTime)
{
	PsmPartition	ionwm = getIonwm();
	CgrRtgObject	*routingObj;

	if (terminusNode->routingObject == 0)
	{
		if (cgr_create_routing_object(terminusNode) < 0)
		{
			putErrmsg("Can't initialize routing object.", NULL);
			return -1;
		}
	}

	routingObj = (CgrRtgObject *) psp(ionwm, terminusNode->routingObject);
	if (routingObj->viaPassageways == 0)
	{
		if (initializeHIRR(routingObj) < 0)
		{
			return -1;
		}
	}

	return 0;
}

/*		CGR invocation functions.				*/

static void	deleteObject(LystElt elt, void *userdata)
{
	void	*object = lyst_data(elt);

	if (object)
	{
		MRELEASE(lyst_data(elt));
	}
}

static int	excludeNode(Lyst excludedNodes, uvast nodeNbr)
{
	//NodeId	*node = (NodeId *) MTAKE(sizeof(NodeId));
	uvast	*node = (uvast *) MTAKE(sizeof(uvast));
	if (node == NULL)
	{
		return -1;
	}

	*node = nodeNbr;
	if (lyst_insert_last(excludedNodes, node) == NULL)
	{
		return -1;
	}

	return 0;
}

static size_t	carryingCapacity(size_t avblVolume)
{
	size_t	computedCapacity = avblVolume / 1.0625;
	size_t	typicalCapacity;

	if (avblVolume > TYPICAL_STACK_OVERHEAD)
	{
		typicalCapacity = avblVolume - TYPICAL_STACK_OVERHEAD;
	}
	else
	{
		typicalCapacity = 0;
	}

	if (computedCapacity < typicalCapacity)
	{
		return computedCapacity;
	}
	else
	{
		return typicalCapacity;
	}
}

static int	proactivelyFragment(Bundle *bundle, Object *bundleObj,
			CgrRoute *route)
{
	Sdr		sdr = getIonsdr();
	Object		stationEidElt;
	Object		stationEid;
	char		eid[SDRSTRING_BUFSZ];
	MetaEid		stationMetaEid;
	VScheme		*vscheme;
	PsmAddress	vschemeElt;
	size_t		fragmentLength;
	Bundle		firstBundle;
	Object		firstBundleObj;
	Bundle		secondBundle;
	Object		secondBundleObj;
	Scheme		schemeBuf;

	CHKERR(bundle->payload.length > 1);
	stationEidElt = sdr_list_first(sdr, bundle->stations);
	CHKERR(stationEidElt);
	stationEid = sdr_list_data(sdr, stationEidElt);
	CHKERR(stationEid);
	if (sdr_string_read(sdr, eid, stationEid) < 0)
	{
		return -1;
	}

	if (parseEidString(eid, &stationMetaEid, &vscheme, &vschemeElt) == 0)
	{
		restoreEidString(&stationMetaEid);
		putErrmsg("Bad station EID", eid);
		return -1;
	}

	fragmentLength = carryingCapacity(route->maxVolumeAvbl);
	if (fragmentLength == 0)
	{
		fragmentLength = 1;	/*	Assume rounding error.	*/
	}

	if (fragmentLength >= bundle->payload.length)
	{
		fragmentLength = bundle->payload.length - 1;
	}
	debugPrint(
			"[ipnfw.c/proactivelyFragment] Fragmenting bundle - bundle source: %llu, bundle timestamp: %u, fragm. offset: %u\n\t\t\t",
			bundle->id.source.ssp.ipn.nodeNbr, bundle->id.creationTime.seconds,
			bundle->id.fragmentOffset);

	if (bpFragment(bundle, *bundleObj, NULL, fragmentLength,
			&firstBundle, &firstBundleObj, &secondBundle,
			&secondBundleObj) < 0)
	{
		return -1;
	}

	/*	Send the second fragment back through the routing
	 *	procedure; adapted from forwardBundle().		*/

	restoreEidString(&stationMetaEid);
	stationEid = sdr_string_create(sdr, eid);
	if (stationEid == 0
	|| sdr_list_insert_first(sdr, secondBundle.stations, stationEid) == 0)
	{
		putErrmsg("Can't note station for second fragment", eid);
		return -1;
	}

	sdr_read(sdr, (char *) &schemeBuf, sdr_list_data(sdr,
			vscheme->schemeElt), sizeof(Scheme));
	secondBundle.fwdQueueElt = sdr_list_insert_first(sdr,
			schemeBuf.forwardQueue, secondBundleObj);
	sdr_write(sdr, secondBundleObj, (char *) &secondBundle, sizeof(Bundle));
	if (vscheme->semaphore != SM_SEM_NONE)
	{
		sm_SemGive(vscheme->semaphore);
	}

	/*	Return the first fragment to be enqueued per plan.	*/

	*bundleObj = firstBundleObj;
	memcpy((char *) bundle, (char *) &firstBundle, sizeof(Bundle));
	debugPrint(
			"[ipnfw.c/proactivelyFragment] Bundle fragmented, enqueueing first fragment and sending second fragment back through the routing procedure");
	return 0;
}

static int	enqueueToEntryNode(CgrRoute *route, Bundle *bundle,
			Object bundleObj, IonNode *terminusNode)
{
	Sdr		sdr = getIonsdr();
	PsmPartition	ionwm = getIonwm();
	BpEvent		event;
	char		neighborEid[MAX_EID_LEN + 1];
	VPlan		*vplan;
	PsmAddress	vplanElt;
	int		priority;
	PsmAddress	elt;
	PsmAddress	addr;
	IonCXref	*contact;
	Object		contactObj;
	IonContact	contactBuf;
	int		i;

	if (bundle->ovrdPending)
	{
		bindOverride(bundle, bundleObj, route->toNodeNbr);
	}

	/*	Note that a copy is being sent on the route through
	 *	this neighbor.						*/

	if (bundle->xmitCopiesCount == MAX_XMIT_COPIES)
	{
		return 0;	/*	Reached forwarding limit.	*/
	}

	bundle->xmitCopies[bundle->xmitCopiesCount] = route->toNodeNbr;
	bundle->xmitCopiesCount++;
	bundle->dlvConfidence = cgr_get_dlv_confidence(bundle, route);

	/*	If the bundle is NOT critical, then:			*/

	if (!(bundle->ancillaryData.flags & BP_MINIMUM_LATENCY))
	{
		/*	We may need to do anticipatory fragmentation
		 *	of the bundle before enqueuing it for
		 *	transmission.					*/

		if (route->maxVolumeAvbl < route->bundleECCC
		&& bundle->payload.length > 1
		&& !(bundle->bundleProcFlags & BDL_DOES_NOT_FRAGMENT))
		{
//printf("*** fragmenting; to node %lu, volume avbl %lu, bundle ECCC %lu.\n", route->toNodeNbr, route->maxVolumeAvbl, route->bundleECCC);
			if (proactivelyFragment(bundle, &bundleObj, route) < 0)
			{
				putErrmsg("Anticipatory fragmentation failed.",
						NULL);
				return -1;
			}
		}

		/*	In any case, we need to post an xmitOverdue
		 *	timeout event to trigger re-forwarding in case
		 *	the bundle doesn't get transmitted during the
		 *	contact in which we expect that to happen.	*/

		event.type = xmitOverdue;
		addr = sm_list_data(ionwm, sm_list_first(ionwm, route->hops));
		contact = (IonCXref *) psp(ionwm, addr);
		event.time = contact->toTime;
		event.ref = bundleObj;
		bundle->overdueElt = insertBpTimelineEvent(&event);
		if (bundle->overdueElt == 0)
		{
			putErrmsg("Can't schedule xmitOverdue.", NULL);
			return -1;
		}

		sdr_write(getIonsdr(), bundleObj, (char *) bundle,
				sizeof(Bundle));
	}

	/*	In any event, we enqueue the bundle for transmission.
	 *	Since we've already determined that the plan to this
	 *	neighbor is not blocked (else the route would not
	 *	be in the list of best routes), the bundle can't go
	 *	into limbo at this point.				*/

	isprintf(neighborEid, sizeof neighborEid, "ipn:" UVAST_FIELDSPEC ".0",
			route->toNodeNbr);
	findPlan(neighborEid, &vplan, &vplanElt);
	CHKERR(vplanElt);
	if (bpEnqueue(vplan, bundle, bundleObj) < 0)
	{
		putErrmsg("Can't enqueue bundle.", NULL);
		return -1;
	}

	/*	And we reserve transmission volume for this bundle
	 *	on every contact along the end-to-end path for the
	 *	bundle.			
	 				*/
	 debugPrint(
			"[ipnfw.c/enqueueToEntryNode] Bundle sent - bundle source: %llu, bundle timestamp (DTN time): %u\n\t\t\tbundle timestamp (ION start offset): %u, fragm. offset: %u, sent to: %llu",
			bundle->id.source.ssp.ipn.nodeNbr, bundle->id.creationTime.seconds,
			(bundle->id.creationTime.seconds + EPOCH_2000_SEC) - ionReferenceTime(NULL),
			bundle->id.fragmentOffset, route->toNodeNbr);
	 

	priority = bundle->priority;
	#if !CGR_UNIBO && CGRREB && WISE_NODE //modified by F. Marchetti


	/****************added by LMazzuca****************/
	int j = 0;
	int hopCount = sm_list_length(ionwm, route->hops);
	CGRRHop routeHopList[hopCount];
	/*************************************************/
#endif
	for (elt = sm_list_first(ionwm, route->hops); elt;
			elt = sm_list_next(ionwm, elt))
	{
		addr = sm_list_data(ionwm, elt);
		contact = (IonCXref *) psp(ionwm, addr);
		contactObj = sdr_list_data(sdr, contact->contactElt);
		sdr_stage(sdr, (char *) &contactBuf, contactObj,
				sizeof(IonContact));
		for (i = priority; i >= 0; i--)
		{
			contactBuf.mtv[i] -= route->bundleECCC;
		}

		sdr_write(sdr, contactObj, (char *) &contactBuf,
				sizeof(IonContact));
#if !CGR_UNIBO && CGRREB && WISE_NODE //modified by F. Marchetti
		/****************added by LMazzuca****************/
		routeHopList[j].fromNode = contact->fromNode;
		routeHopList[j].toNode = contact->toNode;
		routeHopList[j].fromTime = contact->fromTime;
		j++;
		/*************************************************/
#endif
	}
#if !CGR_UNIBO && CGRREB && WISE_NODE //modified by F. Marchetti
	/****************added by LMazzuca****************/
	debugPrint("[ipnfw.c/enqueueToEntryNode] Should saveRouteToExtBlock");
	saveRouteToExtBlock(hopCount, routeHopList, bundle);
	/*************************************************/
#endif

	return 0;
}

#if (MANAGE_OVERBOOKING == 1)
typedef struct
{
	Object	currentElt;	/*	SDR list element.		*/
	Object	limitElt;	/*	SDR list element.		*/
} QueueControl;

static Object	getUrgentLimitElt(BpPlan *plan, int ordinal)
{
	Sdr	sdr = getIonsdr();
	int	i;
	Object	limitElt;

	/*	Find last bundle enqueued for the lowest ordinal
	 *	value that is higher than the bundle's ordinal;
	 *	limit elt is the next bundle in the urgent queue
	 *	following that one (i.e., the first enqueued for
	 *	the bundle's ordinal).  If none, then the first
	 *	bundle in the urgent queue is the limit elt.		*/

	for (i = ordinal + 1; i < 256; i++)
	{
		limitElt = plan->ordinals[i].lastForOrdinal;
		if (limitElt)
		{
			return sdr_list_next(sdr, limitElt);
		}
	}

	return sdr_list_first(sdr, plan->urgentQueue);
}

static Object	nextBundle(QueueControl *queueControls, int *queueIdx)
{
	Sdr		sdr = getIonsdr();
	QueueControl	*queue;
	Object		currentElt;

	queue = queueControls + *queueIdx;
	while (queue->currentElt == 0)
	{
		(*queueIdx)++;
		if ((*queueIdx) > BP_EXPEDITED_PRIORITY)
		{
			return 0;
		}

		queue++;
	}

	currentElt = queue->currentElt;
	if (currentElt == queue->limitElt)
	{
		queue->currentElt = 0;
	}
	else
	{
		queue->currentElt = sdr_list_prev(sdr, queue->currentElt);
	}

	return currentElt;
}

static int	manageOverbooking(CgrRoute *route, Bundle *newBundle,
			CgrTrace *trace)
{
	Sdr		sdr = getIonsdr();
	char		neighborEid[MAX_EID_LEN + 1];
	VPlan		*vplan;
	PsmAddress	vplanElt;
	Object		planObj;
	BpPlan		plan;
	QueueControl	queueControls[] = { {0, 0}, {0, 0}, {0, 0} };
	int		queueIdx = 0;
	int		priority;
	int		ordinal;
	double		protected = 0.0;
	double		overbooked = 0.0;
	Object		elt;
	Object		bundleObj;
	Bundle		bundle;
	int		eccc;

	isprintf(neighborEid, sizeof neighborEid, "ipn:" UVAST_FIELDSPEC ".0",
			route->toNodeNbr);
	priority = newBundle->priority;
	if (priority == 0)
	{
		/*	New bundle's priority is Bulk, can't possibly
		 *	bump any other bundles.				*/

		return 0;
	}

	overbooked += (ONE_GIG * route->overbooked.gigs)
			+ route->overbooked.units;
	if (overbooked == 0.0)
	{
		return 0;	/*	No overbooking to manage.	*/
	}

	protected += (ONE_GIG * route->committed.gigs)
			+ route->committed.units;
	if (protected == 0.0)
	{
		TRACE(CgrPartialOverbooking, overbooked);
	}
	else
	{
		TRACE(CgrFullOverbooking, overbooked);
	}

	findPlan(neighborEid, &vplan, &vplanElt);
	if (vplanElt == 0)
	{
		TRACE(CgrSkipRoute, CgrNoPlan);

		return 0;		/*	No egress plan to node.	*/
	}

	planObj = sdr_list_data(sdr, vplan->planElt);
	sdr_read(sdr, (char *) &plan, planObj, sizeof(BpPlan));
	queueControls[0].currentElt = sdr_list_last(sdr, plan.bulkQueue);
	queueControls[0].limitElt = sdr_list_first(sdr, plan.bulkQueue);
	if (priority > 1)
	{
		queueControls[1].currentElt = sdr_list_last(sdr,
				plan.stdQueue);
		queueControls[1].limitElt = sdr_list_first(sdr,
				plan.stdQueue);
		ordinal = newBundle->ordinal;
		if (ordinal > 0)
		{
			queueControls[2].currentElt = sdr_list_last(sdr,
					plan.urgentQueue);
			queueControls[2].limitElt = getUrgentLimitElt(&plan,
					ordinal);
		}
	}

	while (overbooked > 0.0)
	{
		elt = nextBundle(queueControls, &queueIdx);
		if (elt == 0)
		{
			break;
		}

		bundleObj = sdr_list_data(sdr, elt);
		sdr_stage(sdr, (char *) &bundle, bundleObj, sizeof(Bundle));
		eccc = computeECCC(guessBundleSize(&bundle));

		/*	Skip over all bundles that are protected
		 *	from overbooking because they are in contacts
		 *	following the contact in which the new bundle
		 *	is scheduled for transmission.			*/

		if (protected > 0.0)
		{
			protected -= eccc;
			continue;
		}

		/*	The new bundle has bumped this bundle out of
		 *	its originally scheduled contact.  Rebook it.	*/

		sdr_stage(sdr, (char *) &plan, planObj, sizeof(BpPlan));
		removeBundleFromQueue(&bundle, &plan);
		sdr_write(sdr, planObj, (char *) &plan, sizeof(BpPlan));
		sdr_write(sdr, bundleObj, (char *) &bundle, sizeof(Bundle));
		if (bpReforwardBundle(bundleObj) < 0)
		{
			putErrmsg("Overbooking management failed.", NULL);
			return -1;
		}

		overbooked -= eccc;
	}

	return 0;
}
#endif

static int	proxNodeRedundant(Bundle *bundle, vast nodeNbr)
{
	int	i;

	for (i = 0; i < bundle->xmitCopiesCount; i++)
	{
		if (bundle->xmitCopies[i] == nodeNbr)
		{
			return 1;
		}
	}

	return 0;
}

static int	sendCriticalBundle(Bundle *bundle, Object bundleObj,
			IonNode *terminusNode, Lyst bestRoutes, int preview, int result_cgr)
{
	LystElt		elt;
	LystElt		nextElt;
	CgrRoute	*route;
	Bundle		newBundle;
	Object		newBundleObj;

	/*	Enqueue the bundle on the plan for the entry node of
	 *	EACH identified best route.				*/

	for (elt = lyst_first(bestRoutes); elt; elt = nextElt)
	{
		nextElt = lyst_next(elt);
		route = (CgrRoute *) lyst_data_set(elt, NULL);
		lyst_delete(elt);
		if (preview)
		{
			continue;
		}

		if (proxNodeRedundant(bundle, route->toNodeNbr))
		{
			continue;
		}

		if (bundle->planXmitElt)
		{
			/*	This copy of bundle has already
			 *	been enqueued.				*/

			if (bpClone(bundle, &newBundle, &newBundleObj, 0, 0)
					< 0)
			{
				putErrmsg("Can't clone bundle.", NULL);
				lyst_destroy(bestRoutes);
				return -1;
			}

			bundle = &newBundle;
			bundleObj = newBundleObj;
		}

		if (enqueueToEntryNode(route, bundle, bundleObj, terminusNode))
		{
			putErrmsg("Can't queue for neighbor.", NULL);
			lyst_destroy(bestRoutes);
			return -1;
		}
	}

	lyst_destroy(bestRoutes);
	if (bundle->dlvConfidence >= MIN_NET_DELIVERY_CONFIDENCE
	|| bundle->id.source.ssp.ipn.nodeNbr
			== bundle->destination.ssp.ipn.nodeNbr)
	{
		return 0;	/*	Potential future fwd unneeded.	*/
	}

	if(result_cgr == 0)
	{
		return 0; /*	No potential future forwarding.	*/
	}

	/*	Must put bundle in limbo, keep on trying to send it.	*/

	if (bundle->planXmitElt)
	{
		/*	This copy of bundle has already been enqueued.	*/

		if (bpClone(bundle, &newBundle, &newBundleObj, 0, 0) < 0)
		{
			putErrmsg("Can't clone bundle.", NULL);
			return -1;
		}

		bundle = &newBundle;
		bundleObj = newBundleObj;
	}

	if (enqueueToLimbo(bundle, bundleObj) < 0)
	{
		putErrmsg("Can't put bundle in limbo.", NULL);
		return -1;
	}

	return 0;
}

static unsigned char	initializeSnw(unsigned int ttl, uvast toNode)
{
	/*	Compute spray-and-wait "L" value.  The only required
	 *	parameters are the required expected delay "aEDopt"
	 *	and the number of nodes "M".  Expected delay is
	 *	computed as the product of the delay constraint "a"
	 *	(we choose 8 for this value), the expected delay for
	 *	direct transmission (1 second), and the TTL less
	 *	some margin for safety (we discount by 1/8) -- so
	 *	7 * TTL.  The number of nodes is the length of the
	 *	list of members for the region in which the local
	 *	node and the initial contact's "to" node both reside.
	 *
	 *	The computation is very complex, left for later.	*/

	return 16;	/*	Dummy result, for now.			*/
}

static int	forwardOkay(CgrRoute *route, Bundle *bundle)
{
	PsmPartition	ionwm = getIonwm();
	PsmAddress	hopsElt;
	PsmAddress	contactAddr;
	IonCXref	*contact;

	hopsElt = sm_list_first(ionwm, route->hops);
	contactAddr = sm_list_data(ionwm, hopsElt);
	contact = (IonCXref *) psp(ionwm, contactAddr);
	if (contact->type != CtDiscovered)
	{
		return 1;	/*	No Spray and Wait rule applies.	*/
	}

	/*	Discovered contact, must check Spray and Wait.		*/

	if (bundle->permits == 0)	/*	Not sprayed yet.	*/
	{
		bundle->permits = initializeSnw(bundle->timeToLive,
				contact->toNode);
	}

	if (bundle->permits < 2)	/*	(Should never be 0.)	*/
	{
		/*	When SNW permits count is 1 (or 0), the bundle
		 *	can only be forwarded to the final destination
		 *	node.						*/

		if (contact->toNode != bundle->destination.ssp.ipn.nodeNbr)
		{
			return 0;
		}
	}

	return 1;
}
#if MODERATE_SOURCE_ROUTING
/******************************************************************************
 *
 * \par Function Name: trySourceRouting()
 *
 * \par Purpose: This function tests if it is possible to use a route computed
 *               by one of the preceding nodes. If such a route is found and
 *               is safe to be used then it is possible to avoid the use of CGR.
 *
 * \par Date Written:  07/02/19
 *
 * \retval int		MSR_SUCCESS - Success.
 *               	MSR_UNSPECIFIED_ERROR Generic / unspecified error.
 *               	MSR_EXTENSION_NOT_FOUND - The extension is not be present.
 *               	MSR_CANNOT_COMPUTE_NEW_ROUTE - Unable to compute new route to follow
 *               	MSR_UNABLE_TO_COMPUTE_EQIV_ROUTE - Unable to compute an equivalent route
 *               	MSR_LAST_COMP_ROUTE_NOT_VIABLE - Last computed route cannot be used to forward bundle
 *               	MSR_UNABLE_TO_QUEUE - Unable to queue bundle to neighbor
 *               	MSR_UNKNOWN_TERMINUS - This node does not know terminus node
 *               	MSR_CANNOT_RETRIEVE_EXT_BLOCK - Unable to retrieve extension block from memory
 *
 * \param[in]      bundle  	     A pointer to the Bundle arrived from which
 *                               the route has to be extracted.
 * \param[in]      trace
 *
 * \param[in]      terminusNodeNbr
 *                               Identifier for terminus node
 *
 * \param[in]      bundleObj     Bundle object
 *
 * \par Revision History:
 *
 *  DD/MM/YY  AUTHOR                DESCRIPTION
 *  --------  ------------  -----------------------------------------------
 *  07/02/19  F. Marchetti          Created function
 *  08/02/19  F. Marchetti          Added functionalities. Added checks.
 *                                  Added documentation
 *  12/02/19  F. Marchetti          Added route search. Added functionalities
 *  13/02/19  F. Marchetti          Added checks. Added documentation.
 *                                  Added functionalities
 *  14/02/19  F. Marchetti          Major refactor
 *  15/02/19  F. Marchetti          Major refactor
 *  05/03/19  F. Marchetti          Bug fixing
 *  07/03/19  F. Marchetti          Bug fixing
 *  10/03/19  F. Marchetti          Minor refactor
 *  13/03/19  F. Marchetti          Bug fixing
 *  02/10/19  G.M. De Cola			Changed cgrrBlk allocation to psm large pool;
 *  								Anticipated cgrrBlk deallocation.
 *  08/11/19  G.M. De Cola			Ported and moved function from bp/cgr/libcgr.c to
 *  								bp/ipn/ipnfw.c to stay true to ion-3.7.0 function
 *  								refactoring
 *****************************************************************************/

int trySourceRouting(Bundle *bundle, CgrTrace *trace, uvast terminusNodeNbr, Object bundleObj)
{
	Sdr sdr = getIonsdr();
	CGRRouteBlock *cgrrBlk = NULL;
	CGRRoute myRoute;
	CGRRoute newRoute;
	Object extBlkAddr;
	Object extBlockElt;
	time_t currentTime = 0;
	IonNode *terminusNode = NULL;
	CgrRoute *route = NULL;
	PsmPartition ionwm = 0;
	unsigned char routeFound = 0;

	// Initial setup
	ionwm = getIonwm();
	CHKERR(ionwm != 0);

	OBJ_POINTER(ExtensionBlock, blk);

	// Sanity checks
	CHKERR(bundle != NULL);

	debugPrint("[i: ipnfw.c/trySourceRouting] sanity checks done.");

	//Added by G.M. De Cola
	/* Step 0 - Check if this node is the source node. In that case, call CGR*/
	//Added by G.M. De Cola
	debugPrint("\n################################################################\n"
			"[ipnfw.c/trySourceRouting] Step 0 - Check if this node is the source node. In that case, call CGR\n"
			"################################################################\n");
	if(getOwnNodeNbr() == bundle->id.source.ssp.ipn.nodeNbr)
	{
		debugPrint("[i: ipnfw.c/trySourceRouting] This is the source node, exiting function and calling CGR.");
		return MSR_UNSPECIFIED_ERROR;
	}

	/* Step 1 - Check for the presence of CGRR extension*/
	//Added by G.M. De Cola
	debugPrint("\n################################################################\n"
			"[ipnfw.c/trySourceRouting] Step 1 - Check for the presence of CGRR extension\n"
			"################################################################\n");

	if (!(extBlockElt = findExtensionBlock(bundle, CGRRBlk, 0, 0, 0)))
	{
		debugPrint("[i: ipnfw.c/trySourceRouting] No CGRR Extension Block found in bundle.");
		return MSR_EXTENSION_NOT_FOUND;
	}

	debugPrint("[!: ipnfw.c/trySourceRouting] CGRR Extension Block found in bundle.");

	/* Step 2 - Get deserialized version of CGRR extension block*/
	//Added by G.M. De Cola
	debugPrint("\n################################################################\n"
			"[ipnfw.c/trySourceRouting] Step 2 - Get deserialized version of CGRR extension block\n"
			"################################################################\n");

	//CHKERR(sdr_begin_xn(sdr));
	extBlkAddr = sdr_list_data(sdr, extBlockElt);

	GET_OBJ_POINTER(sdr, ExtensionBlock, blk, extBlkAddr);
	//CHKERR(sdr_end_xn(sdr));
	cgrrBlk = (CGRRouteBlock*) psp(ionwm, psm_malloc(ionwm, sizeof(CGRRouteBlock)));

	if (cgrrBlk == NULL)
	{
		debugPrint("[x: ipnfw.c/trySourceRouting] cannot instantiate memory for newCGRRouteBlock.");
		return MSR_UNSPECIFIED_ERROR;
	}

	debugPrint("[!: ipnfw.c/trySourceRouting] instantiated memory for newCGRRouteBlock.");

	if (cgrr_getCGRRFromExtensionBlock(blk, cgrrBlk) <= 0)
	{
		debugPrint(
				"[!: ipnfw.c/trySourceRouting] unable to get extension from extension block");
		releaseCgrrBlkMemory(cgrrBlk);
		return MSR_CANNOT_RETRIEVE_EXT_BLOCK;
	}

	/* Step 3 - Find last computed route */
	//Added by G.M. De Cola
	debugPrint("\n################################################################\n"
			"[ipnfw.c/trySourceRouting] Step 3 - Find last computed route\n"
			"################################################################\n");

	debugPrint("[i: ipnfw.c/trySourceRouting] looking for an earlier computed route inside extension");

	if (cgrrBlk->recRoutesLength == 0)
	{
		// Since recomputedRoute array's length is zero
		// we only have the route computed by source node
		myRoute = cgrrBlk->originalRoute;
		debugPrint("[i: ipnfw.c/trySourceRouting] found original route");
	}
	else
	{
		// RecomputedRoute array's length is not zero, so we take
		// last computed route
		myRoute = cgrrBlk->recomputedRoutes[cgrrBlk->recRoutesLength - 1];
		debugPrint("[i: ipnfw.c/trySourceRouting] found last recomputed route");
	}

	// Route could be empty as CGRRExtensions is created at bundle's creation time
	// If this route is empty (i.e. there are no hops) we are source node or we have found an error
	if (myRoute.hopCount == 0)
	{
		debugPrint("[!: ipnfw.c/trySourceRouting] route found has no hops");
		releaseCgrrBlkMemory(cgrrBlk);
		return MSR_UNSPECIFIED_ERROR;
	}

	/* Step 4 - Compute new route to follow*/
	//Added by G.M. De Cola
	debugPrint("\n################################################################\n"
			"[ipnfw.c/trySourceRouting] Step 4 - Compute new route to follow\n"
			"################################################################\n");
	releaseCgrrBlkMemory(cgrrBlk);// Added by G.M. De Cola: Releasing memory for cgrrBlk because from now on it is never used

	if (computeNewRoute(&myRoute, &newRoute, terminusNodeNbr) < 0)
	{
		debugPrint(
				"[!: ipnfw.c/trySourceRouting] unable to compute a new route based on last computed route");
		//MRELEASE(cgrrBlk);
		return MSR_CANNOT_COMPUTE_NEW_ROUTE;
	}

	debugPrint("[!: ipnfw.c/trySourceRouting] been able to compute a new route based on last computed route");

	/* Step 5   - Find an existing ION-formatted route Find an existing ION-formatted route or compute it from scratch and test it */
	//Added by G.M. De Cola
	debugPrint("\n################################################################\n"
			"[ipnfw.c/trySourceRouting] Step 5 - Find an existing ION-formatted route or compute it from scratch and test it\n"
			"################################################################\n");
	currentTime = getCtime();

	/* Does this node know terminus node?
	 * Knowledge of terminus node is essential since we need to know
	 * if next node is embargoed for terminus destination */
	if (getTerminusNode(terminusNodeNbr, &terminusNode)
			< 0|| terminusNode == NULL)
	{
		//This node does not know terminusNode
		debugPrint(
				"[!: ipnfw.c/trySourceRouting] this node does not have knowledge about the existence of terminusNode");
		//MRELEASE(cgrrBlk);
		return MSR_UNKNOWN_TERMINUS;
	}

	// A reference to terminus node has been found. I.e. this node knows terminus node

	// Modified by G.M. De Cola
	// Initialize routing object if not
	if (terminusNode->routingObject == 0)
	{
		if (initializeRoutingObject(terminusNode) < 0)
		{
			debugPrint("[!: ipnfw.c/trySourceRouting] Can't initialize routing object.");
			return MSR_UNSPECIFIED_ERROR;
		}
		debugPrint("[i: ipnfw.c/trySourceRouting] Successfully initialized routing object");
	}

	// Try to search a previously computed route among CGR computed ones
	if (findExistingIonRoute(terminusNode, currentTime, &newRoute, &route) < 0)
	{
		/* Unable to find an already CGR-computed route that could lead us to terminusNode in time */
		debugPrint("[!: ipnfw.c/trySourceRouting] unable to find an existing route");
	}
	else
	{
		routeFound = 1;
		debugPrint("[!: ipnfw.c/trySourceRouting] found an existing route");
	}

	/* Try to compute route from known contacts */
	if (!routeFound)
	{
		if (computeRouteFromContacts(bundle, &newRoute, &route, currentTime)
				< 0)
		{
			debugPrint(
					"[!: ipnfw.c/trySourceRouting] unable to compute route found in extension block using known contacts");
			//MRELEASE(cgrrBlk);
			return MSR_UNABLE_TO_COMPUTE_EQIV_ROUTE;
		}
		else
		{
			debugPrint(
					"[!: ipnfw.c/trySourceRouting] computed route found in extension block using known contacts");
		}
	}

	if (route == NULL)
	{
		debugPrint("[x: ipnfw.c/trySourceRouting] unexpected null pointer at line: %d",__LINE__);
		//MRELEASE(cgrrBlk);
		return MSR_UNSPECIFIED_ERROR;
	}

	PsmAddress routeAddr = psa(ionwm, route);

	/* ATTENTION
	 * In order to avoid a segmentation fault we TEMPORARILY need to store
	 * route->hops in a variable; the original value is changed because of a bug,
	 * probably a buffer overflow (not yet found), inside saveRouteToExtBlock()
	 * or enqueueToNeighbor().
	 * This bug happens only in routes that have more than 3 hops and only on
	 * route's second and third node. Nodes with an Intel 64-bit architecture
	 * are apparently immune to this bug.
	 *
	 * EDIT(BY G.M. De Cola)
	 * The bug has been found in serializeExtBlock() called by the cgrr_attach() function, called by the addRoute()
	 * function, called by the saveRouteToExtBlock() in the enqueueToNeighbor() function.
	 * The bugfix is located and explained in the function computeRouteFromContacts():3389 in this file.
	 *
	 *
	 */
	PsmAddress hopsAddr = route->hops;
	// Try this route in order to know if it can be used to forward bundle
	//Modified by G.M. De Cola : 08/11/19 - Changed tryNewRoute to trySRRoute due to function renaming
	if (trySRRoute(route, bundle, trace, currentTime) < 0)
	{
		debugPrint("[!: ipnfw.c/trySourceRouting] last computed route cannot be used to forward bundle");
		sm_list_destroy(ionwm, route->hops, NULL, NULL);
		psm_free(ionwm, routeAddr);
		//MRELEASE(cgrrBlk);
		return MSR_LAST_COMP_ROUTE_NOT_VIABLE;
	}

	debugPrint("[!: ipnfw.c/trySourceRouting] last computed route can be used in order to forward bundle");

	//Modified by G.M. De Cola : 08/11/19 - Changed enqueueToNeighbor call to enqueueToEntryNode due to function renaming in ion-3.7.0
	/* Step 6   - enqueue bundle to route's entry node */
	//Added by G.M. De Cola
	debugPrint("\n################################################################\n"
			"[ipnfw.c/trySourceRouting] Step 6 - enqueue bundle to route's entry node\n"
			"################################################################\n");

	debugPrint("[i: ipnfw.c/trySourceRouting] queuing bundle to route's entry node");
	if (enqueueToEntryNode(route, bundle, bundleObj, terminusNode))
	{
		debugPrint("[i: ipnfw.c/trySourceRouting] unable to queue bundle to route's entry node");
		sm_list_destroy(ionwm, route->hops, NULL, NULL);
		psm_free(ionwm, routeAddr);
		//MRELEASE(cgrrBlk);
		return MSR_UNABLE_TO_QUEUE;
	}

	debugPrint("\n################################################################\n"
			"[ipnfw.c/trySourceRouting] Step 6.1 - Check for PSM memory bug\n"
			"################################################################\n");

	debugPrint("0 -> bug; 1 -> all clear");
	debugPrint("Ris: %d", hopsAddr == route->hops);

	debugPrint("[!: ipnfw.c/trySourceRouting] bundle queued to neighbor");

	/* Step 7 - Releasing allocated memory */
	//Added by G.M. De Cola
	debugPrint("\n################################################################\n"
			"[ipnfw.c/trySourceRouting] Step 7 - Releasing allocated memory\n"
			"################################################################\n");

	sm_list_destroy(ionwm, route->hops, NULL, NULL);
	psm_free(ionwm, routeAddr);
	//MRELEASE(cgrrBlk);

	return MSR_SUCCESS;
}
#endif

static int 	tryCGR(Bundle *bundle, Object bundleObj, IonNode *terminusNode,
			time_t atTime, CgrTrace *trace, int preview)
{
debugPrint(
			"[ipnfw.c/tryCGR] Trying CGR for this bundle\n\t\t\t"
					"bundle source: %llu, bundle timestamp (DTN time): %u, bundle timestamp (ION start offset): %u, fragm. offset: %u",
			bundle->id.source.ssp.ipn.nodeNbr, bundle->id.creationTime.seconds,
			(bundle->id.creationTime.seconds + EPOCH_2000_SEC) - ionReferenceTime(NULL),
			bundle->id.fragmentOffset);

	IonVdb		*ionvdb = getIonVdb();
	CgrVdb		*cgrvdb = cgr_get_vdb();
	int		ionMemIdx;
	Lyst		bestRoutes;
	Lyst		excludedNodes;
	LystElt		elt;
	CgrRoute	*route;
	Bundle		newBundle;
	Object		newBundleObj;

	/*	Determine whether or not the contact graph for the
	 *	terminus node identifies one or more routes over
	 *	which the bundle may be sent in order to get it
	 *	delivered to the terminus node.  If so, use the
	 *	Plan asserted for the entry node of each of the
	 *	best route ("dynamic route").
	 *
	 *	Note that CGR can be used to compute a route to an
	 *	intermediate "station" node selected by another
	 *	routing mechanism (such as static routing), not
	 *	only to the bundle's final destination node.  In
	 *	the simplest case, the bundle's destination is the
	 *	only "station" selected for the bundle.  To avoid
	 *	confusion, we here use the term "terminus" to refer
	 *	to the node to which a route is being computed,
	 *	regardless of whether that node is the bundle's
	 *	final destination or an intermediate forwarding
	 *	station.			 			*/

	CHKERR(bundle && bundleObj && terminusNode);
	TRACE(CgrBuildRoutes, terminusNode->nodeNbr, bundle->payload.length,
			(unsigned int) (atTime - ionReferenceTime(NULL)));

	if (ionvdb->lastEditTime.tv_sec > cgrvdb->lastLoadTime.tv_sec
	|| (ionvdb->lastEditTime.tv_sec == cgrvdb->lastLoadTime.tv_sec
	    && ionvdb->lastEditTime.tv_usec > cgrvdb->lastLoadTime.tv_usec)) 
	{
		/*	Contact plan has been modified, so must discard
		 *	all route lists and reconstruct them as needed.	*/

		cgr_clear_vdb(cgrvdb);
		getCurrentTime(&(cgrvdb->lastLoadTime));
	}

	ionMemIdx = getIonMemoryMgr();
	bestRoutes = lyst_create_using(ionMemIdx);
	excludedNodes = lyst_create_using(ionMemIdx);
	if (bestRoutes == NULL || excludedNodes == NULL)
	{
		putErrmsg("Can't create lists for route computation.", NULL);
		return -1;
	}

	lyst_delete_set(bestRoutes, deleteObject, NULL);
	lyst_delete_set(excludedNodes, deleteObject, NULL);
	if (!bundle->returnToSender)
	{
		/*	Must exclude sender of bundle from consideration
		 *	as a station on the route, to minimize routing
		 *	loops.  If returnToSender is 1 then we are
		 *	re-routing, possibly back through the sender,
		 *	because we have hit a dead end in routing and
		 *	must backtrack.					*/

		if (excludeNode(excludedNodes, bundle->clDossier.senderNodeNbr))
		{
			putErrmsg("Can't exclude sender from routes.", NULL);
			lyst_destroy(excludedNodes);
			lyst_destroy(bestRoutes);
			return -1;
		}
	}

	/************************************************************
	 Added by A. Stramiglio - 19/11/19
	 - Take RGR extension
	 - Convert it into a GeoRouteBlock
	 - find loopNode
	 *************************************************************/

#if (AVOID_LOOP_ENTRY_NODE == 1)
	//take sdr instance
	Sdr sdr = getIonsdr();
	Address extBlkAddr;
	Object extBlockElt;
	uvast nodeNum;
	GeoRoute rgrBlk;

	//Added by L. Persampieri
	int result_rgr_read = -3;
	uvast loopNode;
	loopNode = 0;

	OBJ_POINTER(ExtensionBlock, blk);

	// Sanity checks
	CHKERR(bundle != NULL);

	debugPrint("[i: ipnfw.c/tryCGR] sanity checks done.");

	//Step1 - Check for the presence of RGR extension
	debugPrint("\n################################################################\n"
			"[ipnfw.c/tryCGR] Step 1 - Check for the presence of RGR extension\n"
			"################################################################\n");

	rgrBlk.nodes = NULL;

	if ((extBlockElt = findExtensionBlock(bundle, RGRBlk, 0, 0, 0)))
	{

		debugPrint("[!: ipnfw.c/tryCGR] RGR Extension Block found in bundle.\n");

		nodeNum = getOwnNodeNbr();

		debugPrint("[ipnfw.c/tryCGR] Own node Number: " UVAST_FIELDSPEC, nodeNum);

		//Start sdr transaction
		//CHKERR(sdr_begin_xn(sdr));
		//debugPrint("[i: ipnfw.c/tryCGR] Start sdr transaction.\n");

		extBlkAddr = sdr_list_data(sdr, extBlockElt);
		//debugPrint("[i: ipnfw.c/tryCGR] Value of extBlkAddress = %d.\n", extBlkAddr);

		GET_OBJ_POINTER(sdr, ExtensionBlock, blk, extBlkAddr);
		//debugPrint("[i: ipnfw.c/tryCGR] Print before sdr_read");
		//sdr_read(sdr, (char *) &rgrBlk, extBlkAddr, sizeof(ExtensionBlock));

		//this function converts the extension block into a GeoRouteBlock
		result_rgr_read = rgr_read(blk, &rgrBlk);
		if (result_rgr_read == 0)
		{
			debugPrint("[i: ipnfw.c/tryCGR] Print length of blk  = %d.\n", rgrBlk.length);
			/*
			 //create the lyst
			 ionMemIdx = getIonMemoryMgr();
			 loopEntryNodes = lyst_create_using(ionMemIdx);
			 if (loopEntryNodes == NULL)
			 {
			 putErrmsg("Can't create lists for loopEntryNodes.", NULL);
			 return -1;
			 }

			 lyst_delete_set(loopEntryNodes, deleteObject, NULL);
			 */
			loopNode = findLoopEntryNode(&rgrBlk, nodeNum);

			if (loopNode != 0)
			{
				debugPrint("[ipnfw.c/tryCGR]LoopNode found");
			}

			MRELEASE(&(rgrBlk.nodes));

		}
		else if (result_rgr_read == -2)
		{
			debugPrint("[i: ipnfw.c/tryCGR]Empty GeoRoute");
		}
		else if (result_rgr_read == -3)
		{
			debugPrint("[i: ipnfw.c/tryCGR]Empty Extension");
		}
		else
		{
			debugPrint("[i: ipnfw.c/tryCGR]Memory allocation error.");
			return -1;
		}

	}
	else
	{
		debugPrint("[i: ipnfw.c/tryCGR] No RGR Extension Block found in bundle.\n");
	}

	//end transaction
	//CHKERR(sdr_end_xn(sdr));
	//debugPrint("[i: ipnfw.c/tryCGR] End sdr transaction.\n");

#endif

	/*** A. Stramiglio ********************************************/


	/*	Consult the contact graph to identify the neighboring
	 *	node(s) to forward the bundle to.			*/

	if (terminusNode->routingObject == 0)
	{
		if (cgr_create_routing_object(terminusNode) < 0)
		{
			putErrmsg("Can't initialize routing object.", NULL);
			return -1;
		}
	}

	CgrSAP sap = cgrSap(NULL);
	int result_cgr = cgr_identify_best_routes(terminusNode, bundle,
		excludedNodes, atTime, sap, trace, bestRoutes);

	if(result_cgr < 0)
	{
		putErrmsg("Can't identify best route(s) for bundle.", NULL);
		lyst_destroy(excludedNodes);
		lyst_destroy(bestRoutes);
		return -1;
	}


	lyst_destroy(excludedNodes);
	TRACE(CgrSelectRoutes);
	if (bundle->ancillaryData.flags & BP_MINIMUM_LATENCY)
	{
		/*	Critical bundle; send to all capable neighbors.	*/

		TRACE(CgrUseAllRoutes);
		return sendCriticalBundle(bundle, bundleObj, terminusNode,
				bestRoutes, preview, result_cgr);
	}

	/*	Non-critical bundle; send to the most preferred
	 *	neighbor.						*/

	/*************************************************************************/
#if (ONE_ROUTE_PER_NEIGHBOR == 1 && CGR_UNIBO == 0)
	debugPrint("[ipnfw.c/tryCGR] Comparing all best routes of neighboring nodes to understand which is the best among them\n");

//16/12/19 A.Stramiglio added loopEntryNode in tryNeighboringBestRoutes
if (tryNeighboringBestRoutes(bundle, bestRoutes, loopNode) < 0)
{
	putErrmsg("Failed trying to consider route calling tryNeighboringBestRoutes.", NULL);
	return -1;
}

#endif

//A. Stramiglio
//lyst_destroy(loopEntryNodes);
	/*************************************************************************/

	elt = lyst_first(bestRoutes);
	if (elt)
	{
		route = (CgrRoute *) lyst_data_set(elt, NULL);
		TRACE(CgrUseRoute, route->toNodeNbr);
		if (!preview && forwardOkay(route, bundle))
		{
			if (enqueueToEntryNode(route, bundle, bundleObj,
					terminusNode))
			{
				putErrmsg("Can't queue for neighbor.", NULL);
				return -1;
			}

#if (MANAGE_OVERBOOKING == 1)
			/*	Handle any contact overbooking caused
			 *	by enqueuing this bundle.		*/

			if (manageOverbooking(route, bundle, trace))
			{
				putErrmsg("Can't manage overbooking", NULL);
				return -1;
			}
#endif
		}
	}
	else
	{
		TRACE(CgrNoRoute);
	}


	lyst_destroy(bestRoutes);
	if (bundle->dlvConfidence >= MIN_NET_DELIVERY_CONFIDENCE
	|| bundle->id.source.ssp.ipn.nodeNbr
			== bundle->destination.ssp.ipn.nodeNbr)
	{
		return 0;	/*	Potential future fwd unneeded.	*/
	}

	//Added by L. Persampieri

	if(result_cgr == 0)
	{
		return 0; /*	No potential future forwarding.	*/
	}

	/*	Must put bundle in limbo, keep on trying to send it.	*/

	if (bundle->planXmitElt)
	{
		/*	This copy of bundle has already been enqueued.	*/

		if (bpClone(bundle, &newBundle, &newBundleObj, 0, 0) < 0)
		{
			putErrmsg("Can't clone bundle.", NULL);
			return -1;
		}

		bundle = &newBundle;
		bundleObj = newBundleObj;
	}

	if (enqueueToLimbo(bundle, bundleObj) < 0)
	{
		putErrmsg("Can't put bundle in limbo.", NULL);
		return -1;
	}

	return 0;
}

/*		Contingency functions for when CGR and HIRR don't work.	*/

static int	enqueueToNeighbor(Bundle *bundle, Object bundleObj,
			uvast nodeNbr)
{
	Sdr		sdr = getIonsdr();
	char		eid[MAX_EID_LEN + 1];
	VPlan		*vplan;
	PsmAddress	vplanElt;
	BpPlan		plan;

	isprintf(eid, sizeof eid, "ipn:" UVAST_FIELDSPEC ".0", nodeNbr);
	findPlan(eid, &vplan, &vplanElt);
	if (vplanElt == 0)
	{
		return 0;
	}

	sdr_read(sdr, (char *) &plan, sdr_list_data(sdr, vplan->planElt),
			sizeof(BpPlan));
	if (plan.blocked)
	{
		if (enqueueToLimbo(bundle, bundleObj) < 0)
		{
			putErrmsg("Can't put bundle in limbo.", NULL);
			return -1;
		}
	}
	else
	{
		if (bpEnqueue(vplan, bundle, bundleObj) < 0)
		{
			putErrmsg("Can't enqueue bundle.", NULL);
			return -1;
		}
	}

	return 0;
}

/*		Top-level ipnfw functions				*/

static int	openCgr()
{
	CgrSAP	sap;

	sap = cgrSap(NULL);

	if (sap)	/*	CGR is already open.			*/
	{
		writeMemo("[i] CGR service access point is already open.");
		return 0;
	}

	if (cgr_start_SAP(getOwnNodeNbr(), ionReferenceTime(NULL), &sap) < 0)
	{
		putErrmsg("Failed starting CGR SAP", NULL);
		return -1;
	}

	oK(cgrSap(&sap));
	return 0;
}

static void	closeCgr()
{
	CgrSAP	noSap = NULL;

	cgr_stop_SAP(cgrSap(NULL));
	oK(cgrSap(&noSap));
}


static int	enqueueBundle(Bundle *bundle, Object bundleObj)
{
	Sdr		sdr = getIonsdr();
	IonVdb		*ionvdb = getIonVdb();
	Object		elt;
	char		eid[SDRSTRING_BUFSZ];
	MetaEid		metaEid;
	uvast		nodeNbr;
	VScheme		*vscheme;
	PsmAddress	vschemeElt;
	IonNode		*node;
	PsmAddress	nextNode;
#if CGR_DEBUG == 1
	CgrTrace	*trace = &(CgrTrace) { .fn = printCgrTraceLine };
#else
	CgrTrace	*trace = NULL;
#endif

	elt = sdr_list_first(sdr, bundle->stations);
	if (elt == 0)
	{
		putErrmsg("Forwarding error; stations stack is empty.", NULL);
		return -1;
	}

	sdr_string_read(sdr, eid, sdr_list_data(sdr, elt));
	if (parseEidString(eid, &metaEid, &vscheme, &vschemeElt) == 0)
	{
		putErrmsg("Can't parse node EID string.", eid);
		return bpAbandon(bundleObj, bundle, BP_REASON_NO_ROUTE);
	}

	if (strcmp(vscheme->name, "ipn") != 0)
	{
		putErrmsg("Forwarding error; EID scheme is not 'ipn'.",
				vscheme->name);
		return -1;
	}

	nodeNbr = metaEid.elementNbr;
	restoreEidString(&metaEid);

	/*	Apply routing override, if any.				*/

	if (applyRoutingOverride(bundle, bundleObj, nodeNbr) < 0)
	{
		putErrmsg("Can't send bundle to override neighbor.", NULL);
		return -1;
	}

	/*	If override routing succeeded in enqueuing the bundle
	 *	to a neighbor, accept the bundle and return.		*/

	if (bundle->planXmitElt)
	{
		/*	Enqueued.					*/

		return bpAccept(bundleObj, bundle);
	}

	/*	No applicable override.  Try dynamic routing.		*/

	node = findNode(ionvdb, nodeNbr, &nextNode);
	if (node == NULL)
	{
		node = addNode(ionvdb, nodeNbr);
		if (node == NULL)
		{
			putErrmsg("Can't add node.", NULL);
			return -1;
		}
	}

	if (ionRegionOf(nodeNbr, 0) < 0)
	{
		/*	Destination node is not in any region that
		 *	the local node is in.  Send via passageway(s).	*/

		if (tryHIRR(bundle, bundleObj, node, getCtime()))
		{
			putErrmsg("HIRR failed.", NULL);
			return -1;
		}
	}
	else
	{
		/*	Destination node resides in a region in which
		 *	the local node resides.  Consult contact plan.	*/

		/************* Added by F. Marchetti *************/
		/************* Ported by G.M. De Cola ************/
#if MODERATE_SOURCE_ROUTING && (CGR_UNIBO == 0)
		if (trySourceRouting(bundle, trace, nodeNbr, bundleObj) < 0)
		{
			/*Something went wrong. We might have not found an extension,
			 * the actual node might not exists in the extension,
			 * or the residual route may be invalid. Use CGR instead.
			 */

			debugPrint("[i: ipnfw.c/enqueueBundle] unable to use previously computed route");
#endif
		if (tryCGR(bundle, bundleObj, node, getCtime(), trace, 0))
		{
			putErrmsg("CGR failed.", NULL);
			return -1;
		}
#if MODERATE_SOURCE_ROUTING && (CGR_UNIBO == 0)
		}
		else
		{
			debugPrint("[!: ipnfw.c/enqueueBundle] used previously computed route");
		}
#endif
	}

	/*	If dynamic routing succeeded in enqueuing the bundle
	 *	to a neighbor, accept the bundle and return.		*/

	if (bundle->planXmitElt)
	{
		/*	Enqueued.					*/

		return bpAccept(bundleObj, bundle);
	}

	/*	No luck using the contact graph or region tree to
	 *	compute a route to the destination node.  So see if
	 *	destination node is a neighbor (not identified in the
	 *	contact plan); if so, enqueue for direct transmission.	*/

	if (enqueueToNeighbor(bundle, bundleObj, nodeNbr) < 0)
	{
		putErrmsg("Can't send bundle to neighbor.", NULL);
		return -1;
	}

	if (bundle->planXmitElt)
	{
		/*	Enqueued.					*/

		return bpAccept(bundleObj, bundle);
	}

	/*	No egress plan for direct transmission to destination
	 *	node.  So look for the narrowest applicable static
	 *	route (node range, i.e., "exit") and forward to the
	 *	prescribed "via" endpoint for that exit.		*/

	if (ipn_lookupExit(nodeNbr, eid) == 1)
	{
		/*	Found applicable exit; forward via the
		 *	indicated endpoint.				*/

		sdr_write(sdr, bundleObj, (char *) &bundle, sizeof(Bundle));
		return forwardBundle(bundleObj, bundle, eid);
	}

	/*	No applicable exit.  If there's at least a route
	 *	that might work if some hypothetical contact should
	 *	materialize, we place the bundle in limbo and hope
	 *	for the best.						*/

	if (cgr_prospect(nodeNbr, bundle->expirationTime + EPOCH_2000_SEC) > 0)
	{
		if (enqueueToLimbo(bundle, bundleObj) < 0)
		{
			putErrmsg("Can't put bundle in limbo.", NULL);
			return -1;
		}
	}

	if (bundle->planXmitElt)
	{
		/*	Bundle was enqueued to limbo.			*/

		return bpAccept(bundleObj, bundle);
	}

	return bpAbandon(bundleObj, bundle, BP_REASON_NO_ROUTE);
}

#if defined (ION_LWT)
int	ipnfw(saddr a1, saddr a2, saddr a3, saddr a4, saddr a5,
		saddr a6, saddr a7, saddr a8, saddr a9, saddr a10)
{
#else
int	main(int argc, char *argv[])
{
#endif
	int		running = 1;
	Sdr		sdr;
	VScheme		*vscheme;
	PsmAddress	vschemeElt;
	Scheme		scheme;
	Object		elt;
	Object		bundleAddr;
	Bundle		bundle;
	Object		ovrdAddr;
	IpnOverride	ovrd;

	if (bpAttach() < 0)
	{
		putErrmsg("ipnfw can't attach to BP.", NULL);
		return 1;
	}

	if (ipnInit() < 0)
	{
		putErrmsg("ipnfw can't load routing database.", NULL);
		return 1;
	}

	cgr_start();

	if(openCgr() < 0) {
		putErrmsg("ipnfw can't open cgr", NULL);
		return -1;
	}

	sdr = getIonsdr();
	findScheme("ipn", &vscheme, &vschemeElt);
	if (vschemeElt == 0)
	{
		putErrmsg("'ipn' scheme is unknown.", NULL);
		return 1;
	}

	CHKZERO(sdr_begin_xn(sdr));
	sdr_read(sdr, (char *) &scheme, sdr_list_data(sdr,
			vscheme->schemeElt), sizeof(Scheme));
	sdr_exit_xn(sdr);
	oK(_ipnfwSemaphore(&vscheme->semaphore));
	isignal(SIGTERM, shutDown);

	/*	Main loop: wait until forwarding queue is non-empty,
	 *	then drain it.						*/

	writeMemo("[i] ipnfw is running.");
	while (running && !(sm_SemEnded(vscheme->semaphore)))
	{
		/*	Wrapping forwarding in an SDR transaction
		 *	prevents race condition with bpclock (which
		 *	is destroying bundles as their TTLs expire).	*/

		CHKZERO(sdr_begin_xn(sdr));
		elt = sdr_list_first(sdr, scheme.forwardQueue);
		if (elt == 0)	/*	Wait for forwarding notice.	*/
		{
			sdr_exit_xn(sdr);
			if (sm_SemTake(vscheme->semaphore) < 0)
			{
				putErrmsg("Can't take forwarder semaphore.",
						NULL);
				running = 0;
			}

			continue;
		}

		bundleAddr = (Object) sdr_list_data(sdr, elt);
		sdr_stage(sdr, (char *) &bundle, bundleAddr, sizeof(Bundle));
        
        bundle.priority = bundle.classOfService;
        bundle.ordinal = bundle.ancillaryData.ordinal;

		/*	Note any applicable class of service override.	*/

		if (ipn_lookupOvrd(bundle.ancillaryData.dataLabel,
				bundle.id.source.ssp.ipn.nodeNbr,
				bundle.destination.ssp.ipn.nodeNbr, &ovrdAddr))
		{
			sdr_read(sdr, (char *) &ovrd, ovrdAddr,
					sizeof(IpnOverride));
			if (ovrd.priority != (unsigned char) -1)
			{
                /*	Override requested CoS.		*/
				bundle.priority = ovrd.priority;
				bundle.ordinal = ovrd.ordinal;
			}
		}

		/*	Remove bundle from queue.			*/

		sdr_list_delete(sdr, elt, NULL, NULL);
		bundle.fwdQueueElt = 0;

		/*	Must rewrite bundle to note removal of
		 *	fwdQueueElt, in case the bundle is abandoned
		 *	and bpDestroyBundle re-reads it from the
		 *	database.					*/

		sdr_write(sdr, bundleAddr, (char *) &bundle, sizeof(Bundle));
		if (enqueueBundle(&bundle, bundleAddr) < 0)
		{
			sdr_cancel_xn(sdr);
			putErrmsg("Can't enqueue bundle.", NULL);
			running = 0;	/*	Terminate loop.		*/
			continue;
		}

		if (sdr_end_xn(sdr) < 0)
		{
			putErrmsg("Can't enqueue bundle.", NULL);
			running = 0;	/*	Terminate loop.		*/
		}

		/*	Make sure other tasks have a chance to run.	*/

		sm_TaskYield();
	}

	closeCgr();
	writeErrmsgMemos();
	writeMemo("[i] ipnfw forwarder has ended.");
	ionDetach();
	return 0;
}

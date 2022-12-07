/** \file interface_cgr_ion.c
 *  
 *  \brief    This file provides the implementation of the functions
 *            that make this CGR's implementation compatible with ION.
 *	
 *  \details  In particular the functions that import the contact plan and the BpPlans.
 *            We provide in output the best routes list.
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
#include "interface_cgr_ion.h"

#if UNIBO_CGR

#include "feature-config.h"

#include <sys/time.h>
#include <limits.h>

//include from ion
#include "ion.h"
#include "platform.h"
#include "sdrstring.h"
#include "cgr.h"
#include "bpP.h"
#include "bp.h"

#include <stdio.h>
#include <stdlib.h>

#include "utility_functions_from_ion/general_functions_ported_from_ion.h"
#include "../../include/UniboCGR.h"

#if (CGRREB)
#include "cgrr.h"
#include "cgrr_msr_utils.h"
#include "cgrr_help.h"
#endif
#if (RGREB)
#include "rgr.h"
#endif

/**
 * \brief Get the absolute value of "a"
 *
 * \param[in]   a   The real number for which we want to know the absolute value
 *
 * \hideinitializer
 */
#define absolute(a) (((a) < 0) ? (-(a)) : (a))

#ifndef DEBUG_ION_UNIBO_CGR_INTERFACE
/**
 * \brief Boolean value used to enable some debug's print for this interface.
 * 
 * \details Set to 1 if you want to enable the debug's print, otherwise set to 0.
 *
 * \hideinitializer
 */
#define DEBUG_ION_UNIBO_CGR_INTERFACE 0
#endif

#if DEBUG_ION_UNIBO_CGR_INTERFACE
#define debug_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define debug_printf(fmt, ...) do {  } while(0)
#endif

#define NOMINAL_PRIMARY_BLKSIZE	29 // from ION 4.0.0: bpv7/library/libbpP.c

/**
 * \brief Unibo-CGR instance wrapper. Keep also information about the processed bundle.
 */
typedef struct {
    /**
     * \brief Unibo-CGR instance.
     */
    UniboCGR uniboCgr;
	/**
	 * \brief ION Bundle.
	 */
	Bundle *ionBundle;
	/**
	 * \brief Unibo-CGR Bundle.
	 */
	UniboCGR_Bundle uniboCgrBundle;
	/**
	 * \brief Excluded neighbors for the current bundle.
	 */
    UniboCGR_excluded_neighbors_list uniboCgrExcludedNeighbors;
    /**
     * \brief Unibo-CGR Contact instance.
     */
    UniboCGR_Contact uniboCgrContact;
    /**
     * \brief Unibo-CGR Range instance.
     */
    UniboCGR_Range uniboCgrRange;
    /**
     * \brief Region number.
     *
     * \note Need it for ION-4.1.1 -- probably not needed in next ION versions (since IonCXref does not contain anymore "regionNbr" field)
     */
    uint32_t regionNbr;
	/**
	 * \brief Reference to all CgrRoute object created.
	 */
	Lyst routes;
    /**
     * \note Just an optimization to keep always a route in the "routes" list
     */
    bool first_route;

} ION_UniboCGR;

typedef struct {
    /**
     * \brief Copied from IonVDB during the latest contact plan update.
     */
    struct timeval contact_plan_edit_time;
    ION_UniboCGR* home_cgr;
    ION_UniboCGR* outer_cgr;
} ION_UniboCGR_SAP;

static void ION_UniboCGR_destroy(ION_UniboCGR* instance);
static void destroyRouteElt(LystElt elt, void *arg);

static uint64_t convert_scalar_to_uint64(Scalar* scalar) {
    return  (((uint64_t) scalar->gigs * (uint64_t) ONE_GIG) + (uint64_t) scalar->units);
}

static void convert_uint64_to_scalar(uint64_t scalarIn, Scalar* scalarOut) {
    loadScalar(scalarOut, 0);
    while (scalarIn) {
        if (scalarIn > (uint64_t) INT_MAX) {
            increaseScalar(scalarOut, INT_MAX);
            scalarIn -= INT_MAX;
        } else {
            increaseScalar(scalarOut, (int) scalarIn);
            scalarIn = 0; // done
        }
    }
}

/******************************************************************************
 *
 * \par Function Name:
 *      convert_PsmAddress_to_IonCXref
 *
 * \brief  Convert a PsmAddress to ION's contact type
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return IonCXref*
 *
 * \retval  IonCXref*	The contact converted
 * \retval  NULL        If the address is a NULL pointer
 *
 * \param[in]  ionwm     The partition of the ION's contacts graph
 * \param[in]  address   The address of the contact in the partition
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static IonCXref* convert_PsmAddress_to_IonCXref(PsmPartition ionwm, PsmAddress address)
{
    IonCXref *contact = NULL;

    if (address != 0)
    {
        contact = psp(ionwm, address);
    }

    return contact;
}

/******************************************************************************
 *
 * \par Function Name:
 *      convert_PsmAddress_to_IonRXref
 *
 * \brief Convert a PsmAddress to ION's range type
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return IonRXref*
 *
 * \retval  IonRXref*  The contact converted
 * \retval  NULL       If the address is a NULL pointer
 *
 * \param[in]   ionwm     The partition of the ION's ranges graph
 * \param[in]   address   The address of the range in the partition
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static IonRXref* convert_PsmAddress_to_IonRXref(PsmPartition ionwm, PsmAddress address)
{
    IonRXref *range = NULL;

    if (address != 0)
    {
        range = psp(ionwm, address);
    }

    return range;
}

static CgrRoute* allocateIonRoute(PsmPartition ionwm) {
    CgrRoute* IonRoute  = MTAKE(sizeof(CgrRoute));
    if (!IonRoute) {
        return NULL;
    }
    // memset((char*) newRoute, 0, sizeof(CgrRoute)); // memory block has been already zeroed out by ION memory allocator

    IonRoute->hops = sm_list_create(ionwm);
    if (!IonRoute->hops) {
        MRELEASE(IonRoute);
        return NULL;
    }

    return IonRoute;
}

static void releaseIonRoute(CgrRoute* IonRoute, PsmPartition ionwm) {
    if (!IonRoute) return;
    sm_list_destroy(ionwm, IonRoute->hops, NULL, NULL);
    MRELEASE(IonRoute);
}

static void destroyRouteElt(LystElt elt, void *arg)
{
    (void) arg;
    PsmPartition ionwm = getIonwm();
    CgrRoute *route = lyst_data(elt);

    releaseIonRoute(route, ionwm);
}

static int ionInterface_callback_ComputeApplicableBacklog(uint64_t neighbor,
                                                          UniboCGR_BundlePriority priority,
                                                          uint8_t ordinal,
                                                          uint64_t *applicableBacklog,
                                                          uint64_t *totalBacklog,
                                                          void* userArg) {
    (void) priority; // unused
    (void) ordinal; // unused
    ION_UniboCGR* instance = (ION_UniboCGR*) userArg;
    Sdr sdr;
    char eid[SDRSTRING_BUFSZ];
    VPlan *vplan;
    PsmAddress vplanElt;
    Object planObj;
    BpPlan plan;
    Scalar IonApplicableBacklog, IonTotalBacklog;

    isprintf(eid, sizeof eid, "ipn:%" PRIu64 ".0", neighbor);
    sdr = getIonsdr();
    findPlan(eid, &vplan, &vplanElt);
    if (vplanElt != 0)
    {
        planObj = sdr_list_data(sdr, vplan->planElt);
        sdr_read(sdr, (char*) &plan, planObj, sizeof(BpPlan));
        if (!plan.blocked)
        {
            computePriorClaims(&plan, instance->ionBundle, &IonApplicableBacklog, &IonTotalBacklog);
            *applicableBacklog = convert_scalar_to_uint64(&IonApplicableBacklog);
            *totalBacklog = convert_scalar_to_uint64(&IonTotalBacklog);
            return 0;
        }
    }

    return -1;
}

static ION_UniboCGR* ION_UniboCGR_create(uint64_t local_node, time_t reference_time, time_t current_time, PsmPartition ionwm) {
    ION_UniboCGR* instance = MTAKE(sizeof(ION_UniboCGR));
    if (!instance) {
        putErrmsg("Can't create ION_UniboCGR object.", NULL);
        return NULL;
    }
    UniboCGR_Error uniboCgrError;
    uniboCgrError = UniboCGR_Contact_create(&instance->uniboCgrContact);
    if (UniboCGR_check_error(uniboCgrError)) {
        putErrmsg(UniboCGR_get_error_string(uniboCgrError), NULL);
        ION_UniboCGR_destroy(instance);
        return NULL;
    }
    uniboCgrError = UniboCGR_Range_create(&instance->uniboCgrRange);
    if (UniboCGR_check_error(uniboCgrError)) {
        putErrmsg(UniboCGR_get_error_string(uniboCgrError), NULL);
        ION_UniboCGR_destroy(instance);
        return NULL;
    }
    uniboCgrError = UniboCGR_Bundle_create(&instance->uniboCgrBundle);
    if (UniboCGR_check_error(uniboCgrError)) {
        putErrmsg(UniboCGR_get_error_string(uniboCgrError), NULL);
        ION_UniboCGR_destroy(instance);
        return NULL;
    }
    uniboCgrError = UniboCGR_create_excluded_neighbors_list(&instance->uniboCgrExcludedNeighbors);
    if (UniboCGR_check_error(uniboCgrError)) {
        putErrmsg(UniboCGR_get_error_string(uniboCgrError), NULL);
        ION_UniboCGR_destroy(instance);
        return NULL;
    }
    int ionMemIdx = getIonMemoryMgr();
    instance->routes = lyst_create_using(ionMemIdx);
    if (!instance->routes) {
        putErrmsg("Can't create ION Lyst (routes).", NULL);
        ION_UniboCGR_destroy(instance);
        return NULL;
    }
    lyst_delete_set(instance->routes, destroyRouteElt, NULL);

    CgrRoute* firstRoute = allocateIonRoute(ionwm);
    if (!firstRoute) {
        putErrmsg("Can't create CgrRoute.", NULL);
        ION_UniboCGR_destroy(instance);
        return NULL;
    }

    if (!lyst_insert_last(instance->routes, firstRoute)) {
        putErrmsg("Can't insert CgrRoute into lyst.", NULL);
        ION_UniboCGR_destroy(instance);
        return NULL;
    }

    instance->first_route = true;
    
    uniboCgrError = UniboCGR_open(&instance->uniboCgr,
                                  current_time,
                                  reference_time,
                                  local_node,
                                  PhaseThreeCostFunction_default,
                                  ionInterface_callback_ComputeApplicableBacklog,
                                  instance);
    if (UniboCGR_check_error(uniboCgrError)) {
        putErrmsg(UniboCGR_get_error_string(uniboCgrError), NULL);
        ION_UniboCGR_destroy(instance);
        return NULL;
    }

    return instance;
}

static void ION_UniboCGR_destroy(ION_UniboCGR* instance) {
    if (!instance) return;
    UniboCGR_close(&instance->uniboCgr, getCtime());
    UniboCGR_Contact_destroy(&instance->uniboCgrContact);
    UniboCGR_Range_destroy(&instance->uniboCgrRange);
    UniboCGR_Bundle_destroy(&instance->uniboCgrBundle);
    UniboCGR_destroy_excluded_neighbors_list(&instance->uniboCgrExcludedNeighbors);
    lyst_delete_set(instance->routes, destroyRouteElt, NULL);
    lyst_destroy(instance->routes);
    MRELEASE(instance);
}

#if(DEBUG_ION_UNIBO_CGR_INTERFACE == 1)
/******************************************************************************
 *
 * \par Function Name:
 * 		printDebugIonRoute
 *
 * \brief Print the "ION"'s route to standard output
 *
 *
 * \par Date Written:
 * 	    04/04/20
 *
 * \return void
 *
 * \param[in]   *route   The route that we want to print.
 *
 * \par Notes:
 *            1. This function print only the value of the route that
 *               will effectively used by ION.
 *            2. All the times are differential times from the reference time.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR          |  DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  04/04/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static void printDebugIonRoute(PsmPartition ionwm, CgrRoute *route)
{
	int stop = 0;
	Sdr sdr = getIonsdr();
	PsmAddress addr, addrContact;
	SdrObject contactObj;
	IonCXref *contact;
	IonContact contactBuf;
	if (route != NULL)
	{
		fprintf(stdout, "\nPRINT ION ROUTE\n%-15s %-15s %-15s %-15s %-15s %-15s %s\n", "ToNodeNbr",
				"FromTime", "ToTime", "ETO", "PBAT", "MaxVolumeAvbl", "BundleECCC");
		fprintf(stdout, "%-15llu %-15ld %-15ld %-15ld %-15ld %-15g %lu\n",
				(unsigned long long) route->toNodeNbr, (long int) route->fromTime,
				(long int) route->toTime, (long int) route->eto,
				(long int) route->pbat, route->maxVolumeAvbl, (long unsigned int) route->bundleECCC);
		fprintf(stdout, "%-15s %-15s %-15s %-15s %-15s %s\n", "Confidence", "Hops", "Overbooked (G)",
				"Overbooked (U)", "Protected (G)", "Protected (U)");
		fprintf(stdout, "%-15.2f %-15ld %-15d %-15d %-15d %d\n", route->arrivalConfidence,
				(long int) sm_list_length(ionwm, route->hops), route->overbooked.gigs,
				route->overbooked.units, route->committed.gigs, route->committed.units);
		fprintf(stdout, "%-15s %-15s %-15s %-15s %-15s %-15s %-15s %-15s %s\n", "FromNode",
				"ToNode", "FromTime", "ToTime", "XmitRate", "Confidence", "MTV[Bulk]",
				"MTV[Normal]", "MTV[Expedited]");
		for (addr = sm_list_first(ionwm, route->hops); addr != 0 && !stop;
				addr = sm_list_next(ionwm, addr))
		{
			addrContact = sm_list_data(ionwm, addr);
			stop = 1;
			if (addrContact != 0)
			{
				contact = psp(ionwm, addrContact);
				if (contact != NULL)
				{
					stop = 0;
					contactObj = sdr_list_data(sdr, contact->contactElt);
					sdr_read(sdr, (char*) &contactBuf, contactObj, sizeof(IonContact));
					fprintf(stdout,
							"%-15llu %-15llu %-15ld %-15ld %-15lu %-15.2f %-15g %-15g %g\n",
							(unsigned long long) contact->fromNode,
							(unsigned long long) contact->toNode,
							(long int) contact->fromTime, (long int) contact->toTime,
							(long unsigned int) contact->xmitRate, contact->confidence, contactBuf.mtv[0],
							contactBuf.mtv[1], contactBuf.mtv[2]);
				}
				else
				{
					fprintf(stdout, "Contact: NULL.\n");
				}

			}
			else
			{
				fprintf(stdout, "PsmAddress: 0.\n");
			}
		}

		fflush(stdout);

	}
}
#else
#define printDebugIonRoute(ionwm, route) do {  } while(0)
#endif

/******************************************************************************
 *
 * \par Function Name:
 *      convert_CtScheduled_from_ion_to_cgr
 *
 * \brief  Convert a Scheduled contact from ION to this CGR's implementation
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval  0   Success case
 *
 * \param[in]    ionContact
 * \param[out]   CgrContact
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int convert_CtScheduled_from_ion_to_cgr(ION_UniboCGR* instance, IonCXref *ionContact, UniboCGR_Contact cgrContact)
{
    UniboCGR_Contact_set_sender(cgrContact, ionContact->fromNode);
    UniboCGR_Contact_set_receiver(cgrContact, ionContact->toNode);
    UniboCGR_Contact_set_start_time(instance->uniboCgr, cgrContact, ionContact->fromTime);
    UniboCGR_Contact_set_end_time(instance->uniboCgr, cgrContact, ionContact->toTime);
    UniboCGR_Contact_set_type(cgrContact, UniboCGR_ContactType_Scheduled);
    UniboCGR_Contact_set_xmit_rate(cgrContact, ionContact->xmitRate);
    UniboCGR_Contact_set_confidence(cgrContact, ionContact->confidence);
    return 0;
}

/******************************************************************************
 *
 * \par Function Name:
 *      convert_CtScheduled_from_cgr_to_ion
 *
 * \brief  Convert a Scheduled contact from this CGR's implementation to ION
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval   0	Success case
 *
 * \param[in]    *CgrContact
 * \param[out]   *ionContact
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int convert_CtScheduled_from_cgr_to_ion(ION_UniboCGR* instance, UniboCGR_Contact cgrContact, IonCXref *ionContact)
{
    /* NOTE:
     * the "regionNbr" field disappeared in IRR-preview (Scott Burleigh seminar 2021 @ UniBo).
     * However, this field is necessary in ION-4.1.1.
     * Need to monitor future versions when IRR will be merged into ION baseline.
     */
    ionContact->regionNbr = instance->regionNbr;
    ionContact->fromNode = UniboCGR_Contact_get_sender(cgrContact);
    ionContact->toNode = UniboCGR_Contact_get_receiver(cgrContact);
    ionContact->fromTime = UniboCGR_Contact_get_start_time(instance->uniboCgr, cgrContact);
    ionContact->toTime = UniboCGR_Contact_get_end_time(instance->uniboCgr, cgrContact);
    ionContact->type = CtScheduled;
    ionContact->xmitRate = UniboCGR_Contact_get_xmit_rate(cgrContact);
    ionContact->confidence = UniboCGR_Contact_get_confidence(cgrContact);

	return 0;
}

/******************************************************************************
 *
 * \par Function Name:
 *      convert_contact_from_ion_to_cgr
 *
 * \brief Convert a contact from ION to this CGR's implementation
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval   0  Success case
 * \retval  -2  Unknown contact's type
 *
 * \param[in]   ionContact
 * \param[out]  CgrContact
 *
 * \par	Notes:
 *             1.  Only Scheduled contacts are allowed.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int convert_contact_from_ion_to_cgr(ION_UniboCGR* instance, IonCXref *ionContact, UniboCGR_Contact cgrContact)
{
    if (ionContact->type == CtScheduled) {
        return convert_CtScheduled_from_ion_to_cgr(instance, ionContact, cgrContact);
    }

    return -2;
}

/******************************************************************************
 *
 * \par Function Name:
 *      convert_contact_from_cgr_to_ion
 *
 * \brief Convert a contact from this CGR's implementation to ION
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval  0  Success case
 * \retval  -2  Unknown contact's type
 *
 * \param[in]    CgrContact
 * \param[out]   ionContact
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int convert_contact_from_cgr_to_ion(ION_UniboCGR* instance, UniboCGR_Contact cgrContact, IonCXref *ionContact)
{
    UniboCGR_ContactType cgrContactType = UniboCGR_Contact_get_type(cgrContact);
    if (cgrContactType == UniboCGR_ContactType_Scheduled) {
        return convert_CtScheduled_from_cgr_to_ion(instance, cgrContact, ionContact);
    }

    return -2;
}

/******************************************************************************
 *
 * \par Function Name:
 *      convert_range_from_ion_to_cgr
 *
 * \brief  Convert a range from ION to this CGR's implementation
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval   0   Success case
 *
 * \param[in]    IonRange
 * \param[out]   uniboCgrRange
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int convert_range_from_ion_to_cgr(ION_UniboCGR* instance, IonRXref *IonRange, UniboCGR_Range uniboCgrRange)
{
    UniboCGR_Range_set_sender(uniboCgrRange, IonRange->fromNode);
    UniboCGR_Range_set_receiver(uniboCgrRange, IonRange->toNode);
    UniboCGR_Range_set_start_time(instance->uniboCgr, uniboCgrRange, IonRange->fromTime);
    UniboCGR_Range_set_end_time(instance->uniboCgr, uniboCgrRange, IonRange->toTime);
    UniboCGR_Range_set_one_way_light_time(uniboCgrRange, IonRange->owlt);
    return 0;
}

/******************************************************************************
 *
 * \par Function Name:
 *      convert_range_from_cgr_to_ion
 *
 * \brief  Convert a range from this CGR's implementation to ION
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval  0  Success case
 *
 * \param[in]   uniboCgrRange
 * \param[out]  IonRange
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
/*
static int convert_range_from_cgr_to_ion(ION_UniboCGR* instance, UniboCGR_Range uniboCgrRange, IonRXref *IonRange)
{
    IonRange->fromNode = UniboCGR_Range_get_sender(uniboCgrRange);
    IonRange->toNode = UniboCGR_Range_get_receiver(uniboCgrRange);
    IonRange->fromTime = UniboCGR_Range_get_start_time(instance->uniboCgr, uniboCgrRange);
    IonRange->toTime = UniboCGR_Range_get_end_time(instance->uniboCgr, uniboCgrRange);
    IonRange->owlt = UniboCGR_Range_get_one_way_light_time(uniboCgrRange);
	return 0;
}
*/
#if RGREB
/******************************************************************************
 *
 * \par Function Name:
 *      get_rgr_ext_block
 *
 * \brief  Get the GeoRoute stored into RGR Extension Block
 *
 *
 * \par Date Written:
 *      23/04/20
 *
 * \return int
 *
 * \retval  0  Success case: GeoRoute found
 * \retval -1  GeoRoute not found
 * \retval -2  System error
 *
 * \param[in]   *bundle    The bundle that contains the RGR Extension Block
 * \param[out]  *resultBlk The GeoRoute extracted from the RGR Extension Block, only in success case.
 *                         The resultBLk must be allocated by the caller.
 *
 * \warning bundle    doesn't have to be NULL
 * \warning resultBlk doesn't have to be NULL
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  23/04/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int get_rgr_ext_block(Bundle *bundle, GeoRoute *resultBlk)
{
	Sdr sdr = getIonsdr();
	int result = 0;
	Object extBlockElt;
	Address extBlkAddr;

	OBJ_POINTER(ExtensionBlock, blk);

	/* Step 1 - Check for the presence of RGR extension*/

	if (!(extBlockElt = findExtensionBlock(bundle, RGRBlk, 0)))
	{
		result = -1;
	}
	else
	{
		/* Step 2 - Get deserialized version of RGR extension block*/

		extBlkAddr = sdr_list_data(sdr, extBlockElt);

		GET_OBJ_POINTER(sdr, ExtensionBlock, blk, extBlkAddr);

		result = rgr_read(blk, resultBlk);

		if(result == -1)
		{
			result = -2; // system error
		}
		else if(result < -1)
		{
			result = -1; // geo route not found
		}
		else
		{
			result = 0; // geo route found
		}
	}

	return result;
}

/**
 * \retval 0 success
 * \retval "< 0" fatal error
 */
static int ion_set_geo_route_list(ION_UniboCGR* instance, GeoRoute* rgrBlk) {
    if (rgrBlk == NULL || rgrBlk->nodes == NULL || rgrBlk->length == 0) {
        return 0;
    }

    char* geoRouteString = rgrBlk->nodes;
    const char* last_char = rgrBlk->nodes + rgrBlk->length - 1;
    while (true)
    {
        geoRouteString = strstr(geoRouteString, "ipn:");
        if (geoRouteString == NULL) {
            break;
        }
        geoRouteString += 4;
        if (geoRouteString > last_char) { // protection
            break;
        }
        if (isdigit(*(geoRouteString + 4))) {
            uvast ipn_node = strtouvast(geoRouteString);
            UniboCGR_Error error = UniboCGR_Bundle_add_node_in_geographic_route_list(instance->uniboCgrBundle,
                                                                                     ipn_node);
            if (UniboCGR_check_error(error)) {
                if (UniboCGR_check_fatal_error(error)) {
                    return -1;
                }
            }
        }
    }
    return 0;
}

#endif

#if CGRREB

// path to macro:
// CGRREB (from ION's root directory): bpv*/library/ext/cgrr/cgrr.h

/******************************************************************************
 *
 * \par Function Name:
 *      update_mtv_before_reforwarding
 *
 * \brief  This function has effect only if this bundle has to be reforwarded for some reason.
 *
 * \details We refill the contacts' MTVs of the previous route computed for this bundle.
 *          CGRR Ext. Block is required.
 *
 *
 * \par Date Written:
 *      11/12/20
 *
 * \return int
 *
 * \retval  0  Success case: MTV updated
 * \retval -1  Some error occurred
 *
 * \param[in]   *bundle        The bundle to forward
 * \param[in]   *cgrrExtBlk    The ExtensionBlock type of CGRR
 * \param[in]   *resultBlk     The CGRRouteBlock extracted from the CGRR Extension Block.
 * \param[in]   reference_time The reference time used to convert POSIX time in differential time from it.
 * \param[in]   regionNbr      Contact plan region
 *
 * \warning bundle    doesn't have to be NULL
 * \warning cgrrExtBlk doesn't have to be NULL
 * \warning cgrrBlk   doesn't have to be NULL
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  11/12/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int update_mtv_before_reforwarding(ION_UniboCGR* instance, ExtensionBlock *cgrrExtBlk, CGRRouteBlock *cgrrBlk)
{
    Sdr sdr = getIonsdr();
    IonVdb *ionvdb = getIonVdb();
    PsmPartition ionwm = getIonwm();
	uvast evc;
	CGRRoute *lastRoute = NULL;
    IonContact contactBuf;
    
    if (instance->ionBundle->ancillaryData.flags & BP_MINIMUM_LATENCY) {
        // critical bundle -- nothing to do
        return 0;
    }
    
    int okCgrrEvc = cgrr_getUsedEvc(instance->ionBundle, cgrrExtBlk, &evc);
    if (okCgrrEvc > 1) {
        // cannot update mtv for this bundle (i.e. stored EVC not found in CGRR ext. block)
        return 0;
    }
    if (okCgrrEvc == 1) {
        // bundle is a new fragment -- we can safely restore just the current payload length (and must not recompute the EVC)
        evc = instance->ionBundle->payload.length;
    }
    if (okCgrrEvc < 0) {
        // some bad error occurred
        return -1;
    }
    
    // ok we have evc now

    // now get the last route from CGRRouteBlock
    if(cgrrBlk->recRoutesLength == 0) {
        lastRoute = &(cgrrBlk->originalRoute);
    } else {
        lastRoute = &(cgrrBlk->recomputedRoutes[cgrrBlk->recRoutesLength - 1]);
    }

    const unsigned int time_tolerance = UNIBO_CGR_FEATURE_MSR_TIME_TOLERANCE;
    // here we know it is enabled -- just get time tolerance
    UniboCGR_feature_ModerateSourceRouting_check(instance->uniboCgr);
    // for each contact in route refill the mtv in both Unibo-CGR and ION
    for (unsigned int i = 0; i < lastRoute->hopCount; i++) {
        CGRRHop* hop = &lastRoute->hopList[i];
        for (time_t start_time = hop->fromTime - time_tolerance; start_time <= hop->fromTime + time_tolerance; start_time++) {
            UniboCGR_Contact cgrrHopUniboCgrContact;
            if (UniboCGR_NoError != UniboCGR_find_contact(instance->uniboCgr,
                                                          UniboCGR_ContactType_Scheduled,
                                                          hop->fromNode,
                                                          hop->toNode,
                                                          start_time,
                                                          &cgrrHopUniboCgrContact)) {
                // try with start_time + 1
                continue;
            }

            IonCXref ionContactLocal;
            IonCXref* ionContact = NULL;
            Object contactObj;
            if (convert_contact_from_cgr_to_ion(instance, cgrrHopUniboCgrContact, &ionContactLocal) >= 0) {
                PsmAddress ionContactTreeNode = sm_rbt_search(ionwm, ionvdb->contactIndex, rfx_order_contacts, &ionContactLocal, 0);
                ionContact = convert_PsmAddress_to_IonCXref(ionwm, sm_rbt_data(ionwm, ionContactTreeNode));
                if (ionContact) {
                    contactObj = sdr_list_data(sdr, ionContact->contactElt);
                    sdr_stage(sdr, (char *) &contactBuf, contactObj, sizeof(IonContact));
                }
            }
            
            // contact found -- now refill mtv in both Unibo-CGR and ION
            if (instance->ionBundle->priority >= 0) { // always true
                double mtv_bulk = UniboCGR_Contact_get_mtv_bulk(cgrrHopUniboCgrContact);
                mtv_bulk += (double) evc;
                UniboCGR_Contact_set_mtv_bulk(cgrrHopUniboCgrContact, mtv_bulk);
                if (ionContact) {
                    contactBuf.mtv[0] = mtv_bulk;
                }
            }
            if (instance->ionBundle->priority >= 1) {
                double mtv_normal = UniboCGR_Contact_get_mtv_normal(cgrrHopUniboCgrContact);
                mtv_normal += (double) evc;
                UniboCGR_Contact_set_mtv_normal(cgrrHopUniboCgrContact, mtv_normal);
                if (ionContact) {
                    contactBuf.mtv[1] = mtv_normal;
                }
            }
            if (instance->ionBundle->priority >= 2) {
                double mtv_expedited = UniboCGR_Contact_get_mtv_expedited(cgrrHopUniboCgrContact);
                mtv_expedited += (double) evc;
                UniboCGR_Contact_set_mtv_expedited(cgrrHopUniboCgrContact, mtv_expedited);
                if (ionContact) {
                    contactBuf.mtv[2] = mtv_expedited;
                }
            }
            
            if (ionContact) {
                sdr_write(sdr, contactObj, (char *) &contactBuf, sizeof(IonContact));
            }
        }
    }

	return 0;
}

/******************************************************************************
 *
 * \par Function Name:
 *      get_cgrr_ext_block
 *
 * \brief  Get the CGRRouteBlock stored into CGRR Extension Block
 *
 *
 * \par Date Written:
 *      23/04/20
 *
 * \return int
 *
 * \retval  0  Success case: CGRRouteBlock found
 * \retval -1  CGRRouteBlock not found
 * \retval -2  System error
 *
 * \param[in]   *bundle        The bundle that contains the CGRR Extension Block
 * \param[in]   reference_time The reference time used to convert POSIX time in differential time from it.
 * \param[out]  *extBlk        The ExtensionBlock type of CGRR
 * \param[out] **resultBlk     The CGRRouteBlock extracted from the CGRR Extension Block, only in success case.
 *                             The resultBLk will be allocated by this function.
 *
 * \warning bundle    doesn't have to be NULL
 * \warning resultBlk doesn't have to be NULL
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  23/04/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int get_cgrr_ext_block(Bundle *bundle, ExtensionBlock *extBlk, CGRRouteBlock **resultBlk)
{
	Sdr sdr = getIonsdr();
	Object extBlockElt;
	Address extBlkAddr;
	CGRRouteBlock* cgrrBlk;

	OBJ_POINTER(ExtensionBlock, blk);

	/* Step 1 - Check for the presence of CGRR extension*/

	if (!(extBlockElt = findExtensionBlock(bundle, CGRRBlk, 0)))
	{
		return -1;
	}
    /* Step 2 - Get deserialized version of CGRR extension block*/

    extBlkAddr = sdr_list_data(sdr, extBlockElt);

    GET_OBJ_POINTER(sdr, ExtensionBlock, blk, extBlkAddr);

    cgrrBlk = (CGRRouteBlock*) MTAKE(sizeof(CGRRouteBlock));

    if (cgrrBlk == NULL)
    {
        return -2;
    }
    if (cgrr_getCGRRFromExtensionBlock(blk, cgrrBlk) < 0)
    {
        MRELEASE(cgrrBlk);
        return -2;
    }
    
    *resultBlk = cgrrBlk;
    memcpy(extBlk, blk, sizeof(ExtensionBlock));
	return 0;
}

static int reduceCGRR(ION_UniboCGR* instance) {
    int ok = updateLastCgrrRoute(instance->ionBundle);
    if(ok == -2 || ok == -1)
    {
        return ok;
    }
    if(ok == -3)
    {
        // some error
        return -1;
    }
    
    return 0;
}

static int update_CGRR_evc(ION_UniboCGR* instance) {
    Sdr sdr = getIonsdr();
    Object extBlockElt;

    OBJ_POINTER(ExtensionBlock, blk);

    if ((extBlockElt = findExtensionBlock(instance->ionBundle, CGRRBlk, 0)))
    {
        Address extBlockAddr = sdr_list_data(sdr, extBlockElt);

        GET_OBJ_POINTER(sdr, ExtensionBlock, blk, extBlockAddr);

        uint64_t evc = UniboCGR_Bundle_get_estimated_volume_consumption(instance->uniboCgrBundle);
        if (cgrr_setUsedEvc(instance->ionBundle, blk, (uvast) evc) < 0) {
            return -1;
        }
    }
    return 0;
}

/******************************************************************************
 *
 * \par Function Name:
 *      CGRR_management
 *
 * \brief  Update CGRR after routing.
 *
 *
 * \par Date Written:
 *      25/09/20
 *
 * \return int
 *
 * \retval  0  Success case
 * \retval -1  Some error
 * \retval -2  System error
 * \retval -3  Nothing to do
 *
 * \param[in]   ionwm          ION Working Memory
 * \param[in]   bestRoutes     Routes selected from routing.
 * \param[in]   *bundle        The bundle that contains the CGRR Ext. Block
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  25/09/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int CGRR_management(ION_UniboCGR* instance, Lyst bestRoutes)
{
	int temp;
    LystElt lyst_elt;
    CGRRoute cgrr_route;
    CgrRoute *route;

	if (lyst_length(bestRoutes) == 0)
	{
		return -3;
	}

    if(instance->ionBundle->ancillaryData.flags & BP_MINIMUM_LATENCY) {
        // Critical bundle
        return -1;
    }
    
    if (UniboCGR_feature_ModerateSourceRouting_check(instance->uniboCgr)) {
        if (UniboCGR_get_used_routing_algorithm(instance->uniboCgr) == UniboCGR_RoutingAlgorithm_MSR) {
            // -1 on error, 0 success
            if (reduceCGRR(instance) == 0) {
                if (update_CGRR_evc(instance) < 0) {
                    return -1;
                }
                return 0;
            }
            return -1;
        } else {
            // replace the MSR route in CGRR extension block
            lyst_elt = lyst_first(bestRoutes);
            if (lyst_elt) {
                route = (CgrRoute *) lyst_data(lyst_elt);
            } else {
                return -1;
            }

            temp = getCGRRoute(route, &cgrr_route);

            if(temp == -1 || temp == -2) {
                return temp;
            }
            temp = storeMsrRoute(&cgrr_route, instance->ionBundle);

            if(temp == -1 || temp == -2) {
                return temp;
            }
            MRELEASE(cgrr_route.hopList);

            if (update_CGRR_evc(instance) < 0) {
                return -1;
            }
            return 0;
        }
    } else {
#if UNIBO_CGR_FEATURE_MSR_WISE_NODE

        for (lyst_elt = lyst_first(bestRoutes); lyst_elt; lyst_elt = lyst_next(lyst_elt))
        {
            route = (CgrRoute*) lyst_data(lyst_elt);

            temp = getCGRRoute(route, &cgrr_route);

            if(temp == -2) {
                // system error
                return -2;
            }
            else if(temp == 0 && cgrr_route.hopCount > 0)
            {
                temp = saveRouteToExtBlock(cgrr_route.hopCount, cgrr_route.hopList, instance->ionBundle);

                if (temp == -2) {
                    // CGRR not found -- it may happen?
                    return -1;
                } else if (temp == -1) {
                    // System error
                    return -2;
                }

                MRELEASE(cgrr_route.hopList);
            }
        }
        
        if (update_CGRR_evc(instance) < 0) {
            return -1;
        }
        return 0;
#else
        return 0;
#endif
    }
}

/**
 * \retval 0 success
 * \retval -1 malformed route
 * \retval -2 system error
 */
static int set_UniboCGR_msr_route(ION_UniboCGR* instance, CGRRouteBlock* cgrrBlk, uint32_t time_tolerance) {
    CGRRoute *lastRoute;
    UniboCGR_Error error;
    if (cgrrBlk->recRoutesLength == 0) {
        lastRoute = &(cgrrBlk->originalRoute);
    } else {
        lastRoute = &(cgrrBlk->recomputedRoutes[cgrrBlk->recRoutesLength - 1]);
    }

    uvast local_node = getOwnNodeNbr();
    bool first_hop = true;
    for (unsigned int i = 0; i < lastRoute->hopCount; i++) {
        CGRRHop *hop = &lastRoute->hopList[i];
        if (first_hop && hop->fromNode != local_node) continue; // skip hop

        first_hop = false;
        for (time_t start_time = hop->fromTime - time_tolerance;
            start_time <= hop->fromTime + time_tolerance; start_time++) {

            error = UniboCGR_add_moderate_source_routing_hop(instance->uniboCgr,
                                                             instance->uniboCgrBundle,
                                                             UniboCGR_ContactType_Scheduled,
                                                             hop->fromNode,
                                                             hop->toNode,
                                                             start_time);

            if (error == UniboCGR_NoError) break; // ok hop inserted

            if (error == UniboCGR_ErrorContactNotFound) {
                UniboCGR_log_write(instance->uniboCgr, "contact %lu -> %lu (start %ld) not found!!!",
                                   (long unsigned) hop->fromNode, (long unsigned)hop->toNode, start_time);
                continue; // try again with start_time + 1
            }
            else {
                return -1; // malformed route
            }
        }
    }
#if (UNIBO_CGR_FEATURE_MSR_WISE_NODE)
    const uint32_t msr_lower_bound = 0;
#else
    const uint32_t msr_lower_bound = UNIBO_CGR_FEATURE_MSR_UNWISE_NODE_LOWER_BOUND;
#endif
    error = UniboCGR_finalize_moderate_source_routing_route(instance->uniboCgr,
                                                            instance->uniboCgrBundle,
                                                            msr_lower_bound);
    if (UniboCGR_check_error(error)) {
        if (UniboCGR_check_fatal_error(error)) {
            return -2;
        }
        return -1;
    }
    return 0;
}

#endif

/******************************************************************************
 *
 * \par Function Name:
 *      convert_bundle_from_ion_to_cgr
 *
 * \brief Convert the characteristics of the bundle from ION to
 *        this CGR's implementation and initialize all the
 *        bundle fields used by the CGR.
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval  0  Success case
 * \retval -1  Arguments error
 * \retval -2  MTAKE error
 *
 * \param[in]    toNode             The destination ipn node for the bundle
 * \param[in]    current_time       The current time, in differential time from reference_time
 * \param[in]    reference_time     The reference time used to convert POSIX time in differential time from it.
 * \param[in]    *IonBundle         The bundle in ION
 * \param[out]   *CgrBundle         The bundle in this CGR's implementation.
 *
 * \par	Notes:
 *             1. This function prints the ID of the bundle in the main log file.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *  24/03/20 | L. Persampieri  |  Added geoRouteString
 *  23/10/22 | L. Persampieri  |  Major refactoring.
 *****************************************************************************/
static int convert_bundle_from_ion_to_cgr(ION_UniboCGR* instance, uvast destination)
{
	int temp;
    (void) temp;
    UniboCGR_Bundle_reset(instance->uniboCgrBundle);

    // ION BPv7
    UniboCGR_Bundle_set_bundle_protocol_version(instance->uniboCgrBundle, 7);

    UniboCGR_Bundle_set_destination_node_id(instance->uniboCgrBundle, (uint64_t) destination);

#if (CGRREB)
    CGRRouteBlock *cgrrBlk = NULL;
    ExtensionBlock cgrrExtBlk;
    temp = get_cgrr_ext_block(instance->ionBundle, &cgrrExtBlk, &cgrrBlk);
    if(temp == 0) {
        update_mtv_before_reforwarding(instance, &cgrrExtBlk, cgrrBlk);
        if (UniboCGR_feature_ModerateSourceRouting_check(instance->uniboCgr)) {
            (void) set_UniboCGR_msr_route(instance, cgrrBlk, UNIBO_CGR_FEATURE_MSR_TIME_TOLERANCE);
        }
        releaseCgrrBlkMemory(cgrrBlk);
    }
    if (temp == -2) {
        return -2;
    }
#endif

#if RGREB
    if (UniboCGR_feature_ReactiveAntiLoop_check(instance->uniboCgr)
    || UniboCGR_feature_ProactiveAntiLoop_check(instance->uniboCgr)) {
        GeoRoute geoRoute;
        temp = get_rgr_ext_block(instance->ionBundle, &geoRoute);
        if(temp == 0) {
            temp = ion_set_geo_route_list(instance, &geoRoute);
            MRELEASE(geoRoute.nodes);
            if (temp < 0) {
                return -1;
            }
        }
    }

#endif

    if (UniboCGR_feature_logger_check(instance->uniboCgr)) {
        char* source_node_id_string = NULL;
        readEid(&instance->ionBundle->id.source, &source_node_id_string);
        if (!source_node_id_string) {
            return -2;
        }
        UniboCGR_Bundle_set_source_node_id(instance->uniboCgrBundle, source_node_id_string);
        MRELEASE(source_node_id_string);
        UniboCGR_Bundle_set_sequence_number(instance->uniboCgrBundle, instance->ionBundle->id.creationTime.count);
        bool is_fragment = instance->ionBundle->bundleProcFlags & BDL_IS_FRAGMENT;

        if (is_fragment) {
            UniboCGR_Bundle_set_fragment_offset(instance->uniboCgrBundle, instance->ionBundle->id.fragmentOffset);
            UniboCGR_Bundle_set_fragment_length(instance->uniboCgrBundle, instance->ionBundle->payload.length);
            UniboCGR_Bundle_set_total_application_data_unit_length(instance->uniboCgrBundle, instance->ionBundle->totalAduLength);
        } else {
            UniboCGR_Bundle_set_total_application_data_unit_length(instance->uniboCgrBundle, instance->ionBundle->payload.length);
        }
    }
    // creation time and lifetime are required -- we need always them in order to calculate the "expiration time"
    UniboCGR_Bundle_set_creation_time(instance->uniboCgrBundle, instance->ionBundle->id.creationTime.msec);
    UniboCGR_Bundle_set_lifetime(instance->uniboCgrBundle, instance->ionBundle->timeToLive);

    /* Bundle flags */
    bool is_enabled_critical_flag = instance->ionBundle->ancillaryData.flags & BP_MINIMUM_LATENCY;
    UniboCGR_Bundle_set_flag_critical(instance->uniboCgrBundle, is_enabled_critical_flag);
    bool is_enabled_backward_propagation_flag = !is_enabled_critical_flag && instance->ionBundle->returnToSender;
    UniboCGR_Bundle_set_flag_backward_propagation(instance->uniboCgrBundle, is_enabled_backward_propagation_flag);
    bool is_enabled_do_not_fragment_flag = instance->ionBundle->bundleProcFlags & BDL_DOES_NOT_FRAGMENT;
    UniboCGR_Bundle_set_flag_do_not_fragment(instance->uniboCgrBundle, is_enabled_do_not_fragment_flag);

    UniboCGR_Bundle_set_flag_probe(instance->uniboCgrBundle, false); // not found "probe flag" in ION

    if (instance->ionBundle->priority == 0) {
        UniboCGR_Bundle_set_priority_bulk(instance->uniboCgrBundle);
    } else if (instance->ionBundle->priority == 1) {
        UniboCGR_Bundle_set_priority_normal(instance->uniboCgrBundle);
    } else {
        UniboCGR_Bundle_set_priority_expedited(instance->uniboCgrBundle, (uint8_t) instance->ionBundle->ordinal);
    }

    UniboCGR_Bundle_set_primary_block_length(instance->uniboCgrBundle, NOMINAL_PRIMARY_BLKSIZE);
    UniboCGR_Bundle_set_total_ext_block_length(instance->uniboCgrBundle, instance->ionBundle->extensionsLength);
    UniboCGR_Bundle_set_payload_length(instance->uniboCgrBundle, instance->ionBundle->payload.length);

    UniboCGR_Bundle_set_previous_node_id(instance->uniboCgrBundle, instance->ionBundle->clDossier.senderNodeNbr);

    UniboCGR_Bundle_set_delivery_confidence(instance->uniboCgrBundle, instance->ionBundle->dlvConfidence);

    return 0;
}

static CgrRoute* getIonRoute(ION_UniboCGR* instance, PsmPartition ionwm) {
    if (instance->first_route) {
        return lyst_data(lyst_first(instance->routes));
    } else {
        return allocateIonRoute(ionwm);
    }
}

static void setIonRouteUsed(ION_UniboCGR* instance, CgrRoute* IonRoute) {
    if (!instance->first_route) {
        lyst_insert_last(instance->routes, IonRoute);
    } else {
        instance->first_route = false;
    }
}

static void setIonRouteNotUsed(ION_UniboCGR* instance, CgrRoute* IonRoute, PsmPartition ionwm) {
    if (!instance->first_route) {
        releaseIonRoute(IonRoute, ionwm);
    }
}

static void resetIonRoutes(ION_UniboCGR* instance, PsmPartition ionwm) {
    LystElt first = lyst_first(instance->routes);
    CgrRoute* firstRoute = lyst_data(first);
    // clear hops of first route
    sm_list_clear(ionwm, firstRoute->hops, NULL, NULL);
    // keep always the first route
    LystElt elt = lyst_next(first);
    while (elt) {
        LystElt next = lyst_next(elt);
        lyst_delete(elt);
        elt = next;
    }

    instance->first_route = true;
}

/******************************************************************************
 *
 * \par Function Name:
 *      convert_routes_from_cgr_to_ion
 *
 * \brief Convert a list of routes from CGR's format to ION's format
 *
 *
 * \par Date Written: 
 *      19/02/20
 *
 * \return int 
 *
 * \retval   0  All routes converted
 * \retval  -1  CGR's contact points to NULL
 * \retval  -2  Memory allocation error
 *
 * \param[in]     instance
 * \param[in]     ionwm
 * \param[in]     ionvdb
 * \param[in]     terminusNode   ION destination
 * \param[in]     uniboCgrRoutest
 * \param[out]    IonRoutes
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int convert_routes_from_cgr_to_ion(ION_UniboCGR* instance, PsmPartition ionwm, IonVdb *ionvdb,
                                          UniboCGR_route_list uniboCgrRouteList, Lyst IonRoutes) {
    UniboCGR_Route uniboCgrRoute = NULL;
    UniboCGR_Error okRoute = UniboCGR_get_first_route(instance->uniboCgr, uniboCgrRouteList, &uniboCgrRoute);
    for (/* empty */; okRoute == UniboCGR_NoError; okRoute = UniboCGR_get_next_route(instance->uniboCgr, &uniboCgrRoute)) {
        CgrRoute* IonRoute = getIonRoute(instance, ionwm);
        if (!IonRoute) {
            // fatal error
            return -1;
        }

        IonRoute->toNodeNbr = UniboCGR_Route_get_neighbor(uniboCgrRoute);
        IonRoute->fromTime = UniboCGR_Route_get_best_case_transmission_time(instance->uniboCgr, uniboCgrRoute);
        IonRoute->toTime = UniboCGR_Route_get_expiration_time(instance->uniboCgr, uniboCgrRoute);
        IonRoute->arrivalTime = UniboCGR_Route_get_best_case_arrival_time(instance->uniboCgr, uniboCgrRoute);
        IonRoute->maxVolumeAvbl = UniboCGR_Route_get_route_volume_limit(uniboCgrRoute);
        IonRoute->bundleECCC = UniboCGR_Bundle_get_estimated_volume_consumption(instance->uniboCgrBundle);
        IonRoute->eto = UniboCGR_Route_get_eto(instance->uniboCgr, uniboCgrRoute);
        IonRoute->pbat = UniboCGR_Route_get_projected_bundle_arrival_time(instance->uniboCgr, uniboCgrRoute);
        IonRoute->arrivalConfidence = UniboCGR_Route_get_arrival_confidence(uniboCgrRoute);
        uint64_t overbooked = 0;
        uint64_t committed = 0;
        UniboCGR_Route_get_overbooking_management(uniboCgrRoute, &overbooked, &committed);
        convert_uint64_to_scalar(overbooked, &IonRoute->overbooked);
        convert_uint64_to_scalar(committed, &IonRoute->committed);

        // now create hop list

        UniboCGR_Contact uniboCgrContact = NULL;
        UniboCGR_Error okContact = UniboCGR_get_first_hop(instance->uniboCgr, uniboCgrRoute, &uniboCgrContact);
        bool skipRoute = false;
        for (/* empty */; okContact == UniboCGR_NoError; okContact = UniboCGR_get_next_hop(instance->uniboCgr, &uniboCgrContact)) {
            IonCXref ionContactLocal;
            if (convert_contact_from_cgr_to_ion(instance, uniboCgrContact, &ionContactLocal) < 0) {
                // that sounds like a "core" bug -- should never happen
                setIonRouteNotUsed(instance, IonRoute, ionwm);
                skipRoute = true;
                break;
            }
            PsmAddress ionContactTreeNode = sm_rbt_search(ionwm, ionvdb->contactIndex, rfx_order_contacts, &ionContactLocal, 0);

            if (!ionContactTreeNode) {
                // contact not found -- maybe it has been removed by RFX daemon -- it is not a fatal error but we must skip this route
                setIonRouteNotUsed(instance, IonRoute, ionwm);
                skipRoute = true;
                break;
            }
            PsmAddress ionContactAddr = sm_rbt_data(ionwm, ionContactTreeNode);
            if (!ionContactAddr) {
                // contact not found -- maybe it has been removed by RFX daemon -- it is not a fatal error but we must skip this route
                setIonRouteNotUsed(instance, IonRoute, ionwm);
                skipRoute = true;
                break;
            }
            // insert the contact into hop list
            if (!sm_list_insert_last(ionwm, IonRoute->hops, ionContactAddr)) {
                // fatal error
                setIonRouteNotUsed(instance, IonRoute, ionwm);
                return -1;
            }
        }

        if (UniboCGR_check_fatal_error(okContact)) {
            return -1;
        }

        if (skipRoute) {
            continue;
        }

        // ok route created

        printDebugIonRoute(ionwm, IonRoute);

        setIonRouteUsed(instance, IonRoute);

        // insert the route into output list
        if (!lyst_insert_last(IonRoutes, (void*) IonRoute)) {
            // fatal error
            return -1;
        }
    }

    if (UniboCGR_check_fatal_error(okRoute)) {
        return -1;
    }

    return 0;
}

/**
 * \param instance
 * \param current_time
 * \param ionwm
 * \param ionvdb
 * \return int
 * \retval  0    success
 * \retval "< 0" fatal error
 */
static int update_region_contact_plan(ION_UniboCGR* instance, time_t current_time, PsmPartition ionwm, IonVdb *ionvdb) {
    UniboCGR_Error error = UniboCGR_contact_plan_open(instance->uniboCgr, current_time);

    if (UniboCGR_check_error(error)) {
        putErrmsg(UniboCGR_get_error_string(error), NULL);
        return -1;
    }

    // destroy previous contact plan -- then reload the whole contact plan from ION VDB

    // note: we can do this only because ION stores MTVs into SDR (and we load MTV from SDR).
    //       You are allowed to reset the contact plan before each update
    //       only if you set to true the "copy MTV" flag when you call UniboCGR_contact_plan_add_contact().
    UniboCGR_contact_plan_reset(instance->uniboCgr);

    PsmAddress contactNodeAddr;
    uvast prevFromNode = 0;
    uvast prevToNode = 0;
    for (contactNodeAddr = sm_rbt_first(ionwm, ionvdb->contactIndex); contactNodeAddr; contactNodeAddr = sm_rbt_next(ionwm, contactNodeAddr)) {
        IonCXref* ionContact = convert_PsmAddress_to_IonCXref(ionwm, sm_rbt_data(ionwm, contactNodeAddr));
        if (convert_contact_from_ion_to_cgr(instance, ionContact, instance->uniboCgrContact) < 0) {
            // unknown type
            continue;
        }

        Sdr sdr = getIonsdr();
        IonContact contactBuf;
        Object contactObj = sdr_list_data(sdr, ionContact->contactElt);
        sdr_read(sdr, (char *) &contactBuf, contactObj, sizeof(IonContact));
        UniboCGR_Contact_set_mtv_bulk(instance->uniboCgrContact, contactBuf.mtv[0]);
        UniboCGR_Contact_set_mtv_normal(instance->uniboCgrContact, contactBuf.mtv[1]);
        UniboCGR_Contact_set_mtv_expedited(instance->uniboCgrContact, contactBuf.mtv[2]);

        UniboCGR_Error uniboCgrContactError;
        uniboCgrContactError = UniboCGR_contact_plan_add_contact(instance->uniboCgr, instance->uniboCgrContact, true);
        if (UniboCGR_check_error(uniboCgrContactError)) {
            if (UniboCGR_check_fatal_error(uniboCgrContactError)) {
                // some bad error occurred -- need to close contact plan session and return a negative value
                putErrmsg(UniboCGR_get_error_string(uniboCgrContactError), NULL);
                UniboCGR_contact_plan_close(instance->uniboCgr);
                return -1;
            }
            // skip this contact
            continue;
        }

        // ok contact added

        // check if we already inserted ranges from sender to receiver
        // we can perform this check since ION contacts are ordered by node number
        if (prevFromNode == ionContact->fromNode && prevToNode == ionContact->toNode) continue;

        // new sender/receiver pair -- update "prev" references for next iteration
        prevFromNode = ionContact->fromNode;
        prevToNode = ionContact->toNode;

        // now insert all ranges from sender to receiver

        PsmAddress rangeNodeAddr;
        IonRXref IonRangeLocal;
        memset(&IonRangeLocal, 0, sizeof(IonRXref));
        IonRangeLocal.fromNode = ionContact->fromNode;
        IonRangeLocal.toNode = ionContact->toNode;

        oK(sm_rbt_search(ionwm, ionvdb->rangeIndex, rfx_order_ranges, &IonRangeLocal, &rangeNodeAddr));
        for ( /* empty */ ; rangeNodeAddr; rangeNodeAddr = sm_rbt_next(ionwm, rangeNodeAddr)) {
            IonRXref *IonRange = convert_PsmAddress_to_IonRXref(ionwm, sm_rbt_data(ionwm, rangeNodeAddr));

            if (IonRange->fromNode != IonRangeLocal.fromNode) break; // processed all ranges between sender and receiver
            if (IonRange->toNode != IonRangeLocal.toNode)     break; // processed all ranges between sender and receiver

            convert_range_from_ion_to_cgr(instance, IonRange, instance->uniboCgrRange);
            UniboCGR_Error uniboCgrRangeError;
            uniboCgrRangeError = UniboCGR_contact_plan_add_range(instance->uniboCgr, instance->uniboCgrRange);
            if (UniboCGR_check_error(uniboCgrRangeError)) {
                if (UniboCGR_check_fatal_error(uniboCgrRangeError)) {
                    // some bad error occurred -- need to close contact plan session and return a negative value
                    putErrmsg(UniboCGR_get_error_string(uniboCgrRangeError), NULL);
                    UniboCGR_contact_plan_close(instance->uniboCgr);
                    return -1;
                }
                // skip this range
                continue;
            }
            // ok range added
        }
    }
    // contact plan update done -- close contact plan session and return success (0)
    UniboCGR_contact_plan_close(instance->uniboCgr);
    return 0;
}

/******************************************************************************
 *
 * \par Function Name:
 *      update_contact_plan
 *
 * \brief  Check if the ION's contact plan changed and in affermative case
 *         update the CGR's contact plan.
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval   0  Contact plan has been changed and updated
 * \retval  -1  Contact plan isn't changed
 * \retval  -2  MTAKE error
 *
 * \param[in]   ionwm           The ION's contact plan partition
 * \param[in]   *ionvdb         The ION's volatile database
 * \param[in]   reference_time  The reference time used to convert POSIX time in differential time from it.
 *
 * \warning ionvdb doesn't have to be NULL.
 * \warning ionwm  doesn't have to be NULL.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int update_contact_plan(ION_UniboCGR_SAP* sap, time_t current_time, PsmPartition ionwm, IonVdb *ionvdb) {
    if (ionvdb->lastEditTime.tv_sec == sap->contact_plan_edit_time.tv_sec
    && ionvdb->lastEditTime.tv_usec == sap->contact_plan_edit_time.tv_usec) {
        // nothing to do
        return 0;
    }
    sap->contact_plan_edit_time = ionvdb->lastEditTime;

    if (update_region_contact_plan(sap->home_cgr, current_time, ionwm, ionvdb) < 0) {
        return -1;
    }
    if (update_region_contact_plan(sap->outer_cgr, current_time, ionwm, ionvdb) < 0) {
        return -1;
    }

	return 0;
}

/******************************************************************************
 *
 * \par Function Name:
 *      exclude_neighbors
 *
 * \brief  Exclude all the neighbors that can't be used as first hop
 *         to reach the destination.
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval   0   Success case: List converted
 * \retval  -1   Fatal error
 *
 * \param[in,out]  *instance       Contains the uniboCgrExcludedNeighborsList -- where the excluded neighbor is appended.
 * \param[in]      excludedNodes   The list of the excluded neighbors
 *
 * \par Notes:
 *           1. This function, first, clear the previous excludedNeighbors list
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int exclude_neighbors(ION_UniboCGR *instance, Lyst excludedNodes)
{
    UniboCGR_reset_excluded_neighbors_list(instance->uniboCgrExcludedNeighbors);

    LystElt elt;
	for (elt = lyst_first(excludedNodes); elt; elt = lyst_next(elt))
	{
		uvast node = (uaddr) lyst_data(elt);
		if (node)
		{
            UniboCGR_Error error = UniboCGR_add_excluded_neighbor(instance->uniboCgrExcludedNeighbors, (uint64_t) node);

            if (UniboCGR_check_error(error)) {
                return -1;
            }
		}

	}

	return 0;
}

static ION_UniboCGR* select_UniboCGR_instance(ION_UniboCGR_SAP* sap, uvast destination) {
    uint32_t regionNbr = 0;
    oK(ionRegionOf(getOwnNodeNbr(), destination, &regionNbr));
    int regionIdx = ionPickRegion(regionNbr);

    if (regionIdx == 0) {
        return sap->home_cgr;
    } else if (regionIdx == 1) {
        return sap->outer_cgr;
    }

    return NULL;
}

/**
 * \brief Updates "home" and "outer" region numbers for Unibo-CGR instances
 * \retval 0 success
 */
static int update_region_number(ION_UniboCGR_SAP* sap) {
    Sdr		sdr = getIonsdr();
    Object		iondbObj = getIonDbObject();
    IonDB		iondb;
    sdr_read(sdr, (char *) &iondb, iondbObj, sizeof(IonDB));
    sap->home_cgr->regionNbr = iondb.regions[0].regionNbr;
    sap->outer_cgr->regionNbr = iondb.regions[1].regionNbr;
    return 0;
}

/******************************************************************************
 *
 * \par Function Name:
 *      cgr_identify_best_routes
 *
 * \brief  Entry point to call the CGR from ION, get the best routes to reach
 *         the destination for the bundle.
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval   0       Success (the number of routes found coincides with the length of ionBestRoutes list)
 * \retval  "< 0"    System error
 *
 * \param[in]     *terminusNode     The destination node for the bundle
 * \param[in]     *bundle           The ION's bundle that has to be forwarded
 * \param[in]     excludedNodes     Nodes to which bundle must not be forwarded
 * \param[in]     time              The current time
 * \param[in]     isap              The service access point for this session
 * \param[in]     *trace            CGR calculation tracing control
 * \param[out]    ionBestRoutes     The lyst of best routes found
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *  01/08/20 | L. Persampieri  |  Changed return values.
 *  22/10/22 | L. Persampieri  |  Major refactoring.
 *****************************************************************************/
int	cgr_identify_best_routes(IonNode *terminusNode, Bundle *bundle, Lyst excludedNodes, time_t time, CgrSAP isap,
		CgrTrace *trace, Lyst ionBestRoutes)
{
    (void) trace; // unused
	PsmPartition	ionwm = getIonwm();
	IonVdb		*ionvdb = getIonVdb();
	CgrVdb		*cgrvdb = cgr_get_vdb();
	ION_UniboCGR_SAP *sap = (ION_UniboCGR_SAP*) isap;

    debug_printf("Entry point interface.");

    // maybe region number changed since last routing call
    if (update_region_number(sap) < 0) {
        // fatal error
        return -1;
    }

    // update contact plan for all Unibo-CGR instances

    if (update_contact_plan(sap, time, ionwm, ionvdb) < 0) {
        // fatal error
        return -1;
    }

    // extrapolate a single Unibo-CGR instance (by means of local node & destination common region)
    ION_UniboCGR* instance = select_UniboCGR_instance(sap, terminusNode->nodeNbr);
    if (!instance) {
        // unknown region
        // is conceptually the same as a routing failure (no routes found to reach destination)
        debug_printf("Unibo-CGR instance not found for destination " UVAST_FIELDSPEC ".", terminusNode->nodeNbr);
        return 0;
    }

    UniboCGR_routing_open(instance->uniboCgr, time);

    /* delete previous run routes.
     * ipnfw uses routes in a read-only mode and does not take ownership ((ION)CgrRoute(s) - object - lifetime is in our hands),
     * so, it is actually safe to deallocate routes here.
     *
     * Unibo-CGR developers need to check if this is still true every time
     * a new ION version is released. */
    resetIonRoutes(instance, ionwm);

    if (create_ion_node_routing_object(terminusNode, ionwm, cgrvdb) < 0) {
        UniboCGR_log_write(instance->uniboCgr, "Fatal error - cannot create ION Node Routing Object.");
        UniboCGR_routing_close(instance->uniboCgr);
        return -1;
    }

    instance->ionBundle = bundle;
    if (convert_bundle_from_ion_to_cgr(instance, terminusNode->nodeNbr) < 0) {
        UniboCGR_log_write(instance->uniboCgr, "Fatal error - cannot perform conversion from ION Bundle to Unibo-CGR Bundle.");
        UniboCGR_routing_close(instance->uniboCgr);
        return -1;
    }
    if (exclude_neighbors(instance, excludedNodes) < 0) {
        UniboCGR_log_write(instance->uniboCgr, "Fatal error - cannot initialize excluded neighbors list.");
        UniboCGR_routing_close(instance->uniboCgr);
        return -1;
    }

    // call Unibo-CGR routing algorithm to find best routes
    UniboCGR_route_list uniboCgrBestRoutes = NULL;
    UniboCGR_Error error = UniboCGR_routing(instance->uniboCgr,
                                            instance->uniboCgrBundle,
                                            instance->uniboCgrExcludedNeighbors,
                                            &uniboCgrBestRoutes);

    if (UniboCGR_check_error(error)) {
        UniboCGR_routing_close(instance->uniboCgr);
        if (UniboCGR_check_fatal_error(error)) {
            UniboCGR_log_write(instance->uniboCgr, "%s", UniboCGR_get_error_string(error));
            return -1;
        }
        // route not found
        return 0;
    }
    if (convert_routes_from_cgr_to_ion(instance, ionwm, ionvdb, uniboCgrBestRoutes, ionBestRoutes) < 0) {
        UniboCGR_log_write(instance->uniboCgr, "Fatal error - cannot convert Unibo-CGR routes into ION routes.");
        UniboCGR_routing_close(instance->uniboCgr);
        return -1;
    }
#if (CGRREB)
    if(CGRR_management(instance, ionBestRoutes) < 0) {
        UniboCGR_log_write(instance->uniboCgr, "Fatal error - CGRR_management() failed.");
        UniboCGR_routing_close(instance->uniboCgr);
        return -1;
    }
#endif

    UniboCGR_routing_close(instance->uniboCgr);
    UniboCGR_log_flush(instance->uniboCgr);

	return 0;
}

/******************************************************************************
 *
 * \par Function Name:
 *      cgr_stop_SAP
 *
 * \brief  Deallocate all the memory used by the CGR session.
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return	void
 *
 * \param[in]	sap	The service access point for this CGR session.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void cgr_stop_SAP(CgrSAP isap)
{
    if (!isap) return;

    ION_UniboCGR_SAP* sap = (ION_UniboCGR_SAP*) isap;

    ION_UniboCGR_destroy(sap->home_cgr);
    ION_UniboCGR_destroy(sap->outer_cgr);
    memset(sap, 0, sizeof(ION_UniboCGR_SAP));
    MRELEASE(sap);
}

static int enable_UniboCGR_default_features(UniboCGR uniboCgr, const char* log_directory) {
    UniboCGR_Error error;
    // suppress unused warning with (void)
    (void) error;
    (void) uniboCgr;
    (void) log_directory;

    UniboCGR_feature_open(uniboCgr, getCtime());

#if UNIBO_CGR_FEATURE_LOG
    error = UniboCGR_feature_logger_enable(uniboCgr, log_directory);
    if (UniboCGR_check_error(error)) {
        UniboCGR_feature_close(uniboCgr);
        putErrmsg("Cannot enable Unibo-CGR logger feature", NULL);
        return -1;
    }
#endif
#if UNIBO_CGR_FEATURE_ONE_ROUTE_PER_NEIGHBOR
    uint32_t limitNeighbors = UNIBO_CGR_FEATURE_ONE_ROUTE_PER_NEIGHBOR_LIMIT;
    error = UniboCGR_feature_OneRoutePerNeighbor_enable(uniboCgr, limitNeighbors);
    if (UniboCGR_check_error(error)) {
        UniboCGR_feature_close(uniboCgr);
        putErrmsg("Cannot enable Unibo-CGR one-route-per-neighbor feature", NULL);
        return -1;
    }
#endif
#if UNIBO_CGR_FEATURE_QUEUE_DELAY
    error = UniboCGR_feature_QueueDelay_enable(uniboCgr);
    if (UniboCGR_check_error(error)) {
        UniboCGR_feature_close(uniboCgr);
        putErrmsg("Cannot enable Unibo-CGR queue-delay feature", NULL);
        return -1;
    }
#endif
#if UNIBO_CGR_FEATURE_REACTIVE_ANTI_LOOP
    error = UniboCGR_feature_ReactiveAntiLoop_enable(uniboCgr);
    if (UniboCGR_check_error(error)) {
        UniboCGR_feature_close(uniboCgr);
        putErrmsg("Cannot enable Unibo-CGR reactive-anti-loop feature", NULL);
        return -1;
    }
#endif
#if UNIBO_CGR_FEATURE_PROACTIVE_ANTI_LOOP
    error = UniboCGR_feature_ProactiveAntiLoop_enable(uniboCgr);
    if (UniboCGR_check_error(error)) {
        UniboCGR_feature_close(uniboCgr);
        putErrmsg("Cannot enable Unibo-CGR proactive-anti-loop feature", NULL);
        return -1;
    }
#endif
#if UNIBO_CGR_FEATURE_MODERATE_SOURCE_ROUTING
    error = UniboCGR_feature_ModerateSourceRouting_enable(uniboCgr);
    if (UniboCGR_check_error(error)) {
        UniboCGR_feature_close(uniboCgr);
        putErrmsg("Cannot enable Unibo-CGR moderate-source-routing feature", NULL);
        return -1;
    }
#endif

    UniboCGR_feature_close(uniboCgr);
    return 0;
}

/******************************************************************************
 *
 * \par Function Name:
 *      cgr_start_SAP
 *
 * \brief  Initialize the CGR session.
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval   0   Success case: CGR initialized
 * \retval  -1   Some error occurred during initialization
 *
 * \param[in]		ownNode	The node from which CGR will compute all routes
 * \param[in]		time	The reference time (time 0 for the CGR session)
 * \param[in,out]	sap	    The service access point for this session.
 *                          It must be allocated by the caller.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int	cgr_start_SAP(const uvast ownNode, const time_t time, CgrSAP *isap)
{
    (void) time;
	Sdr		sdr = getIonsdr();
	PsmPartition	ionwm;
	IonVdb		*ionvdb;

	if (isap == NULL || sdr == NULL)
	{
		putErrmsg("Cannot initialize Unibo-CGR.", NULL);
		return -1;
	}

    ionwm = getIonwm();
    ionvdb = getIonVdb();
    CHKERR(sdr_begin_xn(sdr));


    ION_UniboCGR_SAP* sap = MTAKE(sizeof(ION_UniboCGR_SAP));
    if (!sap) {
        *isap = NULL;
        sdr_exit_xn(sdr);
        return -1;
    }
    *isap = sap;

    // initialize Unibo-CGR library
    UniboCGR_setup_memory_allocator(allocFromIonMemory, releaseToIonMemory);

#if UNIBO_CGR_RELATIVE_TIME
    const time_t reference_time =  time;
#else
    const time_t reference_time = 0;
#endif
    const time_t current_time = getCtime();

    sap->home_cgr = ION_UniboCGR_create(ownNode, reference_time, current_time, ionwm);
    if (!sap->home_cgr) {
        putErrmsg("Cannot start Unibo-CGR for home region", NULL);
        cgr_stop_SAP(*isap);
        *isap = NULL;
        sdr_exit_xn(sdr);
        return -1;
    }
    sap->outer_cgr = ION_UniboCGR_create(ownNode, reference_time, current_time, ionwm);
    if (!sap->outer_cgr) {
        putErrmsg("Cannot start Unibo-CGR for outer region", NULL);
        cgr_stop_SAP(*isap);
        *isap = NULL;
        sdr_exit_xn(sdr);
        return -1;
    }
    if (update_region_number(sap) < 0) {
        cgr_stop_SAP(*isap);
        *isap = NULL;
        sdr_exit_xn(sdr);
        return -1;
    }

    if (enable_UniboCGR_default_features(sap->home_cgr->uniboCgr, "cgr_log_home") < 0) {
        putErrmsg("Cannot enable Unibo-CGR features for home region", NULL);
        cgr_stop_SAP(*isap);
        *isap = NULL;
        sdr_exit_xn(sdr);
        return -1;
    }
    if (enable_UniboCGR_default_features(sap->outer_cgr->uniboCgr, "cgr_log_outer") < 0) {
        putErrmsg("Cannot enable Unibo-CGR features for outer region", NULL);
        cgr_stop_SAP(*isap);
        *isap = NULL;
        sdr_exit_xn(sdr);
        return -1;
    }

    if (update_contact_plan(sap, current_time, ionwm, ionvdb) < 0) {
        putErrmsg("Cannot load Unibo-CGR contact plan", NULL);
        cgr_stop_SAP(*isap);
        sdr_exit_xn(sdr);
        return -1;
    }

	sdr_exit_xn(sdr);
    // success
	return 0;
}

#endif

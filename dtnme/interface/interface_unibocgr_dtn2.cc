/** \file interface_unibocgr_dtn2.cc
 *  
 *  \brief    This file provides the implementation of the functions
 *            that make UniboCGR's implementation compatible with DTN2.
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
 *  \author Giacomo Gori, giacomo.gori3@studio.unibo.it
 *
 *  \par Supervisor
 *       Carlo Caini, carlo.caini@unibo.it
 */
#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif
#include "interface_unibocgr_dtn2.h"
#include "feature-config.h"

// include from system
#include <cstdlib>
#include <iostream>

// include from dtnme
#include "../../../RouteEntry.h"

// include from Unibo-CGR
#include "../../include/UniboCGR.h"

// include from ContactPlanManager
#include "../../../../contact_plan/ContactPlanManager.h"
#include "../../../../contact_plan/CPContact.h"
#include "../../../../contact_plan/CPRange.h"


#define NOMINAL_PRIMARY_BLKSIZE	29 // from ION 4.0.0: bpv7/library/libbpP.c
#define EPOCH_2000_SEC 946684800
#define UNUSED(x) (void)(x)

using dtn::CPContact;
using dtn::CPRange;
using dtn::Bundle;
using dtn::ContactPlanManager;
using dtn::UniboCGRBundleRouter;

struct DTNME_UniboCGR {
    struct timeval lastContactPlanUpdate;

    /* * from Unibo-CGR * */

    UniboCGR uniboCgr; // Unibo-CGR instance
    UniboCGR_Bundle uniboCgrBundle; // cached Unibo-CGR bundle
    UniboCGR_Contact uniboCgrContact; // cached Unibo-CGR contact
    UniboCGR_Range uniboCgrRange; // cached Unibo-CGR range
    UniboCGR_excluded_neighbors_list uniboCgrExcludedNeighborsList; // cached Unibo-CGR Excluded Neighbors List

    /* * from DTNME * */

    uint64_t local_node_number = 0; // who am i?
    dtn::UniboCGRBundleRouter* uniborouter; // DTNME router
};

// callback
static int computeApplicableBacklog(uint64_t neighbor,
                                    UniboCGR_BundlePriority priority,
                                    uint8_t ordinal,
                                    uint64_t *applicableBacklog,
                                    uint64_t *totalBacklog,
                                    void* userArg);

static void DTNME_UniboCGR_destroy(DTNME_UniboCGR* instance, time_t current_time_unix) {
    if (!instance) return;
    UniboCGR_close(&instance->uniboCgr, current_time_unix);
    UniboCGR_Contact_destroy(&instance->uniboCgrContact);
    UniboCGR_Range_destroy(&instance->uniboCgrRange);
    UniboCGR_Bundle_destroy(&instance->uniboCgrBundle);
    UniboCGR_destroy_excluded_neighbors_list(&instance->uniboCgrExcludedNeighborsList);
    delete instance;
}

static DTNME_UniboCGR* DTNME_UniboCGR_create(time_t current_time_unix,
                                             time_t reference_time_unix,
                                             uint64_t local_node_number,
                                             dtn::UniboCGRBundleRouter* router) {
    DTNME_UniboCGR* instance = new DTNME_UniboCGR();
    if (!instance) { return nullptr; } // maybe DTNME uses no-throw new()

    instance->local_node_number = local_node_number;
    instance->uniborouter = router;

    UniboCGR_Error uniboCgrError;
    uniboCgrError = UniboCGR_Contact_create(&instance->uniboCgrContact);
    if (UniboCGR_check_error(uniboCgrError)) {
        std::cerr << "Cannot create Unibo-CGR Contact: " << UniboCGR_get_error_string(uniboCgrError) << std::endl;
        DTNME_UniboCGR_destroy(instance, current_time_unix);
        return nullptr;
    }
    uniboCgrError = UniboCGR_Range_create(&instance->uniboCgrRange);
    if (UniboCGR_check_error(uniboCgrError)) {
        std::cerr << "Cannot create Unibo-CGR Range: " << UniboCGR_get_error_string(uniboCgrError) << std::endl;
        DTNME_UniboCGR_destroy(instance, current_time_unix);
        return nullptr;
    }
    uniboCgrError = UniboCGR_Bundle_create(&instance->uniboCgrBundle);
    if (UniboCGR_check_error(uniboCgrError)) {
        std::cerr << "Cannot create Unibo-CGR Bundle: " << UniboCGR_get_error_string(uniboCgrError) << std::endl;
        DTNME_UniboCGR_destroy(instance, current_time_unix);
        return nullptr;
    }
    uniboCgrError = UniboCGR_create_excluded_neighbors_list(&instance->uniboCgrExcludedNeighborsList);
    if (UniboCGR_check_error(uniboCgrError)) {
        std::cerr << "Cannot create Unibo-CGR Excluded Neighbors List: " << UniboCGR_get_error_string(uniboCgrError) << std::endl;
        DTNME_UniboCGR_destroy(instance, current_time_unix);
        return nullptr;
    }
    uniboCgrError = UniboCGR_open(&instance->uniboCgr,
                                  current_time_unix,
                                  reference_time_unix,
                                  local_node_number,
                                  PhaseThreeCostFunction_default,
                                  computeApplicableBacklog,
                                  instance);
    if (UniboCGR_check_error(uniboCgrError)) {
        std::cerr << "Cannot create Unibo-CGR instance: " << UniboCGR_get_error_string(uniboCgrError) << std::endl;
        DTNME_UniboCGR_destroy(instance, current_time_unix);
        return nullptr;
    }

    return instance;
}

static DTNME_UniboCGR* get_unibo_cgr_instance(DTNME_UniboCGR* newInstance = NULL) {
    static DTNME_UniboCGR* instance = NULL;
    if (newInstance) {
        instance = newInstance;
    }
    return instance;
}

// str must be a string in ipn format ( ipn:x.y ). otherwise undefined behaviour.
static uint64_t extract_node_number_from_ipn_eid_string(const std::string& str) {
    std::string delimiter1 = ":";
    std::string delimiter2 = ".";
    std::string s = str.substr(str.find(delimiter1) + 1, str.find(delimiter2) - 1);
    std::stringstream convert;
    uint64_t node_number;
    convert << s;
    convert >> node_number;
    return node_number;
}

/******************************************************************************
 *
 * \par Function Name:
 *      convert_bundle_from_dtn2_to_cgr
 *
 * \brief Convert the characteristics of the bundle from DTN2 to
 *        this CGR's implementation and initialize all the
 *        bundle fields used by the Unibo CGR.
 *
 *
 * \par Date Written:
 *      05/07/20
 *
 * \return int
 *
 * \retval  0  Success case
 * \retval -1  Arguments error
 * \retval -2 Bundle have a DTN name, not IPN. Can't use CGR.
 *
 * \param[in]    uniboCgr           Unibo-CGR instance
 * \param[in]    *Dtn2Bundle        The bundle in DTN2
 * \param[out]   *CgrBundle         The bundle in this CGR's implementation.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  05/07/20 | G. Gori		   |  Initial Implementation and documentation.
 *  03/11/22 | L. Persampieri  |  Adapted to new Unibo-CGR version (2.0).
 *****************************************************************************/
static int convert_bundle_from_dtn2_to_cgr(UniboCGR uniboCgr, dtn::Bundle *Dtn2Bundle, UniboCGR_Bundle CgrBundle)
{
	int result = -1;

	if (Dtn2Bundle && CgrBundle)
	{
        UniboCGR_Bundle_reset(CgrBundle);
        result = -2;
		std::string destination_eid_str = Dtn2Bundle->dest().str();
		if (destination_eid_str.rfind("ipn:", 0) == 0) {

            uint64_t destNode = extract_node_number_from_ipn_eid_string(destination_eid_str);

            UniboCGR_Bundle_set_destination_node_id(CgrBundle, destNode);

            UniboCGR_Bundle_set_flag_do_not_fragment(CgrBundle, Dtn2Bundle->do_not_fragment());
            UniboCGR_Bundle_set_flag_probe(CgrBundle, false); // not found in DTNME
            UniboCGR_Bundle_set_flag_backward_propagation(CgrBundle, false); // not found in DTNME
#ifdef ECOS_ENABLED
            UniboCGR_Bundle_set_flag_critical(CgrBundle, Dtn2Bundle->ecos_critical());
            switch (Dtn2Bundle->priority()) {
                case dtn::Bundle::COS_BULK:
                    UniboCGR_Bundle_set_priority_bulk(CgrBundle);
                    break;
                case dtn::Bundle::COS_EXPEDITED:
                    UniboCGR_Bundle_set_priority_expedited(CgrBundle, Dtn2Bundle->ecos_ordinal());
                    break;
                default:
                    UniboCGR_Bundle_set_priority_normal(CgrBundle);
                    break;
            }
#else
            UniboCGR_Bundle_set_flag_critical(CgrBundle, false);
            UniboCGR_Bundle_set_priority_normal(CgrBundle);
#endif

            UniboCGR_Bundle_set_delivery_confidence(CgrBundle, 0.0f); // not found in DTNME

            if (UniboCGR_feature_logger_check(uniboCgr)) {
                UniboCGR_Bundle_set_source_node_id(CgrBundle, Dtn2Bundle->dest().str().c_str());
                UniboCGR_Bundle_set_sequence_number(CgrBundle, Dtn2Bundle->creation_ts().seqno_);

                if( Dtn2Bundle->is_fragment()) {
                    UniboCGR_Bundle_set_fragment_offset(CgrBundle, Dtn2Bundle->frag_offset());
                    UniboCGR_Bundle_set_fragment_length(CgrBundle, Dtn2Bundle->frag_length());
                } else {
                    UniboCGR_Bundle_set_fragment_offset(CgrBundle, 0);
                    UniboCGR_Bundle_set_fragment_length(CgrBundle, 0);
                }

                UniboCGR_Bundle_set_total_application_data_unit_length(CgrBundle, Dtn2Bundle->orig_length());
            }

            UniboCGR_Bundle_set_primary_block_length(CgrBundle, NOMINAL_PRIMARY_BLKSIZE);
            UniboCGR_Bundle_set_total_ext_block_length(CgrBundle, 0); // not found in DTNME
            UniboCGR_Bundle_set_payload_length(CgrBundle, Dtn2Bundle->payload().length());

            if (Dtn2Bundle->is_bpv6()) {
                UniboCGR_Bundle_set_bundle_protocol_version(CgrBundle, 6);
                // BPv6 -- creation time and lifetime in seconds
                UniboCGR_Bundle_set_creation_time(CgrBundle, Dtn2Bundle->creation_time_secs());
                UniboCGR_Bundle_set_lifetime(CgrBundle, Dtn2Bundle->expiration_secs());
            } else { // BPv7
                UniboCGR_Bundle_set_bundle_protocol_version(CgrBundle, 7);
                // BPv7 -- creation time and lifetime in milliseconds
                UniboCGR_Bundle_set_creation_time(CgrBundle, Dtn2Bundle->creation_time_millis());
                UniboCGR_Bundle_set_lifetime(CgrBundle, Dtn2Bundle->expiration_millis());
            }

            std::string previous_hop_eid_str = Dtn2Bundle->prevhop().str();
            if (previous_hop_eid_str.rfind("ipn:", 0) == 0) {
                uint64_t previous_hop_node_number = extract_node_number_from_ipn_eid_string(previous_hop_eid_str);
                UniboCGR_Bundle_set_previous_node_id(CgrBundle, previous_hop_node_number);
            } else {
                UniboCGR_Bundle_set_previous_node_id(CgrBundle, 0);
            }

            result = 0;
		}
	}

	return result;
}



/******************************************************************************
 *
 * \par Function Name:
 *      convert_routes_from_cgr_to_dtn2
 *
 * \brief Convert a list of routes from CGR's format to DTN2's format
 *
 *
 * \par Date Written: 
 *      16/07/20
 *
 * \return void
 *
 * \param[in]     cgrRoutes       The list of routes in CGR's format
 * \param[out]    *res        	 All the next hops that CGR found, separated by a single space
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  16/07/20 | G. Gori         |  Initial Implementation and documentation.
 *  03/11/22 | L. Persampieri  |  Adapted to new Unibo-CGR version (2.0).
 *****************************************************************************/
static void convert_routes_from_cgr_to_dtn2(UniboCGR uniboCgr, UniboCGR_route_list cgrRoutes, std::string *res)
{
	int numRes = 0;
    UniboCGR_Route uniboCgrRoute = NULL;
    UniboCGR_Error okRoute = UniboCGR_get_first_route(uniboCgr, cgrRoutes, &uniboCgrRoute);
    for (/* empty */; okRoute == UniboCGR_NoError; okRoute = UniboCGR_get_next_route(uniboCgr, &uniboCgrRoute)) {
		if (uniboCgrRoute != NULL)
		{
			if(numRes>0)
			{
				// blank-separated neighbors EID
				res->append(" ");
			}
            uint64_t neighbor = UniboCGR_Route_get_neighbor(uniboCgrRoute);
			std::string toNode = "ipn:";
			std::stringstream streamNode;
			streamNode << toNode << neighbor;
			res->append(streamNode.str());
			// Always using 0 as service number
			res->append(".0");
			numRes++;
		}
	}
}

static void convert_UniboCGR_Contact_to_CPContact(UniboCGR uniboCgr, UniboCGR_Contact inputContact, dtn::CPContact& outputContact) {
    outputContact.setFrom(UniboCGR_Contact_get_sender(inputContact));
    outputContact.setTo(UniboCGR_Contact_get_receiver(inputContact));

    /* note: we must subtract the reference time since Unibo-CGR returns absolute times (Unix) and ContactPlanManager uses relative times. */
    outputContact.setStartTime(UniboCGR_Contact_get_start_time(uniboCgr, inputContact) - dtn::ContactPlanManager::instance()->get_time_zero());
    outputContact.setEndTime(UniboCGR_Contact_get_end_time(uniboCgr, inputContact) - dtn::ContactPlanManager::instance()->get_time_zero());

    outputContact.setTransmissionSpeed(UniboCGR_Contact_get_xmit_rate(inputContact));
}

static void convert_CPContact_to_UniboCGR_Contact(UniboCGR uniboCgr, const dtn::CPContact& inputContact, UniboCGR_Contact outputContact) {
    /* note: default values -- type and confidence are not part of CPContact */
    UniboCGR_Contact_set_type(outputContact, UniboCGR_ContactType_Scheduled);
    UniboCGR_Contact_set_confidence(outputContact, 1.0f);

    UniboCGR_Contact_set_sender(outputContact, inputContact.getFrom());
    UniboCGR_Contact_set_receiver(outputContact, inputContact.getTo());

    /* note: we must add the reference time since Unibo-CGR wants absolute times (Unix) and ContactPlanManager uses relative times. */
    UniboCGR_Contact_set_start_time(uniboCgr, outputContact, inputContact.getStartTime() + dtn::ContactPlanManager::instance()->get_time_zero());
    UniboCGR_Contact_set_end_time(uniboCgr, outputContact, inputContact.getEndTime() + dtn::ContactPlanManager::instance()->get_time_zero());

    UniboCGR_Contact_set_xmit_rate(outputContact, inputContact.getTransmissionSpeed());
}

static void convert_UniboCGR_Range_to_CPRange(UniboCGR uniboCgr, UniboCGR_Range inputRange, dtn::CPRange& outputRange) {
    outputRange.setFrom(UniboCGR_Range_get_sender(inputRange));
    outputRange.setTo(UniboCGR_Range_get_receiver(inputRange));

    /* note: we must subtract the reference time since Unibo-CGR returns absolute times (Unix) and ContactPlanManager uses relative times. */
    outputRange.setStartTime(UniboCGR_Range_get_start_time(uniboCgr, inputRange) - dtn::ContactPlanManager::instance()->get_time_zero());
    outputRange.setEndTime(UniboCGR_Range_get_end_time(uniboCgr, inputRange) - dtn::ContactPlanManager::instance()->get_time_zero());

    outputRange.setDelay(UniboCGR_Range_get_one_way_light_time(inputRange));
}

static void convert_CPRange_to_UniboCGR_Range(UniboCGR uniboCgr, const dtn::CPRange& inputRange, UniboCGR_Range outputRange) {
    UniboCGR_Range_set_sender(outputRange, inputRange.getFrom());
    UniboCGR_Range_set_receiver(outputRange, inputRange.getTo());

    /* note: we must add the reference time since Unibo-CGR wants absolute times (Unix) and ContactPlanManager uses relative times. */
    UniboCGR_Range_set_start_time(uniboCgr, outputRange, inputRange.getStartTime() + dtn::ContactPlanManager::instance()->get_time_zero());
    UniboCGR_Range_set_end_time(uniboCgr, outputRange, inputRange.getEndTime() + dtn::ContactPlanManager::instance()->get_time_zero());

    UniboCGR_Range_set_one_way_light_time(outputRange, inputRange.getDelay());
}

enum UpdateType {
      UpdateType_NoUpdate,
      UpdateType_ChangeEndTime,
      UpdateType_ChangeXmitRate,
      UpdateType_ChangeOneWayLightTime,
      UpdateType_Remove
};

/*
 * Run UniboCGR_contact_plan_change_contact_*() if a CPContact has been changed.
 * Run UniboCGR_contact_plan_remove_contact() if contact is not found.
 * Do nothing otherwise (and return UpdateType_NoUpdate).
 */
static UpdateType handle_contact_update(UniboCGR uniboCgr, UniboCGR_Contact uniboCgrContact, const std::list<CPContact>& contact_list) {
    dtn::CPContact cpContact;
    convert_UniboCGR_Contact_to_CPContact(uniboCgr, uniboCgrContact, cpContact);

    for (std::list<CPContact>::const_iterator it = contact_list.cbegin(); it != contact_list.cend(); ++it) {
        const CPContact &currentCpContact = *it;
        if (cpContact != currentCpContact) continue; // next iteration -- does not match sender/receiver/startTime

        // cpContact found!

        UniboCGR_Error error;

        if (cpContact.getEndTime() != currentCpContact.getEndTime()) {
            error = UniboCGR_contact_plan_change_contact_end_time(uniboCgr,
                                                                  UniboCGR_Contact_get_type(uniboCgrContact),
                                                                  UniboCGR_Contact_get_sender(uniboCgrContact),
                                                                  UniboCGR_Contact_get_receiver(uniboCgrContact),
                                                                  UniboCGR_Contact_get_start_time(uniboCgr, uniboCgrContact),
                                                                  currentCpContact.getEndTime() + dtn::ContactPlanManager::instance()->get_time_zero());
            if (error == UniboCGR_NoError) {
                return UpdateType_ChangeEndTime;
            }
        }
        if (cpContact.getTransmissionSpeed() != currentCpContact.getTransmissionSpeed()) {
            UniboCGR_contact_plan_change_contact_xmit_rate(uniboCgr,
                                                           UniboCGR_Contact_get_type(uniboCgrContact),
                                                           UniboCGR_Contact_get_sender(uniboCgrContact),
                                                           UniboCGR_Contact_get_receiver(uniboCgrContact),
                                                           UniboCGR_Contact_get_start_time(uniboCgr, uniboCgrContact),
                                                           currentCpContact.getTransmissionSpeed());
            if (error == UniboCGR_NoError) {
                return UpdateType_ChangeXmitRate;
            }
        }

        // contact found but do not need to update it
        return UpdateType_NoUpdate;
    }
    UniboCGR_log_write(uniboCgr, "Before contact remove");
    // contact not found -- must remove it from Unibo-CGR
    UniboCGR_contact_plan_remove_contact(uniboCgr,
                                         UniboCGR_Contact_get_type(uniboCgrContact),
                                         UniboCGR_Contact_get_sender(uniboCgrContact),
                                         UniboCGR_Contact_get_receiver(uniboCgrContact),
                                         UniboCGR_Contact_get_start_time(uniboCgr, uniboCgrContact));

    return UpdateType_Remove;
}


/*
 * Run UniboCGR_contact_plan_change_range_*() if a CPRange has been changed.
 * Run UniboCGR_contact_plan_remove_range() if range is not found.
 * Do nothing otherwise (and return UpdateType_NoUpdate).
 */
static UpdateType handle_range_update(UniboCGR uniboCgr, UniboCGR_Range uniboCgrRange, const std::list<CPRange>& range_list) {
    dtn::CPRange cpRange;
    convert_UniboCGR_Range_to_CPRange(uniboCgr, uniboCgrRange, cpRange);

    for (std::list<CPRange>::const_iterator it = range_list.cbegin(); it != range_list.cend(); ++it) {
        const CPRange &currentCpRange = *it;
        if (cpRange != currentCpRange) continue; // next iteration -- does not match sender/receiver/startTime

        // cpRange found!

        UniboCGR_Error error;

        if (cpRange.getEndTime() != currentCpRange.getEndTime()) {
            error = UniboCGR_contact_plan_change_range_end_time(uniboCgr,
                                                                UniboCGR_Range_get_sender(uniboCgrRange),
                                                                UniboCGR_Range_get_receiver(uniboCgrRange),
                                                                UniboCGR_Range_get_start_time(uniboCgr, uniboCgrRange),
                                                                currentCpRange.getEndTime() + dtn::ContactPlanManager::instance()->get_time_zero());
            if (error == UniboCGR_NoError) {
                return UpdateType_ChangeEndTime;
            }
        }
        if (cpRange.getDelay() != currentCpRange.getDelay()) {
            error = UniboCGR_contact_plan_change_range_one_way_light_time(uniboCgr,
                                                                          UniboCGR_Range_get_sender(uniboCgrRange),
                                                                          UniboCGR_Range_get_receiver(uniboCgrRange),
                                                                          UniboCGR_Range_get_start_time(uniboCgr, uniboCgrRange),
                                                                          currentCpRange.getDelay());
            if (error == UniboCGR_NoError) {
                return UpdateType_ChangeOneWayLightTime;
            }
        }

        // range found but do not need to update it
        return UpdateType_NoUpdate;
    }

    // range not found -- must remove it from Unibo-CGR
    UniboCGR_contact_plan_remove_range(uniboCgr,
                                       UniboCGR_Range_get_sender(uniboCgrRange),
                                       UniboCGR_Range_get_receiver(uniboCgrRange),
                                       UniboCGR_Range_get_start_time(uniboCgr, uniboCgrRange));

    return UpdateType_Remove;
}

static void update_contact_plan(DTNME_UniboCGR* instance, time_t current_time_unix) {
    dtn::ContactPlanManager* cpm = dtn::ContactPlanManager::instance();
    UniboCGR_log_write(instance->uniboCgr, "Before contact plan update");
    if (cpm->check_for_updates(&instance->lastContactPlanUpdate)) {
        UniboCGR_log_write(instance->uniboCgr, "Before contact plan open");
        UniboCGR_contact_plan_open(instance->uniboCgr, current_time_unix);
        UniboCGR_log_write(instance->uniboCgr, "Before get contact list");
        std::list<CPContact> contact_list = cpm->get_contact_list();
        UniboCGR_log_write(instance->uniboCgr, "Before contact update");
        { /* check for contact to update/remove */
            UniboCGR_Contact uniboCgrContact = NULL;
            UniboCGR_Error okContact = UniboCGR_get_first_contact(instance->uniboCgr, &uniboCgrContact);
            while (okContact == UniboCGR_NoError) {
                UniboCGR_log_write(instance->uniboCgr, "sender %lu receiver %lu start %ld",
                                   UniboCGR_Contact_get_sender(uniboCgrContact),
                                   UniboCGR_Contact_get_receiver(uniboCgrContact),
                                   UniboCGR_Contact_get_start_time(instance->uniboCgr, uniboCgrContact));
                if (handle_contact_update(instance->uniboCgr, uniboCgrContact, contact_list) != UpdateType_NoUpdate) {
                    // need to iterate again from first contact -- change occurred (current iterator invalidated)
                    okContact = UniboCGR_get_first_contact(instance->uniboCgr, &uniboCgrContact);
                    UniboCGR_log_write(instance->uniboCgr, "loop first contact");
                } else {
                    okContact = UniboCGR_get_next_contact(instance->uniboCgr, &uniboCgrContact);
                    UniboCGR_log_write(instance->uniboCgr, "loop next contact");
                }
            }
        }
        UniboCGR_log_write(instance->uniboCgr, "Before contact add");
        { /* check for contact to add */
            for (std::list<CPContact>::iterator it = contact_list.begin(); it != contact_list.end(); ++it) {
                convert_CPContact_to_UniboCGR_Contact(instance->uniboCgr, *it, instance->uniboCgrContact);
                UniboCGR_contact_plan_add_contact(instance->uniboCgr,
                                                  instance->uniboCgrContact,
                                                  false);
                UniboCGR_log_write(instance->uniboCgr, "loop add contact");
            }
        }
        UniboCGR_log_write(instance->uniboCgr, "Before get range list");
        std::list<CPRange> range_list = cpm->get_range_list();
        UniboCGR_log_write(instance->uniboCgr, "Before range update");
        { /* check for range to update/remove */
            UniboCGR_Range uniboCgrRange = NULL;
            UniboCGR_Error okRange = UniboCGR_get_first_range(instance->uniboCgr, &uniboCgrRange);
            while (okRange == UniboCGR_NoError) {
                if (handle_range_update(instance->uniboCgr, uniboCgrRange, range_list) != UpdateType_NoUpdate) {
                    // need to iterate again from first range -- change occurred (current iterator invalidated)
                    okRange = UniboCGR_get_first_range(instance->uniboCgr, &uniboCgrRange);
                    UniboCGR_log_write(instance->uniboCgr, "loop first range");
                } else {
                    okRange = UniboCGR_get_next_range(instance->uniboCgr, &uniboCgrRange);
                    UniboCGR_log_write(instance->uniboCgr, "loop next range");
                }
            }
        }
        UniboCGR_log_write(instance->uniboCgr, "Before range add");
        { /* check for range to add */
            for (std::list<CPRange>::iterator it = range_list.begin(); it != range_list.end(); ++it) {
                convert_CPRange_to_UniboCGR_Range(instance->uniboCgr, *it, instance->uniboCgrRange);
                UniboCGR_contact_plan_add_range(instance->uniboCgr,
                                                instance->uniboCgrRange);
                UniboCGR_log_write(instance->uniboCgr, "loop add range");
            }
        }

        UniboCGR_contact_plan_close(instance->uniboCgr);
    }
}

/******************************************************************************
 *
 * \par Function Name:
 *      callUniboCGR
 *
 * \brief  Entry point to call the CGR from DTN2, get the best routes to reach
 *         the destination for the bundle.
 *
 *
 * \par Date Written:
 *      05/07/20
 *
 * \return int
 *
 * \retval    0   Routes found and converted
 * \retval   -1   System error
 * \retval   -8   Bundle's conversion error: bundle have DTN name instead of IPN
 *
 * \param[in]     time              The current time
 * \param[in]     *bundle           The DTN2's bundle that has to be forwarded
 * \param[out]    *res          	The list of best routes found written on a string separated by a space
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  05/07/20 | G. Gori		    |  Initial Implementation and documentation.
 *  19/04/22 | Federico Le Pera |  Support to update contact plan only if it changes
 *  03/11/22 | Lorenzo Persampieri |  Adapted to new Unibo-CGR version (2.0).
 *****************************************************************************/
int callUniboCGR(time_t time, dtn::Bundle *bundle, std::string *res)
{
	DTNME_UniboCGR* instance = get_unibo_cgr_instance();

    if (!instance || !bundle) {
        // same as "route not found" case
        *res = "";
        return 0;
    }

    update_contact_plan(instance, time);

    UniboCGR_routing_open(instance->uniboCgr, time);

    int result = convert_bundle_from_dtn2_to_cgr(instance->uniboCgr, bundle, instance->uniboCgrBundle);
    if (result == -2) {
        UniboCGR_routing_close(instance->uniboCgr);
        return -8; // unknown destination scheme
    } else if (result < 0) {
        UniboCGR_routing_close(instance->uniboCgr);
        return -1; // fatal error
    }

    UniboCGR_reset_excluded_neighbors_list(instance->uniboCgrExcludedNeighborsList);

    UniboCGR_route_list cgrRoutes = NULL;
    UniboCGR_Error error = UniboCGR_routing(instance->uniboCgr,
                                            instance->uniboCgrBundle,
                                            instance->uniboCgrExcludedNeighborsList,
                                            &cgrRoutes);

    if (UniboCGR_check_error(error)) {
        UniboCGR_routing_close(instance->uniboCgr);
        if (UniboCGR_check_fatal_error(error)) {
            UniboCGR_log_write(instance->uniboCgr, "%s", UniboCGR_get_error_string(error));
            return -1;
        }
        // route not found
        *res = "";
    } else {
        convert_routes_from_cgr_to_dtn2(instance->uniboCgr, cgrRoutes, res);
        UniboCGR_routing_close(instance->uniboCgr);
    }
    
    return 0;
}

/******************************************************************************
 *
 * \par Function Name:
 *      computeApplicableBacklog
 *
 * \brief  Compute the applicable backlog (SABR 3.2.6.2 b) ) and the total backlog for a neighbor.
 *
 *
 * \par Date Written:
 *      20/07/20
 *
 * \return int
 *
 * \retval   0   Applicable and total backlog computed
 * \retval  -1   Arguments error
 *
 * \param[in]   neighbor                   The neighbor for which we want to compute
 *                                         the applicable and total backlog.
 * \param[in]   priority                   The bundle's cardinal priority
 * \param[in]   ordinal                    The bundle's ordinal priority (used with expedited cardinal priority)
 * \param[out]  *CgrApplicableBacklog      The applicable backlog computed
 * \param[out]  *CgrTotalBacklog           The total backlog computed
 *
 * \par Notes:
 *          1. This function talks with the CL queue, that should be done during
 *             phase two to get a more accurate applicable (and total) backlog
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  20/07/20 | G. Gori         |  Initial Implementation and documentation.
 *  10/08/20 | G. Gori		   |  Fixed oasys::Lock bug
 *  03/11/22 | L. Persampieri  |  Adapted to new Unibo-CGR version (2.0).
 *****************************************************************************/
static int computeApplicableBacklog(uint64_t neighbor,
                                    UniboCGR_BundlePriority priority,
                                    uint8_t ordinal,
                                    uint64_t *applicableBacklog,
                                    uint64_t *totalBacklog,
                                    void* userArg) {
    DTNME_UniboCGR* sap = (DTNME_UniboCGR*) userArg;
	int result = -1;
	UNUSED(ordinal);
	long int byteTot; long int byteApp;
	if (applicableBacklog && totalBacklog)
	{
		byteTot=0; byteApp=0;
		result = sap->uniborouter->getBacklogForNode(neighbor,priority,&byteApp,&byteTot);
		if(result >= 0)
		{
            *applicableBacklog = (uint64_t) byteApp;
            *totalBacklog = (uint64_t) byteTot;
			// debugLog("found %d bundle in queue on neighbor %llu.", result, neighbor);
			result=0;
		}

	}
	else result = -1;
	return result;
}
/**
 * \retval 0 success
 * \retval "< 0" error
 */
static int enable_UniboCGR_default_features(UniboCGR uniboCgr, time_t current_time, const char* log_directory) {
    UniboCGR_Error error;
    // suppress unused warning with (void)
    (void) error;
    (void) uniboCgr;
    (void) log_directory;

    UniboCGR_feature_open(uniboCgr, current_time);

#if UNIBO_CGR_FEATURE_LOG
    error = UniboCGR_feature_logger_enable(uniboCgr, log_directory);
    if (UniboCGR_check_error(error)) {
        UniboCGR_feature_close(uniboCgr);
        std::cerr << "Cannot enable Unibo-CGR logger feature" << std::endl;
        return -1;
    }
#endif
#if UNIBO_CGR_FEATURE_ONE_ROUTE_PER_NEIGHBOR
    uint32_t limitNeighbors = UNIBO_CGR_FEATURE_ONE_ROUTE_PER_NEIGHBOR_LIMIT;
    error = UniboCGR_feature_OneRoutePerNeighbor_enable(uniboCgr, limitNeighbors);
    if (UniboCGR_check_error(error)) {
        UniboCGR_feature_close(uniboCgr);
        std::cerr << "Cannot enable Unibo-CGR one-route-per-neighbor feature" << std::endl;
        return -1;
    }
#endif
#if UNIBO_CGR_FEATURE_QUEUE_DELAY
    error = UniboCGR_feature_QueueDelay_enable(uniboCgr);
    if (UniboCGR_check_error(error)) {
        UniboCGR_feature_close(uniboCgr);
        std::cerr << "Cannot enable Unibo-CGR queue-delay feature" << std::endl;
        return -1;
    }
#endif
    
    UniboCGR_feature_close(uniboCgr);

    return 0;
}

/******************************************************************************
 *
 * \par Function Name:
 *      destroy_contact_graph_routing
 *
 * \brief  Deallocate all the memory used by the CGR.
 *
 *
 * \par Date Written:
 *      14/07/20
 *
 * \return  void
 *
 * \param[in]  current_time  The current time
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  14/07/20 | G. Gori		   |  Initial Implementation and documentation.
 *  03/11/22 | L. Persampieri  |  Adapted to new Unibo-CGR version (2.0).
 *****************************************************************************/
void destroy_contact_graph_routing(time_t current_time)
{
    DTNME_UniboCGR_destroy(get_unibo_cgr_instance(), current_time);
}

/******************************************************************************
 *
 * \par Function Name:
 *      initialize_contact_graph_routing
 *
 * \brief  Initialize all the data used by the CGR.
 *
 *
 * \par Date Written:
 *      01/07/20
 *
 * \return int
 *
 * \retval   1   Success case: CGR initialized
 * \retval  -1   Error case: ownNode can't be 0
 * \retval  -2   can't open or read file with contacts
 * \retval  -3   Error case: log directory can't be opened
 * \retval  -4   Error case: log file can't be opened
 * \retval  -5   Arguments error
 *
 * \param[in]   ownNode   The node that the CGR will consider as contacts graph's root
 * \param[in]   time      The reference unix time (time 0 for the CGR)
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR             |   DESCRIPTION
 *  -------- | ------------------- | -----------------------------------------------
 *  01/07/20 | G. Gori             |  Initial Implementation and documentation.
 *  20/03/22 | Federico Le Pera    |  Now interface_unibocgr using the ContactPlanManager
 *  						       |  to read the contact-plan.txt and init the sturcts of CGR
 *  03/11/22 | Lorenzo Persampieri |  Adapted to new Unibo-CGR version (2.0).
 *****************************************************************************/
int initialize_contact_graph_routing(const uint64_t ownNode, const time_t current_time, dtn::UniboCGRBundleRouter *router)
{
    if (get_unibo_cgr_instance()) { return 1; } // already initialized

#if UNIBO_CGR_RELATIVE_TIME
    const time_t reference_time = ContactPlanManager::instance()->get_time_zero();
#else
    const time_t reference_time = 0;
#endif

    DTNME_UniboCGR* instance = DTNME_UniboCGR_create(current_time, reference_time, ownNode, router);
    if (!instance) {
        std::cerr << "Cannot initialize Unibo-CGR." << std::endl;
        return -1;
    }

    if (enable_UniboCGR_default_features(instance->uniboCgr, current_time, "cgr_log") < 0) {
        std::cerr << "Cannot initialize Unibo-CGR features." << std::endl;
        DTNME_UniboCGR_destroy(instance, current_time);
        return -1;
    }

    update_contact_plan(instance, current_time);

    (void) get_unibo_cgr_instance(instance); // save

    // success
    return 1;
}
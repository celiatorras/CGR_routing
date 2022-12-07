/** \file bundles.h
 *
 *  \brief  This file provides the definition of the CgrBundle type
 *          and of the Priority type.
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

#ifndef LIBRARY_MANAGEBUNDLES_H_
#define LIBRARY_MANAGEBUNDLES_H_

#include <sys/time.h>

#include "../library/list/list_type.h"
#include "../library/commonDefines.h"
#include "../routes/routes.h"
#include "../contact_plan/contacts/contacts.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
	Bulk = 0, Normal, Expedited
} Priority;

typedef struct
{
	char source_node[256];
	uint64_t creation_timestamp;
	uint64_t sequence_number;
    uint64_t fragment_length;
    uint64_t fragment_offset;
} CgrBundleID;

typedef struct
{
    /**
     * \brief Bundle Protocol Version (6,7).
     */
    uint64_t bp_version;
    /**
     * \brief Bundle's lifetime as declared in Primary Block.
     */
    uint64_t lifetime;
	/**
	 * \brief Bundle's ID
	 */
	CgrBundleID id;
	/**
	 * \brief Ipn node number of the Node that sends to me (the own node) the bundle.
	 *
	 * \details Previous hop.
	 */
	uint64_t sender_node;
	/**
	 * \brief Ipn node number of the destination node for the bundle.
	 */
	uint64_t terminus_node;
	/**
	 * \brief Bulk, Normal or Expedited.
	 */
	Priority priority_level;
	/**
	 * \brief This is a bit mask
	 * - Critical bit (SET_CRITICAL, UNSET_CRITICAL and IS_CRITICAL macros)
	 * - Probe bit (SET_PROBE, UNSET_PROBE and IS_PROBE macros)
	 * - Fragmentable bit (SET_FRAGMENTABLE,UNSET_FRAGMENTABLE and IS_FRAGMENTABLE macros)
	 * - Backward propagation bit (SET_BACKWARD_PROPAGATION,UNSET_BACKWARD_PROPAGATION and RETURN_TO_SENDER macros)
	 * - You can clear this mask with CLEAR_FLAGS macro
	 */
	unsigned char flags;
	/**
	 * \brief From 0 to 255, only for Expedited priority_level.
	 */
	unsigned int ordinal;

	uint64_t primary_block_length;
    uint64_t extension_blocks_length;
    uint64_t payload_block_length;
    uint64_t total_adu_length;
	/**
	 *  \brief Estimated volume consumption SABR 2.4.3.
	 */
	uint64_t evc;
	/**
	 * \brief The time when the bundle's lifetime expires (deadline).
	 */
	time_t expiration_time;
	/**
	 * \brief From 0.0 to 1.0.
	 */
	float dlvConfidence;
	/**
	 * \brief The geographic route of the bundle, used by the CGR to avoid loops;
	 *        get from RGR extension block.
	 */
	List geoRoute;
	/**
	 * \brief The list of neighbors that caused a loop for this bundle.
	 */
	List failedNeighbors;
	/**
	 * \brief The MSR route get from CGRR extension block.
	 */
	Route *msrRoute;
    Contact *last_msr_route_contact;
} CgrBundle;

#define CRITICAL               (1) /* (00000001) */
#define PROBE                  (2) /* (00000010) */
#define FRAGMENTABLE           (4) /* (00000100) */
#define BACKWARD_PROPAGATION   (8) /* (00001000) */

#define SET_CRITICAL(bundle) (((bundle)->flags) |= CRITICAL)
#define UNSET_CRITICAL(bundle) (((bundle)->flags) &= ~CRITICAL)

#define SET_PROBE(bundle) (((bundle)->flags) |= PROBE)
#define UNSET_PROBE(bundle) (((bundle)->flags) &= ~PROBE)

#define SET_FRAGMENTABLE(bundle) (((bundle)->flags) |= FRAGMENTABLE)
#define UNSET_FRAGMENTABLE(bundle) (((bundle)->flags) &= ~FRAGMENTABLE)

#define SET_BACKWARD_PROPAGATION(bundle) (((bundle)->flags) |= BACKWARD_PROPAGATION)
#define UNSET_BACKWARD_PROPAGATION(bundle) (((bundle)->flags) &= ~BACKWARD_PROPAGATION)

#define IS_CRITICAL(bundle) (((bundle)->flags) & CRITICAL)
#define IS_PROBE(bundle) (((bundle)->flags) & PROBE)
#define IS_FRAGMENTABLE(bundle) (((bundle)->flags) & FRAGMENTABLE)
#define RETURN_TO_SENDER(bundle) (((bundle)->flags) & BACKWARD_PROPAGATION)

extern int add_ipn_node_to_list(List nodes, uint64_t ipnNode);
extern int search_ipn_node(List nodes, uint64_t target);
extern int set_failed_neighbors_list(CgrBundle *bundle, uint64_t ownNode);
extern int set_geo_route_list(char *geoRouteString, CgrBundle *bundle);
extern int check_bundle(CgrBundle *bundle);
extern uint64_t computeBundleEVC(uint64_t size);
extern void bundle_destroy(CgrBundle *bundle);
extern CgrBundle* bundle_create();
extern void reset_bundle(CgrBundle *bundle);
extern int initialize_bundle(int backward_propagation, int critical, float dlvConfidence,
		time_t expiration_time, Priority priority, unsigned int ordinal, int probe, int fragmentable, uint64_t size,
		uint64_t sender_node, uint64_t terminus_node, CgrBundle *bundle);

/**
 * \brief Print the bundle's ID in the main log file.
 *
 */
extern void print_log_bundle_id(UniboCGRSAP* uniboCgrSap, CgrBundle* bundle);

extern void print_bundle(UniboCGRSAP* uniboCgrSap, FILE *file_call, CgrBundle *bundle, List excludedNodes, time_t currentTime);

#ifdef __cplusplus
}
#endif

#endif /* LIBRARY_MANAGEBUNDLES_H_ */


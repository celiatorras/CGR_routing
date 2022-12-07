/** \file UniboCGR.c
 *
 *  \brief  This file provides the implementation of the Unibo-CGR API functions.
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

#include "config.h"
#include "../include/UniboCGR.h"
#include "UniboCGRSAP.h"
#include "contact_plan/contacts/contacts.h"
#include "contact_plan/ranges/ranges.h"
#include "routes/routes.h"
#include "msr/msr_utils.h"
#include "cgr/cgr.h"
#include "msr/msr.h"
#include "library/log/log.h"
#include "time_analysis/time.h"

#define UNIBO_CGR_DTN_EPOCH 946684800

typedef enum {
    UniboCGR_NoSession = 0,
    UniboCGR_Session_routing,
    UniboCGR_Session_contact_plan,
    UniboCGR_Session_feature
} UniboCGR_Session;

struct UniboCGRSAP {
    /**
     * \brief current Unibo-CGR session (or UniboCGR_NoSession)
     */
    UniboCGR_Session session;
    /**
     * \brief BP version. Default: 7.
     */
    uint64_t bundle_protocol_version;
    /**
     * \brief Seconds elapsed since Unix epoch. Useful for debug/test purposes. In production code must be 0.
     *
     * \details Each time passed to Unibo-CGR will be decreased by this quantity.
     *          E.g. if the current time is "100" and the reference_time is "60"
     *          then Unibo-CGR interprets the current time as "40".
     */
    time_t reference_time;
    /**
     * \brief Keep the Unibo-CGR current time.
     *
     * \details current_time + reference_time == Unix time point (seconds)
     */
    time_t current_time;
    /**
     * \brief Purpose: count the calls to Unibo-CGR. (Currently used just for logging).
     */
    uint32_t count_bundles;
    /**
     * \brief The own ipn node.
     */
    uint64_t localNode;
    /**
     * \brief True if it is safer to discard routing objects before process a routing call.
     * \details e.g. contact/range insertion/removal/change or some routing-related feature has been enabled/disabled
     */
    bool mustClearRoutingObjects;
    /**
     * \brief Function implemented by interface. Used to retrieve information about a given neighbor during phase_two
     */
    computeApplicableBacklogCallback computeApplicableBacklog;
    /**
     * \brief Passed to user-defined callback.
     */
    void* userArg;
    /**
     * \brief Handle the contacts graph.
     */
    ContactSAP* contactSap;
    /**
     * \brief Handle the ranges graph.
     */
    RangeSAP* rangeSap;
    /**
     * \brief Handle the nodes graph.
     */
    NodeSAP* nodeSap;
    /**
     * \brief Initialized and used internally by phase_one.c
     */
    PhaseOneSAP* phaseOneSap;
    /**
     * \brief Initialized and used internally by phase_two.c
     */
    PhaseTwoSAP* phaseTwoSap;
    /**
     * \brief Initialized and used internally by phase_three.c
     */
    PhaseThreeSAP* phaseThreeSap;
    /**
     * \brief Keep info about the current call.
     */
    UniboCgrCurrentCallSAP* uniboCgrCurrentCallSap;
    /**
     * \brief Initialized and used internally by msr.c
     */
    MSRSAP* msrSap;
    /**
     * \brief Handle the Unibo-CGR logging.
     */
    LogSAP* logSap;
    /**
     * \brief Used when time analysis is enabled.
     */
    TimeAnalysisSAP* timeAnalysisSap;

    ListElt* route_iterator;
    ListElt* hop_iterator;
    RbtNode* contact_iterator;
    RbtNode* range_iterator;

    bool feature_logger;
    bool feature_one_route_per_neighbor;
    uint32_t feature_one_route_per_neighbor_limit;
    bool feature_queue_delay;
    bool feature_reactive_anti_loop;
    bool feature_proactive_anti_loop;
    bool feature_moderate_source_routing;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                            UTILITIES                                *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define CHECK_EQUAL_SESSION(s) \
do {                           \
    if ((s) != uniboCgrSap->session) {                                 \
        if ((uniboCgrSap->session) == UniboCGR_NoSession) return UniboCGR_ErrorSessionClosed; \
        return UniboCGR_ErrorWrongSession; \
    } \
} while(0)

#define CHECK_OPEN_SESSION \
do {                           \
    if (UniboCGR_NoSession != uniboCgrSap->session) { \
        return UniboCGR_ErrorSessionAlreadyOpened; \
    } \
} while(0)

static void* default_malloc_wrapper(const char* file, int line, size_t size) {
    (void) file;
    (void) line;
    return malloc(size);
}
static void default_free_wrapper(const char* file, int line, void* addr) {
    (void) file;
    (void) line;
    return free(addr);
}
static malloc_like get_malloc_like_memory_allocator(malloc_like* new_mtake) {
    static malloc_like mtake = default_malloc_wrapper;
    if (new_mtake) {
        mtake = *new_mtake;
    }
    return mtake;
}
static free_like get_free_like_memory_allocator(free_like* new_mrelease) {
    static free_like mrelease = default_free_wrapper;
    if (new_mrelease) {
        mrelease = *new_mrelease;
    }
    return mrelease;
}
static UniboCGR_ContactType CtType_to_UniboCGR_ContactType(CtType type) {
    switch (type) {
        // case TypeRegistration: return UniboCGR_ContactType_Registration;
        case TypeScheduled: return UniboCGR_ContactType_Scheduled;
        // case TypeSuppressed: return UniboCGR_ContactType_Suppressed;
        // case TypePredicted: return UniboCGR_ContactType_Predicted;
        // case TypeHypothetical: return UniboCGR_ContactType_Hypothetical;
        // case TypeDiscovered: return UniboCGR_ContactType_Discovered;
    }
    return UniboCGR_ContactType_Scheduled;
}
static CtType UniboCGR_ContactType_to_CtType(UniboCGR_ContactType type) {
    switch (type) {
        case UniboCGR_ContactType_Unknown:  return TypeScheduled;
        // case UniboCGR_ContactType_Registration: return TypeRegistration;
        case UniboCGR_ContactType_Scheduled: return TypeScheduled;
        // case UniboCGR_ContactType_Suppressed: return TypeSuppressed;
        // case UniboCGR_ContactType_Predicted: return TypePredicted;
        // case UniboCGR_ContactType_Hypothetical: return TypeHypothetical;
        // case UniboCGR_ContactType_Discovered: return TypeDiscovered;
    }
    return TypeScheduled;
}
static UniboCGR_Error UniboCGR_force_update(UniboCGR uniboCgr) {
    if (!uniboCgr) return UniboCGR_ErrorInvalidArgument;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    uniboCgrSap->mustClearRoutingObjects = true;
    int retval = UniboCGRSAP_handle_updates(uniboCgrSap);
    if (retval == 0) {
        return UniboCGR_NoError;
    } else {
        return UniboCgr_ErrorSystem;
    }
}
static void UniboCGR_set_current_time(UniboCGR uniboCgr, time_t current_time) {
    if (!uniboCgr) { return; }

    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    uniboCgrSap->current_time = current_time - uniboCgrSap->reference_time;
    LogSAP_setLogTime(uniboCgrSap, uniboCgrSap->current_time);
}
const char* UniboCGR_get_error_string(UniboCGR_Error error) {
    switch (error) {
        case UniboCGR_NoError:                      return "Unibo-CGR: success.";
        case UniboCGR_ErrorUnknown:                 return "Unibo-CGR: Unknown error.";
        case UniboCgr_ErrorSystem:                  return "Unibo-CGR: System error.";
        case UniboCGR_ErrorInvalidArgument:         return "Unibo-CGR: Invalid argument.";
        case UniboCGR_ErrorInternal:                return "Unibo-CGR: Internal error.";
        case UniboCGR_ErrorCannotOpenLogDirectory:  return "Unibo-CGR: Cannot open log directory.";
        case UniboCGR_ErrorCannotOpenLogFile:       return "Unibo-CGR: Cannot open log file.";
        case UniboCGR_ErrorInvalidNodeNumber:       return "Unibo-CGR: Invalid node number.";
        case UniboCGR_ErrorContactNotFound:         return "Unibo-CGR: Contact not found.";
        case UniboCGR_ErrorFoundOverlappingContact: return "Unibo-CGR: Found an overlapping contact.";
        case UniboCGR_ErrorRangeNotFound:           return "Unibo-CGR: Range not found.";
        case UniboCGR_ErrorFoundOverlappingRange:   return "Unibo-CGR: Found an overlapping range.";
        case UniboCGR_ErrorRouteNotFound:           return "Unibo-CGR: Route not found.";
        case UniboCGR_ErrorInvalidTime:             return "Unibo-CGR: Invalid time.";
        case UniboCGR_ErrorMalformedMSRRoute:       return "Unibo-CGR: Malformed MSR route.";
        case UniboCGR_ErrorSessionAlreadyOpened:    return "Unibo-CGR: Session is already opened.";
        case UniboCGR_ErrorSessionClosed:           return "Unibo-CGR: Session is closed.";
        case UniboCGR_ErrorWrongSession:            return "Unibo-CGR: Wrong session.";
    }
    return "Unibo-CGR: Unknown error.";
}
bool UniboCGR_check_error(UniboCGR_Error error) {
    return (error != UniboCGR_NoError);
}
bool UniboCGR_check_fatal_error(UniboCGR_Error error) {
    switch (error) {
        case UniboCGR_NoError:                      return false;
        case UniboCGR_ErrorUnknown:                 return false;
        case UniboCgr_ErrorSystem:                  return true;
        case UniboCGR_ErrorInvalidArgument:         return false;
        case UniboCGR_ErrorInternal:                return true;
        case UniboCGR_ErrorCannotOpenLogDirectory:  return false;
        case UniboCGR_ErrorCannotOpenLogFile:       return false;
        case UniboCGR_ErrorInvalidNodeNumber:       return false;
        case UniboCGR_ErrorContactNotFound:         return false;
        case UniboCGR_ErrorFoundOverlappingContact: return false;
        case UniboCGR_ErrorRangeNotFound:           return false;
        case UniboCGR_ErrorFoundOverlappingRange:   return false;
        case UniboCGR_ErrorRouteNotFound:           return false;
        case UniboCGR_ErrorInvalidTime:             return false;
        case UniboCGR_ErrorMalformedMSRRoute:       return false;
        case UniboCGR_ErrorSessionAlreadyOpened:    return false;
        case UniboCGR_ErrorSessionClosed:           return false;
        case UniboCGR_ErrorWrongSession:            return false;
    }

    return false;
}
time_t UniboCGR_get_reference_time(UniboCGR uniboCgr) {
    if (!uniboCgr) return 0;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    return uniboCgrSap->reference_time;
}
UniboCGR_Error UniboCGR_find_contact(UniboCGR uniboCgr,
                                     UniboCGR_ContactType contact_type,
                                     uint64_t sender,
                                     uint64_t receiver,
                                     time_t start_time,
                                     UniboCGR_Contact *contactOutput) {
    (void) contact_type; // unused
    if (!uniboCgr) return UniboCGR_ErrorInvalidArgument;
    if (!contactOutput) return UniboCGR_ErrorInvalidArgument;

    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;

    start_time -= uniboCgrSap->reference_time;
    Contact* ct = get_contact(uniboCgrSap, sender, receiver, start_time, &uniboCgrSap->contact_iterator);
    if (ct == NULL) {
        return UniboCGR_ErrorContactNotFound;
    }
    *contactOutput = (UniboCGR_Contact) ct;
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_get_first_contact(UniboCGR uniboCgr,
                                          UniboCGR_Contact* contactOutput) {
    if (!uniboCgr) return UniboCGR_ErrorInvalidArgument;
    if (!contactOutput) return UniboCGR_ErrorInvalidArgument;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    Contact * ct = get_first_contact(uniboCgrSap, &uniboCgrSap->contact_iterator);
    if (ct == NULL) {
        return UniboCGR_ErrorContactNotFound;
    }
    *contactOutput = (UniboCGR_Contact) ct;
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_get_next_contact(UniboCGR uniboCgr,
                                         UniboCGR_Contact* contactOutput) {
    if (!uniboCgr) return UniboCGR_ErrorInvalidArgument;
    if (!contactOutput) return UniboCGR_ErrorInvalidArgument;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    Contact * ct = get_next_contact(&uniboCgrSap->contact_iterator);
    if (ct == NULL) {
        return UniboCGR_ErrorContactNotFound;
    }
    *contactOutput = (UniboCGR_Contact) ct;
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_find_range(UniboCGR uniboCgr,
                                   uint64_t sender,
                                   uint64_t receiver,
                                   time_t start_time,
                                   UniboCGR_Range *rangeOutput) {
    if (!uniboCgr) return UniboCGR_ErrorInvalidArgument;
    if (!rangeOutput) return UniboCGR_ErrorInvalidArgument;

    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;

    start_time -= uniboCgrSap->reference_time;
    Range* range = get_range(uniboCgrSap, sender, receiver, start_time, &uniboCgrSap->range_iterator);
    if (range == NULL) {
        return UniboCGR_ErrorRangeNotFound;
    }
    *rangeOutput = (UniboCGR_Range) range;
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_get_first_range(UniboCGR uniboCgr,
                                        UniboCGR_Range* rangeOutput) {
    if (!uniboCgr) return UniboCGR_ErrorInvalidArgument;
    if (!rangeOutput) return UniboCGR_ErrorInvalidArgument;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    Range * range = get_first_range(uniboCgrSap, &uniboCgrSap->range_iterator);
    if (range == NULL) {
        return UniboCGR_ErrorRangeNotFound;
    }
    *rangeOutput = (UniboCGR_Range) range;
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_get_next_range(UniboCGR uniboCgr,
                                       UniboCGR_Range* rangeOutput) {
    if (!uniboCgr) return UniboCGR_ErrorInvalidArgument;
    if (!rangeOutput) return UniboCGR_ErrorInvalidArgument;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    Range * range = get_next_range(&uniboCgrSap->range_iterator);
    if (range == NULL) {
        return UniboCGR_ErrorRangeNotFound;
    }
    *rangeOutput = (UniboCGR_Range) range;
    return UniboCGR_NoError;
}
static void UniboCGR_log_bundle_routing_call(UniboCGR uniboCgr, UniboCGR_Bundle uniboCgrBundle) {
    if (!uniboCgr || !uniboCgrBundle) return;

    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;

    print_log_bundle_id(uniboCgrSap, bundle);
    if (bundle->id.fragment_offset == 0 && bundle->id.fragment_length == 0) {
        writeLog(uniboCgrSap, "Bundle - Total ADU length: %" PRIu64 ".", bundle->payload_block_length);
    } else {
        writeLog(uniboCgrSap, "Bundle - Total ADU length: %" PRIu64 ".", bundle->total_adu_length);
    }
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                   UNIBO-CGR LIBRARY MANAGEMENT                      *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

UniboCGR_Error UniboCGR_setup_memory_allocator(malloc_like alloc, free_like release) {
    if (!alloc) return UniboCGR_ErrorInvalidArgument;
    if (!release) return UniboCGR_ErrorInvalidArgument;

    // save user-defined memory allocator
    (void) get_malloc_like_memory_allocator(&alloc);
    (void) get_free_like_memory_allocator(&release);

    return UniboCGR_NoError;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                  UNIBO-CGR INSTANCE MANAGEMENT                      *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

UniboCGR_Error UniboCGR_open(UniboCGR* uniboCgr,
                             time_t current_time,
                             time_t time_zero,
                             uint64_t local_node,
                             PhaseThreeCostFunction best_route_selection_function,
                             computeApplicableBacklogCallback computeApplicableBacklog,
                             void* user_arg) {
    if (!uniboCgr) { return UniboCGR_ErrorInvalidArgument; }
    if (local_node == 0) { return UniboCGR_ErrorInvalidNodeNumber; }
    if (time_zero > current_time) { return UniboCGR_ErrorInvalidTime; }
    if (!computeApplicableBacklog) { return UniboCGR_ErrorInvalidArgument; }

    // at the time of writing only default function is supported --
    // -- feel free to add if/else checks here when a new cost functions becomes available
    if (best_route_selection_function != PhaseThreeCostFunction_default) { return UniboCGR_ErrorInvalidArgument; }

    UniboCGRSAP* uniboCgrSap = MWITHDRAW(sizeof(UniboCGRSAP));
    if (!uniboCgrSap) return UniboCgr_ErrorSystem;
    memset(uniboCgrSap, 0, sizeof(UniboCGRSAP));
    uniboCgrSap->feature_one_route_per_neighbor_limit = 1;
    uniboCgrSap->count_bundles = 1;
    uniboCgrSap->computeApplicableBacklog = computeApplicableBacklog;
    uniboCgrSap->userArg = user_arg;
    // just to trigger a check if routing is called before any contact/range insertion
    uniboCgrSap->mustClearRoutingObjects = true;

    uniboCgrSap->reference_time = time_zero;
    uniboCgrSap->current_time = current_time - uniboCgrSap->reference_time;
    uniboCgrSap->localNode = local_node;

    int retval;

    retval = PhaseOneSAP_open(uniboCgrSap);
    if (retval != 0) { UniboCGR_close(uniboCgr, current_time); return UniboCgr_ErrorSystem; }
    retval = PhaseTwoSAP_open(uniboCgrSap);
    if (retval != 0) { UniboCGR_close(uniboCgr, current_time); return UniboCgr_ErrorSystem; }
    retval = PhaseThreeSAP_open(uniboCgrSap);
    if (retval != 0) { UniboCGR_close(uniboCgr, current_time); return UniboCgr_ErrorSystem; }
    if (best_route_selection_function == PhaseThreeCostFunction_default) {
        PhaseThreeSAP_set_cost_function_default(uniboCgrSap);
    }

    retval = UniboCgrCurrentCallSAP_open(uniboCgrSap);
    if (retval != 0) { UniboCGR_close(uniboCgr, current_time); return UniboCgr_ErrorSystem; }
    retval = MSRSAP_open(uniboCgrSap);
    if (retval != 0) { UniboCGR_close(uniboCgr, current_time); return UniboCgr_ErrorSystem; }
    retval = ContactSAP_open(uniboCgrSap);
    if (retval != 0) { UniboCGR_close(uniboCgr, current_time); return UniboCgr_ErrorSystem; }
    retval = RangeSAP_open(uniboCgrSap);
    if (retval != 0) { UniboCGR_close(uniboCgr, current_time); return UniboCgr_ErrorSystem; }
    retval = NodeSAP_open(uniboCgrSap);
    if (retval != 0) { UniboCGR_close(uniboCgr, current_time); return UniboCgr_ErrorSystem; }
    retval = LogSAP_open(uniboCgrSap);
    if (retval != 0) { UniboCGR_close(uniboCgr, current_time); return UniboCgr_ErrorSystem; }
    retval = TimeAnalysisSAP_open(uniboCgrSap);
    if (retval != 0) { UniboCGR_close(uniboCgr, current_time); return UniboCgr_ErrorSystem; }

    *uniboCgr = (UniboCGR) uniboCgrSap;

    return UniboCGR_NoError;
}

void UniboCGR_close(UniboCGR* uniboCgr, time_t current_time) {
    if (!uniboCgr || !*uniboCgr) { return; }

    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) *uniboCgr;

    uniboCgrSap->current_time = current_time - uniboCgrSap->reference_time;
    LogSAP_setLogTime(uniboCgrSap, uniboCgrSap->current_time);
    PhaseOneSAP_close(uniboCgrSap);
    PhaseTwoSAP_close(uniboCgrSap);
    PhaseThreeSAP_close(uniboCgrSap);
    UniboCgrCurrentCallSAP_close(uniboCgrSap);
    MSRSAP_close(uniboCgrSap);
    NodeSAP_close(uniboCgrSap);
    ContactSAP_close(uniboCgrSap);
    RangeSAP_close(uniboCgrSap);
    TimeAnalysisSAP_close(uniboCgrSap);
    writeLog(uniboCgrSap, "Shutdown.");
    LogSAP_close(uniboCgrSap);

    memset(uniboCgrSap, 0, sizeof(UniboCGRSAP));
    MDEPOSIT(uniboCgrSap);
    *uniboCgr = NULL;
}

void* UniboCGRSAP_MWITHDRAW(const char* file, int line, size_t size) {
    malloc_like mtake = get_malloc_like_memory_allocator(NULL);
    return mtake(file, line, size);
}
void UniboCGRSAP_MDEPOSIT(const char* file, int line, void* addr) {
    free_like mrelease = get_free_like_memory_allocator(NULL);
    mrelease(file, line, addr);
}
time_t UniboCGRSAP_get_current_time(UniboCGRSAP* uniboCgrSap) {
    return uniboCgrSap->current_time;
}
uint64_t UniboCGRSAP_get_local_node(UniboCGRSAP* uniboCgrSap) {
    return uniboCgrSap->localNode;
}
uint32_t UniboCGRSAP_get_bundle_count(UniboCGRSAP* uniboCgrSap) {
    return uniboCgrSap->count_bundles;
}
void UniboCGRSAP_increase_bundle_count(UniboCGRSAP* uniboCgrSap) {
    uniboCgrSap->count_bundles++;
}
static void convert_uint64_to_scalar(uint64_t scalarIn, CgrScalar* scalarOut) {
    loadCgrScalar(scalarOut, 0);
    while (scalarIn) {
        debug_printf("scalar in: %" PRIu64, scalarIn);
        if (scalarIn > (uint64_t) INT64_MAX) {
            increaseCgrScalar(scalarOut, INT64_MAX);
            scalarIn -= (uint64_t) INT64_MAX;
        } else {
            increaseCgrScalar(scalarOut, (int64_t) scalarIn);
            scalarIn = 0; // done
        }
    }
}
int UniboCGRSAP_compute_applicable_backlog(UniboCGRSAP* uniboCgrSap,
                                           uint64_t neighbor,
                                           int priority,
                                           uint8_t ordinal,
                                           CgrScalar *applicableBacklog,
                                           CgrScalar *totalBacklog) {
    loadCgrScalar(applicableBacklog, 0);
    loadCgrScalar(totalBacklog, 0);
    uint64_t applicable_backlog_u64 = 0;
    uint64_t total_backlog_u64 = 0;
    int retval = uniboCgrSap->computeApplicableBacklog(neighbor,
                                                       priority,
                                                       ordinal,
                                                       &applicable_backlog_u64,
                                                       &total_backlog_u64,
                                                       uniboCgrSap->userArg);
    if (retval < 0) return -1;

    convert_uint64_to_scalar(applicable_backlog_u64, applicableBacklog);
    convert_uint64_to_scalar(total_backlog_u64, totalBacklog);
    return 0;
}
bool UniboCGRSAP_check_one_route_per_neighbor(UniboCGRSAP* uniboCgrSap, uint32_t* limit) {
    if (limit) {
        *limit = uniboCgrSap->feature_one_route_per_neighbor_limit;
    }
    if (uniboCgrSap->feature_one_route_per_neighbor) {
        return true;
    }
    return false;
}
bool UniboCGRSAP_check_queue_delay(UniboCGRSAP* uniboCgrSap) {
    return uniboCgrSap->feature_queue_delay;
}
bool UniboCGRSAP_check_reactive_anti_loop(UniboCGRSAP* uniboCgrSap) {
    return uniboCgrSap->feature_reactive_anti_loop;
}
bool UniboCGRSAP_check_proactive_anti_loop(UniboCGRSAP* uniboCgrSap) {
    return uniboCgrSap->feature_proactive_anti_loop;
}
bool UniboCGRSAP_check_moderate_source_routing(UniboCGRSAP* uniboCgrSap) {
    return uniboCgrSap->feature_moderate_source_routing;
}
void UniboCGRSAP_set_PhaseOneSAP(UniboCGRSAP* uniboCgrSap, PhaseOneSAP* phaseOneSap) {
    uniboCgrSap->phaseOneSap = phaseOneSap;
}
PhaseOneSAP* UniboCGRSAP_get_PhaseOneSAP(UniboCGRSAP* uniboCgrSap) {
    return uniboCgrSap->phaseOneSap;
}
void UniboCGRSAP_set_PhaseTwoSAP(UniboCGRSAP* uniboCgrSap, PhaseTwoSAP* phaseTwoSap) {
    uniboCgrSap->phaseTwoSap = phaseTwoSap;
}
PhaseTwoSAP* UniboCGRSAP_get_PhaseTwoSAP(UniboCGRSAP* uniboCgrSap) {
    return uniboCgrSap->phaseTwoSap;
}
void UniboCGRSAP_set_PhaseThreeSAP(UniboCGRSAP* uniboCgrSap, PhaseThreeSAP* phaseThreeSap) {
    uniboCgrSap->phaseThreeSap = phaseThreeSap;
}
PhaseThreeSAP* UniboCGRSAP_get_PhaseThreeSAP(UniboCGRSAP* uniboCgrSap) {
    return uniboCgrSap->phaseThreeSap;
}
void UniboCGRSAP_set_UniboCgrCurrentCallSAP(UniboCGRSAP* uniboCgrSap, UniboCgrCurrentCallSAP* uniboCgrCurrentCallSap) {
    uniboCgrSap->uniboCgrCurrentCallSap = uniboCgrCurrentCallSap;
}
UniboCgrCurrentCallSAP* UniboCGRSAP_get_UniboCgrCurrentCallSAP(UniboCGRSAP* uniboCgrSap) {
    return uniboCgrSap->uniboCgrCurrentCallSap;
}
void UniboCGRSAP_set_MSRSAP(UniboCGRSAP* uniboCgrSap, MSRSAP* msrSap) {
    uniboCgrSap->msrSap = msrSap;
}
MSRSAP* UniboCGRSAP_get_MSRSAP(UniboCGRSAP* uniboCgrSap) {
    return uniboCgrSap->msrSap;
}
void UniboCGRSAP_set_ContactSAP(UniboCGRSAP* uniboCgrSap, ContactSAP* contactSap) {
    uniboCgrSap->contactSap = contactSap;
}
ContactSAP* UniboCGRSAP_get_ContactSAP(UniboCGRSAP* uniboCgrSap) {
    return uniboCgrSap->contactSap;
}
void UniboCGRSAP_set_RangeSAP(UniboCGRSAP* uniboCgrSap, RangeSAP* rangeSap) {
    uniboCgrSap->rangeSap = rangeSap;
}
RangeSAP* UniboCGRSAP_get_RangeSAP(UniboCGRSAP* uniboCgrSap) {
    return uniboCgrSap->rangeSap;
}
void UniboCGRSAP_set_NodeSAP(UniboCGRSAP* uniboCgrSap, NodeSAP* nodeSap) {
    uniboCgrSap->nodeSap = nodeSap;
}
NodeSAP* UniboCGRSAP_get_NodeSAP(UniboCGRSAP* uniboCgrSap) {
    return uniboCgrSap->nodeSap;
}
void UniboCGRSAP_set_LogSAP(UniboCGRSAP* uniboCgrSap, LogSAP* logSap) {
    uniboCgrSap->logSap = logSap;
}
LogSAP* UniboCGRSAP_get_LogSAP(UniboCGRSAP* uniboCgrSap) {
    return uniboCgrSap->logSap;
}
void UniboCGRSAP_set_TimeAnalysisSAP(UniboCGRSAP* uniboCgrSap, TimeAnalysisSAP* timeAnalysisSap) {
    uniboCgrSap->timeAnalysisSap = timeAnalysisSap;
}
TimeAnalysisSAP* UniboCGRSAP_get_TimeAnalysisSAP(UniboCGRSAP* uniboCgrSap) {
    return uniboCgrSap->timeAnalysisSap;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                    UNIBO-CGR SESSION FEATURE                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

UniboCGR_Error UniboCGR_feature_open(UniboCGR uniboCgr, time_t time) {
    if (!uniboCgr) return UniboCGR_ErrorInvalidArgument;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP *) uniboCgr;
    CHECK_OPEN_SESSION;
    uniboCgrSap->session = UniboCGR_Session_feature;
    UniboCGR_set_current_time(uniboCgr, time);
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_feature_close(UniboCGR uniboCgr) {
    if (!uniboCgr) return UniboCGR_ErrorInvalidArgument;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP *) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_feature);
    uniboCgrSap->session = UniboCGR_NoSession;
    LogSAP_log_fflush(uniboCgrSap);
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_feature_logger_enable(UniboCGR uniboCgr, const char* log_dir) {
    if (!uniboCgr || !log_dir) return UniboCGR_ErrorInvalidArgument;

    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;

    CHECK_EQUAL_SESSION(UniboCGR_Session_feature);

    int retval = LogSAP_enable(uniboCgrSap, log_dir);
    if (retval == 0) {
        uniboCgrSap->feature_logger = true;
        writeLog(uniboCgrSap, "Unibo-CGR Version %d.%d.%d.",
                 UNIBO_CGR_VERSION_MAJOR,
                 UNIBO_CGR_VERSION_MINOR,
                 UNIBO_CGR_VERSION_PATCH);
        writeLog(uniboCgrSap, "Local node number: %llu.", UniboCGRSAP_get_local_node(uniboCgrSap));
        writeLog(uniboCgrSap, "Reference time (Unix time): %" PRId64" s.", (int64_t) uniboCgrSap->reference_time);
        return UniboCGR_NoError;
    } else if (retval == -2) {
        return UniboCgr_ErrorSystem;
    } else if (retval == -3) {
        return UniboCGR_ErrorCannotOpenLogDirectory;
    } else if (retval == -4) {
        return UniboCGR_ErrorCannotOpenLogFile;
    }

    return UniboCGR_ErrorUnknown;
}
UniboCGR_Error UniboCGR_feature_logger_disable(UniboCGR uniboCgr) {
    if (!uniboCgr) return UniboCGR_ErrorInvalidArgument;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_feature);
    if (uniboCgrSap->feature_logger) {
        LogSAP_disable(uniboCgrSap);
        uniboCgrSap->feature_logger = false;
    }
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_feature_OneRoutePerNeighbor_enable(UniboCGR uniboCgr, uint32_t limit) {
    if (!uniboCgr) return false;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_feature);
    if (limit == 1){
        writeLog(uniboCgrSap, "One route per neighbor disabled.");
        uniboCgrSap->feature_one_route_per_neighbor = false;
        uniboCgrSap->feature_one_route_per_neighbor_limit = 1;
    } else if (!uniboCgrSap->feature_one_route_per_neighbor) {
        if (limit == 0) {
            writeLog(uniboCgrSap, "One route per neighbor enabled (without limits).");
        } else {
            writeLog(uniboCgrSap, "One route per neighbor enabled (at most %" PRIu32 " neighbors).", limit);
        }
        uniboCgrSap->feature_one_route_per_neighbor = true;
        uniboCgrSap->feature_one_route_per_neighbor_limit = limit;
        return UniboCGR_force_update(uniboCgr);
    }
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_feature_OneRoutePerNeighbor_disable(UniboCGR uniboCgr) {
    if (!uniboCgr) return false;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_feature);
    if (uniboCgrSap->feature_one_route_per_neighbor) {
        writeLog(uniboCgrSap, "One route per neighbor disabled.");
        uniboCgrSap->feature_one_route_per_neighbor = false;
        uniboCgrSap->feature_one_route_per_neighbor_limit = 1;
        return UniboCGR_force_update(uniboCgr);
    }
    return UniboCGR_NoError;
}

UniboCGR_Error UniboCGR_feature_QueueDelay_enable(UniboCGR uniboCgr) {
    if (!uniboCgr) return false;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_feature);
    if (!uniboCgrSap->feature_queue_delay) {
        uniboCgrSap->feature_queue_delay = true;
        writeLog(uniboCgrSap, "Queue delay enabled - ETO on all hops.");
        return UniboCGR_force_update(uniboCgr);
    }
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_feature_QueueDelay_disable(UniboCGR uniboCgr) {
    if (!uniboCgr) return false;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_feature);
    if (uniboCgrSap->feature_queue_delay) {
        uniboCgrSap->feature_queue_delay = false;
        writeLog(uniboCgrSap, "Queue delay disabled - ETO only on the first hop.");
        return UniboCGR_force_update(uniboCgr);
    }
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_feature_ModerateSourceRouting_enable(UniboCGR uniboCgr) {
    if (!uniboCgr) return false;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_feature);
    if (!uniboCgrSap->feature_moderate_source_routing) {
        uniboCgrSap->feature_moderate_source_routing = true;
        writeLog(uniboCgrSap, "Moderate Source Routing enabled.");
        return UniboCGR_force_update(uniboCgr);
    }
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_feature_ModerateSourceRouting_disable(UniboCGR uniboCgr) {
    if (!uniboCgr) return false;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_feature);
    if (uniboCgrSap->feature_moderate_source_routing) {
        uniboCgrSap->feature_moderate_source_routing = false;
        writeLog(uniboCgrSap, "Moderate Source Routing disabled.");
        return UniboCGR_force_update(uniboCgr);
    }
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_feature_ReactiveAntiLoop_enable(UniboCGR uniboCgr) {
    if (!uniboCgr) return false;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_feature);
    if (!uniboCgrSap->feature_reactive_anti_loop) {
        uniboCgrSap->feature_reactive_anti_loop = true;
        writeLog(uniboCgrSap, "Reactive anti-loop mechanism enabled.");
        return UniboCGR_force_update(uniboCgr);
    }
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_feature_ReactiveAntiLoop_disable(UniboCGR uniboCgr) {
    if (!uniboCgr) return false;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_feature);
    if (uniboCgrSap->feature_reactive_anti_loop) {
        uniboCgrSap->feature_reactive_anti_loop = false;
        writeLog(uniboCgrSap, "Reactive anti-loop mechanism disabled.");
        return UniboCGR_force_update(uniboCgr);
    }
    return UniboCGR_NoError;
}

UniboCGR_Error UniboCGR_feature_ProactiveAntiLoop_enable(UniboCGR uniboCgr) {
    if (!uniboCgr) return false;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_feature);
    if (!uniboCgrSap->feature_proactive_anti_loop) {
        uniboCgrSap->feature_proactive_anti_loop = true;
        writeLog(uniboCgrSap, "Proactive anti-loop mechanism enabled.");
        return UniboCGR_force_update(uniboCgr);
    }
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_feature_ProactiveAntiLoop_disable(UniboCGR uniboCgr) {
    if (!uniboCgr) return false;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_feature);
    if (uniboCgrSap->feature_proactive_anti_loop) {
        uniboCgrSap->feature_proactive_anti_loop = false;
        writeLog(uniboCgrSap, "Proactive anti-loop mechanism disabled.");
        return UniboCGR_force_update(uniboCgr);
    }
    return UniboCGR_NoError;
}
bool UniboCGR_feature_logger_check(UniboCGR uniboCgr) {
    if (!uniboCgr) return false;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    return uniboCgrSap->feature_logger;
}
bool UniboCGR_feature_OneRoutePerNeighbor_check(UniboCGR uniboCgr, uint64_t* limit) {
    if (!uniboCgr) return false;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    if (uniboCgrSap->feature_one_route_per_neighbor) {
        if (limit) {
            *limit = uniboCgrSap->feature_one_route_per_neighbor_limit;
        }
        return true;
    }
    return false;
}
bool UniboCGR_feature_QueueDelay_check(UniboCGR uniboCgr) {
    if (!uniboCgr) return false;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    return uniboCgrSap->feature_queue_delay;
}

bool UniboCGR_feature_ModerateSourceRouting_check(UniboCGR uniboCgr) {
    if (!uniboCgr) return false;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    return uniboCgrSap->feature_moderate_source_routing;
}
bool UniboCGR_feature_ReactiveAntiLoop_check(UniboCGR uniboCgr) {
    if (!uniboCgr) return false;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    return uniboCgrSap->feature_reactive_anti_loop;
}
bool UniboCGR_feature_ProactiveAntiLoop_check(UniboCGR uniboCgr) {
    if (!uniboCgr) return false;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    return uniboCgrSap->feature_proactive_anti_loop;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                   UNIBO-CGR SESSION CONTACT PLAN                    *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

UniboCGR_Error UniboCGR_contact_plan_open(UniboCGR uniboCgr, time_t time) {
    if (!uniboCgr) return UniboCGR_ErrorInvalidArgument;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP *) uniboCgr;
    CHECK_OPEN_SESSION;
    uniboCgrSap->session = UniboCGR_Session_contact_plan;
    UniboCGR_set_current_time(uniboCgr, time);
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_contact_plan_close(UniboCGR uniboCgr) {
    if (!uniboCgr) return UniboCGR_ErrorInvalidArgument;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP *) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_contact_plan);
    UniboCGR_force_update(uniboCgr);
    LogSAP_log_contact_plan(uniboCgrSap);
    uniboCgrSap->session = UniboCGR_NoSession;
    LogSAP_log_fflush(uniboCgrSap);
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_contact_plan_reset(UniboCGR uniboCgr) {
    if (!uniboCgr) return UniboCGR_ErrorInvalidArgument;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP *) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_contact_plan);
    NodeSAP_close(uniboCgrSap);
    ContactSAP_close(uniboCgrSap);
    RangeSAP_close(uniboCgrSap);
    if (NodeSAP_open(uniboCgrSap) < 0) {
        return UniboCgr_ErrorSystem;
    }
    if (ContactSAP_open(uniboCgrSap) < 0) {
        return UniboCgr_ErrorSystem;
    }
    if (RangeSAP_open(uniboCgrSap) < 0) {
        return UniboCgr_ErrorSystem;
    }
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_contact_plan_add_contact(UniboCGR uniboCgr,
                                                 UniboCGR_Contact uniboCgrContact,
                                                 bool copy_mtv) {
    if (!uniboCgr || !uniboCgrContact) { return UniboCGR_ErrorInvalidArgument; }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_contact_plan);

    Contact* contact = (Contact *) uniboCgrContact;

    if (contact->toTime <= uniboCgrSap->current_time) {
        return UniboCGR_NoError;
    }

    int retval = add_contact_to_graph(uniboCgrSap,
                                      contact->fromNode,
                                      contact->toNode,
                                      contact->fromTime,
                                      contact->toTime,
                                      contact->xmitRate,
                                      contact->confidence,
                                      copy_mtv,
                                      contact->mtv);

    if (retval >= 1) {
        uniboCgrSap->mustClearRoutingObjects = true;
        return UniboCGR_NoError;
    } else if (retval == 0) {
        return UniboCGR_ErrorInvalidArgument;
    } else if (retval == -1) {
        return UniboCGR_ErrorFoundOverlappingContact;
    } else if (retval == -2) {
        return UniboCgr_ErrorSystem;
    }

    return UniboCGR_ErrorUnknown;
}
UniboCGR_Error UniboCGR_contact_plan_change_contact_start_time(UniboCGR uniboCgr,
                                                               UniboCGR_ContactType contact_type,
                                                               uint64_t sender,
                                                               uint64_t receiver,
                                                               time_t start_time,
                                                               time_t new_start_time) {
    (void) contact_type; // actually unused
    if (!uniboCgr) { return UniboCGR_ErrorInvalidArgument; }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_contact_plan);

    start_time -= uniboCgrSap->reference_time;
    new_start_time -= uniboCgrSap->reference_time;

    int temp = revise_contact_start_time(uniboCgrSap, sender, receiver, start_time, new_start_time);

    if (temp == 0) {
        uniboCgrSap->mustClearRoutingObjects = true;
        return UniboCGR_NoError;
    } else if (temp == -1) {
        return UniboCGR_ErrorContactNotFound;
    } else if (temp == -2) {
        return UniboCGR_ErrorInvalidArgument;
    } else if (temp == -3) {
        return UniboCGR_ErrorFoundOverlappingContact;
    }

    return UniboCGR_ErrorUnknown;
}

UniboCGR_Error UniboCGR_contact_plan_change_contact_end_time(UniboCGR uniboCgr,
                                                             UniboCGR_ContactType contact_type,
                                                             uint64_t sender,
                                                             uint64_t receiver,
                                                             time_t start_time,
                                                             time_t new_end_time) {
    (void) contact_type; // actually unused
    if (!uniboCgr) { return UniboCGR_ErrorInvalidArgument; }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_contact_plan);

    start_time -= uniboCgrSap->reference_time;
    new_end_time -= uniboCgrSap->reference_time;

    if (new_end_time <= uniboCgrSap->current_time) {
        UniboCGR_log_write(uniboCgr, "remove end time");
        remove_contact_from_graph(uniboCgrSap,
                                  start_time,
                                  sender,
                                  receiver);
        return UniboCGR_NoError;
    }

    UniboCGR_log_write(uniboCgr, "revise end time");
    int temp = revise_contact_end_time(uniboCgrSap, sender, receiver, start_time, new_end_time);

    if (temp == 0) {
        uniboCgrSap->mustClearRoutingObjects = true;
        return UniboCGR_NoError;
    } else if (temp == -1) {
        return UniboCGR_ErrorContactNotFound;
    } else if (temp == -2) {
        return UniboCGR_ErrorInvalidArgument;
    } else if (temp == -3) {
        return UniboCGR_ErrorFoundOverlappingContact;
    }

    return UniboCGR_ErrorUnknown;
}

UniboCGR_Error UniboCGR_contact_plan_change_contact_type(UniboCGR uniboCgr,
                                                         UniboCGR_ContactType contact_type,
                                                         uint64_t sender,
                                                         uint64_t receiver,
                                                         time_t start_time,
                                                         UniboCGR_ContactType new_type) {
    if (!uniboCgr) { return UniboCGR_ErrorInvalidArgument; }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_contact_plan);

    start_time -= uniboCgrSap->reference_time;
    (void) contact_type;
    (void) sender;
    (void) receiver;
    (void) start_time;
    (void) new_type;

    // TODO actually only scheduled contacts are supported

    if (new_type != UniboCGR_ContactType_Scheduled) {
        return UniboCGR_ErrorUnknown;
    } else {
        return UniboCGR_NoError;
    }

    return UniboCGR_ErrorUnknown;
}
UniboCGR_Error UniboCGR_contact_plan_change_contact_confidence(UniboCGR uniboCgr,
                                                               UniboCGR_ContactType contact_type,
                                                               uint64_t sender,
                                                               uint64_t receiver,
                                                               time_t start_time,
                                                               float new_confidence) {
    (void) contact_type; // actually unused
    if (!uniboCgr) { return UniboCGR_ErrorInvalidArgument; }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_contact_plan);

    start_time -= uniboCgrSap->reference_time;

    int retval = revise_confidence(uniboCgrSap,
                                   sender,
                                   receiver,
                                   start_time,
                                   new_confidence);

    if (retval == 0) {
        uniboCgrSap->mustClearRoutingObjects = true;
        return UniboCGR_NoError;
    } else if (retval == -1) {
        return UniboCGR_ErrorContactNotFound;
    } else if (retval == -2) {
        return UniboCgr_ErrorSystem;
    }

    return UniboCGR_ErrorUnknown;
}

UniboCGR_Error UniboCGR_contact_plan_change_contact_xmit_rate(UniboCGR uniboCgr,
                                                              UniboCGR_ContactType contact_type,
                                                              uint64_t sender,
                                                              uint64_t receiver,
                                                              time_t start_time,
                                                              uint64_t new_xmit_rate) {
    if (!uniboCgr) { return UniboCGR_ErrorInvalidArgument; }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_contact_plan);

    start_time -= uniboCgrSap->reference_time;
    // TODO QUA MANCA IL TIPO DEL CONTATTO
    (void) contact_type;
    int retval = revise_xmit_rate(uniboCgrSap,
                                  sender,
                                  receiver,
                                  start_time,
                                  new_xmit_rate);

    if (retval == 0) {
        uniboCgrSap->mustClearRoutingObjects = true;
        return UniboCGR_NoError;
    } else if (retval == -1) {
        return UniboCGR_ErrorContactNotFound;
    } else if (retval == -2) {
        return UniboCgr_ErrorSystem;
    }

    return UniboCGR_ErrorUnknown;
}
UniboCGR_Error UniboCGR_contact_plan_remove_contact(UniboCGR uniboCgr,
                                                    UniboCGR_ContactType contact_type,
                                                    uint64_t sender,
                                                    uint64_t receiver,
                                                    time_t start_time) {
    (void) contact_type; // actually unused
    if (!uniboCgr) { return UniboCGR_ErrorInvalidArgument; }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_contact_plan);

    UniboCGR_log_write(uniboCgr, "Before contact remove 2");

    start_time -= uniboCgrSap->reference_time;

    remove_contact_from_graph(uniboCgrSap,
                              start_time,
                              sender,
                              receiver);

    uniboCgrSap->mustClearRoutingObjects = true;
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_contact_plan_add_range(UniboCGR uniboCgr,
                                               UniboCGR_Range uniboCgrRange) {
    if (!uniboCgr || !uniboCgrRange) { return UniboCGR_ErrorInvalidArgument; }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_contact_plan);
    Range* range = (Range *) uniboCgrRange;

    if (range->toTime <= uniboCgrSap->current_time) {
        return UniboCGR_NoError;
    }

    int retval = add_range_to_graph(uniboCgrSap,
                                    range->fromNode,
                                    range->toNode,
                                    range->fromTime,
                                    range->toTime,
                                    range->owlt);

    if (retval == 1 || retval == 2) {
        uniboCgrSap->mustClearRoutingObjects = true;
        return UniboCGR_NoError;
    } else if (retval == 0) {
        return UniboCGR_ErrorInvalidArgument;
    } else if (retval == -1) {
        return UniboCGR_ErrorFoundOverlappingRange;
    } else if (retval == -2) {
        return UniboCgr_ErrorSystem;
    }

    return UniboCGR_ErrorUnknown;
}
UniboCGR_Error UniboCGR_contact_plan_change_range_start_time(UniboCGR uniboCgr,
                                                             uint64_t sender,
                                                             uint64_t receiver,
                                                             time_t start_time,
                                                             time_t new_start_time) {
    if (!uniboCgr) { return UniboCGR_ErrorInvalidArgument; }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_contact_plan);

    start_time -= uniboCgrSap->reference_time;
    new_start_time -= uniboCgrSap->reference_time;

    int temp = revise_range_start_time(uniboCgrSap, sender, receiver, start_time, new_start_time);

    if (temp == 0) {
        uniboCgrSap->mustClearRoutingObjects = true;
        return UniboCGR_NoError;
    } else if (temp == -1) {
        return UniboCGR_ErrorRangeNotFound;
    } else if (temp == -2) {
        return UniboCGR_ErrorInvalidArgument;
    } else if (temp == -3) {
        return UniboCGR_ErrorFoundOverlappingRange;
    }

    return UniboCGR_ErrorUnknown;
}

UniboCGR_Error UniboCGR_contact_plan_change_range_end_time(UniboCGR uniboCgr,
                                                           uint64_t sender,
                                                           uint64_t receiver,
                                                           time_t start_time,
                                                           time_t new_end_time) {
    if (!uniboCgr) { return UniboCGR_ErrorInvalidArgument; }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_contact_plan);

    start_time -= uniboCgrSap->reference_time;
    new_end_time -= uniboCgrSap->reference_time;

    if (new_end_time <= uniboCgrSap->current_time) {
        remove_range_from_graph(uniboCgrSap,
                                start_time,
                                sender,
                                receiver);
        return UniboCGR_NoError;
    }

    int temp = revise_range_end_time(uniboCgrSap, sender, receiver, start_time, new_end_time);

    if (temp == 0) {
        uniboCgrSap->mustClearRoutingObjects = true;
        return UniboCGR_NoError;
    } else if (temp == -1) {
        return UniboCGR_ErrorRangeNotFound;
    } else if (temp == -2) {
        return UniboCGR_ErrorInvalidArgument;
    } else if (temp == -3) {
        return UniboCGR_ErrorFoundOverlappingRange;
    }

    return UniboCGR_ErrorUnknown;
}
UniboCGR_Error UniboCGR_contact_plan_change_range_one_way_light_time(UniboCGR uniboCgr,
                                                                     uint64_t sender,
                                                                     uint64_t receiver,
                                                                     time_t start_time,
                                                                     uint64_t new_owlt) {
    if (!uniboCgr) { return UniboCGR_ErrorInvalidArgument; }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_contact_plan);

    start_time -= uniboCgrSap->reference_time;

    int retval = revise_owlt(uniboCgrSap,
                             sender,
                             receiver,
                             start_time,
                             new_owlt);

    if (retval == 0) {
        uniboCgrSap->mustClearRoutingObjects = true;
        return UniboCGR_NoError;
    } else if (retval == -1) {
        return UniboCGR_ErrorRangeNotFound;
    } else if (retval == -2) {
        return UniboCgr_ErrorSystem;
    }

    return UniboCGR_ErrorUnknown;
}
UniboCGR_Error UniboCGR_contact_plan_remove_range(UniboCGR uniboCgr,
                                                  uint64_t sender,
                                                  uint64_t receiver,
                                                  time_t start_time) {
    if (!uniboCgr) { return UniboCGR_ErrorInvalidArgument; }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_contact_plan);

    start_time -= uniboCgrSap->reference_time;

    remove_range_from_graph(uniboCgrSap,
                            start_time,
                            sender,
                            receiver);

    uniboCgrSap->mustClearRoutingObjects = true;
    return UniboCGR_NoError;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                     UNIBO-CGR SESSION ROUTING                       *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

UniboCGR_Error UniboCGR_routing_open(UniboCGR uniboCgr, time_t time) {
    if (!uniboCgr) return UniboCGR_ErrorInvalidArgument;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP *) uniboCgr;
    CHECK_OPEN_SESSION;
    uniboCgrSap->session = UniboCGR_Session_routing;
    UniboCGR_set_current_time(uniboCgr, time);

    start_call_log(uniboCgrSap, uniboCgrSap->count_bundles);
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_routing_close(UniboCGR uniboCgr) {
    if (!uniboCgr) return UniboCGR_ErrorInvalidArgument;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP *) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_routing);
    uniboCgrSap->session = UniboCGR_NoSession;
    end_call_log(uniboCgrSap);
    LogSAP_log_fflush(uniboCgrSap);
    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_routing(UniboCGR uniboCgr,
                                UniboCGR_Bundle uniboCgrBundle,
                                UniboCGR_excluded_neighbors_list excluded_neighbors_list,
                                UniboCGR_route_list* route_list) {
    if (!uniboCgr || !uniboCgrBundle || !excluded_neighbors_list || !route_list) {
        return UniboCGR_ErrorInvalidArgument;
    }

    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CHECK_EQUAL_SESSION(UniboCGR_Session_routing);

    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;

    // first, set expiration time in seconds since DTN EPOCH (01.01.2000 00:00:00 UTC)
    if (bundle->bp_version == 7) {
        // creation time and lifetime are expressed in milliseconds (RFC 9171)
        // need to convert from milliseconds to seconds
        bundle->expiration_time = (time_t) ((bundle->id.creation_timestamp + bundle->lifetime) / 1000);
    } else { // version 6
        // creation time and lifetime are expressed in seconds (RFC 5050)
        bundle->expiration_time = (time_t) (bundle->id.creation_timestamp + bundle->lifetime);
    }
    // second, convert from DTN time to Unix time (01.01.1970 00:00:00 UTC)
    bundle->expiration_time += UNIBO_CGR_DTN_EPOCH;
    // finally, convert from Unix time to relative time
    bundle->expiration_time -= uniboCgrSap->reference_time;

    bundle->evc = computeBundleEVC(
            bundle->primary_block_length
            + bundle->extension_blocks_length
            + bundle->payload_block_length);

    uniboCgrSap->route_iterator = NULL;
    uniboCgrSap->hop_iterator = NULL;

    UniboCGR_log_bundle_routing_call(uniboCgr, uniboCgrBundle);
    List internal_routes = NULL;
    int retval = getBestRoutes(uniboCgrSap,
                               (CgrBundle*) uniboCgrBundle,
                               (List) excluded_neighbors_list,
                               &internal_routes);
    *route_list = (UniboCGR_route_list) internal_routes;

    if (retval >= 0) {
        return UniboCGR_NoError;
    } else if (retval == -1) {
        return UniboCGR_ErrorRouteNotFound;
    } else {
        UniboCGR_Error error = UniboCGR_ErrorUnknown;
        if (retval == -2) {
            error = UniboCgr_ErrorSystem;
        } else if (retval == -3) {
            error =  UniboCGR_ErrorInternal;
        } else if (retval == -4) {
            error =  UniboCGR_ErrorInvalidArgument;
        } else if (retval == -5) {
            error =  UniboCGR_ErrorInvalidTime;
        }

        writeLog(uniboCgrSap, "%s", UniboCGR_get_error_string(error));
        return error;
    }
}
UniboCGR_RoutingAlgorithm UniboCGR_get_used_routing_algorithm(UniboCGR uniboCgr) {
    if (!uniboCgr) return UniboCGR_RoutingAlgorithm_Unknown;
    RoutingAlgorithm algorithm = get_last_call_routing_algorithm((UniboCGRSAP*) uniboCgr);

    switch (algorithm) {
        case unknown_algorithm: return UniboCGR_RoutingAlgorithm_Unknown;
        case cgr: return UniboCGR_RoutingAlgorithm_CGR;
        case msr: return UniboCGR_RoutingAlgorithm_MSR;
    }
    return UniboCGR_RoutingAlgorithm_Unknown;
}

UniboCGR_Error UniboCGR_Contact_create(UniboCGR_Contact* uniboCgrContact) {
    if (!uniboCgrContact) {
        return UniboCGR_ErrorInvalidArgument;
    }
    // note: this is an API contact
    // -- we do not need to initialize all the internal routing stuff
    Contact* contact = MWITHDRAW(sizeof(Contact));
    if (!contact) {
        return UniboCgr_ErrorSystem;
    }
    *uniboCgrContact = (UniboCGR_Contact) contact;
    UniboCGR_Contact_reset(*uniboCgrContact);
    return UniboCGR_NoError;
}
void UniboCGR_Contact_destroy(UniboCGR_Contact* uniboCgrContact) {
    if (!uniboCgrContact) {
        return;
    }
    Contact* contact = (Contact*) *uniboCgrContact;
    MDEPOSIT(contact);
    
    *uniboCgrContact = NULL;
}
void UniboCGR_Contact_reset(UniboCGR_Contact uniboCgrContact) {
    if (!uniboCgrContact) {
        return;
    }
    Contact* contact = (Contact*) uniboCgrContact;
    memset(contact, 0 , sizeof(Contact));

    /* ... default values between here ... */
    contact->type = TypeScheduled;
    /* ... and here ... */
}

void UniboCGR_Contact_set_sender(UniboCGR_Contact uniboCgrContact,
                                 uint64_t sender) {
    if (!uniboCgrContact) {
        return;
    }
    Contact* contact = (Contact*) uniboCgrContact;

    contact->fromNode = sender;
}
void UniboCGR_Contact_set_receiver(UniboCGR_Contact uniboCgrContact,
                                   uint64_t receiver) {
    if (!uniboCgrContact) {
        return;
    }
    Contact* contact = (Contact*) uniboCgrContact;

    contact->toNode = receiver;
}
void UniboCGR_Contact_set_start_time(UniboCGR uniboCgr,
                                     UniboCGR_Contact uniboCgrContact,
                                     time_t start_time) {
    if (!uniboCgr || !uniboCgrContact) {
        return;
    }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    Contact* contact = (Contact*) uniboCgrContact;

    contact->fromTime = start_time - uniboCgrSap->reference_time;
}
void UniboCGR_Contact_set_end_time(UniboCGR uniboCgr,
                                   UniboCGR_Contact uniboCgrContact,
                                   time_t end_time) {
    if (!uniboCgr || !uniboCgrContact) {
        return;
    }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    Contact* contact = (Contact*) uniboCgrContact;

    contact->toTime = end_time - uniboCgrSap->reference_time;
}
void UniboCGR_Contact_set_xmit_rate(UniboCGR_Contact uniboCgrContact,
                                    uint64_t xmit_rate_bytes_per_second) {
    if (!uniboCgrContact) {
        return;
    }
    Contact* contact = (Contact*) uniboCgrContact;

    contact->xmitRate = xmit_rate_bytes_per_second;
}
void UniboCGR_Contact_set_confidence(UniboCGR_Contact uniboCgrContact,
                                     float confidence) {
    if (!uniboCgrContact) {
        return;
    }
    Contact* contact = (Contact*) uniboCgrContact;

    contact->confidence = confidence;
}
void UniboCGR_Contact_set_mtv_bulk(UniboCGR_Contact uniboCgrContact,
                                   double mtv) {
    if (!uniboCgrContact) {
        return;
    }
    Contact* contact = (Contact*) uniboCgrContact;

    contact->mtv[0] = mtv;
}
void UniboCGR_Contact_set_mtv_normal(UniboCGR_Contact uniboCgrContact,
                                     double mtv) {
    if (!uniboCgrContact) {
        return;
    }
    Contact* contact = (Contact*) uniboCgrContact;

    contact->mtv[1] = mtv;
}
void UniboCGR_Contact_set_mtv_expedited(UniboCGR_Contact uniboCgrContact,
                                        double mtv) {
    if (!uniboCgrContact) {
        return;
    }
    Contact* contact = (Contact*) uniboCgrContact;

    contact->mtv[2] = mtv;
}
void UniboCGR_Contact_set_type(UniboCGR_Contact uniboCgrContact,
                               UniboCGR_ContactType type) {
    if (!uniboCgrContact) {
        return;
    }
    Contact* contact = (Contact*) uniboCgrContact;

    contact->type = UniboCGR_ContactType_to_CtType(type);
}
uint64_t UniboCGR_Contact_get_sender(UniboCGR_Contact uniboCgrContact) {
    if (!uniboCgrContact) {
        return 0;
    }
    Contact* contact = (Contact*) uniboCgrContact;

    return contact->fromNode;
}
uint64_t UniboCGR_Contact_get_receiver(UniboCGR_Contact uniboCgrContact) {
    if (!uniboCgrContact) {
        return 0;
    }
    Contact* contact = (Contact*) uniboCgrContact;

    return contact->toNode;
}
time_t UniboCGR_Contact_get_start_time(UniboCGR uniboCgr,
                                       UniboCGR_Contact uniboCgrContact) {
    if (!uniboCgrContact) {
        return 0;
    }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    Contact* contact = (Contact*) uniboCgrContact;

    return contact->fromTime + uniboCgrSap->reference_time;
}
time_t UniboCGR_Contact_get_end_time(UniboCGR uniboCgr,
                                     UniboCGR_Contact uniboCgrContact) {
    if (!uniboCgrContact) {
        return 0;
    }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    Contact* contact = (Contact*) uniboCgrContact;

    return contact->toTime + uniboCgrSap->reference_time;
}
uint64_t UniboCGR_Contact_get_xmit_rate(UniboCGR_Contact uniboCgrContact) {
    if (!uniboCgrContact) {
        return 0;
    }
    Contact* contact = (Contact*) uniboCgrContact;

    return contact->xmitRate;
}
float UniboCGR_Contact_get_confidence(UniboCGR_Contact uniboCgrContact) {
    if (!uniboCgrContact) {
        return 0;
    }
    Contact* contact = (Contact*) uniboCgrContact;

    return contact->confidence;
}
double UniboCGR_Contact_get_mtv_bulk(UniboCGR_Contact uniboCgrContact) {
    if (!uniboCgrContact) {
        return 0;
    }
    Contact* contact = (Contact*) uniboCgrContact;

    return contact->mtv[0];
}
double UniboCGR_Contact_get_mtv_normal(UniboCGR_Contact uniboCgrContact) {
    if (!uniboCgrContact) {
        return 0;
    }
    Contact* contact = (Contact*) uniboCgrContact;

    return contact->mtv[1];
}
double UniboCGR_Contact_get_mtv_expedited(UniboCGR_Contact uniboCgrContact) {
    if (!uniboCgrContact) {
        return 0;
    }
    Contact* contact = (Contact*) uniboCgrContact;

    return contact->mtv[2];
}
UniboCGR_ContactType UniboCGR_Contact_get_type(UniboCGR_Contact uniboCgrContact) {
    if (!uniboCgrContact) {
        return 0;
    }
    Contact* contact = (Contact*) uniboCgrContact;

    return CtType_to_UniboCGR_ContactType(contact->type);
}

UniboCGR_Error UniboCGR_Range_create(UniboCGR_Range* uniboCgrRange) {
    if (!uniboCgrRange) {
        return UniboCGR_ErrorInvalidArgument;
    }
    Range* range = MWITHDRAW(sizeof(Range));
    if (!range) {
        return UniboCgr_ErrorSystem;
    }
    *uniboCgrRange = (UniboCGR_Range) range;
    UniboCGR_Range_reset(*uniboCgrRange);
    return UniboCGR_NoError;
}
void UniboCGR_Range_destroy(UniboCGR_Range* uniboCgrRange) {
    if (!uniboCgrRange) {
        return;
    }
    Range* range = (Range*) *uniboCgrRange;
    MDEPOSIT(range);

    *uniboCgrRange = NULL;
}
void UniboCGR_Range_reset(UniboCGR_Range uniboCgrRange) {
    if (!uniboCgrRange) {
        return;
    }
    Range* range = (Range*) uniboCgrRange;
    memset(range, 0, sizeof(Range));
}
void UniboCGR_Range_set_sender(UniboCGR_Range uniboCgrRange,
                               uint64_t sender) {
    if (!uniboCgrRange) {
        return;
    }
    Range* range = (Range*) uniboCgrRange;

    range->fromNode = sender;
}
void UniboCGR_Range_set_receiver(UniboCGR_Range uniboCgrRange,
                                 uint64_t receiver) {
    if (!uniboCgrRange) {
        return;
    }
    Range* range = (Range*) uniboCgrRange;

    range->toNode = receiver;
}
void UniboCGR_Range_set_start_time(UniboCGR uniboCgr,
                                   UniboCGR_Range uniboCgrRange,
                                   time_t start_time) {
    if (!uniboCgrRange) {
        return;
    }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    Range* range = (Range*) uniboCgrRange;

    range->fromTime = start_time - uniboCgrSap->reference_time;
}
void UniboCGR_Range_set_end_time(UniboCGR uniboCgr,
                                 UniboCGR_Range uniboCgrRange,
                                 time_t end_time) {
    if (!uniboCgrRange) {
        return;
    }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    Range* range = (Range*) uniboCgrRange;

    range->toTime = end_time - uniboCgrSap->reference_time;
}
void UniboCGR_Range_set_one_way_light_time(UniboCGR_Range uniboCgrRange,
                                           uint64_t one_way_light_time_seconds) {
    if (!uniboCgrRange) {
        return;
    }
    Range* range = (Range*) uniboCgrRange;

    range->owlt = one_way_light_time_seconds;
}
uint64_t UniboCGR_Range_get_sender(UniboCGR_Range uniboCgrRange) {
    if (!uniboCgrRange) {
        return 0;
    }
    Range* range = (Range*) uniboCgrRange;

    return range->fromNode;
}
uint64_t UniboCGR_Range_get_receiver(UniboCGR_Range uniboCgrRange) {
    if (!uniboCgrRange) {
        return 0;
    }
    Range* range = (Range*) uniboCgrRange;

    return range->toNode;
}
time_t UniboCGR_Range_get_start_time(UniboCGR uniboCgr,
                                     UniboCGR_Range uniboCgrRange) {
    if (!uniboCgrRange) {
        return 0;
    }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    Range* range = (Range*) uniboCgrRange;

    return range->fromTime + uniboCgrSap->reference_time;
}
time_t UniboCGR_Range_get_end_time(UniboCGR uniboCgr,
                                   UniboCGR_Range uniboCgrRange) {
    if (!uniboCgrRange) {
        return 0;
    }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    Range* range = (Range*) uniboCgrRange;

    return range->toTime + uniboCgrSap->reference_time;
}
uint64_t UniboCGR_Range_get_one_way_light_time(UniboCGR_Range uniboCgrRange) {
    if (!uniboCgrRange) {
        return 0;
    }
    Range* range = (Range*) uniboCgrRange;

    return range->owlt;
}
UniboCGR_Error UniboCGR_Bundle_create(UniboCGR_Bundle* uniboCgrBundle) {
    if (!uniboCgrBundle) {
        return UniboCGR_ErrorInvalidArgument;
    }
    CgrBundle * bundle = bundle_create();
    if (!bundle) {
        return UniboCgr_ErrorSystem;
    }
    *uniboCgrBundle = (UniboCGR_Bundle) bundle;
    UniboCGR_Bundle_reset(*uniboCgrBundle);
    return UniboCGR_NoError;
}
void UniboCGR_Bundle_destroy(UniboCGR_Bundle* uniboCgrBundle) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) *uniboCgrBundle;
    bundle_destroy(bundle);

    *uniboCgrBundle = NULL;
}
void UniboCGR_Bundle_reset(UniboCGR_Bundle uniboCgrBundle) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    reset_bundle(bundle);
}
uint64_t UniboCGR_Bundle_get_estimated_volume_consumption(UniboCGR_Bundle uniboCgrBundle) {
    if (!uniboCgrBundle) {
        return 0;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;

    return bundle->evc;
}
void UniboCGR_Bundle_set_delivery_confidence(UniboCGR_Bundle uniboCgrBundle, float confidence) {
    if (!uniboCgrBundle) return;
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    bundle->dlvConfidence = confidence;
}
void UniboCGR_Bundle_set_bundle_protocol_version(UniboCGR_Bundle uniboCgrBundle, uint64_t version) {
    if (!uniboCgrBundle) return;
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    bundle->bp_version = version;
}
void UniboCGR_Bundle_set_source_node_id(UniboCGR_Bundle uniboCgrBundle,
                                        const char* source_node_id) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    strncpy(bundle->id.source_node, source_node_id, sizeof(bundle->id.source_node) - 1);
    bundle->id.source_node[sizeof(bundle->id.source_node) - 1] = '\0';
}
void UniboCGR_Bundle_set_destination_node_id(UniboCGR_Bundle uniboCgrBundle,
                                             uint64_t destination_node_id) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    bundle->terminus_node = destination_node_id;
}
void UniboCGR_Bundle_set_previous_node_id(UniboCGR_Bundle uniboCgrBundle, uint64_t previous_node_id) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    bundle->sender_node = previous_node_id;
}
void UniboCGR_Bundle_set_primary_block_length(UniboCGR_Bundle uniboCgrBundle, uint64_t size) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    bundle->primary_block_length = size;
}
void UniboCGR_Bundle_set_total_ext_block_length(UniboCGR_Bundle uniboCgrBundle, uint64_t size) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    bundle->extension_blocks_length = size;
}
void UniboCGR_Bundle_set_payload_length(UniboCGR_Bundle uniboCgrBundle, uint64_t size) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    bundle->payload_block_length = size;
}
void UniboCGR_Bundle_set_total_application_data_unit_length(UniboCGR_Bundle uniboCgrBundle, uint64_t length) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    bundle->total_adu_length = length;
}
void UniboCGR_Bundle_set_creation_time(UniboCGR_Bundle uniboCgrBundle,
                                       uint64_t creation_time) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    bundle->id.creation_timestamp = creation_time;
}
void UniboCGR_Bundle_set_sequence_number(UniboCGR_Bundle uniboCgrBundle,
                                         uint64_t sequence_number) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    bundle->id.sequence_number = sequence_number;
}
void UniboCGR_Bundle_set_fragment_offset(UniboCGR_Bundle uniboCgrBundle,
                                         uint64_t fragment_offset) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    bundle->id.fragment_offset = fragment_offset;
}
void UniboCGR_Bundle_set_fragment_length(UniboCGR_Bundle uniboCgrBundle,
                                         uint64_t fragment_length) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    bundle->id.fragment_length = fragment_length;
}
void UniboCGR_Bundle_set_lifetime(UniboCGR_Bundle uniboCgrBundle, uint64_t lifetime) {
    if (!uniboCgrBundle) return;
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    bundle->lifetime = lifetime;
}
void UniboCGR_Bundle_set_priority_bulk(UniboCGR_Bundle uniboCgrBundle) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    bundle->priority_level = Bulk;
    bundle->ordinal = 0;
}
void UniboCGR_Bundle_set_priority_normal(UniboCGR_Bundle uniboCgrBundle) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    bundle->priority_level = Normal;
    bundle->ordinal = 0;
}
void UniboCGR_Bundle_set_priority_expedited(UniboCGR_Bundle uniboCgrBundle,
                                            uint8_t ordinal) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    bundle->priority_level = Expedited;
    bundle->ordinal = ordinal;
}
void UniboCGR_Bundle_set_flag_probe(UniboCGR_Bundle uniboCgrBundle, bool value) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    if (value) {
        SET_PROBE(bundle);
    } else {
        UNSET_PROBE(bundle);
    }
}
void UniboCGR_Bundle_set_flag_do_not_fragment(UniboCGR_Bundle uniboCgrBundle, bool value) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    if (value) {
        UNSET_FRAGMENTABLE(bundle);
    } else {
        SET_FRAGMENTABLE(bundle);
    }
}
void UniboCGR_Bundle_set_flag_critical(UniboCGR_Bundle uniboCgrBundle, bool value) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    if (value) {
        SET_CRITICAL(bundle);
    } else {
        UNSET_CRITICAL(bundle);
    }
}
void UniboCGR_Bundle_set_flag_backward_propagation(UniboCGR_Bundle uniboCgrBundle, bool value) {
    if (!uniboCgrBundle) {
        return;
    }
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    if (value) {
        SET_BACKWARD_PROPAGATION(bundle);
    } else {
        UNSET_BACKWARD_PROPAGATION(bundle);
    }
}
UniboCGR_Error UniboCGR_Bundle_add_node_in_geographic_route_list(UniboCGR_Bundle uniboCgrBundle,
                                                                 uint64_t node_id) {
    if (!uniboCgrBundle) {
        return UniboCGR_ErrorInvalidArgument;
    }

    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    uint64_t* node_id_obj = MWITHDRAW(sizeof(uint64_t));

    if (!node_id_obj) {
        return UniboCgr_ErrorSystem;
    }
    *node_id_obj = node_id;

    list_insert_last(bundle->geoRoute, node_id_obj);

    return UniboCGR_NoError;
}
extern UniboCGR_Error  UniboCGR_add_moderate_source_routing_hop(UniboCGR uniboCgr,
                                                                UniboCGR_Bundle uniboCgrBundle,
                                                                UniboCGR_ContactType uniboCgrContactType,
                                                                uint64_t sender,
                                                                uint64_t receiver,
                                                                time_t start_time) {
    (void) uniboCgrContactType; // actually unused
    if (!uniboCgr) return UniboCGR_ErrorInvalidArgument;
    if (!uniboCgrBundle) return UniboCGR_ErrorInvalidArgument;

    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;
    uint64_t local_node = UniboCGRSAP_get_local_node(uniboCgrSap);

    uint32_t hop_number = list_get_length(bundle->msrRoute->hops);
    if (hop_number == 0 && sender != local_node) return UniboCGR_ErrorMalformedMSRRoute;
    if (hop_number > 0 && sender == local_node) return UniboCGR_ErrorMalformedMSRRoute;
    if (receiver == local_node && bundle->terminus_node != local_node) return UniboCGR_ErrorMalformedMSRRoute;

    //CtType contact_type = UniboCGR_ContactType_to_CtType(uniboCgrContactType);

    // TODO qua manca tipo contatto

    Contact* contact = get_contact(uniboCgrSap, sender, receiver, start_time, NULL);

    if (!contact) return UniboCGR_ErrorContactNotFound;

    Contact* prev_contact = listElt_get_data(list_get_last_elt(bundle->msrRoute->hops));

    if (prev_contact) {
        if (prev_contact->toNode != sender) {
            return UniboCGR_ErrorMalformedMSRRoute;
        }
        contact->routingObject->arrivalConfidence = contact->confidence;
        contact->routingObject->arrivalConfidence *= prev_contact->routingObject->arrivalConfidence;
    } else {
        contact->routingObject->arrivalConfidence = contact->confidence;
    }

    bundle->last_msr_route_contact = contact;

    return UniboCGR_NoError;
}
UniboCGR_Error  UniboCGR_finalize_moderate_source_routing_route(UniboCGR uniboCgr,
                                                                UniboCGR_Bundle uniboCgrBundle,
                                                                uint32_t hint_hop_lower_bound) {
    if (!uniboCgr) return UniboCGR_ErrorInvalidArgument;
    if (!uniboCgrBundle) return UniboCGR_ErrorInvalidArgument;

    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    CgrBundle* bundle = (CgrBundle*) uniboCgrBundle;

    if (!bundle->last_msr_route_contact) return UniboCGR_ErrorMalformedMSRRoute;

    if (populate_msr_route(uniboCgrSap->current_time, bundle->last_msr_route_contact, bundle->msrRoute) < 0) {
        return UniboCgr_ErrorSystem;
    }

    if (hint_hop_lower_bound > 0 && bundle->msrRoute->hops->length < hint_hop_lower_bound) {
        return UniboCGR_ErrorMalformedMSRRoute;
    }

    return UniboCGR_NoError;
}
uint64_t UniboCGR_Route_get_neighbor(UniboCGR_Route uniboCgrRoute) {
    if (!uniboCgrRoute) {
        return 0;
    }
    Route* route = (Route*) uniboCgrRoute;

    return route->neighbor;
}
float UniboCGR_Route_get_arrival_confidence(UniboCGR_Route uniboCgrRoute) {
    if (!uniboCgrRoute) {
        return 0;
    }
    Route* route = (Route*) uniboCgrRoute;

    return route->arrivalConfidence;
}
time_t UniboCGR_Route_get_best_case_arrival_time(UniboCGR uniboCgr,
                                                 UniboCGR_Route uniboCgrRoute) {
    if (!uniboCgr || !uniboCgrRoute) {
        return 0;
    }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    Route* route = (Route*) uniboCgrRoute;

    return route->arrivalTime + uniboCgrSap->reference_time;
}
time_t   UniboCGR_Route_get_eto(UniboCGR uniboCgr, UniboCGR_Route uniboCgrRoute) {
    if (!uniboCgr || !uniboCgrRoute) {
        return 0;
    }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    Route* route = (Route*) uniboCgrRoute;

    return route->eto + uniboCgrSap->reference_time;
}
double UniboCGR_Route_get_route_volume_limit(UniboCGR_Route uniboCgrRoute) {
    if (!uniboCgrRoute) {
        return 0;
    }
    Route* route = (Route*) uniboCgrRoute;

    return route->routeVolumeLimit;
}
time_t UniboCGR_Route_get_projected_bundle_arrival_time(UniboCGR uniboCgr,
                                                        UniboCGR_Route uniboCgrRoute) {
    if (!uniboCgr || !uniboCgrRoute) {
        return 0;
    }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    Route* route = (Route*) uniboCgrRoute;

    return route->pbat + uniboCgrSap->reference_time;
}
time_t UniboCGR_Route_get_best_case_transmission_time(UniboCGR uniboCgr,
                                                      UniboCGR_Route uniboCgrRoute) {
    if (!uniboCgr || !uniboCgrRoute) {
        return 0;
    }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    Route* route = (Route*) uniboCgrRoute;

    return route->fromTime + uniboCgrSap->reference_time;
}
time_t UniboCGR_Route_get_expiration_time(UniboCGR uniboCgr,
                                          UniboCGR_Route uniboCgrRoute) {
    if (!uniboCgr || !uniboCgrRoute) {
        return 0;
    }
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    Route* route = (Route*) uniboCgrRoute;

    return route->toTime + uniboCgrSap->reference_time;
}
uint64_t UniboCGR_Route_get_total_one_way_light_time(UniboCGR_Route uniboCgrRoute) {
    if (!uniboCgrRoute) {
        return 0;
    }
    Route* route = (Route*) uniboCgrRoute;

    return route->owltSum;
}
void UniboCGR_Route_get_overbooking_management(UniboCGR_Route uniboCgrRoute,
                                               uint64_t* overbooked, uint64_t* committed) {
    if (!uniboCgrRoute) {
        return;
    }
    Route* route = (Route*) uniboCgrRoute;

    if (overbooked) {
        *overbooked = (uint64_t) (route->overbooked.gigs * ONE_GIG + route->overbooked.units);
    }
    if (committed) {
        *committed = (uint64_t) (route->committed.gigs * ONE_GIG + route->committed.units);
    }
}
UniboCGR_Error UniboCGR_get_first_hop(UniboCGR uniboCgr, UniboCGR_Route uniboCgrRoute, UniboCGR_Contact* uniboCgrContact) {
    if (!uniboCgr || !uniboCgrRoute || !uniboCgrContact) return UniboCGR_ErrorInvalidArgument;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    Route* route = (Route*) uniboCgrRoute;

    uniboCgrSap->hop_iterator = list_get_first_elt(route->hops);
    *uniboCgrContact = listElt_get_data(uniboCgrSap->hop_iterator);
    if (!*uniboCgrContact) return UniboCGR_ErrorContactNotFound;

    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_get_next_hop(UniboCGR uniboCgr, UniboCGR_Contact* uniboCgrContact) {
    if (!uniboCgr || !uniboCgrContact) return UniboCGR_ErrorInvalidArgument;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;

    uniboCgrSap->hop_iterator = list_get_next_elt(uniboCgrSap->hop_iterator);
    *uniboCgrContact = listElt_get_data(uniboCgrSap->hop_iterator);
    if (!*uniboCgrContact) return UniboCGR_ErrorContactNotFound;

    return UniboCGR_NoError;
}
uint32_t UniboCGR_route_list_get_length(UniboCGR_route_list list) {
    if (!list) {
        return 0;
    }
    return ((List) list)->length;
}
UniboCGR_Error UniboCGR_get_first_route(UniboCGR uniboCgr, UniboCGR_route_list uniboCgrRouteList, UniboCGR_Route* uniboCgrRoute) {
    if (!uniboCgr || !uniboCgrRoute) return UniboCGR_ErrorInvalidArgument;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;

    uniboCgrSap->route_iterator = list_get_first_elt((List) uniboCgrRouteList);
    *uniboCgrRoute = listElt_get_data(uniboCgrSap->route_iterator);
    if (!*uniboCgrRoute) return UniboCGR_ErrorRouteNotFound;

    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_get_next_route(UniboCGR uniboCgr, UniboCGR_Route* uniboCgrRoute) {
    if (!uniboCgr || !uniboCgrRoute) return UniboCGR_ErrorInvalidArgument;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;

    uniboCgrSap->route_iterator = list_get_next_elt(uniboCgrSap->route_iterator);
    *uniboCgrRoute = listElt_get_data(uniboCgrSap->route_iterator);
    if (!*uniboCgrRoute) return UniboCGR_ErrorRouteNotFound;

    return UniboCGR_NoError;
}
UniboCGR_Error UniboCGR_create_excluded_neighbors_list(UniboCGR_excluded_neighbors_list* list) {
    if (!list) { return UniboCGR_ErrorInvalidArgument; }
    *list = (UniboCGR_excluded_neighbors_list) list_create(NULL, NULL, NULL, MDEPOSIT_wrapper);
    if (!*list) { return UniboCgr_ErrorSystem; }
    return UniboCGR_NoError;
}
void  UniboCGR_destroy_excluded_neighbors_list(UniboCGR_excluded_neighbors_list* list) {
    if (!list || !*list) return;
    free_list((List) *list);
    *list = NULL;
}
void  UniboCGR_reset_excluded_neighbors_list(UniboCGR_excluded_neighbors_list list) {
    if (!list) return;
    free_list_elts((List) list);
}
UniboCGR_Error UniboCGR_add_excluded_neighbor(UniboCGR_excluded_neighbors_list list, uint64_t excluded_neighbor) {
    if (!list) return UniboCGR_ErrorInvalidArgument;
    uint64_t* node_id_obj = MWITHDRAW(sizeof(uint64_t));

    if (!node_id_obj) {
        return UniboCgr_ErrorSystem;
    }
    *node_id_obj = excluded_neighbor;

    bool found = false;
    for (ListElt* elt = list_get_first_elt((List) list); elt != NULL; elt = elt->next) {
        // just for safety -- should never happen
        if (!elt->data) continue;
        uint64_t* elt_node_id = (uint64_t*) elt->data;

        if (*elt_node_id == excluded_neighbor) {
            found = true;
            break;
        }
    }

    if (!found) {
        list_insert_last((List) list, node_id_obj);
    }

    return UniboCGR_NoError;
}
int UniboCGRSAP_handle_updates(UniboCGRSAP* uniboCgrSap) {
    if (uniboCgrSap->mustClearRoutingObjects) {
        uniboCgrSap->mustClearRoutingObjects = false;
        reset_NodesTree(uniboCgrSap);
        if (build_local_node_neighbors_list(uniboCgrSap) < 0) {
            return -2;
        }
    }
    return 0;
}
void UniboCGR_log_write(UniboCGR uniboCgr, const char* fmt, ...) {
    if (!uniboCgr) return;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    if (!uniboCgrSap->feature_logger) return;
    va_list args;
    va_start(args, fmt);
    vwriteLog(uniboCgrSap, fmt, args);
    va_end(args);

    UniboCGR_log_flush(uniboCgr);
}
void UniboCGR_log_flush(UniboCGR uniboCgr) {
    if (!uniboCgr) return;
    UniboCGRSAP* uniboCgrSap = (UniboCGRSAP*) uniboCgr;
    log_fflush(uniboCgrSap);
}
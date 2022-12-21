/** \file UniboCGR.h
 *
 *  \brief  [API] Public abstraction layer for Unibo-CGR core.
 *
 *  \details This file contains all the public definitions of functions enabling any BP application
 *           to interface with Unibo-CGR. <br>
 *           A BP application might want to instantiate multiple Unibo-CGR instances, this can
 *           be done by means of UniboCGR_open() that returns a completely private instance. <br>
 *           Is worth to note the each Unibo-CGR instance has the ownership of its, implicitly private, contact plan. <br>
 *           If a BP implementation instantiates multiple Unibo-CGR instances it is recommended to avoid
 *           any contact duplication (since local information, like MTVs, are not shared). However that is just an
 *           hint and not a strict-requirement. <br>
 *           A BP application might want to run multiple Unibo-CGR instances in a multithreading environment,
 *           that is possible as long as the memory allocator provided to the Unibo-CGR library
 *           is thread-safe (note that the memory allocator defaults to malloc() and free() -- thread-safe). <br>
 *           Speaking about multithreading should be noted that each Unibo-CGR instance is completely
 *           lock-free, so a single instance must be managed by a single thread in order to avoid race conditions;
 *           if you want to share a single Unibo-CGR instance between multiple threads you must encapsulate
 *           Unibo-CGR sessions in critical sections (what is a Unibo-CGR session is defined below). <br>
 *           Finally, let us remark that the memory allocator is shared by all Unibo-CGR instances and should be
 *           setup (only once) before any use of the Unibo-CGR library, otherwise you might fall into undefined behaviour.
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
#ifndef UNIBO_CGR_UNIBOCGR_H
#define UNIBO_CGR_UNIBOCGR_H


#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*                                                 TYPEDEF
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typedef struct UniboCGR_opaque* UniboCGR;
typedef struct UniboCGR_Bundle_opaque* UniboCGR_Bundle;
typedef struct UniboCGR_Contact_opaque* UniboCGR_Contact;
typedef struct UniboCGR_Range_opaque* UniboCGR_Range;
typedef struct UniboCGR_Route_opaque* UniboCGR_Route;
typedef struct UniboCGR_route_list_opaque* UniboCGR_route_list;
typedef struct UniboCGR_excluded_neighbors_list_opaque* UniboCGR_excluded_neighbors_list;

/*
 * Note: At the time of writing only UniboCGR_ContactType_Scheduled is supported
 */
typedef enum {
    UniboCGR_ContactType_Unknown = 0,
    // UniboCGR_ContactType_Registration,
    UniboCGR_ContactType_Scheduled,
    // UniboCGR_ContactType_Suppressed,
    // UniboCGR_ContactType_Predicted,
    // UniboCGR_ContactType_Hypothetical,
    // UniboCGR_ContactType_Discovered
} UniboCGR_ContactType;

typedef enum {
    UniboCGR_RoutingAlgorithm_Unknown = 0,
    UniboCGR_RoutingAlgorithm_CGR,
    UniboCGR_RoutingAlgorithm_MSR
} UniboCGR_RoutingAlgorithm;

typedef enum {
    UniboCGR_BundlePriority_Bulk      = 0,
    UniboCGR_BundlePriority_Normal    = 1,
    UniboCGR_BundlePriority_Expedited = 2
} UniboCGR_BundlePriority;

typedef int (*computeApplicableBacklogCallback)(
        uint64_t neighbor,
        UniboCGR_BundlePriority priority,
        uint8_t ordinal,
        uint64_t *applicableBacklog,
        uint64_t *totalBacklog,
        void* userArg);

typedef enum {
    PhaseThreeCostFunction_default = 0
} PhaseThreeCostFunction;


// user-defined memory allocator
// file and line are the usual __FILE__ and __LINE__ inlined
typedef void* (*malloc_like)(const char* file, int line, size_t size);
typedef void (*free_like)(const char* file, int line, void* addr);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                              ERROR HANDLING
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


typedef enum {
    UniboCGR_NoError                        =  0,
    UniboCGR_ErrorUnknown                   = -1,
    UniboCgr_ErrorSystem                    = -2,
    UniboCGR_ErrorInvalidArgument           = -3,
    UniboCGR_ErrorInternal                  = -4,
    UniboCGR_ErrorCannotOpenLogDirectory    = -5,
    UniboCGR_ErrorCannotOpenLogFile         = -6,
    UniboCGR_ErrorInvalidNodeNumber         = -7,
    UniboCGR_ErrorContactNotFound           = -8,
    UniboCGR_ErrorFoundOverlappingContact   = -9,
    UniboCGR_ErrorRangeNotFound             = -10,
    UniboCGR_ErrorFoundOverlappingRange     = -11,
    UniboCGR_ErrorRouteNotFound             = -12,
    UniboCGR_ErrorInvalidTime               = -13,
    UniboCGR_ErrorMalformedMSRRoute         = -14,
    UniboCGR_ErrorSessionAlreadyOpened      = -15,
    UniboCGR_ErrorSessionClosed             = -16,
    UniboCGR_ErrorWrongSession              = -17
} UniboCGR_Error;

extern const char* UniboCGR_get_error_string(UniboCGR_Error error);
extern bool UniboCGR_check_error(UniboCGR_Error error);
extern bool UniboCGR_check_fatal_error(UniboCGR_Error error);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                      UNIBO-CGR LIBRARY MANAGEMENT
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * This section contains functions that should be used to initialize and destroy the Unibo-CGR library.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * \brief Setup the memory allocator for the Unibo-CGR library.
 *
 * \details You should call this function at most one time in the whole application lifetime.
 *          If you change the memory allocator after Unibo-CGR's instances initialization
 *          then you ends up in undefined behaviour.
 *
 * \param[in]  alloc    A user-defined function that behaves like malloc()
 * \param[in]  release  A user-defined function that behaves like free()
 *
 * \note The user-defined memory allocator must return pointers aligned to any fundamental type
 *       (e.g. usually 16 bytes alignment on 64-bit systems).
 * \note The memory allocator is shared between all Unibo-CGR instances so be sure to provide
 *       a thread-safe allocator for multithreaded applications.
 *
 * \note Defaults to malloc() and free() (thread-safe).
 */
extern UniboCGR_Error UniboCGR_setup_memory_allocator(malloc_like alloc, free_like release);


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                 UNIBO-CGR INSTANCE MANAGEMENT
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Each Unibo-CGR instance is created by means of UniboCGR_open() and destroyed with UniboCGR_close().
 *
 * Once an instance has been created you might open a session a time.
 * Each session is segmented by means of UniboCGR_*_open() and UniboCGR_*_close().
 * Where "*" might be:
 * - feature
 * - contact_plan
 * - routing
 *
 * You must not overlap Unibo-CGR sessions
 * (e.g. you must UniboCGR_*_close() a session before open a new one).
 *
 * -
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * \brief
 *
 * \param[out] uniboCgr Newly created Unibo-CGR instance.
 *
 * \param current_time  Seconds elapsed since Unix Epoch.
 *
 * \param time_zero Reference time used internally by Unibo-CGR.
 *                  Internally all times are relative to this quantity,
 *                  but this is transparent for the external user.
 *                  You must set this value to 0 in production code,
 *                  you might find it useful in a test/debug session when
 *                  you analyze Unibo-CGR logs.
 *
 * \param local_node Administrative Endpoint node number of the BP node.
 *
 * \param best_route_selection_function The algorithm to use during CGR phase three (best route(s) selection
 *                                      from candidate routes list)
 *
 * \param compute_applicable_backlog_callback Callback to BP queue (of a given neighbor) in order to retrieve
 *                                            information about applicable and total backlog.
 *                                            The applicable backlog is defined in CCSDS SABR Blue Book.
 *                                            The total backlog is the total amount of bytes already enqueued
 *                                            in the BP queue (neighbor).
 *
 * \param callback_arg Argument passed to user-defined callbacks (e.g. compute_applicable_backlog_callback() above)
 */
extern UniboCGR_Error UniboCGR_open(UniboCGR* uniboCgr,
                                    time_t current_time,
                                    time_t time_zero,
                                    uint64_t local_node,
                                    PhaseThreeCostFunction best_route_selection_function,
                                    computeApplicableBacklogCallback compute_applicable_backlog_callback,
                                    void* callback_arg);

extern void UniboCGR_close(UniboCGR* uniboCgr, time_t current_time);



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                 UNIBO-CGR SESSION CONTACT PLAN UPDATE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Each Unibo-CGR contact plan session is opened by means of UniboCGR_contact_plan_open() and
 * closed with UniboCGR_contact_plan_close().
 *
 * During a contact plan update session you might:
 * - add/change/remove a contact.
 *    For change/remove operations the contact is identified by means of {sender, receiver, start time}.
 *    Maybe in the future also the contact's type may be used to identify a contact.
 *    At the time of writing only the "scheduled" type is supported.
 *    Contact's time interval must not overlap with other contacts between same sender and receiver.
 * - add/change/remove a range.
 *    For change/remove operations the range is identified by means of {sender, receiver, start time}.
 *    Range's time interval must not overlap with other contacts between same sender and receiver.
 *
 * Note that both contacts and ranges are unidirectional from sender to receiver.
 *
 * Note: all times (start time, end time) are expressed in seconds elapsed since Unix Epoch.
 * Note: owlt (one-way-light-time) is the end-to-end delay from a sender to a receiver (! unidirectional).
 * Note: confidence must be in range [0.0, 1.0] (bounds included)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


extern UniboCGR_Error UniboCGR_contact_plan_open(UniboCGR uniboCgr, time_t time);

extern UniboCGR_Error UniboCGR_contact_plan_close(UniboCGR uniboCgr);

// erase current contact plan
extern UniboCGR_Error UniboCGR_contact_plan_reset(UniboCGR uniboCgr);

extern UniboCGR_Error UniboCGR_contact_plan_add_contact(UniboCGR uniboCgr,
                                                        UniboCGR_Contact uniboCgrContact,
                                                        bool copy_mtv);

extern UniboCGR_Error UniboCGR_contact_plan_remove_contact(UniboCGR uniboCgr,
                                                           UniboCGR_ContactType contact_type,
                                                           uint64_t sender,
                                                           uint64_t receiver,
                                                           time_t start_time);

extern UniboCGR_Error UniboCGR_contact_plan_change_contact_start_time(UniboCGR uniboCgr,
                                                                      UniboCGR_ContactType contact_type,
                                                                      uint64_t sender,
                                                                      uint64_t receiver,
                                                                      time_t start_time,
                                                                      time_t new_start_time);

extern UniboCGR_Error UniboCGR_contact_plan_change_contact_end_time(UniboCGR uniboCgr,
                                                                    UniboCGR_ContactType contact_type,
                                                                    uint64_t sender,
                                                                    uint64_t receiver,
                                                                    time_t start_time,
                                                                    time_t new_end_time);

extern UniboCGR_Error UniboCGR_contact_plan_change_contact_type(UniboCGR uniboCgr,
                                                                UniboCGR_ContactType contact_type,
                                                                uint64_t sender,
                                                                uint64_t receiver,
                                                                time_t start_time,
                                                                UniboCGR_ContactType new_type);

extern UniboCGR_Error UniboCGR_contact_plan_change_contact_confidence(UniboCGR uniboCgr,
                                                                      UniboCGR_ContactType contact_type,
                                                                      uint64_t sender,
                                                                      uint64_t receiver,
                                                                      time_t start_time,
                                                                      float new_confidence);

extern UniboCGR_Error UniboCGR_contact_plan_change_contact_xmit_rate(UniboCGR uniboCgr,
                                                                     UniboCGR_ContactType contact_type,
                                                                     uint64_t sender,
                                                                     uint64_t receiver,
                                                                     time_t start_time,
                                                                     uint64_t new_xmit_rate);

extern UniboCGR_Error UniboCGR_contact_plan_add_range(UniboCGR uniboCgr,
                                                      UniboCGR_Range uniboCgrRange);

extern UniboCGR_Error UniboCGR_contact_plan_remove_range(UniboCGR uniboCgr,
                                                         uint64_t sender,
                                                         uint64_t receiver,
                                                         time_t start_time);

extern UniboCGR_Error UniboCGR_contact_plan_change_range_start_time(UniboCGR uniboCgr,
                                                                    uint64_t sender,
                                                                    uint64_t receiver,
                                                                    time_t start_time,
                                                                    time_t new_start_time);

extern UniboCGR_Error UniboCGR_contact_plan_change_range_end_time(UniboCGR uniboCgr,
                                                                  uint64_t sender,
                                                                  uint64_t receiver,
                                                                  time_t start_time,
                                                                  time_t new_end_time);

extern UniboCGR_Error UniboCGR_contact_plan_change_range_one_way_light_time(UniboCGR uniboCgr,
                                                                            uint64_t sender,
                                                                            uint64_t receiver,
                                                                            time_t start_time,
                                                                            uint64_t new_owlt);



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                         UNIBO-CGR SESSION ROUTING
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * TODO
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

extern UniboCGR_Error UniboCGR_routing_open(UniboCGR uniboCgr, time_t time);

/**
 * \brief Call Unibo-CGR Routing algorithm.
 * \param uniboCgr
 * \param[in] uniboCgrBundle
 * \param[in] excludedNeighborsList List of neighbors that must not appear as "proximate nodes" in the best routes list.
 * \param[out] routeList  List of best routes found by Unibo-CGR.
 *                        Unibo-CGR maintains ownership of this list -- you must not deallocate it.
 */
extern UniboCGR_Error UniboCGR_routing(UniboCGR uniboCgr,
                                       UniboCGR_Bundle uniboCgrBundle,
                                       UniboCGR_excluded_neighbors_list excludedNeighborsList,
                                       UniboCGR_route_list* routeList);

// meaningful only during routing session and after UniboCGR_routing() call.
extern UniboCGR_RoutingAlgorithm UniboCGR_get_used_routing_algorithm(UniboCGR uniboCgr);

extern UniboCGR_Error UniboCGR_routing_close(UniboCGR uniboCgr);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                     UNIBO-CGR SESSION FEATURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Each Unibo-CGR feature session is opened by means of UniboCGR_feature_open() and
 * closed with UniboCGR_feature_close().
 *
 * As other sessions, each Unibo-CGR feature is enabled/disabled in a per-instance basis
 * (e.g. different Unibo-CGR instances might have different features enabled).
 *
 * Routing-related feature might trigger a full reset of the internal computed routes
 * (e.g. Unibo-CGR's phase one might be performed again).
 * However, contact plan information about contact utilization
 * (like contact MTVs) will not be discarded.
 *
 * Features:
 *
 * - logger
 *     Create a log directory in which a "log.txt" and "call_#" files will be created.
 *     log.txt contains global information that should be used to redirect the researcher
 *     into a specific call_# file that contains in-depth information about a single
 *     routing call.
 *     Since it is a time/resource consuming feature is recommended to keep
 *     it disabled at default in production code and enable it only when routing tests
 *     are performed.
 *
 * - one route per neighbor
 *     The routes computation will be performed until the routing algorithm found
 *     at least one "candidate" route to bundle's destination per each known neighbor
 *     of the local node. A fixed limit is also supported.
 *
 * - queue delay
 *     Keep trace of queue delay not just on the local node (ETO) but on all route's hops
 *     by means of information like contact's MTV.
 *     It behaves like ETO on all hops.
 *
 * - reactive anti-loop
 *     TODO
 *
 * - proactive anti-loop
 *     TODO
 *
 * - moderate source routing
 *     Enable moderate source routing algorithm.
 *     More information here: TODO link
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

extern UniboCGR_Error UniboCGR_feature_open(UniboCGR uniboCgr, time_t time);
extern UniboCGR_Error UniboCGR_feature_close(UniboCGR uniboCgr);

extern UniboCGR_Error UniboCGR_feature_logger_enable(UniboCGR uniboCgr, const char* log_dir);
extern UniboCGR_Error UniboCGR_feature_logger_disable(UniboCGR uniboCgr);

/**
 * \param[in] limit 0 means unlimited ("pure" one route per neighbor). "> 0" force an upper bound on
 *                  the number of neighbors for which a candidate route is required.
 *
 * \note  One route per neighbor - without limit - is forced by CCSDS SABR when CGR is used
 *        to find a route for a critical bundle. That behaviour is not modified by
 *        enabling/disabling this feature (also, the user-defined limit is overridden).
 */
extern UniboCGR_Error UniboCGR_feature_OneRoutePerNeighbor_enable(UniboCGR uniboCgr, uint32_t limit);
extern UniboCGR_Error UniboCGR_feature_OneRoutePerNeighbor_disable(UniboCGR uniboCgr);

extern UniboCGR_Error UniboCGR_feature_QueueDelay_enable(UniboCGR uniboCgr);
extern UniboCGR_Error UniboCGR_feature_QueueDelay_disable(UniboCGR uniboCgr);

extern UniboCGR_Error UniboCGR_feature_ModerateSourceRouting_enable(UniboCGR uniboCgr);
extern UniboCGR_Error UniboCGR_feature_ModerateSourceRouting_disable(UniboCGR uniboCgr);

extern UniboCGR_Error UniboCGR_feature_ReactiveAntiLoop_enable(UniboCGR uniboCgr);
extern UniboCGR_Error UniboCGR_feature_ReactiveAntiLoop_disable(UniboCGR uniboCgr);

extern UniboCGR_Error UniboCGR_feature_ProactiveAntiLoop_enable(UniboCGR uniboCgr);
extern UniboCGR_Error UniboCGR_feature_ProactiveAntiLoop_disable(UniboCGR uniboCgr);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                UTILITIES
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * In this section you find functions that are not session-related - that is, you can use them in any
 * Unibo-CGR session.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * \brief Retrieve Unibo-CGR reference time. Same value that you passed into UniboCGR_open().
 */
extern time_t UniboCGR_get_reference_time(UniboCGR uniboCgr);

extern bool UniboCGR_feature_logger_check(UniboCGR uniboCgr);
extern bool UniboCGR_feature_OneRoutePerNeighbor_check(UniboCGR uniboCgr, uint64_t* limit);
extern bool UniboCGR_feature_QueueDelay_check(UniboCGR uniboCgr);
extern bool UniboCGR_feature_ModerateSourceRouting_check(UniboCGR uniboCgr);
extern bool UniboCGR_feature_ReactiveAntiLoop_check(UniboCGR uniboCgr);
extern bool UniboCGR_feature_ProactiveAntiLoop_check(UniboCGR uniboCgr);

extern void UniboCGR_log_write(UniboCGR uniboCgr, const char* fmt, ...);
extern void UniboCGR_log_flush(UniboCGR uniboCgr);

extern UniboCGR_Error UniboCGR_create_excluded_neighbors_list(UniboCGR_excluded_neighbors_list* list);
extern void  UniboCGR_destroy_excluded_neighbors_list(UniboCGR_excluded_neighbors_list* list);
extern void  UniboCGR_reset_excluded_neighbors_list(UniboCGR_excluded_neighbors_list list);
extern UniboCGR_Error UniboCGR_add_excluded_neighbor(UniboCGR_excluded_neighbors_list list, uint64_t excluded_neighbor);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                          BUNDLE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Bundle abstraction between Unibo-CGR core and interface.
 * Basically read/write access to a Bundle object.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* * Bundle Object lifetime * */

extern UniboCGR_Error UniboCGR_Bundle_create(UniboCGR_Bundle* uniboCgrBundle);
extern void           UniboCGR_Bundle_destroy(UniboCGR_Bundle* uniboCgrBundle);
/**
 * \details You must call this function after or before each routing invocation.
 */
extern void           UniboCGR_Bundle_reset(UniboCGR_Bundle uniboCgrBundle);

/**
 * \details Required. Supported version values are 6 and 7 at the time of writing.
 *          Note that the creation time and lifetime are strictly dependent on the bundle protocol version,
 *          e.g. in BPv6 they are expressed in seconds and in BPv7 in milliseconds.
 *          So you must carefully set the BP version for each bundle you feed to Unibo-CGR routing.
 */
extern void UniboCGR_Bundle_set_bundle_protocol_version(UniboCGR_Bundle uniboCgrBundle, uint64_t version);

/**
 * \details Optional. Just for logs.
 */
extern void UniboCGR_Bundle_set_source_node_id(UniboCGR_Bundle uniboCgrBundle, const char* source_node_id);
/**
 * \details Required (need it for expiration time computation).
 *          Same value as expressed in Bundle's Primary Block.
 */
extern void UniboCGR_Bundle_set_creation_time(UniboCGR_Bundle uniboCgrBundle, uint64_t creation_time);
/**
 * \details Optional. Just for logs.
 */
extern void UniboCGR_Bundle_set_sequence_number(UniboCGR_Bundle uniboCgrBundle, uint64_t sequence_number);
/**
 * \details Optional. Just for logs.
 */
extern void UniboCGR_Bundle_set_fragment_offset(UniboCGR_Bundle uniboCgrBundle, uint64_t fragment_offset);
/**
 * \details Optional. Just for logs.
 */
extern void UniboCGR_Bundle_set_fragment_length(UniboCGR_Bundle uniboCgrBundle, uint64_t fragment_length);

/**
 * \details Required (need it for expiration time computation).
 *          Same value as expressed in Bundle's Primary Block.
 */
extern void UniboCGR_Bundle_set_lifetime(UniboCGR_Bundle uniboCgrBundle, uint64_t lifetime);

/**
 * \details Optional. Just for logs.
 */
extern void UniboCGR_Bundle_set_total_application_data_unit_length(UniboCGR_Bundle uniboCgrBundle, uint64_t length);

/**
 * \details Required. Bundle destination node number.
 */
extern void UniboCGR_Bundle_set_destination_node_id(UniboCGR_Bundle uniboCgrBundle, uint64_t destination_node_id);
/**
 * \details Required. Previous Hop node number (0 otherwise).
 */
extern void UniboCGR_Bundle_set_previous_node_id(UniboCGR_Bundle uniboCgrBundle, uint64_t previous_node_id);

/* *  Values required to compute internally the Estimated Volume Consumption * */

/**
 * \details Required. Primary Block length (bytes).
 */
extern void UniboCGR_Bundle_set_primary_block_length(UniboCGR_Bundle uniboCgrBundle, uint64_t length);
/**
 * \details Required. Total Extension Block length (bytes).
 */
extern void UniboCGR_Bundle_set_total_ext_block_length(UniboCGR_Bundle uniboCgrBundle, uint64_t length);
/**
 * \details Required. Payload Block length (bytes).
 */
extern void UniboCGR_Bundle_set_payload_length(UniboCGR_Bundle uniboCgrBundle, uint64_t length);

// retrieve evc -- note that its computation is performed during the routing call
extern uint64_t UniboCGR_Bundle_get_estimated_volume_consumption(UniboCGR_Bundle uniboCgrBundle);

// must be 0.0 at default -- values in range (0.0, 1.0] are used for re-forwarding in ION
extern void UniboCGR_Bundle_set_delivery_confidence(UniboCGR_Bundle uniboCgrBundle, float confidence);

// priority

extern void UniboCGR_Bundle_set_priority_bulk(UniboCGR_Bundle uniboCgrBundle);
extern void UniboCGR_Bundle_set_priority_normal(UniboCGR_Bundle uniboCgrBundle);
extern void UniboCGR_Bundle_set_priority_expedited(UniboCGR_Bundle uniboCgrBundle, uint8_t ordinal);

// these flags are defined in CCSDS SABR Blue Book

extern void UniboCGR_Bundle_set_flag_probe(UniboCGR_Bundle uniboCgrBundle, bool value);
extern void UniboCGR_Bundle_set_flag_do_not_fragment(UniboCGR_Bundle uniboCgrBundle, bool value);
extern void UniboCGR_Bundle_set_flag_critical(UniboCGR_Bundle uniboCgrBundle, bool value);
extern void UniboCGR_Bundle_set_flag_backward_propagation(UniboCGR_Bundle uniboCgrBundle, bool value);

/**
 * \details used to populate an internal list representing the "geographic route".
 *          you should call this function for each node in the geographic route if you would like
 *          to make use of anti-loop features
 *
 * \retval UniboCGR_NoError     Success
 * \retval UniboCGR_ErrorSystem Fatal error
 */
extern UniboCGR_Error UniboCGR_Bundle_add_node_in_geographic_route_list(UniboCGR_Bundle uniboCgrBundle, uint64_t node_id);

/**
 * \brief Append a new contact to the Bundle's MSR route.
 *
 * \details The contact "sender" must be the same as the previous contact "receiver".
 *
 * \param uniboCgr
 * \param uniboCgrBundle
 * \param[in] uniboCgrCgrHop
 * \param[out] uniboCgrContact If not NULL it will contains the contact just appended to the route (only on success case).
 *
 * \retval UniboCGR_NoError              Success
 * \retval UniboCGR_ErrorContactNotFound Error
 */
extern UniboCGR_Error  UniboCGR_add_moderate_source_routing_hop(UniboCGR uniboCgr,
                                                                UniboCGR_Bundle uniboCgrBundle,
                                                                UniboCGR_ContactType uniboCgrContactType,
                                                                uint64_t sender,
                                                                uint64_t receiver,
                                                                time_t start_time);
// you must call this function when you have terminated to feed all hops by means of UniboCGR_add_moderate_source_routing_hop()
extern UniboCGR_Error  UniboCGR_finalize_moderate_source_routing_route(UniboCGR uniboCgr,
                                                                       UniboCGR_Bundle uniboCgrBundle,
                                                                       uint32_t hint_hop_lower_bound);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                          CONTACT
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Contact abstraction between Unibo-CGR core and interface.
 * Basically read/write access to a Contact object.
 *
 * Note that the "set" methods are not propagated into the internal Unibo-CGR contacts graph.
 * This UniboCGR_Contact object API must be used to prepare (set() functions) a Contact object
 * for further insertion/removal/change and to view (get() functions) each route's hop.
 *
 * If you want to inform Unibo-CGR about a Contact insertion/removal/change you must use
 * the proper function (e.g. UniboCGR_contact_plan_add_contact()) of the Unibo-CGR instance.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


extern UniboCGR_Error UniboCGR_Contact_create(UniboCGR_Contact* uniboCgrContact);
extern void           UniboCGR_Contact_destroy(UniboCGR_Contact* uniboCgrContact);
extern void           UniboCGR_Contact_reset(UniboCGR_Contact uniboCgrContact);
extern void UniboCGR_Contact_set_sender(UniboCGR_Contact uniboCgrContact, uint64_t sender);
extern void UniboCGR_Contact_set_receiver(UniboCGR_Contact uniboCgrContact, uint64_t receiver);
extern void UniboCGR_Contact_set_start_time(UniboCGR uniboCgr, UniboCGR_Contact uniboCgrContact, time_t start_time);
extern void UniboCGR_Contact_set_end_time(UniboCGR uniboCgr, UniboCGR_Contact uniboCgrContact, time_t end_time);
extern void UniboCGR_Contact_set_xmit_rate(UniboCGR_Contact uniboCgrContact, uint64_t xmit_rate_bytes_per_second);
extern void UniboCGR_Contact_set_confidence(UniboCGR_Contact uniboCgrContact, float confidence);
extern void UniboCGR_Contact_set_mtv_bulk(UniboCGR_Contact uniboCgrContact, double mtv);
extern void UniboCGR_Contact_set_mtv_normal(UniboCGR_Contact uniboCgrContact, double mtv);
extern void UniboCGR_Contact_set_mtv_expedited(UniboCGR_Contact uniboCgrContact, double mtv);
extern void UniboCGR_Contact_set_type(UniboCGR_Contact uniboCgrContact, UniboCGR_ContactType type);
extern uint64_t               UniboCGR_Contact_get_sender(UniboCGR_Contact uniboCgrContact);
extern uint64_t               UniboCGR_Contact_get_receiver(UniboCGR_Contact uniboCgrContact);
extern time_t                 UniboCGR_Contact_get_start_time(UniboCGR uniboCgr, UniboCGR_Contact uniboCgrContact);
extern time_t                 UniboCGR_Contact_get_end_time(UniboCGR uniboCgr, UniboCGR_Contact uniboCgrContact);
extern uint64_t               UniboCGR_Contact_get_xmit_rate(UniboCGR_Contact uniboCgrContact);
extern float                  UniboCGR_Contact_get_confidence(UniboCGR_Contact uniboCgrContact);
extern double                 UniboCGR_Contact_get_mtv_bulk(UniboCGR_Contact uniboCgrContact);
extern double                 UniboCGR_Contact_get_mtv_normal(UniboCGR_Contact uniboCgrContact);
extern double                 UniboCGR_Contact_get_mtv_expedited(UniboCGR_Contact uniboCgrContact);
extern UniboCGR_ContactType   UniboCGR_Contact_get_type(UniboCGR_Contact uniboCgrContact);

/* * Iterator for Unibo-CGR contacts graph.  * */

/* NOTE: After each contact insertion/removal/change you MUST reset the iterator by means of UniboCGR_get_first_contact().
 *       Undefined behaviour otherwise.
 */

/**
 * \note the internal iterator will point to the contact returned by this function.
 *       you can iterate forward by means of UniboCGR_get_next_contact().
 */
extern UniboCGR_Error UniboCGR_find_contact(UniboCGR uniboCgr,
                                            UniboCGR_ContactType contact_type,
                                            uint64_t sender,
                                            uint64_t receiver,
                                            time_t start_time,
                                            UniboCGR_Contact *contactOutput);
/**
 * \note the internal iterator will point to the contact returned by this function.
 *       you can iterate forward by means of UniboCGR_get_next_contact().
 */
extern UniboCGR_Error UniboCGR_get_first_contact(UniboCGR uniboCgr,
                                                 UniboCGR_Contact* contactOutput);

/**
 * \note the internal iterator will point to the contact returned by this function.
 *       you can iterate forward by means of UniboCGR_get_next_contact().
 */
extern UniboCGR_Error UniboCGR_get_next_contact(UniboCGR uniboCgr,
                                                UniboCGR_Contact* contactOutput);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                          RANGE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Range abstraction between Unibo-CGR core and interface.
 * Basically write-only access to a Range object.
 *
 * Note that the "set" methods are not propagated into the internal Unibo-CGR ranges graph.
 * UniboCGR_Range object API must be used to prepare (set() functions) a Range object
 * for further insertion/removal/change into a UniboCGR instance.
 *
 * If you want to inform Unibo-CGR about a Range insertion/removal/change you must use
 * the proper function (e.g. UniboCGR_add_range()) of the UniboCGR instance.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

extern UniboCGR_Error UniboCGR_Range_create(UniboCGR_Range* uniboCgrRange);
extern void           UniboCGR_Range_destroy(UniboCGR_Range* uniboCgrRange);
extern void           UniboCGR_Range_reset(UniboCGR_Range uniboCgrRange);
extern void UniboCGR_Range_set_sender(UniboCGR_Range uniboCgrRange, uint64_t sender);
extern void UniboCGR_Range_set_receiver(UniboCGR_Range uniboCgrRange, uint64_t receiver);
extern void UniboCGR_Range_set_start_time(UniboCGR uniboCgr, UniboCGR_Range uniboCgrRange, time_t start_time);
extern void UniboCGR_Range_set_end_time(UniboCGR uniboCgr, UniboCGR_Range uniboCgrRange, time_t end_time);
extern void UniboCGR_Range_set_one_way_light_time(UniboCGR_Range uniboCgrRange, uint64_t one_way_light_time_seconds);
extern uint64_t UniboCGR_Range_get_sender(UniboCGR_Range uniboCgrRange);
extern uint64_t UniboCGR_Range_get_receiver(UniboCGR_Range uniboCgrRange);
extern time_t   UniboCGR_Range_get_start_time(UniboCGR uniboCgr, UniboCGR_Range uniboCgrRange);
extern time_t   UniboCGR_Range_get_end_time(UniboCGR uniboCgr, UniboCGR_Range uniboCgrRange);
extern uint64_t UniboCGR_Range_get_one_way_light_time(UniboCGR_Range uniboCgrRange);

/* * Iterator for Unibo-CGR ranges graph.  * */

/* NOTE: After each range insertion/removal/change you MUST reset the iterator by means of UniboCGR_get_first_contact().
 *       Undefined behaviour otherwise.
 */

/**
 * \note the internal iterator will point to the range returned by this function.
 *       you can iterate forward by means of UniboCGR_get_next_range().
 */
extern UniboCGR_Error UniboCGR_find_range(UniboCGR uniboCgr,
                                          uint64_t sender,
                                          uint64_t receiver,
                                          time_t start_time,
                                          UniboCGR_Range *rangeOutput);
/**
 * \note the internal iterator will point to the range returned by this function.
 *       you can iterate forward by means of UniboCGR_get_next_range().
 */
extern UniboCGR_Error UniboCGR_get_first_range(UniboCGR uniboCgr,
                                               UniboCGR_Range* rangeOutput);
/**
 * \note the internal iterator will point to the contact returned by this function.
 *       you can iterate forward by means of UniboCGR_get_next_range().
 */
extern UniboCGR_Error UniboCGR_get_next_range(UniboCGR uniboCgr,
                                              UniboCGR_Range* rangeOutput);


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                          ROUTE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * TODO doc
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

extern uint64_t UniboCGR_Route_get_neighbor(UniboCGR_Route uniboCgrRoute);
extern float    UniboCGR_Route_get_arrival_confidence(UniboCGR_Route uniboCgrRoute);
extern time_t   UniboCGR_Route_get_best_case_arrival_time(UniboCGR uniboCgr, UniboCGR_Route uniboCgrRoute);
extern time_t   UniboCGR_Route_get_eto(UniboCGR uniboCgr, UniboCGR_Route uniboCgrRoute);
extern double   UniboCGR_Route_get_route_volume_limit(UniboCGR_Route uniboCgrRoute);
extern time_t   UniboCGR_Route_get_projected_bundle_arrival_time(UniboCGR uniboCgr, UniboCGR_Route uniboCgrRoute);
extern time_t   UniboCGR_Route_get_best_case_transmission_time(UniboCGR uniboCgr, UniboCGR_Route uniboCgrRoute);
extern time_t   UniboCGR_Route_get_expiration_time(UniboCGR uniboCgr, UniboCGR_Route uniboCgrRoute);
extern uint64_t UniboCGR_Route_get_total_one_way_light_time(UniboCGR_Route uniboCgrRoute);
extern void     UniboCGR_Route_get_overbooking_management(UniboCGR_Route uniboCgrRoute, uint64_t* overbooked, uint64_t* committed);

extern UniboCGR_Error UniboCGR_get_first_hop(UniboCGR uniboCgr, UniboCGR_Route uniboCgrRoute, UniboCGR_Contact* uniboCgrContact);
extern UniboCGR_Error UniboCGR_get_next_hop(UniboCGR uniboCgr, UniboCGR_Contact* uniboCgrContact);

extern uint32_t UniboCGR_route_list_get_length(UniboCGR_route_list list);
extern UniboCGR_Error UniboCGR_get_first_route(UniboCGR uniboCgr, UniboCGR_route_list uniboCgrRouteList, UniboCGR_Route* uniboCgrRoute);
extern UniboCGR_Error UniboCGR_get_next_route(UniboCGR uniboCgr, UniboCGR_Route* uniboCgrRoute);

#ifdef __cplusplus
}
#endif

#endif //UNIBO_CGR_UNIBOCGR_H

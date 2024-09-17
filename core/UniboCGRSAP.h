/** \file UniboCGRSAP.h
 *
 *  \brief  Private declaration of functions (related to UniboCGRSAP struct) used internally by Unibo-CGR core.
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

#ifndef UNIBO_CGR_UNIBOCGRSAP_H
#define UNIBO_CGR_UNIBOCGRSAP_H

#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/time.h>

#include "library_from_ion/scalar/scalar.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UniboCGRSAP UniboCGRSAP;
typedef struct PhaseOneSAP PhaseOneSAP;
typedef struct PhaseTwoSAP PhaseTwoSAP;
typedef struct PhaseThreeSAP PhaseThreeSAP;
typedef struct UniboCgrCurrentCallSAP UniboCgrCurrentCallSAP;
typedef struct MSRSAP MSRSAP;
typedef struct ContactSAP ContactSAP;
typedef struct RangeSAP RangeSAP;
typedef struct NodeSAP NodeSAP;
typedef struct LogSAP LogSAP;
typedef struct TimeAnalysisSAP TimeAnalysisSAP;

#define MWITHDRAW(size) UniboCGRSAP_MWITHDRAW(__FILE__, __LINE__, size)
#define MDEPOSIT(addr) UniboCGRSAP_MDEPOSIT(__FILE__, __LINE__, addr)

/**
 * \brief Unibo-CGR core dynamic memory allocator (malloc()).
 *
 * \note You might want to use the MWITHDRAW() macro wrapper.
 */
extern void* UniboCGRSAP_MWITHDRAW(const char* file, int line, size_t size);
/**
 * \brief Unibo-CGR core dynamic memory allocator (free()).
 *
 * \note You might want to use the MDEPOSIT() macro wrapper.
 */
extern void UniboCGRSAP_MDEPOSIT(const char* file, int line, void* addr);

extern time_t UniboCGRSAP_get_current_time(UniboCGRSAP* uniboCgrSap);
extern uint64_t UniboCGRSAP_get_local_node(UniboCGRSAP* uniboCgrSap);
extern uint32_t UniboCGRSAP_get_bundle_count(UniboCGRSAP* uniboCgrSap);
extern void UniboCGRSAP_increase_bundle_count(UniboCGRSAP* uniboCgrSap);

extern int UniboCGRSAP_handle_updates(UniboCGRSAP* uniboCgrSap);
/**
 * \brief Wrapper for the BP-defined computeApplicableBacklog() callback.
 *
 * \return int
 * \retval ">= 0" success
 * \retval "< 0"  some error occurred
 */
extern int UniboCGRSAP_compute_applicable_backlog(UniboCGRSAP* uniboCgrSap,
                                                  uint64_t neighbor,
                                                  int priority,
                                                  uint8_t ordinal,
                                                  CgrScalar *applicableBacklog,
                                                  CgrScalar *totalBacklog);

extern bool UniboCGRSAP_check_one_route_per_neighbor(UniboCGRSAP* uniboCgrSap, uint32_t* limit);
extern bool UniboCGRSAP_check_queue_delay(UniboCGRSAP* uniboCgrSap);
extern bool UniboCGRSAP_check_reactive_anti_loop(UniboCGRSAP* uniboCgrSap);
extern bool UniboCGRSAP_check_proactive_anti_loop(UniboCGRSAP* uniboCgrSap);
extern bool UniboCGRSAP_check_moderate_source_routing(UniboCGRSAP* uniboCgrSap);

extern void         UniboCGRSAP_set_PhaseOneSAP(UniboCGRSAP* uniboCgrSap, PhaseOneSAP* phaseOneSap);
extern PhaseOneSAP* UniboCGRSAP_get_PhaseOneSAP(UniboCGRSAP* uniboCgrSap);

extern void         UniboCGRSAP_set_PhaseTwoSAP(UniboCGRSAP* uniboCgrSap, PhaseTwoSAP* phaseTwoSap);
extern PhaseTwoSAP* UniboCGRSAP_get_PhaseTwoSAP(UniboCGRSAP* uniboCgrSap);

extern void           UniboCGRSAP_set_PhaseThreeSAP(UniboCGRSAP* uniboCgrSap, PhaseThreeSAP* phaseThreeSap);
extern PhaseThreeSAP* UniboCGRSAP_get_PhaseThreeSAP(UniboCGRSAP* uniboCgrSap);

extern void                    UniboCGRSAP_set_UniboCgrCurrentCallSAP(UniboCGRSAP* uniboCgrSap, UniboCgrCurrentCallSAP* uniboCgrCurrentCallSap);
extern UniboCgrCurrentCallSAP* UniboCGRSAP_get_UniboCgrCurrentCallSAP(UniboCGRSAP* uniboCgrSap);

extern void    UniboCGRSAP_set_MSRSAP(UniboCGRSAP* uniboCgrSap, MSRSAP* msrSap);
extern MSRSAP* UniboCGRSAP_get_MSRSAP(UniboCGRSAP* uniboCgrSap);

extern void        UniboCGRSAP_set_ContactSAP(UniboCGRSAP* uniboCgrSap, ContactSAP* contactSap);
extern ContactSAP* UniboCGRSAP_get_ContactSAP(UniboCGRSAP* uniboCgrSap);

extern void      UniboCGRSAP_set_RangeSAP(UniboCGRSAP* uniboCgrSap, RangeSAP* rangeSap);
extern RangeSAP* UniboCGRSAP_get_RangeSAP(UniboCGRSAP* uniboCgrSap);

extern void     UniboCGRSAP_set_NodeSAP(UniboCGRSAP* uniboCgrSap, NodeSAP* nodeSap);
extern NodeSAP* UniboCGRSAP_get_NodeSAP(UniboCGRSAP* uniboCgrSap);

extern void    UniboCGRSAP_set_LogSAP(UniboCGRSAP* uniboCgrSap, LogSAP* logSap);
extern LogSAP* UniboCGRSAP_get_LogSAP(UniboCGRSAP* uniboCgrSap);

extern void             UniboCGRSAP_set_TimeAnalysisSAP(UniboCGRSAP* uniboCgrSap, TimeAnalysisSAP* timeAnalysisSap);
extern TimeAnalysisSAP* UniboCGRSAP_get_TimeAnalysisSAP(UniboCGRSAP* uniboCgrSap);

#ifdef __cplusplus
}
#endif

#endif //UNIBO_CGR_UNIBOCGRSAP_H

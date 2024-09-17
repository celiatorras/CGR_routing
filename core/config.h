/** \file config.h
 *
 *  \brief  This file contains the macros that are unrelated to Unibo-CGR features.
 *
 *  \details For ION & DTNME users: since version 2.0 the most important macros have been moved
 *           to feature-config.h (into the BP interface directory).
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

#ifndef UNIBO_CGR_CORE_CONFIG_H_
#define UNIBO_CGR_CORE_CONFIG_H_

#include <stdlib.h>

#define UNIBO_CGR_VERSION_MAJOR 2
#define UNIBO_CGR_VERSION_MINOR 0
#define UNIBO_CGR_VERSION_PATCH 0

/************************************************************************************/
/************************************************************************************/
/******************************** LOGGING AND DEBUGGING *****************************/
/************************************************************************************/
/************************************************************************************/

/*
 * In this section you find some macro to manage the debug print of Unibo-CGR.
 */

#ifndef DEBUG_CGR
/**
 * \brief Boolean: Set to 1 if you want some utility functions for debugging, 0 otherwhise
 *
 * \hideinitializer
 */
#define DEBUG_CGR 0
#endif

#ifndef CGR_DEBUG_FLUSH
/**
 * \brief Boolean: Set to 1 if you want to flush all debug print and log when you are
 *        in a debugging session, set to 0 otherwise.
 *
 * \par Notes:
 *          1. Remember that flushes every print is a bad practice, use it only for (hard) debug.
 *
 * \hideinitializer
 */
#define CGR_DEBUG_FLUSH 0
#endif

/************************************************************************************/
/************************************************************************************/
/************************************************************************************/
/************************************************************************************/
/************************************************************************************/





/************************************************************************************/
/************************************************************************************/
/***************************** COMPUTATIONAL LOAD ANALYSIS **************************/
/************************************************************************************/
/************************************************************************************/

/* TODO does not work well when multiple instances are running (need to create one file for each instance).
 *
 * Enabling one (or more) of the following macros you will find
 * the ./total_<local_node>.csv
 * file where are reported all the times specified by the following macros.
 *
 * If you want to test how long does Unibo-CGR take
 * be sure that in your machine the Linux kernel is running;
 * otherwise do not enable the following macros.
 *
 * \par Notes:
 *      - Note that log and debug prints add considerable time.
 *        Disable them if you want to calculate more precise times.
 *      - To obtain precise results, activate only one of the
 *        following macros at a time, otherwise keep in mind
 *        the overhead added by calculating the innermost times
 *        to the outermost ones.
 */

#ifndef COMPUTE_TOTAL_CORE_TIME
/**
 * \brief Boolean: Set to 1 if you want to know how much time requires
 *        Unibo-CGR's core routing algorithm. O otherwise.
 *
 * \hideinitializer
 */
#define COMPUTE_TOTAL_CORE_TIME 0
#endif

#ifndef COMPUTE_PHASES_TIME
/**
 * \brief Boolean: Set to 1 if you want to know how much time requires
 *        each Unibo-CGR's phase. O otherwise.
 *
 * \hideinitializer
 */
#define COMPUTE_PHASES_TIME 0
#endif

#ifndef COMPUTE_TOTAL_INTERFACE_TIME
/**
 * \brief Exactly like COMPUTE_TOTAL_CORE_TIME, but with this macro you will consider
 *        also the interface's overhead.
 *
 * \hideinitializer
 */
#define COMPUTE_TOTAL_INTERFACE_TIME 0
#endif

#if (COMPUTE_TOTAL_CORE_TIME || COMPUTE_PHASES_TIME || COMPUTE_TOTAL_INTERFACE_TIME)
#undef TIME_ANALYSIS_ENABLED
#define TIME_ANALYSIS_ENABLED 1
#else
#undef TIME_ANALYSIS_ENABLED
#define TIME_ANALYSIS_ENABLED 0
#endif

/************************************************************************************/
/************************************************************************************/
/************************************************************************************/
/************************************************************************************/
/************************************************************************************/





/************************************************************************************/
/************************************************************************************/
/****************************** BUILT-IN CONFIGURATIONS  ****************************/
/************************************************************************************/
/************************************************************************************/

/*
 * In this section you find some macro to enable a preconfigured set of features.
 *
 * Notes:
 *      - The macros in this section are mutually exclusive.
 */

#ifndef CCSDS_SABR_DEFAULTS
/**
 * \brief   Enable to get Unibo-CGR that follows all and only the criteria listed by CCSDS SABR.
 *          Boolean (1 enable, 0 disable).
 *
 * \details Set to 1 to enforce a CCSDS SABR like behavior
 *          (it will disable all Unibo-CGR enhancements when in contrast to CCSDS SABR).
 *          Otherwise set to 0.
 *
 * \hideinitializer
 */
#define CCSDS_SABR_DEFAULTS 0
#endif

#if (CCSDS_SABR_DEFAULTS == 1)
#define PERC_CONVERGENCE_LAYER_OVERHEAD (3.0)
#define MIN_CONVERGENCE_LAYER_OVERHEAD 100
#endif


/************************************************************************************/
/************************************************************************************/
/************************************************************************************/
/************************************************************************************/
/************************************************************************************/


/************************************************************************************/
/************************************************************************************/
/******************************* CCSDS SABR CONSTANTS  ******************************/
/************************************************************************************/
/************************************************************************************/

/*
 * In this section you find some constants defined by SABR.
 *
 */

#ifndef PERC_CONVERGENCE_LAYER_OVERHEAD
/**
 * \brief   Value used to compute the estimated convergence-layer overhead.
 *
 * \details It must be greater or equal to 0.0 .
 *          Keep in mind that ION 3.7.0 use 6.25% as default value.
 *          That is defined 3.00% in CCSDS SABR.
 */
#define PERC_CONVERGENCE_LAYER_OVERHEAD (6.25)
#endif

#ifndef MIN_CONVERGENCE_LAYER_OVERHEAD
/**
 * \brief   Minimum value for the estimated convergence-layer overhead.
 *
 * \details It assumes the same meaning of the macro TIPYCAL_STACK_OVERHEAD in ION's bpP.h.
 *
 * \par Notes:
 *           - CCSDS SABR: 100
 *           - ION 3.7.0:   36 (default value in ION but it can be modified)
 *
 */
#define MIN_CONVERGENCE_LAYER_OVERHEAD 36
#endif

/************************************************************************************/
/************************************************************************************/
/************************************************************************************/
/************************************************************************************/
/************************************************************************************/




/************************************************************************************/
/************************************************************************************/
/******************************* FATAL ERROR SECTION  *******************************/
/************************************************************************************/
/************************************************************************************/
/*
 * This section is used to find errors at compile-time.
 */
/**
 * \cond
 */

#if (DEBUG_CGR != 0 && DEBUG_CGR != 1)
#error DEBUG_CGR must be 0 or 1
#endif

#if (CGR_DEBUG_FLUSH != 0 && CGR_DEBUG_FLUSH != 1)
#error CGR_DEBUG_FLUSH must be 0 or 1
#endif

#if (CCSDS_SABR_DEFAULTS != 0 && CCSDS_SABR_DEFAULTS != 1)
#error CCSDS_SABR_DEFAULTS must be 0 or 1.
#endif

#if (MIN_CONVERGENCE_LAYER_OVERHEAD < 0)
#error MIN_CONVERGENCE_LAYER_OVERHEAD cannot be negative.
#endif
/**
 * \endcond
 */

/************************************************************************************/
/************************************************************************************/
/************************************************************************************/
/************************************************************************************/
/************************************************************************************/

#endif /* UNIBO_CGR_CORE_CONFIG_H_ */

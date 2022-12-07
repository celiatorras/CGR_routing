/** \file log.h
 *
 *  \brief  This file provides the declaration of functions used by
 *          this cgr's implementation to print log files.
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

#ifndef SOURCES_LIBRARY_LOG_H_
#define SOURCES_LIBRARY_LOG_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdarg.h>

#include "../../UniboCGRSAP.h"
#include "../list/list_type.h"
#include "../commonDefines.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern bool LogSAP_is_enabled(UniboCGRSAP *uniboCgrSap);

#define writeLog(uniboCgrSap, f_, ...) \
do {                                   \
    if (LogSAP_is_enabled(uniboCgrSap)) {  \
        LogSAP_writeLog((uniboCgrSap), (f_), ##__VA_ARGS__); \
    }                                 \
} while(0)

#define vwriteLog(uniboCgrSap, f_, args) \
do {                                   \
    if (LogSAP_is_enabled(uniboCgrSap)) {  \
        LogSAP_vwriteLog((uniboCgrSap), (f_), (args)); \
    }                                 \
} while(0)

#define writeLogFlush(uniboCgrSap, f_, ...) \
do {                                   \
    if (LogSAP_is_enabled(uniboCgrSap)) {  \
        LogSAP_writeLogFlush((uniboCgrSap), (f_), ##__VA_ARGS__); \
    }                                 \
} while(0)

#define vwriteLogFlush(uniboCgrSap, f_, args) \
do {                                   \
    if (LogSAP_is_enabled(uniboCgrSap)) {  \
        LogSAP_vwriteLogFlush((uniboCgrSap), (f_), (args)); \
    }                                 \
} while(0)

#define log_fflush(uniboCgrSap) \
do {                                   \
    if (LogSAP_is_enabled(uniboCgrSap)) { \
        LogSAP_log_fflush(uniboCgrSap); \
    }  \
} while(0)

extern int LogSAP_open(UniboCGRSAP *uniboCgrSap);
extern void LogSAP_close(UniboCGRSAP *uniboCgrSap);
extern int LogSAP_enable(UniboCGRSAP *uniboCgrSap, const char* dir_path);
extern void LogSAP_disable(UniboCGRSAP *uniboCgrSap);

extern FILE* openBundleFile(UniboCGRSAP *uniboCgrSap);
extern void closeBundleFile(FILE** file_call);
extern void LogSAP_writeLog(UniboCGRSAP *uniboCgrSap, const char *format, ...);
extern void LogSAP_vwriteLog(UniboCGRSAP* uniboCgrSap, const char* format, va_list args);
extern void LogSAP_writeLogFlush(UniboCGRSAP *uniboCgrSap, const char *format, ...);
extern void LogSAP_vwriteLogFlush(UniboCGRSAP* uniboCgrSap, const char *format, va_list args);
extern void LogSAP_setLogTime(UniboCGRSAP *uniboCgrSap, time_t time);
extern void LogSAP_log_contact_plan(UniboCGRSAP *uniboCgrSap);
extern int print_string(FILE *file, char *string);
extern int print_ull_list(FILE *file, List list, char *brief, char *separator);
extern void LogSAP_log_fflush(UniboCGRSAP *uniboCgrSap);

#if (DEBUG_CGR == 1 && LOG == 1)
/**
 * \brief Print a log in the main log file, compiled only with DEBUG_CGR and LOG enabled.
 *
 * \hideinitializer
 */
#define debugLog(uniboCgrSap, f_, ...) writeLog((uniboCgrSap), (f_), ##__VA_ARGS__)
/**
 * \brief Print a log in the main log file and flush the stream, compiled only with DEBUG_CGR and LOG enabled.
 *
 * \hideinitializer
 */
#define debugLogFlush(uniboCgrSap, f_, ...) writeLogFlush((uniboCgrSap), (f_), ##__VA_ARGS__)
#else
#define debugLog(uniboCgrSap, f_, ...) do {  } while(0)
#define debugLogFlush(uniboCgrSap, f_, ...) do {  } while(0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* SOURCES_LIBRARY_LOG_H_ */

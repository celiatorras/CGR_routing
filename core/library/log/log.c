/** \file log.c
 * 
 *  \brief  This file provides the implementation of a library
 *          to print various log files. Only for Unix-like systems.
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

#include "log.h"

#include <dirent.h>
#include "../../contact_plan/contacts/contacts.h"
#include "../../contact_plan/ranges/ranges.h"

struct LogSAP {
    /**
     * \brief enable/disable logger.
     */
    bool enabled;
	/**
	 * \brief The main log file.
	 */
	FILE *file_log;
	/**
	 * \brief Path of the logs directory.
	 */
	char log_dir[256];
	/**
	 * \brief Length of the log_dir string. (strlen)
	 */
	size_t len_log_dir;
	/**
	 * \brief The time used by the log files.
	 */
	time_t currentTime;
	/**
	 * \brief The last time when the logs have been printed.
	 */
	time_t lastFlushTime;
	/**
	 * \brief The buffer used to print the logs in the main log file.
	 */
	char buffer[256]; //don't touch the size of the buffer
};

static int createLogDir(LogSAP* sap, const char* dir_path);
static int cleanLogDir(LogSAP* sap);
static int openLogFile(LogSAP* sap);
static void closeLogFile(LogSAP* sap);

/******************************************************************************
 *
 * \par Function Name:
 *      LogSAP_open
 *
 * \brief Initialize logger resource. Disabled at default.
 *
 *
 * \par Date Written:
 *      21/10/22
 *
 * \return int
 *
 * \retval  0 success
 * \retval -2 syscall error
 * \retval -3 cannot open log directory
 * \retval -4 cannot open log file
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  21/10/22 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int LogSAP_open(UniboCGRSAP *uniboCgrSap) {
    if (UniboCGRSAP_get_LogSAP(uniboCgrSap)) return 0;

    LogSAP* sap = MWITHDRAW(sizeof(LogSAP));
    if (!sap) {
        return -2;
    }
    memset(sap, 0, sizeof(LogSAP));
    sap->lastFlushTime = -1;
    sap->currentTime = -1;
    UniboCGRSAP_set_LogSAP(uniboCgrSap, sap);
    return 0;
}
bool LogSAP_is_enabled(UniboCGRSAP *uniboCgrSap) {
    return UniboCGRSAP_get_LogSAP(uniboCgrSap)->enabled;
}
/******************************************************************************
 *
 * \par Function Name:
 *      LogSAP_enable
 *
 * \brief Enable logger resource
 *
 *
 * \par Date Written:
 *      21/10/22
 *
 * \param[in] time dir_path  log directory
 *
 * \return int
 *
 * \retval  0 success
 * \retval -2 syscall error
 * \retval -3 cannot open log directory
 * \retval -4 cannot open log file
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  21/10/22 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int LogSAP_enable(UniboCGRSAP *uniboCgrSap, const char* dir_path) {
    LogSAP* sap = UniboCGRSAP_get_LogSAP(uniboCgrSap);
    sap->enabled = true;
    int retval;
    retval = createLogDir(sap, dir_path);
    if (retval < 0) {
        LogSAP_close(uniboCgrSap);
        return -3;
    }
    retval = cleanLogDir(sap);
    if (retval < 0) {
        LogSAP_close(uniboCgrSap);
        return retval;
    }
    retval = openLogFile(sap);
    if (retval < 0) {
        LogSAP_close(uniboCgrSap);
        return -4;
    }
    sap->lastFlushTime = UniboCGRSAP_get_current_time(uniboCgrSap);

    LogSAP_setLogTime(uniboCgrSap, UniboCGRSAP_get_current_time(uniboCgrSap));

    return 0;
}

void LogSAP_disable(UniboCGRSAP *uniboCgrSap) {
    LogSAP* sap = UniboCGRSAP_get_LogSAP(uniboCgrSap);
    if (!sap->enabled) return;
    sap->enabled = false;
    sap->currentTime = -1;
    sap->lastFlushTime = -1;
    closeLogFile(sap);
}

/******************************************************************************
 *
 * \par Function Name:
 *      LogSAP_close
 *
 * \brief Release logger resource
 *
 *
 * \par Date Written:
 *      21/10/22
 *
 * \return void
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  21/10/22 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void LogSAP_close(UniboCGRSAP *uniboCgrSap) {
    LogSAP* sap = UniboCGRSAP_get_LogSAP(uniboCgrSap);
    if (sap) {
        LogSAP_disable(uniboCgrSap);

        memset(sap, 0, sizeof(LogSAP));
        MDEPOSIT(sap);
        UniboCGRSAP_set_LogSAP(uniboCgrSap, NULL);
    }
}

/******************************************************************************
 *
 * \par Function Name:
 *      writeLog
 *
 * \brief Write a log line on the main log file
 *
 *
 * \par Date Written:
 *      24/01/20
 *
 * \return void
 *
 * \param[in]  *format   Use like a printf function
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  24/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void LogSAP_writeLog(UniboCGRSAP* uniboCgrSap, const char *format, ...)
{
	va_list args;
	LogSAP *sap = UniboCGRSAP_get_LogSAP(uniboCgrSap);

	if (sap->file_log != NULL)
	{
		va_start(args, format);
		vwriteLog(uniboCgrSap, format, args);
		va_end(args);
	}
}

/******************************************************************************
 *
 * \par Function Name:
 *      vwriteLog
 *
 * \brief Write a log line on the main log file
 *
 *
 * \par Date Written:
 *      24/01/20
 *
 * \return void
 *
 * \param[in]  *format   Use like a printf function
 * \param[in]  args      Argument list (like vprintf)
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  21/10/22 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void LogSAP_vwriteLog(UniboCGRSAP* uniboCgrSap, const char* format, va_list args)
{
    LogSAP* sap = UniboCGRSAP_get_LogSAP(uniboCgrSap);
    if (!sap->file_log) return;

    fprintf(sap->file_log, "%s", sap->buffer); //[            time]:
    vfprintf(sap->file_log, format, args);
    fputc('\n', sap->file_log);

    debug_fflush(stdout);
}

/******************************************************************************
 *
 * \par Function Name:
 *      writeLogFlush
 *
 * \brief Write a log line on the main log file and flush the output stream
 *
 *
 * \par Date Written:
 *      03/04/20
 *
 * \return void
 *
 * \param[in]  *format   Use like a printf function
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  03/04/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void LogSAP_writeLogFlush(UniboCGRSAP* uniboCgrSap, const char *format, ...)
{
	va_list args;
	LogSAP *sap = UniboCGRSAP_get_LogSAP(uniboCgrSap);

	if (sap->file_log != NULL)
	{
		va_start(args, format);
        vwriteLogFlush(uniboCgrSap, format, args);
		va_end(args);
	}
}

/******************************************************************************
 *
 * \par Function Name:
 *      vwriteLogFlush
 *
 * \brief Write a log line on the main log file and flush the output stream
 *
 *
 * \par Date Written:
 *      21/10/22
 *
 * \return void
 *
 * \param[in]  *format   Use like a printf function
 * \param[in]  args      Argument list (like vprintf())
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  21/10/22 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void LogSAP_vwriteLogFlush(UniboCGRSAP* uniboCgrSap, const char *format, va_list args)
{
    LogSAP *sap = UniboCGRSAP_get_LogSAP(uniboCgrSap);

    if (sap->file_log != NULL)
    {
        vwriteLog(uniboCgrSap, format, args);
        fflush(sap->file_log);
    }
}


/******************************************************************************
 *
 * \par Function Name:
 *      log_fflush
 *
 * \brief Flush the stream
 *
 *
 * \par Date Written:
 *      11/04/20
 *
 * \return void
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  11/04/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void LogSAP_log_fflush(UniboCGRSAP* uniboCgrSap)
{
	LogSAP *sap = UniboCGRSAP_get_LogSAP(uniboCgrSap);
	if(sap->file_log != NULL)
	{
		fflush(sap->file_log);
		sap->lastFlushTime = sap->currentTime;
	}
}

/******************************************************************************
 *
 * \par Function Name:
 *      setLogTime
 *
 * \brief  Set the time that will be printed in the next log lines.
 *
 *
 * \par Date Written:
 *      24/01/20
 *
 * \return void
 *
 * \param[in]  time   	The time that we want to print in the next log lines
 *
 * \par Notes:
 *            1.   Set the first 24 characters of the buffer
 *                 used to print the log (translated: "[    ...xxxxx...]: "
 *                 where ...xxxxx... is the time)
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  24/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void LogSAP_setLogTime(UniboCGRSAP* uniboCgrSap, time_t time)
{
	LogSAP *sap = UniboCGRSAP_get_LogSAP(uniboCgrSap);

    if (!sap->enabled) return;

	if (time != sap->currentTime && time >= 0)
	{
		sap->currentTime = time;
		sprintf(sap->buffer, "[%20ld]: ", (long int) sap->currentTime);
		//set the first 24 characters of the buffer
		if(sap->currentTime - sap->lastFlushTime > 5) // After 5 seconds.
		{
			log_fflush(uniboCgrSap);
		}
	}
}

/******************************************************************************
 *
 * \par Function Name:
 *      print_string
 *
 * \brief  Print a string to the indicated file
 *
 *
 * \par Date Written:
 *      24/01/20
 *
 * \return int
 *
 * \retval   0   Success case
 * \retval  -1   Arguments error
 * \retval  -2   Write error
 *
 * \param[in]  file       The file where we want to print the string
 * \param[in]  *toPrint   The string that we want to print
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  24/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int print_string(FILE *file, char *toPrint)
{
	int result = -1;
	if (file != NULL && toPrint != NULL)
	{
		result = fprintf(file, "%s", toPrint);

		result = (result >= 0) ? 0 : -2;

		debug_fflush(file);
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      createLogDir
 *
 * \brief  Create the main directory where the log files will be populated
 *
 *
 * \par Date Written:
 *      24/01/20
 *
 * \param[in] dir_path /path/to/log/directory/
 *
 * \return int
 *
 * \retval   0   success
 * \retval  -1   Cannot create log directory
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  24/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int createLogDir(LogSAP* sap, const char* dir_path)
{
    memset(sap->log_dir, 0, sizeof(sap->log_dir));
    strncpy(sap->log_dir, dir_path, sizeof(sap->log_dir) - 2U);
    int last_char_pos = ((int) strlen(sap->log_dir)) - 1;
    if (last_char_pos >= 0 && sap->log_dir[last_char_pos] != '/') {
        strcat(sap->log_dir, "/");
    }
    sap->len_log_dir = strlen(sap->log_dir);

    if (mkdir(sap->log_dir, 0777) != 0 && errno != EEXIST) {
        return -1;
    }

    return 0;
}

/******************************************************************************
 *
 * \par Function Name:
 *      openBundleFile
 *
 * \brief  Open the file where the cgr will print the characteristics of the
 *         bundle and all the decisions taken for the forwarding of that bundle
 *
 *
 * \par Date Written:
 *      24/01/20
 *
 * \return FILE*
 * 
 * \retval  FILE*   The file opened
 * \retval  NULL    Error case
 *
 * \param[in]  num  The * in the file's name (see note 2.)
 *
 *       \par Notes:
 *                    1. The file will be opened in write only mode.
 *                    2. Currently the name of the file follows the pattern: call_#*
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  24/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
FILE* openBundleFile(UniboCGRSAP* uniboCgrSap)
{
	FILE *file = NULL;
	LogSAP *sap = UniboCGRSAP_get_LogSAP(uniboCgrSap);

	if (sap->len_log_dir > 0)
	{
		sprintf(sap->log_dir + sap->len_log_dir, "call_#%" PRIu32, UniboCGRSAP_get_bundle_count(uniboCgrSap));
		file = fopen(sap->log_dir, "w");

		sap->log_dir[sap->len_log_dir] = '\0';
	}

	return file;
}

/******************************************************************************
 *
 * \par Function Name:
 *      closeBundleFile
 *
 * \brief  Close the "call file"
 *
 *
 * \par Date Written:
 *      24/01/20
 *
 * \return null
 *
 * \param[in,out]  **file_call  The FILE to close, at the end file_call will points to NULL
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  24/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void closeBundleFile(FILE **file_call)
{
	if (file_call != NULL && *file_call != NULL)
	{
		fflush(*file_call);
		fclose(*file_call);
		*file_call = NULL;
	}
}

static bool starts_with(const char* string, const char* prefix, size_t prefix_len) {
    if (strlen(string) < prefix_len) return false;
    return (0 == strncmp(string, prefix, prefix_len));
}

/******************************************************************************
 *
 * \par Function Name:
 *      cleanLogDir
 *
 * \brief Clean the previously create directory where the log will be printed.
 *
 *
 * \par Date Written:
 *      24/01/20
 *
 * \return int
 *
 * \retval   0   Success case
 * \retval  -2   System call error
 *
 * \par Notes:
 * 			1. The only file that will not be deleted is "log.txt"
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  24/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int cleanLogDir(LogSAP *sap)
{
	DIR *dir;
	struct dirent *file;

    if ((dir = opendir(sap->log_dir)) == NULL) {
        return -2;
    }

    sap->len_log_dir = (int) strlen(sap->log_dir);
    const char* call_prefix = "call_#";
    const size_t call_prefix_length = strlen(call_prefix);
    const char* contact_prefix = "contacts.txt";
    const size_t contact_prefix_length = strlen(contact_prefix);
    const char* range_prefix = "ranges.txt";
    const size_t range_prefix_length = strlen(range_prefix);
    while ((file = readdir(dir)) != NULL)
    {
        if (strcmp(file->d_name, ".") == 0) continue;
        if (strcmp(file->d_name, "..") == 0) continue;
        if (file->d_type != DT_REG) continue;
        // remove the file only if the filename starts with prefix
        if (starts_with(file->d_name, call_prefix, call_prefix_length)
        || starts_with(file->d_name, contact_prefix, contact_prefix_length)
        || starts_with(file->d_name, range_prefix, range_prefix_length)) {
            strcpy(sap->log_dir + sap->len_log_dir, file->d_name);
            // here sap->log_dir is the full path to the file
            remove(sap->log_dir);
            // restore sap->log_dir to full path to directory
            sap->log_dir[sap->len_log_dir] = '\0';
        }
    }

    closedir(dir);
	return 0;
}

/******************************************************************************
 *
 * \par Function Name:
 *      openLogFile
 *
 * \brief Open the main log file
 *
 *
 * \par Date Written:
 *      24/01/20
 *
 * \return int
 * 
 * \retval   1   Success case, main log file opened.
 * \retval   0   If we never called 'createLogDir' or the directory
 *               wasn't created due to an error or the main log file already exists
 * \retval  -1   The file cannot be opened for some reason
 *
 * \par Notes:
 * 			1. The file will be created in write only mode.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  24/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int openLogFile(LogSAP *sap)
{
    size_t len = sap->len_log_dir;
    strcat(sap->log_dir, "log.txt");

    sap->file_log = fopen(sap->log_dir, "w");

    if (sap->file_log == NULL) {
        return -1;
    }

    sap->log_dir[len] = '\0';

	return 0;
}

/******************************************************************************
 *
 * \par Function Name:
 *      closeLogFile
 *
 * \brief  Close the main log file
 *
 *
 * \par Date Written:
 *      24/01/20
 *
 * \return  void
 *
 * \par Notes:
 * 			1. fd_log will be setted to -1.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  24/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void closeLogFile(LogSAP *sap)
{
	if (sap->file_log != NULL)
	{
		fflush(sap->file_log);
		fclose(sap->file_log);
		sap->file_log = NULL;
	}
}

/******************************************************************************
 *
 * \par Function Name:
 *      LogSAP_log_contact_plan
 *
 * \brief  Print all the information to keep trace of the current state of the graphs
 *
 *
 * \par Date Written:
 *      30/01/20
 *
 * \return void
 *
 * \par Notes:
 * 			1.	The contacts graph is printed in append mode in the file "contacts.txt"
 * 			2.	The ranges graph is printed in append mode in the file "ranges.txt"
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  30/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *  21/10/22 | L. Persampieri  |  Renamed function.
 *****************************************************************************/
void LogSAP_log_contact_plan(UniboCGRSAP* uniboCgrSap)
{
	long unsigned int len;
	FILE * file_contacts, *file_ranges;
	LogSAP *sap = UniboCGRSAP_get_LogSAP(uniboCgrSap);

    if (!sap->enabled) return;

    len = sap->len_log_dir;
    sap->log_dir[len] = '\0';
    strcat(sap->log_dir, "contacts.txt");
    if ((file_contacts = fopen(sap->log_dir, "w")) == NULL)
    {
        perror("Error contacts graph's file cannot be opened");
        return;
    }
    sap->log_dir[len] = '\0';
    strcat(sap->log_dir, "ranges.txt");
    if ((file_ranges = fopen(sap->log_dir, "w")) == NULL)
    {
        perror("Error contacts graph's file cannot be opened");
        fclose(file_contacts);
        return;
    }
    sap->log_dir[len] = '\0';

    printContactsGraph(uniboCgrSap, file_contacts);
    printRangesGraph(uniboCgrSap, file_ranges);

    fflush(file_contacts);
    fclose(file_contacts);
    fflush(file_ranges);
    fclose(file_ranges);
    file_contacts = NULL;
    file_ranges = NULL;
}

/******************************************************************************
 *
 * \par Function Name:
 *      print_ull_list
 *
 * \brief  Print a list of unsigned long long element
 *
 *
 * \par Date Written:
 *      29/03/20
 *
 * \return int
 *
 * \retval   0   Success case
 * \retval  -1   Arguments error
 * \retval  -2   Write error
 *
 * \param[in]  file        The file where we want to print the list
 * \param[in]  list        The list to print
 * \param[in]  *brief      A description printed before the list
 * \param[in]  *separator  The separator used between the elements
 *
 * \warning brief must be a well-formed string
 * \warning separator must be a well-formed string
 * \warning All the elements of the list (ListElt->data) have to be != NULL
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  29/03/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int print_ull_list(FILE *file, List list, char *brief, char *separator)
{
	int len, result = -1, temp;
	ListElt *elt;
	if (file != NULL && list != NULL && brief != NULL && separator != NULL)
	{
		result = 0;
		len = fprintf(file, "%s", brief);
		if (len < 0)
		{
			result = -2;
		}
		for (elt = list->first; elt != NULL && result == 0; elt = elt->next)
		{
			temp = fprintf(file, "%llu%s", *((unsigned long long*) elt->data),
					(elt == list->last) ? "" : separator);
			if (temp >= 0)
			{
				len += temp;
			}
			else
			{
				result = -2;
			}
			if (len > 85)
			{
				temp = fputc('\n', file);

				if (temp < 0)
				{
					result = -2;
				}

				len = 0;
			}
		}

		temp = fputc('\n', file);
		if (temp < 0)
		{
			result = -2;
		}

	}

	return result;
}
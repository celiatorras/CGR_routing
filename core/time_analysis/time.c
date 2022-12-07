/** \file time.c
 *
 *  \brief   This file contains the implementation the functions
 *           to perform a Unibo-CGR computational load analysis
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
 *
 *  \par Acknowledgements
 *       Thanks to Federico Marchetti for providing me with
 *       a draft of this code.
 */


#include "time.h"

#if (TIME_ANALYSIS_ENABLED)
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include "../UniboCGRSAP.h"
#include "../cgr/cgr_phases.h"

typedef struct
{
	int beginOk;
	struct timespec beginTime;
	int endOk;
	struct timespec endTime;
} TimeRecorded;

typedef struct
{
	int configured;
	int fd;
} TimeFile;

typedef struct
{
	TimeRecorded phase_time;
	uint64_t call_counter;
	uint64_t timer;
} PhaseTimeLogger;

typedef struct
{
	PhaseTimeLogger phase_time_logger[3];
} PhasesTime;

typedef struct
{
    TimeRecorded total_time;
    uint64_t timer;
} TotalTime;

struct TimeAnalysisSAP {
    PhasesTime phasesTime;
    TimeFile timeFile;
    TotalTime totalCoreTime;
    TotalTime totalInterfaceTime;
};

static TimeFile * get_time_file(UniboCGRSAP* uniboCgrSap)
{
    return &UniboCGRSAP_get_TimeAnalysisSAP(uniboCgrSap)->timeFile;
}

#if (COMPUTE_PHASES_TIME)

/******************** PHASES TIME SECTION ********************/

/******************************************************************************
 *
 * \par Function Name:
 *      get_phases_time
 *
 * \brief  Get the PhasesTime struct.
 *
 *
 * \par Date Written:
 *      20/12/20
 *
 * \return PhasesTime*
 *
 * \retval PhasesTime*  The reference to PhasesTime struct.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  20/12/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static PhasesTime *get_phases_time(UniboCGRSAP* uniboCgrSap)
{
	return &UniboCGRSAP_get_TimeAnalysisSAP(uniboCgrSap)->phasesTime;
}

/******************************************************************************
 *
 * \par Function Name:
 *      record_phases_start_time
 *
 * \brief  Save the begin time of the CGR's phases passed as argument.
 *
 *
 * \par Date Written:
 *      20/12/20
 *
 * \return void
 *
 * \param[in]  phase  The CGR's phase
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  20/12/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void record_phases_start_time(UniboCGRSAP* uniboCgrSap, UniboCgrPhase phase)
{
	PhasesTime *phasesTime = get_phases_time(uniboCgrSap);
	int found = 0;
	int phaseSelected = 0;

	if(phase == phaseOne)
	{
		phaseSelected = 0; // phase one
		found = 1;
	}
	else if(phase == phaseTwo)
	{
		phaseSelected = 1; // phase two
		found = 1;
	}
	else if(phase == phaseThree)
	{
		phaseSelected = 2; // phase three
		found = 1;
	}
	else
	{
		// nothing to do
		found = 0;
	}

	if (found)
	{
		phasesTime->phase_time_logger[phaseSelected].phase_time.beginOk =
				clock_gettime(CLOCK_REALTIME, &(phasesTime->phase_time_logger[phaseSelected].phase_time.beginTime));
	}
}

/******************************************************************************
 *
 * \par Function Name:
 *      record_phases_stop_time
 *
 * \brief  Save the end time of the CGR's phases passed as argument.
 *
 *
 * \par Date Written:
 *      20/12/20
 *
 * \return void
 *
 * \param[in]  phase  The CGR's phase
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  20/12/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void record_phases_stop_time(UniboCGRSAP* uniboCgrSap, UniboCgrPhase phase)
{
	PhasesTime *phasesTime;
	int endOk;
	int found;
	UniboCgrPhase phaseSelected;
	struct timespec endTime;

	endOk = clock_gettime(CLOCK_REALTIME, &endTime);

	if(endOk != 0)
	{
		// some error
		return;
	}

	found = 0;
	phaseSelected = 0;

	phasesTime = get_phases_time(uniboCgrSap);

	if(phase == phaseOne)
	{
		phaseSelected = 0; // phase one
		found = 1;
	}
	else if(phase == phaseTwo)
	{
		phaseSelected = 1; // phase two
		found = 1;
	}
	else if(phase == phaseThree)
	{
		phaseSelected = 2; // phase three
		found = 1;
	}
	else
	{
		// nothing to do
		found = 0;
	}

	if(found)
	{
		if(phasesTime->phase_time_logger[phaseSelected].phase_time.beginOk == 0)
		{
			phasesTime->phase_time_logger[phaseSelected].timer +=
					(endTime.tv_sec
							- phasesTime->phase_time_logger[phaseSelected].phase_time.beginTime.tv_sec)
							* 1000000000 + endTime.tv_nsec
							- phasesTime->phase_time_logger[phaseSelected].phase_time.beginTime.tv_nsec;

			phasesTime->phase_time_logger[phaseSelected].call_counter += 1;
		}

	}

}

/*************************************************************/

#endif

#if (COMPUTE_TOTAL_CORE_TIME || COMPUTE_TOTAL_INTERFACE_TIME)

#if (COMPUTE_TOTAL_CORE_TIME)

/******************** TOTAL CORE TIME SECTION ********************/

/******************************************************************************
 *
 * \par Function Name:
 *      get_total_core_time
 *
 * \brief  Get the TotalTime struct (for core).
 *
 *
 * \par Date Written:
 *      20/12/20
 *
 * \return TotalTime*
 *
 * \retval TotalTime*  The reference to TotalTime struct.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  20/12/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static TotalTime *get_total_core_time(UniboCGRSAP* uniboCgrSap)
{
	return &UniboCGRSAP_get_TimeAnalysisSAP(uniboCgrSap)->totalCoreTime;
}

/******************************************************************************
 *
 * \par Function Name:
 *      record_total_core_start_time
 *
 * \brief  Save the begin time of the Unibo-CGR's core (1 bundle routing).
 *
 *
 * \par Date Written:
 *      20/12/20
 *
 * \return void
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  20/12/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void record_total_core_start_time(UniboCGRSAP* uniboCgrSap)
{
	TotalTime *totalTime = get_total_core_time(uniboCgrSap);

	totalTime->total_time.beginOk = clock_gettime(CLOCK_REALTIME, &(totalTime->total_time.beginTime));
}

/******************************************************************************
 *
 * \par Function Name:
 *      record_total_core_stop_time
 *
 * \brief  Save the end time of the Unibo-CGR's core (1 bundle routing).
 *
 *
 * \par Date Written:
 *      20/12/20
 *
 * \return void
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  20/12/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void record_total_core_stop_time(UniboCGRSAP* uniboCgrSap)
{
	TotalTime *totalTime;
	int endOk;
	struct timespec endTime;

	endOk = clock_gettime(CLOCK_REALTIME, &endTime);

	if(endOk != 0)
	{
		// some error
		return;
	}

	totalTime = get_total_core_time(uniboCgrSap);

	totalTime->timer = (endTime.tv_sec - totalTime->total_time.beginTime.tv_sec) * 1000000000 + endTime.tv_nsec - totalTime->total_time.beginTime.tv_nsec;
}

/************************************************************/

#endif

#if (COMPUTE_TOTAL_INTERFACE_TIME)

/******************** TOTAL INTERFACE TIME SECTION ********************/

/******************************************************************************
 *
 * \par Function Name:
 *      get_total_interface_time
 *
 * \brief  Get the TotalTime struct (for interface).
 *
 *
 * \par Date Written:
 *      20/12/20
 *
 * \return TotalTime*
 *
 * \retval TotalTime*  The reference to TotalTime struct.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  20/12/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static TotalTime *get_total_interface_time(UniboCGRSAP* uniboCgrSap)
{
	return &UniboCGRSAP_get_TimeAnalysisSAP(uniboCgrSap)->totalInterfaceTime;
}

/******************************************************************************
 *
 * \par Function Name:
 *      record_total_interface_start_time
 *
 * \brief  Save the begin time of the Unibo-CGR's interface (1 bundle routing).
 *
 *
 * \par Date Written:
 *      20/12/20
 *
 * \return void
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  20/12/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void record_total_interface_start_time(UniboCGRSAP* uniboCgrSap)
{
	TotalTime *totalTime = get_total_interface_time(uniboCgrSap);

	totalTime->total_time.beginOk = clock_gettime(CLOCK_REALTIME, &(totalTime->total_time.beginTime));
}

/******************************************************************************
 *
 * \par Function Name:
 *      record_total_interface_stop_time
 *
 * \brief  Save the end time of the Unibo-CGR's interface (1 bundle routing).
 *
 *
 * \par Date Written:
 *      20/12/20
 *
 * \return void
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  20/12/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void record_total_interface_stop_time(UniboCGRSAP* uniboCgrSap)
{
	TotalTime *totalTime;
	int endOk;
	struct timespec endTime;

	endOk = clock_gettime(CLOCK_REALTIME, &endTime);

	if(endOk != 0)
	{
		// some error
		return;
	}

	totalTime = get_total_interface_time(uniboCgrSap);

	totalTime->timer = (endTime.tv_sec - totalTime->total_time.beginTime.tv_sec) * 1000000000 + endTime.tv_nsec - totalTime->total_time.beginTime.tv_nsec;

}

/************************************************************/

#endif

#endif

/******************************************************************************
 *
 * \par Function Name:
 *      print_time_results
 *
 * \brief  Print on file total_<local_node_number>.csv the recorded times.
 *
 *
 * \par Date Written:
 *      20/12/20
 *
 * \return void
 *
 * \param[in]  currentTime  The Unibo-CGR's internal current time
 * \param[in]  callNumber   The progressive Unibo-CGR's core call number
 * \param[in]  *id          The bundle's ID
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  20/12/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void print_time_results(UniboCGRSAP* uniboCgrSap, time_t currentTime, unsigned int callNumber, CgrBundleID *id)
{
	TimeFile *timeFile = get_time_file(uniboCgrSap);
#if (COMPUTE_PHASES_TIME)
	PhasesTime *phasesTime = get_phases_time(uniboCgrSap);
#endif
#if (COMPUTE_TOTAL_CORE_TIME)
	TotalTime *totalCoreTime = get_total_core_time(uniboCgrSap);
#endif
#if (COMPUTE_TOTAL_INTERFACE_TIME)
	TotalTime *totalInterfaceTime = get_total_interface_time(uniboCgrSap);
#endif
	char row[256];
	PhasesTime copiedPhasesTime;
	int i;
	uint64_t totalCore = 0;
	uint64_t totalInterface = 0;
	int written;
	CgrBundleID tempId;

	if(id == NULL)
	{
		memset(&tempId, 0, sizeof(CgrBundleID));
		id = &tempId;
	}

#if (COMPUTE_TOTAL_CORE_TIME)
	totalCore = totalCoreTime->timer;
	totalCoreTime->timer = 0;
#endif
#if (COMPUTE_TOTAL_INTERFACE_TIME)
	totalInterface = totalInterfaceTime->timer;
	totalInterfaceTime->timer = 0;
#endif

#if (COMPUTE_PHASES_TIME)
	for(i = 0; i < 3; i++)
	{
		copiedPhasesTime.phase_time_logger[i].timer = phasesTime->phase_time_logger[i].timer;
		copiedPhasesTime.phase_time_logger[i].call_counter = phasesTime->phase_time_logger[i].call_counter;

		phasesTime->phase_time_logger[i].timer = 0;
		phasesTime->phase_time_logger[i].call_counter = 0;
	}
#else
	for(i = 0; i < 3; i++)
	{
		copiedPhasesTime.phase_time_logger[i].timer = 0;
		copiedPhasesTime.phase_time_logger[i].call_counter = 0;
	}
#endif

	written = sprintf(row, "%" PRIu64 ",%ld,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
                   UniboCGRSAP_get_local_node(uniboCgrSap), (long int) currentTime, callNumber,
                   id->source_node, id->creation_timestamp, id->sequence_number, id->fragment_length,
                   id->fragment_offset, totalInterface, totalCore,
                   copiedPhasesTime.phase_time_logger[0].timer,
                   copiedPhasesTime.phase_time_logger[0].call_counter,
                   copiedPhasesTime.phase_time_logger[1].timer,
                   copiedPhasesTime.phase_time_logger[1].call_counter,
                   copiedPhasesTime.phase_time_logger[2].timer,
                   copiedPhasesTime.phase_time_logger[2].call_counter);

	if(written > 0 && timeFile->configured && timeFile->fd >= 0)
	{
		if (write(timeFile->fd, row, written * sizeof(char)) < 0)
		{
			verbose_debug_printf("Error during value reporting\n");
		}
	}
}

/******************************************************************************
 *
 * \par Function Name:
 *      TimeAnalysisSAP_open
 *
 * \brief  Initialize files and data for the computational load analysis.
 *
 *
 * \par Date Written:
 *      20/12/20
 *
 * \return int
 *
 * \retval  0 success
 * \retval -2 MWITHDRAW error
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  20/12/20 | L. Persampieri  |  Initial Implementation and documentation.
 *  21/10/22 | L. Persampieri  |  Renamed function.
 *****************************************************************************/
int TimeAnalysisSAP_open(UniboCGRSAP* uniboCgrSap)
{
    if (UniboCGRSAP_get_TimeAnalysisSAP(uniboCgrSap)) return 0;
    TimeAnalysisSAP* sap = MWITHDRAW(sizeof(TimeAnalysisSAP));
    if (!sap) { return -2; }
    UniboCGRSAP_set_TimeAnalysisSAP(uniboCgrSap, sap);
    memset(sap, 0, sizeof(TimeAnalysisSAP));
	TimeFile *timeFile = get_time_file(uniboCgrSap);
	uint64_t node_id = UniboCGRSAP_get_local_node(uniboCgrSap);
	char file_name[50];
	char *header = "local_node,current_time,call_num,src,ts,sqn_num,fragL,fragO,total_interface,total_core,ph_1_time,ph_1_calls,ph_2_time,ph_2_calls,ph_3_time,ph_3_calls\n";

	if (timeFile->configured == 0 || timeFile->fd < 0)
	{
		sprintf(file_name, "total_%" PRIu64 ".csv", node_id);
		timeFile->fd = open(file_name, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

		if (timeFile->fd < 0)
		{
			debug_printf("Error during phase time file creation");
		}
		else if (write(timeFile->fd, header, strlen(header) * sizeof(char)) < 0)
		{
			debug_printf("Error during phase header writing\n");
			close(timeFile->fd);
		}
		else
		{
			timeFile->configured = 1;
		}
	}

    return 0;
}

/******************************************************************************
 *
 * \par Function Name:
 *      TimeAnalysisSAP_close
 *
 * \brief  Close files and clear data.
 *
 *
 * \par Date Written:
 *      20/12/20
 *
 * \return void
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  20/12/20 | L. Persampieri  |  Initial Implementation and documentation.
 *  21/10/22 | L. Persampieri  |  Renamed function.
 *****************************************************************************/
void TimeAnalysisSAP_close(UniboCGRSAP* uniboCgrSap)
{
    if (!UniboCGRSAP_get_TimeAnalysisSAP(uniboCgrSap)) return;

	TimeFile *timeFile = get_time_file(uniboCgrSap);

	if(timeFile->configured && timeFile->fd >= 0)
	{
		close(timeFile->fd);
	}

    memset(UniboCGRSAP_get_TimeAnalysisSAP(uniboCgrSap), 0, sizeof(TimeAnalysisSAP));
    MDEPOSIT(UniboCGRSAP_get_TimeAnalysisSAP(uniboCgrSap));
    UniboCGRSAP_set_TimeAnalysisSAP(uniboCgrSap, NULL);
}

#endif

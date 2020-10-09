/*
 *	bpextensions.c:	Bundle Protocol extension definitions
 *			module.
 *
 *	Copyright (c) 2008, California Institute of Technology.
 *	ALL RIGHTS RESERVED.  U.S. Government Sponsorship
 *	acknowledged.
 *
 *	Author: Scott Burleigh, JPL
 */

/*	Add external function declarations between here...		*/

#include "pnb.h"
#include "bpq.h"
#include "meb.h"
#include "bae.h"
#include "hcb.h"
#include "snw.h"
#include "rgr.h"
#include "cgrr.h"
#if defined(BPSEC)
#include "bib.h"
#include "bcb.h"
#endif

/*	... and here.							*/

static ExtensionDef	extensionDefs[] =
			{
		{ "pnb", PreviousNodeBlk,
				pnb_offer,
				{pnb_processOnFwd,
				pnb_processOnAccept,
				pnb_processOnEnqueue,
				pnb_processOnDequeue,
				0},
				pnb_release,
				pnb_copy,
				0,
				0,
				0,
				pnb_parse,
				pnb_check,
				pnb_record,
				pnb_clear
		},
#if defined(BPSEC)
		{ "bcb", BlockConfidentialityBlk,
				bcbOffer,
				{0,
				0,
				0,
				bcbProcessOnDequeue,
				0},
				bcbRelease,
				bcbCopy,
				bcbAcquire,
				bcbReview,
				bcbDecrypt,
				0,
				0,
                                bcbRecord,
				bcbClear
		},
		{ "bib", BlockIntegrityBlk,
				bibOffer,
				{0,
				0,
				0,
				0,
				0},
				bibRelease,
				bibCopy,
				0,
				bibReview,
				0,
				bibParse,
				bibCheck,
				bibRecord,
				bibClear
		},
#endif
		{ "bpq", QualityOfServiceBlk,
				qos_offer,
				{qos_processOnFwd,
				qos_processOnAccept,
				qos_processOnEnqueue,
				qos_processOnDequeue,
				0},
				qos_release,
				qos_copy,
				0,
				0,
				0,
				qos_parse,
				qos_check,
				qos_record,
				qos_clear
		},
		{ "meb", MetadataBlk,
				meb_offer,
				{meb_processOnFwd,
				meb_processOnAccept,
				meb_processOnEnqueue,
				meb_processOnDequeue,
				0},
				meb_release,
				meb_copy,
				meb_acquire,
				0,
				0,
				0,
				meb_check,
				meb_record,
				meb_clear
		},
		{ "bae", BundleAgeBlk,
				bae_offer,
				{bae_processOnFwd,
				bae_processOnAccept,
				bae_processOnEnqueue,
				bae_processOnDequeue,
				0},
				bae_release,
				bae_copy,
				0,
				0,
				0,
				bae_parse,
				bae_check,
				bae_record,
				bae_clear
		},
		{ "hcb", HopCountBlk,
				hcb_offer,
				{hcb_processOnFwd,
				hcb_processOnAccept,
				hcb_processOnEnqueue,
				hcb_processOnDequeue,
				0},
				hcb_release,
				hcb_copy,
				0,
				0,
				0,
				hcb_parse,
				hcb_check,
				hcb_record,
				hcb_clear
		},
		{ "snw", SnwPermitsBlk,
				snw_offer,
				{snw_processOnFwd,
				snw_processOnAccept,
				snw_processOnEnqueue,
				snw_processOnDequeue,
				0},
				snw_release,
				snw_copy,
				0,
				0,
				0,
				snw_parse,
				snw_check,
				snw_record,
				snw_clear
		},
#if RGREB
		{ "rgr", RGRBlk,
				rgr_offer,
				{rgr_processOnFwd,
				 rgr_processOnAccept,
				 rgr_processOnEnqueue,
				 rgr_processOnDequeue,
				 0},
				 rgr_release,
				 rgr_copy,
				 rgr_acquire,
				 0,
				 0,
				 rgr_parse,
				 rgr_check,
				 rgr_record,
				 rgr_clear
		},
#endif
#if CGRREB
		{ "cgrr", CGRRBlk,
				cgrr_offer,
				{0,
				0,
				0,
				0,
				0},
				cgrr_release,
				cgrr_copy,
				cgrr_acquire,
				0,
				0,
				0,
				0,
				cgrr_record,
				cgrr_clear
		},
#endif
				{ "unknown",-1,0,{0,0,0,0,0},0,0,0,0,0,0,0,0,0 }
			};

/*	NOTE: the order of appearance of extension definitions in the
 *	baseline extensionSpecs array determines the order in which
 *	these extension blocks will be inserted into locally sourced
 *	bundles between the primary block and the payload block.	*/

static ExtensionSpec	extensionSpecs[] =
			{
				{ PreviousNodeBlk, 0, 0, 0, 0 },
				{ QualityOfServiceBlk, 0, 0, 0, 0 },
				{ BundleAgeBlk, 0, 0, 0, 0 },
				{ SnwPermitsBlk, 0, 0, 0, 0 },
#if RGREB
				{ RGRBlk, 0, 0, 0, 0 },
#endif
#if CGRREB
				{ CGRRBlk, 0, 0, 0, 0 },
#endif
#if defined(BPSEC)
				{ BlockIntegrityBlk, 0, 0, 0, 0 },
				{ BlockConfidentialityBlk, 1, 0, 0, 0 },
#endif
				{ UnknownBlk, 0, 0, 0, 0 }
			};

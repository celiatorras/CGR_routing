/***************************************************************
 * \file scalar.h
 *
 * \brief  This file defines the type CgrScalar and some functions
 *         to manage it.
 *
 * \par Ported from ION 3.7.0 by
 *      Lorenzo Persampieri, lorenzo.persampieri@studio.unibo.it
 * 
 * \par Supervisor
 *      Carlo Caini, carlo.caini@unibo.it
 *
 ***************************************************************/

#ifndef LIBRARY_CGR_SCALAR
#define LIBRARY_CGR_SCALAR

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define	ONE_GIG			(1 << 30)

typedef struct
{
    int64_t gigs;
    int64_t units;
} CgrScalar;

extern void loadCgrScalar(CgrScalar*, int64_t);
extern void increaseCgrScalar(CgrScalar*, int64_t);
extern void reduceCgrScalar(CgrScalar*, int64_t);
extern void multiplyCgrScalar(CgrScalar*, int64_t);
extern void divideCgrScalar(CgrScalar*, int64_t);
extern void copyCgrScalar(CgrScalar *to, CgrScalar *from);
extern void addToCgrScalar(CgrScalar*, CgrScalar*);
extern void subtractFromCgrScalar(CgrScalar*, CgrScalar*);
extern int CgrScalarIsValid(CgrScalar*);

#ifdef __cplusplus
}
#endif

#endif

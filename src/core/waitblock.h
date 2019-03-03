//
// Copyright 2019 Staysail Systems, Inc. <info@staysail.tech>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#ifndef CORE_WAITBLOCK_H
#define CORE_WAITBLOCK_H

#include "core/nng_impl.h"

// nni_waitblock is just a simple wait point.  Useful for blocking system
// calls, for example.
typedef struct nni_waitblock {
	nni_mtx wb_mtx;
	nni_cv  wb_cv;
	bool    wb_done;
} nni_waitblock;

extern void nni_waitblock_init(nni_waitblock *);
extern void nni_waitblock_fini(nni_waitblock *);
extern void nni_waitblock_wait(nni_waitblock *);
extern void nni_waitblock_done(nni_waitblock *);

#endif // CORE_WAITBLOCK_H

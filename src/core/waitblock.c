//
// Copyright 2019 Staysail Systems, Inc. <info@staysail.tech>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#include "waitblock.h"

void
nni_waitblock_init(nni_waitblock *wb)
{
	nni_mtx_init(&wb->wb_mtx);
	nni_cv_init(&wb->wb_cv, &wb->wb_mtx);
	wb->wb_done = false;
}

void
nni_waitblock_fini(nni_waitblock *wb)
{
	nni_cv_fini(&wb->wb_cv);
	nni_mtx_fini(&wb->wb_mtx);
}

void
nni_waitblock_reset(nni_waitblock *wb)
{
	nni_mtx_lock(&wb->wb_mtx);
	wb->wb_done = false;
	nni_mtx_unlock(&wb->wb_mtx);
}

void
nni_waitblock_wait(nni_waitblock *wb)
{
	nni_mtx_lock(&wb->wb_mtx);
	while (!wb->wb_done) {
		nni_cv_wait(&wb->wb_cv);
	}
	nni_mtx_unlock(&wb->wb_mtx);
}

void
nni_waitblock_done(nni_waitblock *wb)
{
	nni_mtx_lock(&wb->wb_mtx);
	wb->wb_done = true;
	nni_cv_wake(&wb->wb_cv);
	nni_mtx_unlock(&wb->wb_mtx);
}

//
// Copyright 2018 Staysail Systems, Inc. <info@staysail.tech>
// Copyright 2018 Capitar IT Group BV <info@capitar.com>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nng/nng.h>
#include <nng/supplemental/util/platform.h>

#if defined(NNG_HAVE_PAIR1)
#include <nng/protocol/pair1/pair.h>

#elif defined(NNG_HAVE_PAIR0)
#include <nng/protocol/pair0/pair.h>

#else

static void die(const char *, ...);

static int
nng_pair_open(nng_socket *arg)
{
	(void) arg;
	die("No pair protocol enabled in this build!");
	return (NNG_ENOTSUP);
}
#endif // NNG_ENABLE_PAIR

static void latency_client(const char *, size_t, int);
static void latency_server(const char *, size_t, int);
static void throughput_client(const char *, size_t, int);
static void throughput_server(const char *, size_t, int);
static void do_remote_lat(int argc, char **argv);
static void do_local_lat(int argc, char **argv);
static void do_remote_thr(int argc, char **argv);
static void do_local_thr(int argc, char **argv);
static void do_inproc_thr(int argc, char **argv);
static void do_inproc_lat(int argc, char **argv);
static void die(const char *, ...);

// perf implements the same performance tests found in the standard
// nanomsg & mangos performance tests.  As with mangos, the decision
// about which test to run is determined by the program name (ARGV[0}])
// that it is run under.
//
// Options are:
//
// - remote_lat - remote latency side (client, aka latency_client)
// - local_lat  - local latency side (server, aka latency_server)
// - local_thr  - local throughput side
// - remote_thr - remote throughput side
// - inproc_lat - inproc latency
// - inproc_thr - inproc throughput
//

bool
matches(const char *arg, const char *name)
{
	const char *ptr = arg;
	const char *x;

	while (((x = strchr(ptr, '/')) != NULL) ||
	    ((x = strchr(ptr, '\\')) != NULL) ||
	    ((x = strchr(ptr, ':')) != NULL)) {
		ptr = x + 1;
	}
	for (;;) {
		if (*name == '\0') {
			break;
		}
		if (tolower(*ptr) != *name) {
			return (false);
		}
		ptr++;
		name++;
	}

	switch (*ptr) {
	case '\0':
		return (true);
	case '.': // extension; ignore it.
		return (true);
	default: // some other trailing bit.
		return (false);
	}
}

int
main(int argc, char **argv)
{
	char *prog;

	// Allow -m <remote_lat> or whatever to override argv[0].
	if ((argc >= 3) && (strcmp(argv[1], "-m") == 0)) {
		prog = argv[2];
		argv += 3;
		argc -= 3;
	} else {
		prog = argv[0];
		argc--;
		argv++;
	}
	if (matches(prog, "remote_lat") || matches(prog, "latency_client")) {
		do_remote_lat(argc, argv);
	} else if (matches(prog, "local_lat") ||
	    matches(prog, "latency_server")) {
		do_local_lat(argc, argv);
	} else if (matches(prog, "local_thr") ||
	    matches(prog, "throughput_server")) {
		do_local_thr(argc, argv);
	} else if (matches(prog, "remote_thr") ||
	    matches(prog, "throughput_client")) {
		do_remote_thr(argc, argv);
	} else if (matches(prog, "inproc_thr")) {
		do_inproc_thr(argc, argv);
	} else if (matches(prog, "inproc_lat")) {
		do_inproc_lat(argc, argv);
	} else {
		die("Unknown program mode? Use -m <mode>.");
	}
}

int
nop(void)
{
	return (0);
}

static void
die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	exit(2);
}

static int
parse_int(const char *arg, const char *what)
{
	long  val;
	char *eptr;

	val = strtol(arg, &eptr, 10);
	// Must be a postive number less than around a billion.
	if ((val < 0) || (val > (1 << 30)) || (*eptr != 0) || (eptr == arg)) {
		die("Invalid %s", what);
	}
	return ((int) val);
}

void
do_local_lat(int argc, char **argv)
{
	long int msgsize;
	long int trips;

	if (argc != 3) {
		die("Usage: local_lat <listen-addr> <msg-size> <roundtrips>");
	}

	msgsize = parse_int(argv[1], "message size");
	trips   = parse_int(argv[2], "round-trips");

	latency_server(argv[0], msgsize, trips);
}

void
do_remote_lat(int argc, char **argv)
{
	int msgsize;
	int trips;

	if (argc != 3) {
		die("Usage: remote_lat <connect-to> <msg-size> <roundtrips>");
	}

	msgsize = parse_int(argv[1], "message size");
	trips   = parse_int(argv[2], "round-trips");

	latency_client(argv[0], msgsize, trips);
}

void
do_local_thr(int argc, char **argv)
{
	int msgsize;
	int trips;

	if (argc != 3) {
		die("Usage: local_thr <listen-addr> <msg-size> <count>");
	}

	msgsize = parse_int(argv[1], "message size");
	trips   = parse_int(argv[2], "count");

	throughput_server(argv[0], msgsize, trips);
}

void
do_remote_thr(int argc, char **argv)
{
	int msgsize;
	int trips;

	if (argc != 3) {
		die("Usage: remote_thr <connect-to> <msg-size> <count>");
	}

	msgsize = parse_int(argv[1], "message size");
	trips   = parse_int(argv[2], "count");

	throughput_client(argv[0], msgsize, trips);
}

struct inproc_args {
	int         count;
	int         msgsize;
	const char *addr;
	void (*func)(const char *, size_t, int);
};

static void
do_inproc(void *args)
{
	struct inproc_args *ia = args;

	ia->func(ia->addr, ia->msgsize, ia->count);
}

void
do_inproc_lat(int argc, char **argv)
{
	nng_thread *       thr;
	struct inproc_args ia;
	int                rv;

	if (argc != 2) {
		die("Usage: inproc_lat <msg-size> <count>");
	}

	ia.addr    = "inproc://latency_test";
	ia.msgsize = parse_int(argv[0], "message size");
	ia.count   = parse_int(argv[1], "count");
	ia.func    = latency_server;

	if ((rv = nng_thread_create(&thr, do_inproc, &ia)) != 0) {
		die("Cannot create thread: %s", nng_strerror(rv));
	}

	// Sleep a bit.
	nng_msleep(100);

	latency_client("inproc://latency_test", ia.msgsize, ia.count);
	nng_thread_destroy(thr);
}

void
do_inproc_thr(int argc, char **argv)
{
	nng_thread *       thr;
	struct inproc_args ia;
	int                rv;

	if (argc != 2) {
		die("Usage: inproc_thr <msg-size> <count>");
	}

	ia.addr    = "inproc://tput_test";
	ia.msgsize = parse_int(argv[0], "message size");
	ia.count   = parse_int(argv[1], "count");
	ia.func    = throughput_server;

	if ((rv = nng_thread_create(&thr, do_inproc, &ia)) != 0) {
		die("Cannot create thread: %s", nng_strerror(rv));
	}

	// Sleep a bit.
	nng_msleep(100);

	throughput_client("inproc://tput_test", ia.msgsize, ia.count);
	nng_thread_destroy(thr);
}

typedef struct {
	nng_socket s;
	int        trips;
	nng_aio *  rxaio;
	nng_aio *  txaio;
	size_t     msgsize;
	bool       done;
	nng_mtx *  mtx;
	nng_cv *   cv;
} latency_client_data;

void
latency_client_tx_cb(void *arg)
{
	latency_client_data *d   = arg;
	nng_aio *            aio = d->txaio;
	int                  rv;

	if ((rv = nng_aio_result(aio)) != 0) {
		die("sendmsg: %s", nng_strerror(rv));
	}
	nng_aio_set_msg(aio, NULL);
	nng_recv_aio(d->s, d->rxaio);
}

void
latency_client_rx_cb(void *arg)
{
	latency_client_data *d   = arg;
	nng_aio *            aio = d->rxaio;
	int                  rv;
	nng_msg *            msg;

	if ((rv = nng_aio_result(aio)) != 0) {
		die("recvmsg: %s", nng_strerror(rv));
	}
	msg = nng_aio_get_msg(aio);
	if (nng_msg_len(msg) != d->msgsize) {
		die("bad message ssize");
	}
	// Send the reply; we just bounce back the same message.
	d->trips--;
	if (d->trips == 0) {
		nng_mtx_lock(d->mtx);
		d->done = true;
		nng_cv_wake(d->cv);
		nng_mtx_unlock(d->mtx);
		nng_msg_free(msg);
		return;
	}
	nng_aio_set_msg(d->rxaio, NULL);
	nng_aio_set_msg(d->txaio, msg);
	nng_send_aio(d->s, d->txaio);
}

void
latency_client(const char *addr, size_t msgsize, int trips)
{
	nng_msg *msg;
	nng_time start, end;
	int      rv;
	float    total;
	float    latency;

	latency_client_data d;

	memset(&d, 0, sizeof(d));
	if (((rv = nng_mtx_alloc(&d.mtx)) != 0) ||
	    ((rv = nng_cv_alloc(&d.cv, d.mtx)) != 0) ||
	    ((rv = nng_aio_alloc(&d.txaio, latency_client_tx_cb, &d)) != 0) ||
	    ((rv = nng_aio_alloc(&d.rxaio, latency_client_rx_cb, &d)) != 0)) {
		die("failed initializing: %s", nng_strerror(rv));
	}

	if ((rv = nng_pair_open(&d.s)) != 0) {
		die("nng_socket: %s", nng_strerror(rv));
	}

	// XXX: set no delay
	// XXX: other options (TLS in the future?, Linger?)

	if ((rv = nng_dial(d.s, addr, NULL, 0)) != 0) {
		die("nng_dial: %s", nng_strerror(rv));
	}

	if (nng_msg_alloc(&msg, msgsize) != 0) {
		die("nng_msg_alloc: %s", nng_strerror(rv));
	}
	d.msgsize = msgsize;
	d.trips   = trips;

	start = nng_clock();
	nng_aio_set_msg(d.txaio, msg);
	nng_send_aio(d.s, d.txaio);

	nng_mtx_lock(d.mtx);
	while (!d.done) {
		nng_cv_wait(d.cv);
	}
	nng_mtx_unlock(d.mtx);

	end = nng_clock();

	nng_close(d.s);
	nng_cv_free(d.cv);
	nng_mtx_free(d.mtx);
	nng_aio_free(d.txaio);
	nng_aio_free(d.rxaio);

	total   = (float) ((end - start)) / 1000;
	latency = ((float) ((total * 1000000)) / (trips * 2));
	printf("total time: %.3f [s]\n", total);
	printf("message size: %d [B]\n", (int) msgsize);
	printf("round trip count: %d\n", trips);
	printf("average latency: %.3f [us]\n", latency);
}

typedef struct {
	nng_socket s;
	int        trips;
	nng_aio *  txaio;
	nng_aio *  rxaio;
	size_t     msgsize;
	bool       done;
	nng_mtx *  mtx;
	nng_cv *   cv;
} latency_server_data;

void
latency_srv_tx_cb(void *arg)
{
	latency_server_data *d   = arg;
	nng_aio *            aio = d->txaio;
	int                  rv;

	if ((rv = nng_aio_result(aio)) != 0) {
		die("sendmsg: %s", nng_strerror(rv));
	}
	d->trips--;
	if (d->trips == 0) {
		nng_mtx_lock(d->mtx);
		d->done = true;
		nng_cv_wake(d->cv);
		nng_mtx_unlock(d->mtx);
		return;
	}
	nng_recv_aio(d->s, d->rxaio);
}

void
latency_srv_rx_cb(void *arg)
{
	latency_server_data *d   = arg;
	nng_aio *            aio = d->rxaio;
	int                  rv;
	nng_msg *            msg;

	if ((rv = nng_aio_result(aio)) != 0) {
		die("recvmsg: %s", nng_strerror(rv));
	}

	msg = nng_aio_get_msg(aio);
	if (nng_msg_len(msg) != d->msgsize) {
		die("bad message ssize");
	}
	// Send the reply; we just bounce back the same message.
	nng_aio_set_msg(d->rxaio, NULL);
	nng_aio_set_msg(d->txaio, msg);
	nng_send_aio(d->s, d->txaio);
}

void
latency_server(const char *addr, size_t msgsize, int trips)
{
	int                 rv;
	latency_server_data d;

	memset(&d, 0, sizeof(d));
	if (((rv = nng_mtx_alloc(&d.mtx)) != 0) ||
	    ((rv = nng_cv_alloc(&d.cv, d.mtx)) != 0) ||
	    ((rv = nng_aio_alloc(&d.rxaio, latency_srv_rx_cb, &d)) != 0) ||
	    ((rv = nng_aio_alloc(&d.txaio, latency_srv_tx_cb, &d)) != 0)) {
		die("failed initializing: %s", nng_strerror(rv));
	}

	if ((rv = nng_pair_open(&d.s)) != 0) {
		die("nng_socket: %s", nng_strerror(rv));
	}

	// XXX: set no delay
	// XXX: other options (TLS in the future?, Linger?)

	if ((rv = nng_listen(d.s, addr, NULL, 0)) != 0) {
		die("nng_listen: %s", nng_strerror(rv));
	}
	d.trips   = trips;
	d.msgsize = msgsize;

	nng_recv_aio(d.s, d.rxaio);

	nng_mtx_lock(d.mtx);
	while (!d.done) {
		nng_cv_wait(d.cv);
	}
	nng_mtx_unlock(d.mtx);
	// Wait a bit for things to drain... linger should do this.
	// 100ms ought to be enough.
	nng_msleep(100);
	nng_close(d.s);
	nng_cv_free(d.cv);
	nng_mtx_free(d.mtx);
	nng_aio_free(d.rxaio);
	nng_aio_free(d.txaio);
}

// Our throughput story is quite a mess.  Mostly I think because of the poor
// caching and message reuse.  We should probably implement a message pooling
// API somewhere.

typedef struct {
	nng_socket s;
	int        trips;
	size_t     msgsize;
	nng_aio *  aio;
	bool       done;
	nng_mtx *  mtx;
	nng_cv *   cv;
} tput_srv_data;

void
tput_srv_cb(void *arg)
{
	tput_srv_data *d   = arg;
	nng_aio *      aio = d->aio;
	nng_msg *      msg;
	int            rv;

	if ((rv = nng_aio_result(aio)) != 0) {
		die("recvmsg: %s", nng_strerror(rv));
	}
	msg = nng_aio_get_msg(aio);

	if (nng_msg_len(msg) != d->msgsize) {
		die("wrong message size: %d != %d", nng_msg_len(msg),
		    d->msgsize);
	}
	nng_aio_set_msg(aio, NULL);
	nng_msg_free(msg);
	d->trips--;
	if (d->trips == 0) {
		nng_mtx_lock(d->mtx);
		d->done = true;
		nng_cv_wake(d->cv);
		nng_mtx_unlock(d->mtx);
		return;
	}
	nng_recv_aio(d->s, aio);
}

void
throughput_server(const char *addr, size_t msgsize, int count)
{
	nng_socket    s;
	nng_msg *     msg;
	int           rv;
	tput_srv_data d;
	uint64_t      start, end;
	float         msgpersec, mbps, total;

	memset(&d, 0, sizeof(d));
	if (((rv = nng_mtx_alloc(&d.mtx)) != 0) ||
	    ((rv = nng_cv_alloc(&d.cv, d.mtx)) != 0) ||
	    ((rv = nng_aio_alloc(&d.aio, tput_srv_cb, &d)) != 0)) {
		die("failed initializing: %s", nng_strerror(rv));
	}
	d.trips   = count;
	d.msgsize = msgsize;

	if ((rv = nng_pair_open(&d.s)) != 0) {
		die("nng_socket: %s", nng_strerror(rv));
	}
	rv = nng_setopt_int(d.s, NNG_OPT_RECVBUF, 128);
	if (rv != 0) {
		die("nng_setopt(nng_opt_recvbuf): %s", nng_strerror(rv));
	}

	// XXX: set no delay
	// XXX: other options (TLS in the future?, Linger?)

	if ((rv = nng_listen(d.s, addr, NULL, 0)) != 0) {
		die("nng_listen: %s", nng_strerror(rv));
	}

	// Receive first synchronization message.
	if ((rv = nng_recvmsg(d.s, &msg, 0)) != 0) {
		die("nng_recvmsg: %s", nng_strerror(rv));
	}
	nng_msg_free(msg);
	start = nng_clock();

	nng_recv_aio(d.s, d.aio);
	nng_mtx_lock(d.mtx);
	while (!d.done) {
		nng_cv_wait(d.cv);
	}
	nng_mtx_unlock(d.mtx);

	end = nng_clock();
	// Send a synchronization message (empty) to the other side,
	// and wait a bit to make sure it goes out the wire.
	nng_send(d.s, "", 0, 0);
	nng_msleep(200);
	nng_close(s);
	total     = (float) ((end - start)) / 1000;
	msgpersec = (float) (count) / total;
	mbps      = (float) (msgpersec * 8 * msgsize) / (1024 * 1024);
	printf("total time: %.3f [s]\n", total);
	printf("message size: %d [B]\n", (int) msgsize);
	printf("message count: %d\n", count);
	printf("throughput: %.f [msg/s]\n", msgpersec);
	printf("throughput: %.3f [Mb/s]\n", mbps);
}

typedef struct {
	nng_socket s;
	int        trips;
	size_t     msgsize;
	nng_aio *  aio;
	bool       done;
	nng_mtx *  mtx;
	nng_cv *   cv;
} tput_cli_data;

void
tput_cli_cb(void *arg)
{
	tput_srv_data *d   = arg;
	nng_aio *      aio = d->aio;
	nng_msg *      msg;
	int            rv;

	if ((rv = nng_aio_result(aio)) != 0) {
		die("sendmsg: %s", nng_strerror(rv));
	}

	d->trips--;
	if (d->trips == 0) {
		nng_mtx_lock(d->mtx);
		d->done = true;
		nng_cv_wake(d->cv);
		nng_mtx_unlock(d->mtx);
		return;
	}
	if ((rv = nng_msg_alloc(&msg, d->msgsize)) != 0) {
		die("msg_alloc: %s", nng_strerror(rv));
	}
	nng_aio_set_msg(aio, msg);
	nng_send_aio(d->s, aio);
}

void
throughput_client(const char *addr, size_t msgsize, int count)
{
	nng_msg *     msg;
	int           rv;
	tput_cli_data d;

	memset(&d, 0, sizeof(d));
	if (((rv = nng_mtx_alloc(&d.mtx)) != 0) ||
	    ((rv = nng_cv_alloc(&d.cv, d.mtx)) != 0) ||
	    ((rv = nng_aio_alloc(&d.aio, tput_cli_cb, &d)) != 0)) {
		die("failed initializing: %s", nng_strerror(rv));
	}
	d.trips   = count;
	d.msgsize = msgsize;

	// We send one extra zero length message to start the timer.
	count++;

	if ((rv = nng_pair_open(&d.s)) != 0) {
		die("nng_socket: %s", nng_strerror(rv));
	}

	// XXX: set no delay
	// XXX: other options (TLS in the future?, Linger?)

	rv = nng_setopt_int(d.s, NNG_OPT_SENDBUF, 128);
	if (rv != 0) {
		die("nng_setopt(nng_opt_sendbuf): %s", nng_strerror(rv));
	}

	rv = nng_setopt_ms(d.s, NNG_OPT_RECVTIMEO, 5000);
	if (rv != 0) {
		die("nng_setopt(nng_opt_recvtimeo): %s", nng_strerror(rv));
	}

	if ((rv = nng_dial(d.s, addr, NULL, 0)) != 0) {
		die("nng_dial: %s", nng_strerror(rv));
	}

	if ((rv = nng_msg_alloc(&msg, 0)) != 0) {
		die("nng_msg_alloc: %s", nng_strerror(rv));
	}
	if ((rv = nng_sendmsg(d.s, msg, 0)) != 0) {
		die("nng_sendmsg: %s", nng_strerror(rv));
	}

	// One message for synchronization on remote side.
	if ((rv = nng_msg_alloc(&msg, msgsize)) != 0) {
		die("nng_msg_alloc: %s", nng_strerror(rv));
	}

	nng_aio_set_msg(d.aio, msg);
	nng_send_aio(d.s, d.aio);

	// Attempt to get the completion indication from the other side.
	if (nng_recvmsg(d.s, &msg, 0) == 0) {
		nng_msg_free(msg);
	}

	nng_close(d.s);
}

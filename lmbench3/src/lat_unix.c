/*
 * tcp_xact.c - simple TCP transaction latency test
 *
 * Three programs in one -
 *	server usage:	lat_unix -s
 *	client usage:	lat_unix [-P <parallelism>] [-W <warmup>] [-N <repetitions>] hostname
 *	shutdown:	lat_unix -S hostname
 *
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id$\n";
#include "bench.h"

struct _state {
	int	sv[2];
	int	pid;
	int	msize;
	char*	buf;
};
void	initialize(void* cookie);
void	benchmark(iter_t iterations, void* cookie);
void	cleanup(void* cookie);

int
main(int ac, char **av)
{
	int parallel = 1;
	int warmup = 0;
	int repetitions = TRIES;
	struct _state state;
	int c;
	char* usage = "[-m <message size>] [-P <parallelism>] [-W <warmup>] [-N <repetitions>]\n";

	state.msize = 1;

	while (( c = getopt(ac, av, "m:P:W:N:")) != EOF) {
		switch(c) {
		case 'm':
			state.msize = atoi(optarg);
			break;
		case 'P':
			parallel = atoi(optarg);
			if (parallel <= 0) lmbench_usage(ac, av, usage);
			break;
		case 'W':
			warmup = atoi(optarg);
			break;
		case 'N':
			repetitions = atoi(optarg);
			break;
		default:
			lmbench_usage(ac, av, usage);
			break;
		}
	}
	if (optind < ac) {
		lmbench_usage(ac, av, usage);
	}
	benchmp(initialize, benchmark, cleanup, 0, parallel, 
		warmup, repetitions, &state);

	micro("AF_UNIX sock stream latency", get_n());
	return(0);
}

void initialize(void* cookie)
{
	struct _state* pState = (struct _state*)cookie;
	void	exit();

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pState->sv) == -1) {
		perror("socketpair");
	}

	pState->buf = malloc(pState->msize);
	if (pState->buf == NULL) {
		fprintf(stderr, "buffer allocation\n");
		exit(1);
	}

	if (pState->pid = fork())
		return;

	/* Child sits and ping-pongs packets back to parent */
	signal(SIGTERM, exit);
	while (read(pState->sv[0], pState->buf, pState->msize) == pState->msize) {
		write(pState->sv[0], pState->buf, pState->msize);
	}
	exit(0);
}

void benchmark(iter_t iterations, void* cookie)
{
	struct _state* pState = (struct _state*)cookie;

	while (iterations-- > 0) {
		if (write(pState->sv[1], pState->buf, pState->msize) != pState->msize
		    || read(pState->sv[1], pState->buf, pState->msize) != pState->msize) {
			/* error handling: how do we signal failure? */
			cleanup(cookie);
			exit(0);
		}
	}
}

void cleanup(void* cookie)
{
	struct _state* pState = (struct _state*)cookie;

	kill(pState->pid, SIGTERM);
	wait(0);
}


/*
 * bw_tcp.c - simple TCP bandwidth test
 *
 * Three programs in one -
 *	server usage:	bw_tcp -s
 *	client usage:	bw_tcp [-m <message size>] [-M <total bytes>] [-P <parallelism>] [-W <warmup>] [-N <repetitions>] hostname 
 *	shutdown:	bw_tcp -hostname
 *
 * Copyright (c) 2000 Carl Staelin.
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id$\n";
#include "bench.h"

typedef struct _state {
	int	sock;
	long	move;
	int	msize;
	char	*server;
	int	fd;
	char	*buf;
} state_t;

void	server_main();
void	client_main(int parallel, state_t *state);
void	source(int data);

void	initialize(void* cookie);
void	loop_transfer(iter_t iterations, void *cookie);
void	cleanup(void* cookie);

int main(int ac, char **av)
{
	int	parallel = 1;
	int	warmup = 0;
	int	repetitions = TRIES;
	int	shutdown = 0;
	state_t state;
	char	*usage = "-s\n OR [-m <message size>] [-M <bytes to move>] [-P <parallelism>] [-W <warmup>] [-N <repetitions>] server\n OR -S serverhost\n";
	int	c;
	
	state.msize = 0;
	state.move = 0;

	/* Rest is client argument processing */
	while (( c = getopt(ac, av, "sS:m:M:P:W:N:")) != EOF) {
		switch(c) {
		case 's': /* Server */
			if (fork() == 0) {
				server_main();
			}
			exit(0);
			break;
		case 'S': /* shutdown serverhost */
		{
			int	conn;
			int	n = htonl(0);
			conn = tcp_connect(optarg, TCP_DATA, SOCKOPT_NONE);
			write(conn, &n, sizeof(int));
			exit(0);
		}
		case 'm':
			state.msize = bytes(optarg);
			break;
		case 'M':
			state.move = bytes(optarg);
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

	if (optind < ac - 2 || optind >= ac) {
		lmbench_usage(ac, av, usage);
	}

	state.server = av[optind++];

	if (state.msize == 0 && state.move == 0) {
		state.msize = state.move = 10 * 1024 * 1024;
	} else if (state.msize == 0) {
		state.msize = state.move;
	} else if (state.move == 0) {
		state.move = state.msize;
	}

	/* make the number of bytes to move a multiple of the message size */
	if (state.move % state.msize) {
		state.move += state.msize - state.move % state.msize;
	}

	/*
	 * Initialize connection by running warming up for at least
	 * five seconds, then measure performance over one second.
	 * This minimizes the effect of opening and initializing TCP 
	 * connections.
	 */
	benchmp(initialize, loop_transfer, cleanup, 
		SHORT, parallel, SHORT + warmup, repetitions, &state );
	/* (void)fprintf(stderr, "Socket bandwidth using %s\n", state.server); */
	fprintf(stderr, "%.6f ", state.msize / (1000. * 1000.));
	mb(state.move * get_n() * parallel);
}

void
initialize(void *cookie)
{
	int	c;
	state_t *state = (state_t *) cookie;

	state->buf = valloc(state->msize);
	if (!state->buf) {
		perror("valloc");
		exit(1);
	}
	touch(state->buf, state->msize);

	state->sock = tcp_connect(state->server, TCP_DATA, SOCKOPT_READ);
	c = htonl(state->msize);
	if (write(state->sock, &c, sizeof(int)) != sizeof(int)) {
		perror("control write");
		exit(1);
	}
}

void 
loop_transfer(iter_t iterations, void *cookie)
{
	int	c;
	long	todo;
	state_t *state = (state_t *) cookie;

	while (iterations-- > 0) {
		for (todo = state->move; todo > 0; todo -= c) {
			if ((c = read(state->sock, state->buf, state->msize)) <= 0) {
				exit(1);
			}
		}
	}
}

void
cleanup(void* cookie)
{
	state_t *state = (state_t *) cookie;

	/* close connection */
	(void)close(state->sock);
}


void child()
{
	wait(0);
	signal(SIGCHLD, child);
}

void server_main()
{
	int	data, newdata;

	GO_AWAY;

	signal(SIGCHLD, child);
	data = tcp_server(TCP_DATA, SOCKOPT_WRITE);

	for ( ;; ) {
		newdata = tcp_accept(data, SOCKOPT_WRITE);
		switch (fork()) {
		    case -1:
			perror("fork");
			break;
		    case 0:
			source(newdata);
			exit(0);
		    default:
			close(newdata);
			break;
		}
	}
}

/*
 * Read the number of bytes to be transfered.
 * Write that many bytes on the data socket.
 */
void source(int data)
{
	int	tmp, n, m, nbytes;
	char	*buf;

	/*
	 * read the message size
	 */
	if (read(data, &tmp, sizeof(int)) != sizeof(int)) {
		perror("control nbytes");
		exit(7);
	}
	m = ntohl(tmp);

	/*
	 * A hack to allow turning off the absorb daemon.
	 */
     	if (m == 0) {
		tcp_done(TCP_DATA);
		kill(getppid(), SIGTERM);
		exit(0);
	}

	buf = valloc(m);
	bzero(buf, m);

	/*
	 * Keep sending messages until the connection is closed
	 */
	while (write(data, buf, m) > 0) {
#ifdef	TOUCH
		touch(buf, m);
#endif
	}
	free(buf);
}

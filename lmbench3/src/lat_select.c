/*
 * lat_select.c - time select system call
 *
 * usage: lat_select [-P <parallelism>] [-W <warmup>] [-N <repetitions>] [n]
 *
 * Copyright (c) 1996 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 */
char	*id = "$Id$\n";

#include "bench.h"

void initialize(void *cookie);
void cleanup(void *cookie);
void doit(iter_t iterations, void *cookie);
void writer(int w, int r);
void server(void* cookie);

typedef int (*open_f)(void* cookie);
int  open_file(void* cookie);
int  open_socket(void* cookie);

typedef struct _state {
	char	fname[L_tmpnam];
	open_f	fid_f;
	pid_t	pid;
	int	sock;
	int	fid;
	int	num;
	int	max;
	fd_set  set;
} state_t;

int main(int ac, char **av)
{
	state_t state;
	int parallel = 1;
	int warmup = 0;
	int repetitions = TRIES;
	int c;
	char* usage = "[-n <#descriptors>] [-P <parallelism>] [-W <warmup>] [-N <repetitions>] file|tcp\n";
	char	buf[256];

	morefds();  /* bump fd_cur to fd_max */
	state.num = 200;
	while (( c = getopt(ac, av, "P:W:N:n:")) != EOF) {
		switch(c) {
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
		case 'n':
			state.num = bytes(av[optind]);
			break;
		default:
			lmbench_usage(ac, av, usage);
			break;
		}
	}

	if (optind + 1 != ac) {
		lmbench_usage(ac, av, usage);
	}

	if (streq("tcp", av[optind])) {
		state.fid_f = open_socket;
		server(&state);
		benchmp(initialize, doit, cleanup, 0, parallel, 
			warmup, repetitions, &state);
		sprintf(buf, "Select on %d tcp fd's", state.num);
		micro(buf, get_n());
	} else if (streq("file", av[optind])) {
		state.fid_f = open_file;
		server(&state);
		benchmp(initialize, doit, cleanup, 0, parallel, 
			warmup, repetitions, &state);
		sprintf(buf, "Select on %d fd's", state.num);
		micro(buf, get_n());
	} else {
		lmbench_usage(ac, av, usage);
	}

	close(state.fid);
	unlink(state.fname);
	if (state.pid > 0)
		kill(state.pid, SIGKILL);

	exit(0);
}

void
server(void* cookie)
{
	int pid;
	state_t* state = (state_t*)cookie;

	pid = getpid();

	/* Create a socket for clients to connect to */
	state->sock = tcp_server(TCP_CONNECT, SOCKOPT_NONE);
	if (state->sock <= 0) {
		perror("lat_select: Could not open tcp server socket");
		exit(1);
	}

	/* Create a temporary file for clients to open */
	tmpnam(state->fname);
	state->fid = open(state->fname, O_RDWR|O_APPEND|O_CREAT, 0666);
	if (state->fid <= 0) {
		char buf[L_tmpnam+128];
		sprintf(buf, "lat_select: Could not create temp file %s", state->fname);
		perror(buf);
		exit(1);
	}

	/* Start a server process to accept client connections */
	switch(state->pid = fork()) {
	case 0:
		/* child server process */
		close(state->fid);
		while (pid == getppid()) {
			int newsock = tcp_accept(state->sock, SOCKOPT_NONE);
			read(newsock, &state->fid, 1);
			close(newsock);
		}
		exit(0);
	case -1:
		/* error */
		perror("lat_select::server(): fork() failed");
		exit(1);
	default:
		break;
	}
}

int
open_socket(void* cookie)
{
	return tcp_connect("localhost", TCP_CONNECT, SOCKOPT_NONE);
}

int open_file(void* cookie)
{
	state_t* state = (state_t*)cookie;

	return open(state->fname, O_RDONLY);
}

void
doit(iter_t iterations, void * cookie)
{
	state_t * 	state = (state_t *)cookie;
	fd_set		nosave;
	static struct timeval tv;
	static count = 0;
	
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	while (iterations-- > 0) {
		nosave = state->set;
		select(state->num, 0, &nosave, 0, &tv);
	}
}

void
initialize(void *cookie)
{
	char	c;
	state_t * state = (state_t *)cookie;

	int	i, last = 0 /* lint */;
	int	N = state->num, fid, fd;

	fid = (*state->fid_f)(cookie);
	if (fid <= 0) {
		perror("Could not open device");
		exit(1);
	}
	state->max = 0;
	FD_ZERO(&(state->set));
	for (fd = 0; fd < N; fd++) {
		i = dup(fid);
		if (i == -1) break;
		if (i > state->max)
			state->max = i;
		FD_SET(i, &(state->set));
	}
	close(fid);
}

void
cleanup(void *cookie)
{
	int	i;
	state_t * state = (state_t *)cookie;

	for (i = 0; i <= state->max; ++i) {
		if (FD_ISSET(i, &(state->set)))
			close(i);
	}
	FD_ZERO(&(state->set));
}

	     

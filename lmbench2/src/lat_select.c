/*
 * lat_select.c - time select system call
 *
 * usage: lat_select tcp|file [n]
 *
 * Copyright (c) 1996 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 */
char	*id = "$Id$\n";

#include "bench.h"

void
doit(int n, fd_set *set)
{
	fd_set	nosave = *set;
	static	struct timeval tv;
	select(n, 0, &nosave, 0, &tv);
}

int
main(int ac, char **av)
{
	int	n, N, last, fd, fid;
	pid_t	pid, ppid;
	fd_set	set;
	char	buf[L_tmpnam+256];
	char	fname[L_tmpnam];
	char*	report_file = "Select on %d fd's";
	char*	report_tcp  = "Select on %d tcp fd's";
	char*	report;
	char*	usage = "lat_select tcp|file [n]\n";

	morefds();
	N = 200;
	fname[0] = 0;
	pid = 0;
	report = report_file;

	if (ac != 2 && ac != 3) {
		fprintf(stderr, usage);
		exit(1);
	}

	if (streq(av[1], "tcp")) {
		report = report_tcp;
		
		/* Create a socket for clients to connect to */
		fd = tcp_server(TCP_CONNECT, SOCKOPT_NONE);
		if (fd <= 0) {
			perror("lat_select: Could not open tcp server socket");
			exit(1);
		}

		/* Start server process to accept client connections */
		ppid = getpid();
		switch(pid = fork()) {
		case 0:
			/* child server process */
			while (ppid == getppid()) {
				int newsock = tcp_accept(fd, SOCKOPT_NONE);
				read(newsock, &fid, 1);
				close(newsock);
			}
			close(fd);
			exit(0);
		case -1:
			/* error */
			perror("lat_select::server(): fork() failed");
			exit(1);
		default:
			break;
		}
		close(fd);
		fd = tcp_connect("localhost", TCP_CONNECT, SOCKOPT_NONE);
		if (fd <= 0) {
			perror("lat_select: Could not open socket");
			exit(1);
		}
	} else if (streq(av[1], "file")) {
		/* Create a temporary file for clients to open */
		tmpnam(fname);
		fd = open(fname, O_RDWR|O_APPEND|O_CREAT, 0666);
		if (fd <= 0) {
			char buf[L_tmpnam+128];
			sprintf(buf, 
				"lat_select: Could not create temp file %s", fname);
			perror(buf);
			exit(1);
		}
	} else {
		fprintf(stderr, usage);
		exit(1);
	}

	if (ac == 3) N = atoi(av[2]);

	FD_ZERO(&set);
	last = 0;
	for (n = 0; n < N; n++) {
		fid = dup(fd);
		if (fid == -1) break;
		if (fid > last) last = fid;
		FD_SET(fid, &set);
	}
	last++;
	BENCH(doit(last, &set), 0);
	sprintf(buf, report, n);
	micro(buf, get_n());
	if (pid) kill(SIGKILL, pid);
	if (fname[0]) unlink(fname);
	exit(0);
}

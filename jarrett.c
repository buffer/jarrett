/*
 * jarrett.c
 * Creation date: 03.04.2005
 * Copyright(c) 2005 Angelo Dell'Aera <buffer@antifork.org>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 */


#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>

#define _GNU_SOURCE
#include <getopt.h>

#define BUFLEN         8192
#define BACKLOG_QUEUE  1
#define SOCKNUM        2

struct option long_options[] = {
        {"help",         no_argument,       NULL, 'h'},
	{"mode",         required_argument, NULL, 'm'},
	{"server",       required_argument, NULL, 's'},
	{"client",       required_argument, NULL, 'c'},	
	{"daemon",       no_argument,       NULL, 'd'},
	{"spy",          no_argument,       NULL, 'S'},
        {NULL, 0, NULL, 0}
};


#define MIXED_MODE        0x00
#define CLIENT_MODE       0x01
#define SERVER_MODE       0x02
#define SPY_MODE          0x04

#define MODE_MASK         (MIXED_MODE | CLIENT_MODE | SERVER_MODE)
#define MODE(mode)        (mode & MODE_MASK)

#define MIXED_OPT         "mixed"
#define MIXED_OPT_SHORT   "m"
#define CLIENT_OPT        "client"
#define CLIENT_OPT_SHORT  "c"
#define SERVER_OPT        "server"
#define SERVER_OPT_SHORT  "s"


struct options {
	int      mode;
	char     *server[SOCKNUM];
	int      server_no;
	char     *client[SOCKNUM];
        int      client_no;
} opt;


struct jsock {
	int                fd;
	char               *host;
        char               *port;
	struct sockaddr_in sockaddr;
};


void banner()
{
	fprintf(stderr, "\nJarrett Host-To-Host Connector\n");
	fprintf(stderr, "Copyright(c) 2005 Angelo Dell'Aera <buffer@antifork.org>\n\n");
}


void usage(char *name)
{
        fprintf(stderr, "Usage : %s [-c <ip:port>] [-s <ip:port>] [-m mode] [OPTIONS]\n\n", name);
	fprintf(stderr, "OPTIONS\n\n");
	fprintf(stderr, "-c --client  : specify a client mode host\n");
	fprintf(stderr, "-s --server  : specify a server mode host\n");
	fprintf(stderr, "-m --mode    : jarrett mode (client, server, mixed) [default : mixed]\n");
	fprintf(stderr, "-y --spy     : spy mode\n");
	fprintf(stderr, "-d --daemon  : runs jarrett in background\n\n");
}


char *parse_hp(char *s)
{

	char  *p = s;

	while (*p != ':')
		p++;

	*p = '\0';
	return ++p;
}


int parse_mode_success(char *s, char *opt, char *opt_short)
{
	return (!strncmp(s, opt, strlen(opt)) ||
		!strncmp(s, opt_short, 1));
}


void parse_mode(char *s)
{

	if (parse_mode_success(s, SERVER_OPT, SERVER_OPT_SHORT))
		opt.mode = SERVER_MODE;
	else if (parse_mode_success(s, CLIENT_OPT, CLIENT_OPT_SHORT))
                opt.mode = CLIENT_MODE;
	else if (parse_mode_success(s, MIXED_OPT, MIXED_OPT_SHORT))
		opt.mode = MIXED_MODE;
	else {
		fprintf(stderr, "Supported modes : (c)lient | (s)erver | (m)ixed\n");
		fprintf(stderr, "Exiting...\n\n");
		exit(EXIT_FAILURE);
	}
}

	
unsigned long getlongbyname(u_char *host)
{

	struct in_addr  addr;
	struct hostent *host_ent;

	if (strcasecmp(host, "any") == 0)
		return INADDR_ANY;

	if ((addr.s_addr = inet_addr(host)) == -1) {

		if ((host_ent = gethostbyname(host)) == NULL) 
			fprintf(stderr,"'%s': gethostbyname() or inet_addr() err: %s", 
				host, strerror(errno));
		
		bcopy(host_ent->h_addr, (char *)&addr.s_addr, host_ent->h_length);
	}

	return addr.s_addr;
}


int tcp_accept(struct jsock sock)
{

        int                  listenfd, connfd;
        struct sockaddr_in   servaddr;
	socklen_t            clilen;

        listenfd = socket(AF_INET, SOCK_STREAM, 0);
        if (listenfd < 0) {
                perror("socket");
                exit(EXIT_FAILURE);
        }

        memset(&servaddr, 0, sizeof(struct sockaddr_in));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(atoi(sock.port));
        servaddr.sin_addr.s_addr = INADDR_ANY;

	if ( (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}

	if ( (listen(listenfd, BACKLOG_QUEUE)) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}

	clilen = sizeof(struct sockaddr_in);

	fprintf(stderr, "Waiting for inbound connection.. ");
	connfd = accept(listenfd, (struct sockaddr *)&sock.sockaddr, &clilen);
	if (connfd < 0) {
		perror("bind");
                exit(EXIT_FAILURE);
        }

	fprintf(stderr, "%s connected!\n", inet_ntoa(sock.sockaddr.sin_addr));

	close(listenfd);
	return connfd;
}


int tcp_connect(struct jsock sock)
{

	int                  sockfd;
	struct sockaddr_in   addr;
	int                  res;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(atoi(sock.port));
	addr.sin_addr.s_addr = getlongbyname(sock.host);

	fprintf(stderr, "Connecting to %s:%d.. ", sock.host, atoi(sock.port));

	res = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
	if (res < 0) {
                fprintf(stderr, "failed! Exiting...\n");
                exit(EXIT_FAILURE);
        }

	inet_aton(sock.host, &sock.sockaddr.sin_addr);
	sock.sockaddr.sin_port = addr.sin_port;

	fprintf(stderr, "connected!\n\n");
	return sockfd;
}


void spy(const struct jsock *sock, char *buf, int src)
{
	int dst;

	if ( !(opt.mode && SPY_MODE))
		return;

	dst = (src) ? 0 : 1;

	fprintf(stderr, "%s:%d --> %s:%d\n", 
		inet_ntoa(sock[src].sockaddr.sin_addr), 
		htons(sock[src].sockaddr.sin_port), 
		inet_ntoa(sock[dst].sockaddr.sin_addr),
		htons(sock[dst].sockaddr.sin_port));

	fprintf(stderr, "\t%s", buf);
}


void do_proxy(const struct jsock *sock)
{
	char     buf[BUFLEN];
	fd_set   rset;
	int      maxfdp;
	int      bytes = 1;

	FD_ZERO(&rset);

	while(bytes) {
		FD_SET(sock[0].fd, &rset);
		FD_SET(sock[1].fd, &rset);
		maxfdp = ((sock[0].fd > sock[1].fd) ? sock[0].fd : sock[1].fd) + 1;

		if (select(maxfdp, &rset, NULL, NULL, NULL) < 0) {
			perror("select");
			exit(EXIT_FAILURE);
		}

		if (FD_ISSET(sock[0].fd, &rset)) {
			bytes = recv(sock[0].fd, buf, sizeof(buf), 0);
			spy(sock, buf, 0);
			send(sock[1].fd, buf, bytes, 0);
		}

		if (FD_ISSET(sock[1].fd, &rset)) {
                        bytes = recv(sock[1].fd, buf, sizeof(buf), 0);
			spy(sock, buf, 1);
                        send(sock[0].fd, buf, bytes, 0);
                }
	}
}


int check_options_client()
{
	return ((MODE(opt.mode) == CLIENT_MODE) &&
		((opt.server_no != 2) || (opt.client_no != 0)));
}


int check_options_server()
{
	return ((MODE(opt.mode) == SERVER_MODE) &&
		((opt.server_no != 0) || (opt.client_no != 2)));
}


int check_options_mixed()
{
	return ((MODE(opt.mode) == MIXED_MODE) &&
		((opt.server_no != 1) || (opt.client_no != 1)));
}


void check_options_failed(char *name, char *errmsg)
{
	usage(name);
	fprintf(stderr, "%s\n\n", errmsg);
	exit(EXIT_FAILURE);
}
					

void check_options(char *name)
{
	if ( (opt.server_no + opt.client_no) != SOCKNUM)
		check_options_failed(name, 
				     "Two hosts needed, exiting...");
		
	if (check_options_client()) 
		check_options_failed(name, 
				     "Client mode needs two server hosts, exiting...");

	if (check_options_server()) 
                check_options_failed(name, 
				     "Server mode needs two client hosts, exiting...");

	if (check_options_mixed()) 
                check_options_failed(name, 
				     "Mixed mode needs a server host and a client host, exiting...");
}


void parse_options(int argc, char **argv)
{

	int c;

	memset(&opt, 0, sizeof(opt));

        while ((c = getopt_long(argc, argv,"hm:s:c:dy", long_options, NULL)) != EOF) {
		
		switch(c) {
		case 'h':
			usage(argv[0]);
			exit(EXIT_FAILURE);
			break;
		case 'm':
			parse_mode(optarg);
			break;
		case 's':
			opt.server[opt.server_no++] = strdup(optarg);
			break;
		case 'c':
                        opt.client[opt.client_no++] = strdup(optarg);
                        break;
		case 'd':
			fprintf(stderr, "Jarrett going in background...\n\n");
			daemon(1, 0);
			break;
		case 'y':
			opt.mode |= SPY_MODE;
			break;
		default:
                        usage(argv[0]);
                        exit(EXIT_FAILURE);
                        break;
		}
	}

	check_options(argv[0]);
}
		
		
int main(int argc, char **argv)
{
	struct jsock sock[2];
	int   i, j;

	banner();
	parse_options(argc, argv);

	memset(sock, 0, SOCKNUM * sizeof(struct jsock));

	for (i = 0; i < opt.server_no; i++) {
		sock[i].port = parse_hp(opt.server[i]);
		sock[i].host = strdup(opt.server[i]);
		sock[i].fd = tcp_connect(sock[i]);
	}

	for (j = 0; j < opt.client_no; j++, i++) {
		sock[i].port = parse_hp(opt.client[j]);
		sock[i].host = strdup(opt.client[j]);
		sock[i].fd = tcp_accept(sock[i]);
	}

	fprintf(stderr, "\nJarrett working...\n");
	fprintf(stderr, "Host 1 : %s:%d\nHost 2 : %s:%d\n\n",
                inet_ntoa(sock[0].sockaddr.sin_addr),
                htons(sock[0].sockaddr.sin_port),
                inet_ntoa(sock[1].sockaddr.sin_addr),
                htons(sock[1].sockaddr.sin_port));

	do_proxy(sock);

	fprintf(stderr, "Connections lost, exiting\n");

	for(i = 0; i < SOCKNUM; i++)
		close(sock[i].fd);

	return 0;
}

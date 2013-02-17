#include <stdlib.h>
#include "list.h"
#include "queue.h"
#include "graphhost.h"
#include <string.h>
#include <strings.h>

/*#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/select.h>
#include <stropts.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include <ioLib.h>
#include <sockLib.h>
#include <hostLib.h>
#include <selectLib.h>

/* Listens on a specified port (listenport), and returns the file
 * descriptor to the listening socket.
 */
int
sockets_listen_int(int port, sa_family_t sin_family, uint32_t s_addr)
{
	struct sockaddr_in serv_addr;
	int err;
	int sd;

	/* Create a TCP socket */
	sd = socket(sin_family, SOCK_STREAM, 0);
	if(sd == -1){
		perror("");
		exit(1);
	}

	/* Zero out the serv_addr struct */
	bzero((char *) &serv_addr, sizeof(struct sockaddr_in));

	/* Set up the listener sockaddr_in struct */
	serv_addr.sin_family = sin_family;
	serv_addr.sin_addr.s_addr = s_addr;
	serv_addr.sin_port = htons(port);

	/* Bind the socket to the listener sockaddr_in */
	err = bind(sd, (struct sockaddr *) &serv_addr,
		sizeof(struct sockaddr_in));
	if(err != 0){
		perror("");
		exit(1);
	}

	/* Listen on the socket for incoming conncetions */
	err = listen(sd, 5);
	if(err != 0){
		perror("");
		exit(1);
	}

	/* Make sure we aren't killed by SIGPIPE */
	signal(SIGPIPE, SIG_IGN);

	return sd;
}

struct graphhost_t *
GraphHost_create(int port)
{
	int i;
	struct graphhost_t *inst;
	int pipefd[2];

	/* Allocate memory for the graphhost_t structure */
	inst = malloc(sizeof(struct graphhost_t));

	/* Store the port to listen on */
	inst->port = port;

	/* Create a pipe for IPC with the thread */
	pipe(pipefd);
	inst->ipcfd_r = pipefd[0];
	inst->ipcfd_w = pipefd[1];

	/* Launch the thread */
	pthread_create(&inst->thread, NULL, sockets_threadmain, (void *)inst);

	return inst;
}

void
GraphHost_destroy(struct graphhost_t *inst)
{

	/* Tell the other thread to stop */
	write(inst->ipcfd_w, "x", 1);

	/* Join to the other thread */
	pthread_join(inst->thread, NULL);

	/* Close file descriptors and clean up */
	close(inst->ipcfd_r);
	close(inst->ipcfd_w);
	free(inst);

	return;
}

/*
struct socketconn_t *
sockets_listaddsocket(struct list_t *list, struct socketconn_t *conn)
{
	struct socketconn_t *conn;

	conn = malloc(sizeof(struct socketconn_t));
	conn->fd = fd;

	conn->selectflags = 0;
	conn->dataset = NULL;
	conn->elem = list_add_after(conn, NULL, conn);

	return conn;
}
*/

void
sockets_listremovesocket(struct list_t *list, struct socketconn_t *conn)
{
	list_delete(list, conn->elem);

	return;
}

void
sockets_accept(struct list_t *connlist, int listenfd)
{
	int new_fd;
	int on;
	socklen_t clilen;
	struct socketconn_t *conn;
	struct queue_t *queue;
	struct sockaddr_in cli_addr;
	int error;
	int flags;

	clilen = sizeof(struct sockaddr_in);

	/* Accept a new connection */
	new_fd = accept(listenfd, (struct sockaddr *) &cli_addr,
		&clilen);

	/* Make sure that the file descriptor is valid */
	if(new_fd < 1) {
		perror("");
		return;
	}

	/* Set the socket non-blocking. */
	/*flags = fcntl(new_fd, F_GETFL, 0);
	fcntl(new_fd, F_SETFL, flags | O_NONBLOCK);*/

	/* Set the socket non-blocking. */
	on = 1;
	error = ioctl(new_fd, (int)FIONBIO, (char *)&on);
	if(error == -1){
		perror("");
		close(new_fd);
		return;
	}

	/* Set up the 20-element write queue for the socket */
	queue = queue_init(20);

	conn = malloc(sizeof(struct socketconn_t));
	conn->fd = new_fd;
	conn->selectflags = SOCKET_READ | SOCKET_ERROR;
	/* conn->dataset = NULL; */
	conn->datasets = list_create();
	conn->queue = queue;
	conn->buf = NULL;
	conn->buflength = 0;
	conn->orphan = 0;
	/* Add it to the list, this makes it a bit non-thread-safe */
	conn->elem = list_add_after(connlist, NULL, conn);

	return;
}

/* Mark the socket for closing and deleting */
void
sockets_close(struct list_t *list, struct list_elem_t *elem)
{
	struct socketconn_t *conn = elem->data;

	conn->orphan = 1;

	return;
}

/* NOTE: Does not remove the element from the list */
void
sockets_remove_orphan(struct socketconn_t *conn)
{
	struct writebuf_t *writebuf;
	struct list_elem_t *dataset;

	/* Give up on the current buffer */
	if(conn->buf != NULL) {
		free(conn->buf);
	}

	/* Give up on all other queued buffers too */
	while(queue_dequeue(conn->queue, (void **)&writebuf) == 0) {
		free(writebuf->buf);
		free(writebuf);
	}

	for(dataset = conn->datasets->start; dataset != NULL; dataset = dataset->next) {
		free(dataset->data);
	}

	list_destroy(conn->datasets);

	/* Free it when we get back to it, this is a hack */
	conn->orphan = 1;

	/* free(conn->dataset); */
	close(conn->fd);
	free(conn);

	return;
}

/* Closes and clears orpahans from the list */
void
sockets_clear_orphans(struct list_t *list)
{
	struct list_elem_t *elem;
	struct list_elem_t *last = NULL;
	struct socketconn_t *conn;

	for(elem = list->start; elem != NULL; ) {
		last = elem;
		elem = elem->next;

		if(last != NULL) {
			conn = last->data;
			if(conn->orphan == 1) {
				conn = last->data;
				sockets_remove_orphan(conn);
				list_delete(list, last);
			}
		}
	}

	return;
}

#if 0
int
sockets_readh(struct list_t *list, struct list_elem_t *elem)
{
	struct socketconn_t *conn = elem->data;
	char *buf;
	size_t length;
	int error;

	error = recv(conn->fd, &length, 4, 0);
	if(error < 1) {
		/* Clean up the socket here */
		sockets_close(list, elem);
		return 0;
	}

	/* Swap byte order */
	length = ntohl(length);

	/* Sanity check on the size */
	if(length > 64) {
		/* Clean up the socket here */
		sockets_close(list, elem);
		return 0;
	}

	buf = malloc(length+1);

	error = recv(conn->fd, buf, length, 0);
	if(error < 1) {
		/* Clean up the socket here */
		free(buf);
		sockets_close(list, elem);
		return 0;
	}

	buf[length] = '\0';

	conn->dataset = buf;

	return 0;
}

#endif

int
sockets_readh(struct list_t *list, struct list_elem_t *elem)
{
	struct socketconn_t *conn = elem->data;
	char *buf;
	char inbuf[16];
	size_t length;
	int error;

	error = recv(conn->fd, inbuf, 16, 0);
	if(error < 1) {
		/* Clean up the socket here */
		sockets_close(list, elem);
		return 0;
	}

	inbuf[15] = 0;

	buf = strdup(inbuf);
	/* buf = malloc(strlen(inbuf)+1);
	strcpy(buf, inbuf); */

	list_add_after(conn->datasets, NULL, buf);
	/* conn->dataset = buf; */

	return 0;
}


int
sockets_writeh(struct list_t *list, struct list_elem_t *elem)
{
	int error;
	struct socketconn_t *conn = elem->data;
	struct writebuf_t *writebuf;

	while(1) {

		/* Get another buffer to send */
		if(conn->buflength == 0) {
			error = queue_dequeue(conn->queue, (void **)&writebuf);
			/* There are no more buffers in the queue */
			if(error != 0) {
				/* Call the write finished callback in the upper layer */
				/*if(conninfo->listener->wdoneh != NULL)
					conninfo->listener->wdoneh(conninfo); */

				/* Stop selecting on write */
				conn->selectflags &= ~(SOCKET_WRITE);

				return 0;
			}
			conn->buf = writebuf->buf;
			conn->buflength = writebuf->buflength;
			conn->bufoffset = 0;
			free(writebuf);
		}

		/* These descriptors are ready for writing */
		conn->bufoffset += send(conn->fd, conn->buf, conn->buflength - conn->bufoffset, 0);

		/* Have we finished writing the buffer? */
		if(conn->bufoffset == conn->buflength) {

			/* Reset the write buffer */
			conn->buflength = 0;
			conn->bufoffset = 0;
			free(conn->buf);
			conn->buf = NULL;
		}else{
			/* We haven't finished writing, keep selecting. */
			return 0;
		}

	}

	/* We always return from within the loop, this is unreachable */
	return -1;
}

/* Queue a buffer for writing. Returns 0 on success, returns -1 if buffer
 * wasn't queued. Only one buffer can be queued for writing at a time.
 */
int
sockets_queuewrite(struct graphhost_t *inst, struct socketconn_t *conn, uint8_t *buf, size_t buflength)
{
	int error;
	struct writebuf_t *writebuf;

	writebuf = malloc(sizeof(struct writebuf_t));
	writebuf->buf = malloc(buflength);
	writebuf->buflength = buflength;
	memcpy(writebuf->buf, buf, buflength);
	error = queue_queue(conn->queue, writebuf);
	if(error != 0) {
		free(writebuf->buf);
		free(writebuf);
		return 0;
	}

	/* Select on write */
	conn->selectflags |= SOCKET_WRITE;
	write(inst->ipcfd_w, "r", 1);

	return 0;
}

void *
sockets_threadmain(void *arg)
{
	int listenfd;
	struct graphhost_t *inst = arg;
	struct socketconn_t *conn;
	struct list_elem_t *elem;
	int maxfd;
	int fd;
	uint8_t ipccmd;

	fd_set readfds;
	fd_set writefds;
	fd_set errorfds;

	pthread_mutex_init(&inst->mutex, NULL);

	/* Create a list to store all the open connections */
	inst->connlist = list_create();

	/* Listen on a socket */
	listenfd = sockets_listen_int(inst->port, AF_INET, 0x00000000);

	/* Set the running flag after we've finished initializing everything */
	inst->running = 1;

	while(1) {

		/* Clear the fdsets */
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_ZERO(&errorfds);

		/* Reset the maxfd */
		maxfd = listenfd;

		/* Add the file descriptors to the list */
		pthread_mutex_lock(&inst->mutex);
		for(elem = inst->connlist->start; elem != NULL;
			elem = elem->next) {
			conn = elem->data;
			fd = conn->fd;

			if(conn->orphan == 1) continue;

			if(maxfd < fd)
				maxfd = fd;
			if(conn->selectflags & SOCKET_READ)
				FD_SET(fd, &readfds);
			if(conn->selectflags & SOCKET_WRITE)
				FD_SET(fd, &writefds);
			if(conn->selectflags & SOCKET_ERROR)
				FD_SET(fd, &errorfds);
		}
		pthread_mutex_unlock(&inst->mutex);

		/* Select on the listener fd */
		FD_SET(listenfd, &readfds);

		/* ipcfd will recieve data when the thread needs to exit */
		FD_SET(inst->ipcfd_r, &readfds);

		/* Select on the file descrpitors */
		select(maxfd+1, &readfds, &writefds, &errorfds, NULL);

		pthread_mutex_lock(&inst->mutex);
		for(elem = inst->connlist->start; elem != NULL;
			elem = elem->next) {
			conn = elem->data;
			fd = conn->fd;

			if(FD_ISSET(fd, &readfds)) {
				/* Handle reading */
				sockets_readh(inst->connlist, elem);
			}
			if(FD_ISSET(fd, &writefds)) {
				/* Handle writing */
				sockets_writeh(inst->connlist, elem);
			}
			if(FD_ISSET(fd, &errorfds)) {
				/* Handle errors */
				sockets_close(inst->connlist, elem);
			}
		}

		/* Close all the file descriptors marked for closing */
		sockets_clear_orphans(inst->connlist);
		pthread_mutex_unlock(&inst->mutex);

		/* Check for listener condition */
		if(FD_ISSET(listenfd, &readfds)) {
			/* Accept connections */
			sockets_accept(inst->connlist, listenfd);
		}

		/* Handle IPC commands */
		if(FD_ISSET(inst->ipcfd_r, &readfds)) {
			read(inst->ipcfd_r, &ipccmd, 1);
			if(ipccmd == 'x') {
				break;
			}
		}
	}

	/* We're done, clear the running flag and clean up */
	inst->running = 0;
	pthread_mutex_lock(&inst->mutex);

	/* Mark all the open file descriptors for closing */
	for(elem = inst->connlist->start; elem != NULL;
		elem = elem->next) {
		sockets_close(inst->connlist, elem);
		/* We don't need to delete the element form the
		   because we just delete all of them below. */
	}

	/* Actually close all the open file descriptors */
	sockets_clear_orphans(inst->connlist);

	/* Delete the list */
	list_destroy(inst->connlist);

	/* Close the listener file descriptor */
	close(listenfd);

	/* Destroy the mutex */
	pthread_mutex_unlock(&inst->mutex);
	pthread_mutex_destroy(&inst->mutex);

	pthread_exit(NULL);
	return NULL;
}

int
GraphHost_graphData(float x, float y, char *dataset, struct graphhost_t *graphhost)
{
	struct list_elem_t *elem;
	struct list_elem_t *datasetp;
	struct socketconn_t *conn;
	struct graph_payload_t payload;
	struct graph_payload_t* qpayload;
	char *dataset_str;
	uint32_t tmp;

	if(!graphhost->running) return -1;

	/* Change to network byte order */
	tmp = htonl(*((int *)&x));
	payload.x = *((float *)&tmp);
	tmp = htonl(*((int *)&y));
	payload.y = *((float *)&tmp);
	strncpy(payload.dataset, dataset, 15);

	/* Giant lock approach */
	pthread_mutex_lock(&graphhost->mutex);

	for(elem = graphhost->connlist->start; elem != NULL; elem = elem->next) {
		conn = elem->data;
		for(datasetp = conn->datasets->start; datasetp != NULL; datasetp = datasetp->next) {
			dataset_str = datasetp->data;
			if(dataset_str != NULL && strcmp(dataset_str, dataset) == 0) {
				/* Send the value off */
				qpayload = malloc(sizeof(struct graph_payload_t));
				memcpy(qpayload, &payload, sizeof(struct graph_payload_t));
				sockets_queuewrite(graphhost, conn, (void *)&payload, sizeof(struct graph_payload_t));
			}
		}
	}

	pthread_mutex_unlock(&graphhost->mutex);

	return 0;
}

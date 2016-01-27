#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include <cci.h>

#include "protocol.h"

// XXX This is test purpose
int iters = 10;
int send_done = 0;
int recv_done = 0;
static int i = 0;
char buffer[8192];
cci_conn_attribute_t attr = CCI_CONN_ATTR_RO;

// Basic setting for CCI
uint32_t caps = 0;
cci_endpoint_t *endpoint = NULL;
cci_os_handle_t *fd = NULL;
cci_connection_t *connection = NULL;
int done = 0;

// Setting for client
const uint32_t timeout = 30 * 1000000;
char *server_uri = NULL;
int flags = 0;

static void error_handling(int errcode, const char* funcname)
{
	if (errcode) {
		fprintf(stderr, "%s() failed with %s\n", funcname,
				cci_strerror(NULL, errcode));
		exit(EXIT_FAILURE);
	}
}

static void parse_opts(int argc, char** argv)
{
	int ret;
	int c;

	while ((c = getopt(argc, argv, "h:c:b")) != -1) {
		switch (c) {
		case 'h':
			server_uri = strdup(optarg);
			break;
		case 'c':
			if (strncasecmp("ru", optarg, 2) == 0) {
				attr = CCI_CONN_ATTR_RU;
			} else if (strncasecmp("ro", optarg, 2) == 0) {
				attr = CCI_CONN_ATTR_RO;
			} else if (strncasecmp("uu", optarg, 2) == 0) {
				attr = CCI_CONN_ATTR_UU;
			}
			break;
		case 'b':
			flags |= CCI_FLAG_BLOCKING;
			break;
		default:
			fprintf(stderr, "usage: %s -h <server_uri> [-c <type>]\n",
			        argv[0]);
			fprintf(stderr, "\t-c\tConnection type (UU, RU, or RO) "
			                "set by client; RO by default\n");
			exit(EXIT_FAILURE);
		}
	}

	if (!server_uri) {
		fprintf(stderr, "usage: %s -h <server_uri> [-c <type>]\n", argv[0]);
		fprintf(stderr, "\t-c\tConnection type (UU, RU, or RO) "
                                        "set by client; RO by default\n");
		exit(EXIT_FAILURE);
	}

	ret = cci_set_opt(endpoint, CCI_OPT_ENDPT_SEND_TIMEOUT, &timeout);
	error_handling(ret, "cci_set_opt");
}

static void send_callback(cci_event_t *event)
{
	fprintf(stderr, "send %d completed with %d\n",
			(int)((uintptr_t) event->send.context),
			event->send.status);

	assert(event->send.context == (void*)(uintptr_t)i);
	i++;
	assert(event->send.connection == connection);
	assert(event->send.connection->context == (void*)Connect);

	if (done == 0) {
		send_done++;
	} else if (done == 1) {
		done = 2;
	}
}

static void recv_callback(cci_event_t *event)
{
	int len = event->recv.len;

	assert(event->recv.connection == connection);
	assert(event->recv.connection->context == (void*)Connect);

	memcpy(buffer, event->recv.ptr, len);
	buffer[len] = '\0';
	fprintf(stderr, "received \"%s\"\n", buffer);

	recv_done++;
}

static void connect_callback(cci_event_t *event)
{
	done = 1;

	assert(event->connect.connection != NULL);
	assert(event->connect.connection->context == (void*)Connect);

	connection = event->connect.connection;
}

static void
poll_events()
{
	int ret;
	cci_event_t *event;

	ret = cci_get_event(endpoint, &event);
	if (ret == CCI_SUCCESS && event) {
		switch (event->type) {
		case CCI_EVENT_SEND:
			send_callback(event);
			break;
		case CCI_EVENT_RECV:
			recv_callback(event);
			break;
		case CCI_EVENT_CONNECT:
			connect_callback(event);
			break;
		default:
			fprintf(stderr, "ignoring event type %d\n",
					event->type);
		}

		cci_return_event(event);

		if (done == 0 && send_done == iters && recv_done == iters) {
			done = 1;
		}
	}
}

static void do_client()
{
	int ret;
	int i;

	while (!done) {
		poll_events();
	}

	if (!connection) {
		exit(0);
	}

	done = 0;

	for (i = 0; i < iters; ++i) {
		char data[128];

		memset(data, 0, sizeof(data));
		sprintf(data, "%4d", i);
		sprintf(data + 4, "Hello World!");
		
		ret = cci_send(connection, data, (int32_t) strlen(data) + 4,
				(void*)(uintptr_t)i, flags);
		error_handling(ret, "cci_send");

		if (flags & CCI_FLAG_BLOCKING) {
			fprintf(stderr, "send %d completed with %d\n", i, ret);
		}
	}

	while (!done) {
		poll_events();
	}

	ret = cci_send(connection, "bye", 3, (void*)(uintptr_t) iters, flags);
	error_handling(ret, "bye - cci_send");

	if (flags & CCI_FLAG_BLOCKING) {
		done = 2;
	}

	while (done != 2) {
		poll_events();
	}
}

int main(int argc, char** argv)
{
	int ret;

	ret = cci_init(CCI_ABI_VERSION, 0, &caps);
	error_handling(ret, "cci_init");

	ret = cci_create_endpoint(NULL, 0, &endpoint, fd);
	error_handling(ret, "cci_create_endpoint");

	parse_opts(argc, argv);

	ret = cci_connect(endpoint, server_uri, "Hello World!", 12,
			attr, (void*)Connect, 0, NULL);
	error_handling(ret, "cci_connect");

	do_client();

	ret = cci_destroy_endpoint(endpoint);
	error_handling(ret, "cci_destroy_endpoint");

	ret = cci_finalize();
	error_handling(ret, "cci_finalize");

	return 0;
}

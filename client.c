#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include <cci.h>
#include <pthread.h>

#include "protocol.h"

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
static unsigned _client_id = 0xdeadbeef;
static pthread_t _polling_thread_id;

char buffer[8192];

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
	// unused
	(void) event;
}

static void recv_callback(cci_event_t *event)
{
	int len = event->recv.len;

	assert(event->recv.connection == connection);
	assert(event->recv.connection->context == (void*)(uintptr_t)_client_id);

	memcpy(buffer, event->recv.ptr, len);
	buffer[len] = '\0';
	fprintf(stderr, "server: %s\n", buffer);
}

static void connect_callback(cci_event_t *event)
{
	assert(event->connect.connection != NULL);
	assert(event->connect.connection->context == (void*)(uintptr_t)_client_id);

	connection = event->connect.connection;
}

static void* polling_thread(void* args)
{
	int ret;
	cci_event_t *event;

	while (!done) {
		ret = cci_get_event(endpoint, &event);
		if (ret != CCI_SUCCESS) {
			if (ret != CCI_EAGAIN) {
				error_handling(ret, "cci_get_event");
			}

			continue;
		}

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
			break;
		}

		cci_return_event(event);
	}

	pthread_exit(NULL);
}

static void do_client()
{
	int ret;
	int i;
	int transaction_id = 0xbeafbeaf;
	const int k_quit_message_id = 0xcbbccbcb;

	pthread_create(&_polling_thread_id, NULL, polling_thread, NULL);

	while (!done) {
		char msg[BUFSIZ];
		fgets(msg, BUFSIZ, stdin);

		if (strncmp(msg, "quit", strlen("quit")) == 0) {
			ret = cci_send(connection, "quit", 4, (void*)(uintptr_t) k_quit_message_id, flags);
			error_handling(ret, "bye - cci_send");

			done = 1;
			break;
		}

		ret = cci_send(connection, msg, (int32_t) strlen(msg),
				(void*)(uintptr_t)transaction_id++, flags);
		error_handling(ret, "cci_send");
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

	ret = cci_connect(endpoint, server_uri, "connect", 7,
			attr, (void*)(uintptr_t)_client_id, 0, NULL);
	error_handling(ret, "cci_connect");

	do_client();

	ret = cci_destroy_endpoint(endpoint);
	error_handling(ret, "cci_destroy_endpoint");

	ret = cci_finalize();
	error_handling(ret, "cci_finalize");

	return 0;
}

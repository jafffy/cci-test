#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <cci.h>
#include <pthread.h>

#include "protocol.h"

uint32_t caps = 0;
cci_endpoint_t *endpoint = NULL;
cci_os_handle_t *ep_fd = NULL;
cci_connection_t *connection = NULL;
int accept = 1;
int done = 0;
pthread_t _polling_thread_id;

static void error_handling(int errcode, const char* funcname)
{
	if (errcode) {
		fprintf(stderr, "%s() failed with %s\n", funcname,
				cci_strerror(NULL, errcode));
		exit(EXIT_FAILURE);
	}
}

static void parse_opts(cci_endpoint_t* endpoint)
{
	int ret;
	char* uri;

	ret = cci_get_opt(endpoint, CCI_OPT_ENDPT_URI, &uri);
	error_handling(ret, "cci_get_opt");

	fprintf(stderr, "Opened %s\n", uri);

	free(uri);
}

static void recv_callback(cci_event_t *event)
{
	int ret;
	char buf[8192];
	int len = event->recv.len;

	assert(event->recv.connection == connection);
	assert(event->recv.connection->context == (void*)Accept);

	if (len == 3) {
		done = 1;
		return;
	}

	memset(buf, 0, 8192);
	memcpy(buf, event->recv.ptr, len);
	fprintf(stderr, "client: %s\n", buf);
}

static void send_callback(cci_event_t *event)
{
	assert(event->send.connection == connection);
	assert(event->send.connection->context == (void*)Accept);
}

static void connect_request_callback(cci_event_t *event)
{
	if (accept) {
		cci_accept(event, (void*)Accept);
	} else {
		cci_reject(event);
	}
}

static void accept_callback(cci_event_t *event)
{
	assert(event->accept.connection != NULL);
	assert(event->accept.connection->context == (void*)Accept);
	
	connection = event->accept.connection;
}

static void* polling_thread(void* args)
{
	int ret;

	while (!done) {
		cci_event_t *event;

		ret = cci_get_event(endpoint, &event);
		if (ret != CCI_SUCCESS) {
			if (ret != CCI_EAGAIN) {
				error_handling(ret, "cci_get_event");
			}

			continue;
		}

		switch (event->type) {
		case CCI_EVENT_RECV:
			recv_callback(event);
			break;
		case CCI_EVENT_SEND:
			send_callback(event);
			break;
		case CCI_EVENT_CONNECT_REQUEST:
			connect_request_callback(event);
			break;
		case CCI_EVENT_ACCEPT:
			accept_callback(event);
			break;
		default:
			fprintf(stderr, "event type %d\n", event->type);
			break;
		}

		cci_return_event(event);
	}

	pthread_exit(NULL);
}

static void do_server()
{
	int ret;
	int transaction_id = 0xdeadbeaf;

	pthread_create(&_polling_thread_id, NULL, polling_thread, NULL);

	while (!done) {
		char msg[BUFSIZ];
		scanf("%s", msg);

		ret = cci_send(connection, msg, (int32_t)strlen(msg),
				(void*)(uintptr_t)transaction_id++, 0);
		error_handling(ret, "cci_send");
	}
}

int main()
{
	int ret;

	ret = cci_init(CCI_ABI_VERSION, 0, &caps);
	error_handling(ret, "cci_init");

	ret = cci_create_endpoint(NULL, 0, &endpoint, ep_fd);
	error_handling(ret, "cci_create_endpoint");

	// Parse options
	parse_opts(endpoint);

	do_server();

	cci_destroy_endpoint(endpoint);
	cci_finalize();

	return 0;
}

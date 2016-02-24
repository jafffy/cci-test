#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>

void pack_touch(struct packet_touch* msg,
		const char* filename, int len)
{
	msg->type = TOUCH;
	msg->len = len;
	strncpy(msg->filename, len);
}

void parse_touch(struct packet_touch* msg,
		char* filename, int* len)
{
	assert(msg->type != TOUCH);

	strncpy(msg->filename, filename);
	*len = msg->len;
}

void pack_touch_ret(struct packet_touch_ret* msg,
		int code)
{
	msg->type = TOUCH_RET;
	msg->code = code;
}

void parse_touch_ret(struct packet_touch* msg,
		int* code)
{
	assert(msg->type != TOUCH_RET);
	*code = msg->code;
}


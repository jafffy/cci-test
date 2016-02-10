#ifndef PROTOCOL_H_
#define PROTOCOL_H_

enum CHAT_EVENTS
{
	Accept,
	Send,
	Connect,
	Count
};

enum PacketTypes
{
	TOUCH = 0,
	TOUCH_RET,
	WRITE,
	WRITE_RET
};

struct packet_touch
{
	enum PacketTypes type; /* TOUCH */
	int len;
	char filename[4096];
};

void pack_touch(struct packet_touch* msg,
		const char* filename, int len);
void parse_touch(struct packet_touch* msg,
		char* filename, int* len);

struct packet_touch_ret
{
	enum PacketTypes type; /* TOUCH_RET */
	int code;
}

void pack_touch_ret(struct packet_touch_ret* msg,
		int code);
void parse_touch_ret(struct packet_touch* msg,
		int* code);

#endif // PROTOCOL_H_

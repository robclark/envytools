#ifndef DEMMT_PUSHBUF_H
#define DEMMT_PUSHBUF_H

#include <stdint.h>
#include "rnndec.h"

#define ADDR_CACHE_SIZE 400
struct cache_entry
{
	int mthd;
	struct rnndecaddrinfo *info;
	struct cache_entry *next;
};

#define OBJECT_SIZE (0x8000 * 4)

struct obj
{
	uint32_t handle;
	uint32_t class;
	uint32_t name;
	char *desc;
	struct rnndeccontext *ctx;
	const struct gpu_object_decoder *decoder;
	struct cache_entry *cache[ADDR_CACHE_SIZE];
	uint32_t *data;
	struct gpu_object *gpu_object;
};

#define MAX_OBJECTS 256
struct fifo_state
{
	struct
	{
		uint64_t addr;
		uint64_t entries;
	}
	ib;

	struct
	{
		uint32_t addr;
	}
	user;

	struct obj *subchans[8];
	struct obj objects[MAX_OBJECTS];
};
struct fifo_state *get_fifo_state(struct gpu_object *fifo);

struct pushbuf_decode_state
{
	int incr;
	int subchan;
	int addr;
	int size;
	int skip;
	uint32_t next_command_offset;
	int long_command;
	int mthd;
	int mthd_data_available;
	uint32_t mthd_data;

	struct gpu_object *fifo;
};

struct ib_decode_state
{
	int word;

	uint64_t address;
	int unk8;
	int not_main;
	int size;
	int no_prefetch;

	struct pushbuf_decode_state pstate;

	struct gpu_mapping *gpu_mapping;
};

struct user_decode_state
{
	uint32_t prev_dma_put;
	uint32_t dma_put;
	struct gpu_mapping *last_gpu_mapping;
	struct pushbuf_decode_state pstate;
};
void pushbuf_add_object(uint32_t handle, uint32_t class, struct gpu_object *gpu_obj);
void pushbuf_add_object_name(uint32_t handle, uint32_t fifo_name, struct gpu_object *gpu_obj);

uint64_t pushbuf_decode(struct pushbuf_decode_state *state, uint32_t data, char *output, int safe);

uint64_t pushbuf_print(struct pushbuf_decode_state *pstate, struct gpu_mapping *gpu_mapping, uint64_t gpu_address, int commands);

void ib_decode(struct ib_decode_state *state, uint32_t data, char *output);
void ib_decode_end(struct ib_decode_state *state);

void user_decode(struct user_decode_state *state, uint32_t addr, uint32_t data, char *output);
void user_decode_end(struct user_decode_state *state);

void decode_method_raw(int mthd, uint32_t data, struct obj *obj, char *dec_obj,
		char *dec_mthd, char *dec_val);

struct obj **get_subchans(struct pushbuf_decode_state *pstate);
struct obj *current_subchan_object(struct pushbuf_decode_state *pstate);

#endif

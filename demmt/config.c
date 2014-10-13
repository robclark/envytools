/*
 * Copyright (C) 2014 Marcin Ślusarz <marcin.slusarz@gmail.com>.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

// for gnu version of basename (string.h)
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "buffer.h"
#include "config.h"
#include "object_state.h"
#include "macro.h"
#include "nvrm.h"

int dump_raw_ioctl_data = 0;
int dump_decoded_ioctl_data = 1;
int dump_tsc = 1;
int dump_tic = 1;
int dump_vp = 1;
int dump_fp = 1;
int dump_gp = 1;
int dump_cp = 1;
int dump_tep = 1;
int dump_tcp = 1;
int dump_buffer_usage = 1;
int decode_pb = 1;
int dump_sys_mmap = 1;
int dump_sys_munmap = 1;
int dump_sys_mremap = 1;
int dump_sys_open = 1;
int dump_msg = 1;
int dump_sys_write = 1;
int print_gpu_addresses = 0;
int pager_enabled = 1;

int chipset;
int ib_supported;
int indent_logs = 0;
int is_nouveau = 0;
int dump_memory_writes = 1;
int dump_memory_reads = 1;
int info = 1;

static void usage()
{
	fflush(stdout);
	fprintf(stderr, "Usage: demmt [OPTION]\n"
			"Decodes binary trace files generated by Valgrind MMT. Reads standard input or file passed by -l.\n\n"
			"  -l file\t\tuse \"file\" as input\n"
			"         \t\t- it can be compressed by gzip, bzip2 or xz\n"
			"         \t\t- demmt extracts chipset version from characters following \"nv\"\n"
			"  -m 'chipset'\t\tset chipset version (required, but see -l)\n"
			"  -q\t\t\t(quiet) print only the most important data (= -d all -e pb,shader,macro,tsc,tic,cp,classes=all,buffer-usage,nvrm-class-desc)\n"
			"  -c 0/1\t\tdisable/enable colors (default: 1)\n"
			"  -g 0/1\t\t= -d/-e gpu-addr (default: 0)\n"
			"  -o 0/1\t\t= -d/-e ioctl-raw (default: 0)\n"
			"  -r 0/1\t\t= -d/-e macro-rt-verbose (default: 0)\n"
			"  -p 0/1\t\tdisable/enable pager (default: 1 if stdout is a terminal)\n"
			"  -i 0/1\t\tdisable/enable log indentation (default: 0)\n"
			"  -f\t\t\tfind possible pushbuf pointers (IB / USER)\n"
			"  -n id[,offset]\tset pushbuf pointer to \"id\" and optionally offset within this buffer to \"offset\"\n"
			"  -a\t\t\t= -d classes=all\n"
			"  -x\t\t\tforce pushbuf decoding even without pushbuf pointer\n"
			"\n"
			"  -d msg_type1[,msg_type2[,msg_type3....]] - disable messages\n"
			"  -e msg_type1[,msg_type2[,msg_type3....]] - enable messages\n"
			"     message types:\n"
			"     - write - memory write\n"
			"     - read - memory read\n"
			"     - gpu-addr - gpu address\n"
			"     - mem = read,write\n"
			"     - pb - push buffer\n"
			"     - class=[all,0x...] - class decoder\n"
			"     - tsc - texture sampler control block\n"
			"     - tic - texture image control block\n"
			"     - vp - vertex program\n"
			"     - fp - fragment program\n"
			"     - gp - geometry program\n"
			"     - cp - compute program\n"
			"     - tep\n"
			"     - tcp\n"
			"     - shader = vp,fp,gp,tep,tcp\n"
			"     - macro-rt-verbose - verbose macro interpreter\n"
			"     - macro-rt - macro interpreter \n"
			"     - macro-dis - macro disasm\n"
			"     - macro = macro-rt,macro-dis\n"
			"     - sys_mmap\n"
			"     - sys_munmap\n"
			"     - sys_mremap\n"
			"     - sys_open\n"
			"     - sys_write\n"
			"     - sys = sys_mmap,sys_munmap,sys_mremap,sys_open,sys_write,ioctl-desc\n"
			"     - ioctl-raw - raw ioctl data\n"
			"     - ioctl-desc - decoded ioctl\n"
			"     - ioctl = ioctl-raw,ioctl-desc\n"
			"     - nvrm-ioctl=[all,name] name=create,call,host_map,etc...\n"
			"     - nvrm-mthd=[all,0x...] - method call\n"
			"     - nvrm-handle-desc - handle description\n"
			"     - nvrm-class-desc - class description\n"
			"     - nvrm-unk-0-fields - unk zero fields\n"
			"     - nvrm = nvrm-ioctl=all,nvrm-mthd=all,nvrm-handle-desc,nvrm-class-desc\n"
			"     - buffer-usage\n"
			"     - msg - textual valgrind message\n"
			"     - info - various informations\n"
			"     - all - everything above\n"
			"\n");
	exit(1);
}

#define DEF_INT_FUN(name, var) static void _filter_##name(int en) { var = en; }

DEF_INT_FUN(info, info);
DEF_INT_FUN(write, dump_memory_writes);
DEF_INT_FUN(read, dump_memory_reads);
DEF_INT_FUN(gpu_addr, print_gpu_addresses);
DEF_INT_FUN(ioctl_raw, dump_raw_ioctl_data);
DEF_INT_FUN(ioctl_desc, dump_decoded_ioctl_data);
DEF_INT_FUN(tsc, dump_tsc);
DEF_INT_FUN(tic, dump_tic);
DEF_INT_FUN(vp, dump_vp);
DEF_INT_FUN(fp, dump_fp);
DEF_INT_FUN(gp, dump_gp);
DEF_INT_FUN(cp, dump_cp);
DEF_INT_FUN(tep, dump_tep);
DEF_INT_FUN(tcp, dump_tcp);
DEF_INT_FUN(macro_rt_verbose, macro_rt_verbose);
DEF_INT_FUN(macro_rt, macro_rt);
DEF_INT_FUN(macro_dis_enabled, macro_dis_enabled);
DEF_INT_FUN(buffer_usage, dump_buffer_usage);
DEF_INT_FUN(decode_pb, decode_pb);
DEF_INT_FUN(sys_mmap, dump_sys_mmap);
DEF_INT_FUN(sys_munmap, dump_sys_munmap);
DEF_INT_FUN(sys_mremap, dump_sys_mremap);
DEF_INT_FUN(sys_open, dump_sys_open);
DEF_INT_FUN(sys_write, dump_sys_write);
DEF_INT_FUN(msg, dump_msg);
DEF_INT_FUN(nvrm_describe_handles, nvrm_describe_handles);
DEF_INT_FUN(nvrm_describe_classes, nvrm_describe_classes);
DEF_INT_FUN(nvrm_show_unk_zero_fields, nvrm_show_unk_zero_fields);
DEF_INT_FUN(pager_enabled, pager_enabled);

static void _filter_class(const char *token, int en)
{
	struct gpu_object_decoder *o;
	uint32_t class_ = strtoul(token, NULL, 16);

	for (o = obj_decoders; o->class_ != 0; o++)
		if (o->class_ == class_)
		{
			o->disabled = !en;
			break;
		}
}
static void _filter_all_classes(int en)
{
	struct gpu_object_decoder *o;

	for (o = obj_decoders; o->class_ != 0; o++)
		o->disabled = !en;
}

static void _filter_nvrm_mthd(const char *token, int en)
{
	uint32_t mthd = strtoul(token, NULL, 16);
	int i;

	for (i = 0; i < nvrm_mthds_cnt; ++i)
		if (nvrm_mthds[i].mthd == mthd)
		{
			nvrm_mthds[i].disabled = !en;
			break;
		}
}

static void _filter_all_nvrm_mthds(int en)
{
	int i;
	for (i = 0; i < nvrm_mthds_cnt; ++i)
		nvrm_mthds[i].disabled = !en;
}

static void _filter_nvrm_ioctl(const char *token, int en)
{
	int i;
	for (i = 0; i < nvrm_ioctls_cnt; ++i)
		if (strcasecmp(nvrm_ioctls[i].name + strlen("NVRM_IOCTL_"), token) == 0)
		{
			nvrm_ioctls[i].disabled = !en;
			break;
		}
}

static void _filter_all_nvrm_ioctls(int en)
{
	int i;
	for (i = 0; i < nvrm_ioctls_cnt; ++i)
		nvrm_ioctls[i].disabled = !en;
}

static void handle_filter_opt(const char *_arg, int en)
{
	char *arg = strdup(_arg);
	char *tokens = arg;

	while (tokens)
	{
		char *token = strsep(&tokens, ",");
		if (strcmp(token, "write") == 0)
			_filter_write(en);
		else if (strcmp(token, "read") == 0)
			_filter_read(en);
		else if (strcmp(token, "gpu-addr") == 0)
			_filter_gpu_addr(en);
		else if (strcmp(token, "mem") == 0)
		{
			_filter_write(en);
			_filter_read(en);
		}
		else if (strcmp(token, "ioctl-raw") == 0)
			_filter_ioctl_raw(en);
		else if (strcmp(token, "ioctl-desc") == 0)
			_filter_ioctl_desc(en);
		else if (strcmp(token, "ioctl") == 0)
		{
			if (!en)
				_filter_ioctl_raw(en);
			_filter_ioctl_desc(en);
		}
		else if (strcmp(token, "tsc") == 0)
			_filter_tsc(en);
		else if (strcmp(token, "tic") == 0)
			_filter_tic(en);
		else if (strcmp(token, "vp") == 0)
			_filter_vp(en);
		else if (strcmp(token, "fp") == 0)
			_filter_fp(en);
		else if (strcmp(token, "gp") == 0)
			_filter_gp(en);
		else if (strcmp(token, "cp") == 0)
			_filter_cp(en);
		else if (strcmp(token, "tep") == 0)
			_filter_tep(en);
		else if (strcmp(token, "tcp") == 0)
			_filter_tcp(en);
		else if (strcmp(token, "shader") == 0)
		{
			_filter_vp(en);
			_filter_fp(en);
			_filter_gp(en);
			_filter_tcp(en);
			_filter_tep(en);
		}
		else if (strcmp(token, "macro-rt-verbose") == 0)
			_filter_macro_rt_verbose(en);
		else if (strcmp(token, "macro-rt") == 0)
			_filter_macro_rt(en);
		else if (strcmp(token, "macro-dis") == 0)
			_filter_macro_dis_enabled(en);
		else if (strcmp(token, "macro") == 0)
		{
			if (!en)
				_filter_macro_rt_verbose(en);
			_filter_macro_rt(en);
			_filter_macro_dis_enabled(en);
		}
		else if (strcmp(token, "buffer-usage") == 0)
			_filter_buffer_usage(en);
		else if (strncmp(token, "class=", 6) == 0)
		{
			if (strcmp(token + 6, "all") == 0)
				_filter_all_classes(en);
			else
				_filter_class(token + 6, en);
		}
		else if (strcmp(token, "pb") == 0)
			_filter_decode_pb(en);
		else if (strcmp(token, "sys_mmap") == 0)
			_filter_sys_mmap(en);
		else if (strcmp(token, "sys_munmap") == 0)
			_filter_sys_munmap(en);
		else if (strcmp(token, "sys_mremap") == 0)
			_filter_sys_mremap(en);
		else if (strcmp(token, "sys_open") == 0)
			_filter_sys_open(en);
		else if (strcmp(token, "msg") == 0)
			_filter_msg(en);
		else if (strcmp(token, "sys_write") == 0)
			_filter_sys_write(en);
		else if (strcmp(token, "sys") == 0)
		{
			_filter_sys_mmap(en);
			_filter_sys_munmap(en);
			_filter_sys_mremap(en);
			_filter_sys_open(en);
			_filter_sys_write(en);

			if (!en)
				_filter_ioctl_raw(en);
			_filter_ioctl_desc(en);
		}
		else if (strcmp(token, "nvrm-handle-desc") == 0)
			_filter_nvrm_describe_handles(en);
		else if (strcmp(token, "nvrm-class-desc") == 0)
			_filter_nvrm_describe_classes(en);
		else if (strncmp(token, "nvrm-mthd=", 10) == 0)
		{
			if (strcmp(token + 10, "all") == 0)
				_filter_all_nvrm_mthds(en);
			else
				_filter_nvrm_mthd(token + 10, en);
			if (en)
			{
				_filter_nvrm_ioctl("call", en);
				_filter_ioctl_desc(en);
			}
		}
		else if (strcmp(token, "nvrm-unk-0-fields") == 0)
			_filter_nvrm_show_unk_zero_fields(en);
		else if (strncmp(token, "nvrm-ioctl=", 11) == 0)
		{
			if (strcmp(token + 11, "all") == 0)
				_filter_all_nvrm_ioctls(en);
			else
				_filter_nvrm_ioctl(token + 11, en);
			if (en)
				_filter_ioctl_desc(en);
		}
		else if (strcmp(token, "nvrm") == 0)
		{
			if (en)
				_filter_ioctl_desc(en);
			_filter_all_nvrm_ioctls(en);
			_filter_all_nvrm_mthds(en);
			_filter_nvrm_describe_handles(en);
			_filter_nvrm_describe_classes(en);
		}
		else if (strcmp(token, "info") == 0)
			_filter_info(en);
		else if (strcmp(token, "all") == 0)
		{
			_filter_info(en);
			_filter_write(en);
			_filter_read(en);
			_filter_gpu_addr(en);
			_filter_ioctl_raw(en);
			_filter_ioctl_desc(en);
			_filter_tsc(en);
			_filter_tic(en);
			_filter_vp(en);
			_filter_fp(en);
			_filter_gp(en);
			_filter_cp(en);
			_filter_tep(en);
			_filter_tcp(en);
			_filter_macro_rt_verbose(en);
			_filter_macro_rt(en);
			_filter_macro_dis_enabled(en);
			_filter_buffer_usage(en);
			_filter_all_classes(en);
			_filter_decode_pb(en);
			_filter_sys_mmap(en);
			_filter_sys_munmap(en);
			_filter_sys_mremap(en);
			_filter_sys_open(en);
			_filter_msg(en);
			_filter_sys_write(en);
			_filter_nvrm_describe_handles(en);
			_filter_nvrm_describe_classes(en);
			_filter_all_nvrm_mthds(en);
			_filter_nvrm_show_unk_zero_fields(en);
			_filter_all_nvrm_ioctls(en);
		}
		else
		{
			fprintf(stderr, "unknown token: %s\n", token);
			fflush(stderr);
			exit(1);
		}
	}

	free(arg);
}

char *read_opts(int argc, char *argv[])
{
	char *filename = NULL;
	if (argc < 2)
		usage();

	pager_enabled = isatty(1);

	int c;
	while ((c = getopt (argc, argv, "m:n:o:g:fqac:l:i:xr:he:d:p:")) != -1)
	{
		switch (c)
		{
			case 'm':
			{
				char *chip = optarg;
				if (strncasecmp(chip, "NV", 2) == 0)
					chip += 2;
				chipset = strtoul(chip, NULL, 16);
				break;
			}
			case 'n':
			{
				char *endptr;
				pb_pointer_buffer = strtoul(optarg, &endptr, 10);
				if (endptr && endptr[0] == ',')
					pb_pointer_offset = strtoul(endptr + 1, NULL, 0);
				break;
			}
			case 'o':
				if (optarg[0] == '1')
					_filter_ioctl_raw(1);
				else if (optarg[0] == '0')
					_filter_ioctl_raw(0);
				else
				{
					fprintf(stderr, "-o accepts only 0 and 1\n");
					exit(1);
				}
				break;
			case 'g':
				if (optarg[0] == '1')
					_filter_gpu_addr(1);
				else if (optarg[0] == '0')
					_filter_gpu_addr(0);
				else
				{
					fprintf(stderr, "-g accepts only 0 and 1\n");
					exit(1);
				}
				break;
			case 'f':
				find_pb_pointer = 1;
				handle_filter_opt("all", 0);
				break;
			case 'q':
				handle_filter_opt("all", 0);
				_filter_decode_pb(1);
				_filter_tsc(1);
				_filter_tic(1);
				_filter_cp(1);
				handle_filter_opt("shader", 1);
				handle_filter_opt("macro", 1);
				_filter_buffer_usage(1);
				_filter_all_classes(1);
				_filter_nvrm_describe_classes(1);
				break;
			case 'a':
				_filter_all_classes(0);
				break;
			case 'c':
				if (optarg[0] == '1')
					colors = &envy_def_colors;
				else if (optarg[0] == '0')
					colors = &envy_null_colors;
				else
				{
					fprintf(stderr, "-c accepts only 0 and 1\n");
					exit(1);
				}
				break;
			case 'l':
			{
				filename = strdup(optarg);
				const char *base = basename(filename);
				if (chipset == 0 && strncasecmp(base, "nv", 2) == 0)
					chipset = strtoul(base + 2, NULL, 16);
				break;
			}
			case 'i':
				if (optarg[0] == '1')
					indent_logs = 1;
				else if (optarg[0] == '0')
					indent_logs = 0;
				else
				{
					fprintf(stderr, "-i accepts only 0 and 1\n");
					exit(1);
				}
				break;
			case 'x':
				force_pushbuf_decoding = 1;
				break;
			case 'r':
				if (optarg[0] == '1')
					_filter_macro_rt_verbose(1);
				else if (optarg[0] == '0')
					_filter_macro_rt_verbose(0);
				else
				{
					fprintf(stderr, "-r accepts only 0 and 1\n");
					exit(1);
				}
				break;
			case 'p':
				if (optarg[0] == '1')
					_filter_pager_enabled(1);
				else if (optarg[0] == '0')
					_filter_pager_enabled(0);
				else
				{
					fprintf(stderr, "-p accepts only 0 and 1\n");
					exit(1);
				}
				break;
			case 'h':
			case '?':
				usage();
				break;
			case 'd':
				handle_filter_opt(optarg, 0);
				break;
			case 'e':
				handle_filter_opt(optarg, 1);
				break;
		}
	}

	if (chipset == 0)
		usage();
	ib_supported = chipset >= 0x80 || chipset == 0x50;

	if (!colors)
		colors = &envy_def_colors;

	return filename;
}

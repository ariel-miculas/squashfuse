/*
 * Copyright (c) 2014 Dave Vasilevsky <dave@vasilevsky.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "fuseprivate.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nonstd.h"

#if HAVE_DECL_FUSE_CMDLINE_HELP
    #include <fuse_lowlevel.h>
#endif

int sqfs_listxattr(sqfs *fs, sqfs_inode *inode, char *buf, size_t *size) {
	sqfs_xattr x;
	size_t count = 0;
	
	if (sqfs_xattr_open(fs, inode, &x))
		return -EIO;
	
	while (x.remain) {
		size_t n;
		if (sqfs_xattr_read(&x))
			 return EIO;
		n = sqfs_xattr_name_size(&x);
		count += n + 1;
		
		if (buf) {
			if (count > *size)
				return ERANGE;
			if (sqfs_xattr_name(&x, buf, true))
				return EIO;
			buf += n;
			*buf++ = '\0';
		}
	}
	*size = count;
	return 0;
}

void sqfs_minimal_fuse_usage() {
	fprintf(stderr, "\nSelection of FUSE options:\n");
	fprintf(stderr,"    -h   --help            print help\n");
	fprintf(stderr,"    -V   --version         print version\n");
	fprintf(stderr,"    -d   -o debug          enable debug output (implies -f)\n");
	fprintf(stderr,"    -f                     foreground operation\n");
}

void sqfs_usage(char *progname, bool fuse_usage, bool ll_usage) {
	fprintf(stderr, "%s (c) 2012 Dave Vasilevsky\n\n", PACKAGE_STRING);
	fprintf(stderr, "Usage: %s [options] ARCHIVE MOUNTPOINT\n",
		progname ? progname : PACKAGE_NAME);
	fprintf(stderr, "\n%s options:\n", progname);
	fprintf(stderr, "    -o offset=N            offset N bytes into ARCHIVE to mount\n");
	fprintf(stderr, "    -o subdir=PATH         mount subdirectory PATH of ARCHIVE\n");
	if (ll_usage) {
		fprintf(stderr, "    -o timeout=N           idle N seconds for automatic unmount\n");
		fprintf(stderr, "    -o uid=N               set file owner to uid N\n");
		fprintf(stderr, "    -o gid=N               set file group to gid N\n");
	}

	if (fuse_usage) {
		if (ll_usage) {
#ifdef SQFS_MULTITHREADED
#if HAVE_DECL_FUSE_CMDLINE_HELP
			fprintf(stderr, "\nFUSE options:\n");
			fuse_cmdline_help();
#else
			struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
			fuse_opt_add_arg(&args, ""); /* progname */
			fuse_opt_add_arg(&args, "-ho");
			fprintf(stderr, "\n");
			fuse_parse_cmdline(&args, NULL, NULL, NULL);
#endif
#else
			/* Skip the multithreaded options */
			sqfs_minimal_fuse_usage();
#endif
		} else {
			/* Standard FUSE help includes confusing 
			 * multi-threaded options so don't use it
			 */
			sqfs_minimal_fuse_usage();
			fprintf(stderr,"    -o allow_other         allow access by other users\n");
			fprintf(stderr,"    -o allow_root          allow access by the superuser\n");
		}
	}
	exit(-2);
}

int sqfs_opt_proc(void *data, const char *arg, int key,
		struct fuse_args *outargs) {
	sqfs_opts *opts = (sqfs_opts*)data;
	if (key == FUSE_OPT_KEY_NONOPT) {
		if (opts->mountpoint) {
			return -1; /* Too many args */
		} else if (opts->image) {
			opts->mountpoint = 1;
			return 1;
		} else {
			opts->image = arg;
			return 0;
		}
	} else if (key == FUSE_OPT_KEY_OPT) {
		if (strncmp(arg, "-h", 2) == 0 || strncmp(arg, "--h", 3) == 0)
			return -1;
	}
	return 1; /* Keep */
}

int sqfs_statfs(sqfs *sq, struct statvfs *st) {
	struct squashfs_super_block *sb = &sq->sb;

	st->f_bsize = sb->block_size;
	st->f_frsize = sb->block_size;
	st->f_blocks = ((sb->bytes_used - 1) >> sb->block_log) + 1;
	st->f_bfree = 0;
	st->f_bavail = 0;
	st->f_files = sb->inodes;
	st->f_ffree = 0;
	st->f_favail = 0;
	st->f_namemax = SQUASHFS_NAME_LEN;

	return 0;
}

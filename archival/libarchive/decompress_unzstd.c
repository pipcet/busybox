/*
 * This file uses ZSTD library code which is 
 * Copyright (C) Yann Collet, Facebook, Inc.
 * See the license block in zstd.h for more information.
 *
 * This file is:
 * Copyright (C) 2021 Jeff Pohlmeyer <yetanothergeek@gmail.com>
 * adapted from decompress_unxz.c which is
 * Copyright (C) 2010 Denys Vlasenko <vda.linux@googlemail.com>
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

#include "libbb.h"
#include "bb_archive.h"

#define ZSTD_LEGACY_SUPPORT 0
#define ZSTD_LIB_DEPRECATED 0
#define HUF_FORCE_DECOMPRESS_X1 1
#define ZSTD_FORCE_DECOMPRESS_SEQUENCES_SHORT 1
#define ZSTD_NO_INLINE 1
#define ZSTD_STRIP_ERROR_STRINGS 1
#define ZSTD_TRACE 0


#include "zstd/common/entropy_common.c"
#include "zstd/common/fse_decompress.c"
#include "zstd/common/zstd_common.c"
#include "zstd/common/xxhash.c"
#include "zstd/decompress/huf_decompress.c"
#include "zstd/decompress/zstd_ddict.c"
#include "zstd/decompress/zstd_decompress.c"
#include "zstd/decompress/zstd_decompress_block.c"


IF_DESKTOP(long long) int FAST_FUNC
unpack_zstd_stream(transformer_state_t *xstate)
{
	int skip = 0;
	IF_DESKTOP(long long) int total = 0;
	size_t chunk;
	ZSTD_inBuffer zin;
	ZSTD_outBuffer zout;
	ZSTD_DStream *zstrm = ZSTD_createDStream();

	if (!zstrm) { 
		bb_error_msg("create stream failed");
		total = -1;
		goto end;
	}

	chunk = ZSTD_initDStream(zstrm);

	if (ZSTD_isError(chunk)) {
		bb_error_msg("init stream failed");
		total = -1;
		goto end;
	}

	zin.size = ZSTD_DStreamInSize();
	zin.src = xmalloc(zin.size);
	zout.size = ZSTD_DStreamOutSize();
	zout.dst = xmalloc(zout.size);

	if (!xstate || xstate->signature_skipped) {
		skip = xstate->signature_skipped;
		memcpy((void*)zin.src, xstate->magic.b, skip);
	}

	while (1) {
		zin.size = safe_read(xstate->src_fd, (void*)zin.src+skip, chunk) + skip;
		skip = 0;
		if (zin.size <= 0) {
			break;
		}
		zin.pos = 0;
		while (zin.pos < zin.size) {
			zout.pos=0;
			chunk = ZSTD_decompressStream(zstrm, &zout , &zin);
			if (ZSTD_isError(chunk)) {
				bb_error_msg("corrupted data");
				total = -1;
				goto end;
			}
			xtransformer_write(xstate, zout.dst, zout.pos);
			IF_DESKTOP(total += zout.pos;)
		}
	}

end:
	if (zstrm) {
		ZSTD_freeDStream(zstrm);
	}
	if (zin.src) {
		free((void*)zin.src);
	}
	if (zout.dst) {
		free(zout.dst);
	}
	return total;
}


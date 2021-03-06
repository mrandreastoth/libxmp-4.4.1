/*
 * TrackerPacker_v3.c   Copyright (C) 1998 Asle / ReDoX
 *
 * Converts tp2/tp3 packed MODs back to PTK MODs
 *
 * Modified in 2007,2014,2016 by Claudio Matsuoka
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"


static int depack_tp23(HIO_HANDLE *in, FILE *out, int ver)
{
	uint8 c1, c2, c3, c4;
	uint8 pnum[128];
	uint8 pdata[1024];
	uint8 tmp[50];
	uint8 note, ins, fxt, fxp;
	uint8 npat, nins;
	uint8 len;
	int trk_ofs[128][4];
	int i, j, k;
	int pat_ofs = 999999;
	int size, ssize = 0;
	int max_trk_ofs = 0;

	memset(trk_ofs, 0, 128 * 4 * 4);
	memset(pnum, 0, 128);

	hio_seek(in, 8, SEEK_CUR);
	pw_move_data(out, in, 20);		/* title */
	nins = hio_read16b(in) / 8;		/* number of sample */

	for (i = 0; i < nins; i++) {
		pw_write_zero(out, 22);		/*sample name */

		c3 = hio_read8(in);		/* read finetune */
		c4 = hio_read8(in);		/* read volume */

		write16b(out, size = hio_read16b(in)); /* size */
		ssize += size * 2;

		write8(out, c3);		/* write finetune */
		write8(out, c4);		/* write volume */

		write16b(out, hio_read16b(in));	/* loop start */
		write16b(out, hio_read16b(in));	/* loop size */
	}

	memset(tmp, 0, 30);
	tmp[29] = 0x01;

	for (; i < 31; i++) {
		fwrite(tmp, 30, 1, out);
	}

	/* read size of pattern table */
	hio_read8(in);
	write8(out, len = hio_read8(in));	/* sequence length */

	/* Sanity check */
	if (len >= 128) {
		return -1;
	}

	write8(out, 0x7f);			/* ntk byte */

	for (npat = i = 0; i < len; i++) {
		pnum[i] = hio_read16b(in) / 8;
		if (pnum[i] > npat)
			npat = pnum[i];
	}

	/* read tracks addresses */
	/* bypass 4 bytes or not ?!? */
	/* Here, I choose not :) */

	for (i = 0; i <= npat; i++) {
		for (j = 0; j < 4; j++) {
			trk_ofs[i][j] = hio_read16b(in);
			if (trk_ofs[i][j] > max_trk_ofs)
				max_trk_ofs = trk_ofs[i][j];
		}
	}

	fwrite(pnum, 128, 1, out);		/* write pattern list */
	write32b(out, PW_MOD_MAGIC);		/* ID string */

	pat_ofs = hio_tell(in) + 2;

	/* pattern datas */
	for (i = 0; i <= npat; i++) {
		memset(pdata, 0, 1024);

		for (j = 0; j < 4; j++) {
			int where;

			hio_seek(in, pat_ofs + trk_ofs[i][j], SEEK_SET);

			for (k = 0; k >= 0 && k < 64; k++) {
				uint8 *p = pdata + k * 16 + j * 4;

				c1 = hio_read8(in);
				if ((c1 & 0xc0) == 0xc0) {
					k += 0x100 - c1 - 1;
					continue;
				}

				if ((c1 & 0xc0) == 0x80) {
					c2 = hio_read8(in);
					if (ver == 2) {
						fxt = (c1 >> 2) & 0x0f;
					} else {
						fxt = (c1 >> 1) & 0x0f;
					}
					fxp = c2;
					if ((fxt == 0x05) || (fxt == 0x06)
						|| (fxt == 0x0a)) {
						if (fxp > 0x80)
							fxp = 0x100 - fxp;
						else if (fxp <= 0x80)
							fxp = (fxp << 4) & 0xf0;
					}
					if (fxt == 0x08)
						fxt = 0x00;
					p[2] = fxt;
					p[3] = fxp;
					continue;
				}

				c2 = hio_read8(in);

				ins = ((c2 >> 4) & 0x0f) | ((c1 >> 2) & 0x10);

				if (ver == 2) {
					note = (c1 & 0xfe) >> 1;

					if (note >= 37) {
						return -1;
					}

					fxt = c2 & 0x0f;

					if (fxt == 0x00) {
						p[0] = ins & 0xf0;
						p[0] |= ptk_table[note][0];
						p[1] = ptk_table[note][1];
						p[2] = ((ins << 4) & 0xf0) | fxt;
						continue;
					}
				} else {
					if ((c1 & 0x40) == 0x40) {
						note = 0x7f - c1;
					} else {
						note = c1 & 0x3f;
					}
	
					if (note >= 37) {
						return -1;
					}
	
					fxt = c2 & 0x0f;
	
					if (fxt == 0x00) {
						p[0] = ins & 0xf0;
						p[0] |= ptk_table[note][0];
						p[1] = ptk_table[note][1];
						p[2] = (ins << 4) & 0xf0;
						continue;
					}
				}

				c3 = hio_read8(in);

				if (fxt == 0x08)
					fxt = 0x00;

				fxp = c3;
				if (fxt == 0x05 || fxt == 0x06 || fxt == 0x0a) {
					if (fxp > 0x80) {
						fxp = 0x100 - fxp;
					} else if (fxp <= 0x80) {
						fxp = (fxp << 4) & 0xf0;
					}
				}

				p[0] = (ins & 0xf0) | ptk_table[note][0];
				p[1] = ptk_table[note][1];
				p[2] = ((ins << 4) & 0xf0) | fxt;
				p[3] = fxp;
			}
			where = hio_tell(in);
			if (where < 0) {
				return -1;
			}
			if (where > max_trk_ofs) {
				max_trk_ofs = where;
			}
		}
		fwrite(pdata, 1024, 1, out);
	}

	/* Sample data */
	if (ver > 2 && max_trk_ofs & 0x01) {
		max_trk_ofs += 1;
	}

	hio_seek(in, max_trk_ofs, SEEK_SET);
	pw_move_data(out, in, ssize);

	return 0;
}

static int depack_tp3(HIO_HANDLE *in, FILE *out)
{
	return depack_tp23(in, out, 3);
}

static int depack_tp2(HIO_HANDLE *in, FILE *out)
{
	return depack_tp23(in, out, 2);
}

static int test_tp23(uint8 *data, char *t, int s, char *magic)
{
	int i;
	int npat, nins, ssize;

	PW_REQUEST_DATA(s, 1024);

	if (memcmp(data, magic, 8))
		return -1;

	/* number of sample */
	nins = readmem16b(data + 28);

	if (nins == 0 || nins & 0x07)
		return -1;

	nins >>= 3;

	for (i = 0; i < nins; i++) {
		uint8 *d = data + i * 8;

		/* test finetunes */
		if (d[30] > 0x0f)
			return -1;

		/* test volumes */
		if (d[31] > 0x40)
			return -1;
	}

	/* test sample sizes */
	ssize = 0;
	for (i = 0; i < nins; i++) {
		uint8 *d = data + i * 8;
		int len = readmem16b(d + 32) << 1;	/* size */
		int start = readmem16b(d + 34) << 1;	/* loop start */
		int lsize = readmem16b(d + 36) << 1;	/* loop size */

		if (len > 0xffff || start > 0xffff || lsize > 0xffff)
			return -1;

		if (start + lsize > len + 2)
			return -1;

		if (start != 0 && lsize == 0)
			return -1;

		ssize += len;
	}

	if (ssize <= 4)
		return -1;

	/* pattern list size */
	npat = data[nins * 8 + 31];
	if (npat == 0 || npat > 128)
		return -1;

	pw_read_title(data + 8, t, 20);

	return 0;
}

static int test_tp3(uint8 *data, char *t, int s)
{
	return test_tp23(data, t, s, "CPLX_TP3");
}

static int test_tp2(uint8 *data, char *t, int s)
{
	return test_tp23(data, t, s, "MEXX_TP2");
}

const struct pw_format pw_tp3 = {
	"Tracker Packer v3",
	test_tp3,
	depack_tp3
};

const struct pw_format pw_tp2 = {
	"Tracker Packer v2",
	test_tp2,
	depack_tp2
};

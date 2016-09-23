/*******************************************************************************
 * SM3 function implementation
 * Copyright 2016 Yanbo Li dreamfly281@gmail.com, goldboar@163.com
 * MIT License
 ******************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include "sm3.h"
#include "debug.h"

#define S(x,n) ((x << n) | (x  >> (32 - n)))

#define P0(x) (x ^ S(x, 9) ^ S(x,17))
#define P1(x) (x ^ S(x,15) ^ S(x,23))

#define PW(t)                                                           \
        (                                                               \
                temp = W[t - 16] ^ W[t - 9] ^ (S(W[t - 3], 15)),        \
                P1(temp) ^ W[t - 6] ^ (S(W[t - 13], 7))                 \
        )


#define FF1(x,y,z) (x ^ y ^ z)
#define FF2(x,y,z) ((x & y) | (x & z) | (y & z))

#define GG1(x,y,z) (x ^ y ^ z)
#define GG2(x,y,z) ((x & y) | ((~x) & z))

#define T1 0x79cc4519
#define T2 0x7a879d8a

void sm3_init(struct sm3_ctx *ctx)
{
        ctx->total[0] = 0;
        ctx->total[1] = 0;

        ctx->state[0] = 0x7380166f;
        ctx->state[1] = 0x4914b2b9;
        ctx->state[2] = 0x172442d7;
        ctx->state[3] = 0xda8a0600;
        ctx->state[4] = 0xa96f30bc;
        ctx->state[5] = 0x163138aa;
        ctx->state[6] = 0xe38dee4d;
        ctx->state[7] = 0xb0fb0e4e;
}

static void sm3_process(struct sm3_ctx *ctx, const u8 data[64])
{
        u32 temp, W[68], WP[64], A, B, C, D, E, F, G, H, SS1, SS2, TT1, TT2;
        int i, j, k;

        print_dump_dbg("The filled message is:", data,  ctx->total[0]);

        for (i = 0; i < 16; i++)
                W[i] = ntohl(((u32 *) data)[i]);

        W[16] = PW(16);
        W[17] = PW(17);
        W[18] = PW(18);
        W[19] = PW(19);

        print_dump_u32_dbg("The extent message W is:", W, 68);
        print_dump_u32_dbg("The extent message WP is:", WP, 64);

        A = ctx->state[0];
        B = ctx->state[1];
        C = ctx->state[2];
        D = ctx->state[3];
        E = ctx->state[4];
        F = ctx->state[5];
        G = ctx->state[6];
        H = ctx->state[7];

        for (i = 0; i < 16; i++) {
                WP[i] = W[i] ^ W[i+4];

                SS1 = S(A, 12) + E + S(T1, i);
                SS1 = S(SS1, 7);
                SS2 = SS1 ^ S(A, 12);
                TT1 = FF1(A,B,C) + D + SS2 + WP[i];
                TT2 = GG1(E,F,G) + H + SS1 + W[i];
                D = C;
                C = S(B,9);
                B = A;
                A = TT1;
                H = G;
                G = S(F,19);
                F = E;
                E = P0(TT2);
        }

        for(i = 16; i < 64; i++) {
                k = i + 4;
                W[k] = PW(k);
                WP[i] = W[i] ^ W[i + 4];

                j = i % 32;

                SS1 = S(A, 12) + E + S(T2, j);
                SS1 = S(SS1, 7);
                SS2 = SS1 ^ S(A, 12);
                TT1 = FF2(A,B,C) + D + SS2 + WP[i];
                TT2 = GG2(E,F,G) + H + SS1 + W[i];
                D = C;
                C = S(B, 9);
                B = A;
                A = TT1;
                H = G;
                G = S(F, 19);
                F = E;
                E = P0(TT2);
        }

        ctx->state[0] ^= A;
        ctx->state[1] ^= B;
        ctx->state[2] ^= C;
        ctx->state[3] ^= D;
        ctx->state[4] ^= E;
        ctx->state[5] ^= F;
        ctx->state[6] ^= G;
        ctx->state[7] ^= H;
}

static void sm3_update(struct sm3_ctx *ctx, const u8 *msg, u32 len)
{
        u32 left, fill;

        if (!len)
                return;

        left = ctx->total[0] & 0x3F;
        fill = 64 - left;

        ctx->total[0] += len;
        ctx->total[0] &= 0xFFFFFFFF;

        if (ctx->total[0] < len)
                ctx->total[1]++;

        if (left && (len >= fill)) {
                memcpy((void *)(ctx->buffer + left), (void *)msg, fill);
                sm3_process(ctx, ctx->buffer);
                len -= fill;
                msg += fill;
                left = 0;
        }

        while (len >= 64) {
                sm3_process(ctx, msg);
                len -= 64;
                msg += 64;
        }

        if (len)
                memcpy((void *)(ctx->buffer + left), (void *)msg, len);
}

static u8 sm3_padding[64] = {
        0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void sm3_finish(struct sm3_ctx *ctx, u8 digest[32])
{
        u32 last, padn;
        u32 high, low;
        u8 msglen[8];
        int i;

        high = (ctx->total[0] >> 29)
                | (ctx->total[1] << 3);
        low = (ctx->total[0] << 3);

        ((u32 *)msglen)[0] = htonl(high);
        ((u32 *)msglen)[1] = htonl(low);

        last = ctx->total[0] & 0x3F;
        padn = (last < 56 ) ? (56 - last) : (120 - last);

        sm3_update(ctx, sm3_padding, padn);
        sm3_update(ctx, msglen, 8);

        print_dbg("ctx->state: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
               ctx->state[0], ctx->state[1], ctx->state[2], ctx->state[3],
               ctx->state[4], ctx->state[5], ctx->state[6], ctx->state[7]);

        for (i = 0; i < 8; i++)
                ((u32 *)digest)[i] = htonl(ctx->state[i]);
}

static void _sm3_hash(const u8 *msg, u32 len, u8 *digest)
{
        struct sm3_ctx ctx;
        sm3_init(&ctx);
        sm3_update(&ctx, msg, len);
        sm3_finish(&ctx, digest);
}

/* TODO: the msg length may over max int length  */
int sm3_hash(const u8 *msg, unsigned int len, u8 digest[],
             unsigned int digest_len)
{
        int i;

        if (!digest || !msg || !len || (digest_len < SM3_DIGEST_LEN))
                return -EINVAL;

        print_dump_dbg("The hash message", msg, len);

        _sm3_hash(msg, len, digest);

        print_dump_dbg("The hash digest is:", digest, digest_len);

        return 0;
}

/*
 * WMA compatible decoder
 * Copyright (c) 2002 The FFmpeg Project.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "wmafixed.h"
#include "mdct.h"
#include <stdlib.h>
#include <string.h>

static fixed32 tcos0[1024], tsin0[1024]; //these are the sin and cos rotations used by the MDCT
static uint16_t revtab0[1024];

/**
 * init MDCT or IMDCT computation.
 */
int libwma_mdct_init(MDCTContext *s, int nbits, int inverse)
{
    int n;
   // fixed32 alpha;

    memset(s, 0, sizeof(*s));
    n = 1 << nbits;            //nbits ranges from 12 to 8 inclusive

    s->nbits = nbits;
    s->n = n;

    (&s->fft)->nbits = nbits-2;
    (&s->fft)->inverse = inverse;

    return 0;

}

/**
 * Compute inverse MDCT of size N = 2^nbits
 * @param output N samples
 * @param input N/2 samples
 * @param tmp N/2 samples
 */
void libwma_imdct_calc(MDCTContext *s,
                   fixed32 *output,
                   fixed32 *input)
{
    int k, n8, n4, n2, n, j,scale;
    const fixed32 *in1, *in2;
    FFTComplex *z1 = (FFTComplex *)output;
    FFTComplex *z2 = (FFTComplex *)input;
    int revtabshift = 12 - s->nbits;

    n = 1 << s->nbits;

    n2 = n >> 1;
    n4 = n >> 2;
    n8 = n >> 3;


    /* pre rotation */
    in1 = input;
    in2 = input + n2 - 1;

    for(k = 0; k < n4; k++)
    {
        int kshift = k<<revtabshift;
        j=revtab0[kshift];
        CMUL(&z1[j].re, &z1[j].im, *in2, *in1, tcos0[kshift], tsin0[kshift]);
        in1 += 2;
        in2 -= 2;
    }

    scale = libwma_fft_calc_unscaled(&s->fft, z1);

    /* post rotation + reordering */

    for(k = 0; k < n4; k++)
    {
        int kshift = k<<revtabshift;
        CMUL(&z2[k].re, &z2[k].im, (z1[k].re), (z1[k].im), tcos0[kshift], tsin0[kshift]);
    }

    for(k = 0; k < n8; k++)
    {
        fixed32 r1,r2,r3,r4,r1n,r2n,r3n;

        r1 = z2[n8 + k].im;
        r1n = r1 * -1;
        r2 = z2[n8-1-k].re;
        r2n = r2 * -1;
        r3 = z2[k+n8].re;
        r3n = r3 * -1;
        r4 = z2[n8-k-1].im;

        output[2*k] = r1n;
        output[n2-1-2*k] = r1;

        output[2*k+1] = r2;
        output[n2-1-2*k-1] = r2n;

        output[n2 + 2*k]= r3n;
        output[n-1- 2*k]= r3n;

        output[n2 + 2*k+1]= r4;
        output[n-2 - 2 * k] = r4;
    }
}

int libwma_mdct_init_global(void)
{
    int i,j,m;
    /* init the MDCT bit reverse table here rather then in fft_init */

    for(i=0;i<1024;i++)           /*hard coded to a 2048 bit rotation*/
    {                             /*smaller sizes can reuse the largest*/
        m=0;
        for(j=0;j<10;j++)
        {
            m |= ((i >> j) & 1) << (10-j-1);
        }

       revtab0[i]=m;
    }

    for(i=0;i<1024;i++)
    {
        //fixed32 pi2 = fixmul32(0x20000, M_PI_F);
        fixed32 ip = itofix32(i) + 0x2000;
        ip = ip >> 12;
        //ip = fixdiv32(ip,itofix32(n)); // PJJ optimize
        //alpha = fixmul32(TWO_M_PI_F, ip);
        //s->tcos[i] = -fixcos32(alpha);        //alpha between 0 and pi/2
        //s->tsin[i] = -fixsin32(alpha);

        //I can't remember why this works, but it seems to agree for ~24 bits, maybe more!
        tsin0[i] = - fsincos(ip<<16, &(tcos0[i]));
        tcos0[i] *=-1;
  }

    libwma_fft_init_global();

    return 0;
}


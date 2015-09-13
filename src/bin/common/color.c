/*
 * The copyright in this software is being made available under the 2-clauses 
 * BSD License, included below. This software may be subject to other third 
 * party and contributor rights, including patent rights, and no such rights
 * are granted under this license.
 *
 * Copyright (c) 2002-2014, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2014, Professor Benoit Macq
 * Copyright (c) 2001-2003, David Janssens
 * Copyright (c) 2002-2003, Yannick Verschueren
 * Copyright (c) 2003-2007, Francois-Olivier Devaux 
 * Copyright (c) 2003-2014, Antonin Descampe
 * Copyright (c) 2005, Herve Drolon, FreeImage Team
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "opj_apps_config.h"
#include "openjpeg.h"
#include "color.h"

#ifdef OPJ_HAVE_LIBLCMS2
#include <lcms2.h>
#endif
#ifdef OPJ_HAVE_LIBLCMS1
#include <lcms.h>
#endif

#ifdef OPJ_USE_LEGACY
#define OPJ_CLRSPC_GRAY CLRSPC_GRAY
#define OPJ_CLRSPC_SRGB CLRSPC_SRGB
#endif

/*--------------------------------------------------------
Matrix for sYCC, Amendment 1 to IEC 61966-2-1

Y :   0.299   0.587    0.114   :R
Cb:  -0.1687 -0.3312   0.5     :G
Cr:   0.5    -0.4187  -0.0812  :B

Inverse:

R: 1        -3.68213e-05    1.40199      :Y
G: 1.00003  -0.344125      -0.714128     :Cb - 2^(prec - 1)
B: 0.999823  1.77204       -8.04142e-06  :Cr - 2^(prec - 1)

-----------------------------------------------------------*/
static void sycc_to_rgb(int offset, int upb, int y, int cb, int cr,
	int *out_r, int *out_g, int *out_b)
{
	int r, g, b;

	cb -= offset; cr -= offset;
	r = y + (int)(1.402 * (float)cr);
	if(r < 0) r = 0; else if(r > upb) r = upb; *out_r = r;

	g = y - (int)(0.344 * (float)cb + 0.714 * (float)cr);
	if(g < 0) g = 0; else if(g > upb) g = upb; *out_g = g;

	b = y + (int)(1.772 * (float)cb);
	if(b < 0) b = 0; else if(b > upb) b = upb; *out_b = b;
}

static void sycc444_to_rgb(opj_image_t *img)
{
	int *d0, *d1, *d2, *r, *g, *b;
	const int *y, *cb, *cr;
	unsigned int maxw, maxh, max, i;
	int offset, upb;

	upb = (int)img->comps[0].prec;
	offset = 1<<(upb - 1); upb = (1<<upb)-1;

	maxw = (unsigned int)img->comps[0].w; maxh = (unsigned int)img->comps[0].h;
	max = maxw * maxh;

	y = img->comps[0].data;
	cb = img->comps[1].data;
	cr = img->comps[2].data;

	d0 = r = (int*)malloc(sizeof(int) * (size_t)max);
	d1 = g = (int*)malloc(sizeof(int) * (size_t)max);
	d2 = b = (int*)malloc(sizeof(int) * (size_t)max);

	for(i = 0U; i < max; ++i)
	{
		sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
		++y; ++cb; ++cr; ++r; ++g; ++b;
	}
	free(img->comps[0].data); img->comps[0].data = d0;
	free(img->comps[1].data); img->comps[1].data = d1;
	free(img->comps[2].data); img->comps[2].data = d2;

}/* sycc444_to_rgb() */

static void sycc422_to_rgb(opj_image_t *img)
{	
	int *d0, *d1, *d2, *r, *g, *b;
	const int *y, *cb, *cr;
	unsigned int maxw, maxh, max;
	int offset, upb;
	unsigned int i, j;

	upb = (int)img->comps[0].prec;
	offset = 1<<(upb - 1); upb = (1<<upb)-1;

	maxw = (unsigned int)img->comps[0].w; maxh = (unsigned int)img->comps[0].h;
	max = maxw * maxh;

	y = img->comps[0].data;
	cb = img->comps[1].data;
	cr = img->comps[2].data;

	d0 = r = (int*)malloc(sizeof(int) * (size_t)max);
	d1 = g = (int*)malloc(sizeof(int) * (size_t)max);
	d2 = b = (int*)malloc(sizeof(int) * (size_t)max);

	for(i=0U; i < maxh; ++i)
	{
		for(j=0U; j < (maxw & ~(unsigned int)1U); j += 2U)
		{
			sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
			++y; ++r; ++g; ++b;
			sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
			++y; ++r; ++g; ++b; ++cb; ++cr;
		}
		if (j < maxw) {
			sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
			++y; ++r; ++g; ++b; ++cb; ++cr;
		}
	}
	free(img->comps[0].data); img->comps[0].data = d0;
	free(img->comps[1].data); img->comps[1].data = d1;
	free(img->comps[2].data); img->comps[2].data = d2;

#if defined(USE_JPWL) || defined(USE_MJ2)
	img->comps[1].w = maxw; img->comps[1].h = maxh;
	img->comps[2].w = maxw; img->comps[2].h = maxh;
#else
	img->comps[1].w = (OPJ_UINT32)maxw; img->comps[1].h = (OPJ_UINT32)maxh;
	img->comps[2].w = (OPJ_UINT32)maxw; img->comps[2].h = (OPJ_UINT32)maxh;
#endif
	img->comps[1].dx = img->comps[0].dx;
	img->comps[2].dx = img->comps[0].dx;
	img->comps[1].dy = img->comps[0].dy;
	img->comps[2].dy = img->comps[0].dy;

}/* sycc422_to_rgb() */

static void sycc420_to_rgb(opj_image_t *img)
{
	int *d0, *d1, *d2, *r, *g, *b, *nr, *ng, *nb;
	const int *y, *cb, *cr, *ny;
	unsigned int maxw, maxh, max;
	int offset, upb;
	unsigned int i, j;

	upb = (int)img->comps[0].prec;
	offset = 1<<(upb - 1); upb = (1<<upb)-1;

	maxw = (unsigned int)img->comps[0].w; maxh = (unsigned int)img->comps[0].h;
	max = maxw * maxh;

	y = img->comps[0].data;
	cb = img->comps[1].data;
	cr = img->comps[2].data;

	d0 = r = (int*)malloc(sizeof(int) * (size_t)max);
	d1 = g = (int*)malloc(sizeof(int) * (size_t)max);
	d2 = b = (int*)malloc(sizeof(int) * (size_t)max);

	for(i=0U; i < (maxh & ~(unsigned int)1U); i += 2U)
	{
		ny = y + maxw;
		nr = r + maxw; ng = g + maxw; nb = b + maxw;

		for(j=0; j < (maxw & ~(unsigned int)1U); j += 2U)
		{
			sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
			++y; ++r; ++g; ++b;
			sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
			++y; ++r; ++g; ++b;

			sycc_to_rgb(offset, upb, *ny, *cb, *cr, nr, ng, nb);
			++ny; ++nr; ++ng; ++nb;
			sycc_to_rgb(offset, upb, *ny, *cb, *cr, nr, ng, nb);
			++ny; ++nr; ++ng; ++nb; ++cb; ++cr;
		}
		if(j < maxw)
		{
			sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
			++y; ++r; ++g; ++b;

			sycc_to_rgb(offset, upb, *ny, *cb, *cr, nr, ng, nb);
			++ny; ++nr; ++ng; ++nb; ++cb; ++cr;
		}
		y += maxw; r += maxw; g += maxw; b += maxw;
	}
	if(i < maxh)
	{
		for(j=0U; j < (maxw & ~(unsigned int)1U); j += 2U)
		{
			sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);

			++y; ++r; ++g; ++b;

			sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);

			++y; ++r; ++g; ++b; ++cb; ++cr;
		}
		if(j < maxw)
		{
			sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
		}
	}

	free(img->comps[0].data); img->comps[0].data = d0;
	free(img->comps[1].data); img->comps[1].data = d1;
	free(img->comps[2].data); img->comps[2].data = d2;

#if defined(USE_JPWL) || defined(USE_MJ2)
	img->comps[1].w = maxw; img->comps[1].h = maxh;
	img->comps[2].w = maxw; img->comps[2].h = maxh;
#else
	img->comps[1].w = (OPJ_UINT32)maxw; img->comps[1].h = (OPJ_UINT32)maxh;
	img->comps[2].w = (OPJ_UINT32)maxw; img->comps[2].h = (OPJ_UINT32)maxh;
#endif
	img->comps[1].dx = img->comps[0].dx;
	img->comps[2].dx = img->comps[0].dx;
	img->comps[1].dy = img->comps[0].dy;
	img->comps[2].dy = img->comps[0].dy;

}/* sycc420_to_rgb() */

void color_sycc_to_rgb(opj_image_t *img)
{
	if(img->numcomps < 3)
	{
		img->color_space = OPJ_CLRSPC_GRAY;
		return;
	}

	if((img->comps[0].dx == 1)
	&& (img->comps[1].dx == 2)
	&& (img->comps[2].dx == 2)
	&& (img->comps[0].dy == 1)
	&& (img->comps[1].dy == 2)
	&& (img->comps[2].dy == 2))/* horizontal and vertical sub-sample */
  {
		sycc420_to_rgb(img);
  }
	else
	if((img->comps[0].dx == 1)
	&& (img->comps[1].dx == 2)
	&& (img->comps[2].dx == 2)
	&& (img->comps[0].dy == 1)
	&& (img->comps[1].dy == 1)
	&& (img->comps[2].dy == 1))/* horizontal sub-sample only */
  {
		sycc422_to_rgb(img);
  }
	else
	if((img->comps[0].dx == 1)
	&& (img->comps[1].dx == 1)
	&& (img->comps[2].dx == 1)
	&& (img->comps[0].dy == 1)
	&& (img->comps[1].dy == 1)
	&& (img->comps[2].dy == 1))/* no sub-sample */
  {
		sycc444_to_rgb(img);
  }
	else
  {
		fprintf(stderr,"%s:%d:color_sycc_to_rgb\n\tCAN NOT CONVERT\n", __FILE__,__LINE__);
		return;
  }
	img->color_space = OPJ_CLRSPC_SRGB;

}/* color_sycc_to_rgb() */

#if defined(OPJ_HAVE_LIBLCMS2) || defined(OPJ_HAVE_LIBLCMS1)

#ifdef OPJ_HAVE_LIBLCMS1
/* Bob Friesenhahn proposed:*/
#define cmsSigXYZData   icSigXYZData
#define cmsSigLabData   icSigLabData
#define cmsSigCmykData  icSigCmykData
#define cmsSigYCbCrData icSigYCbCrData
#define cmsSigLuvData   icSigLuvData
#define cmsSigGrayData  icSigGrayData
#define cmsSigRgbData   icSigRgbData
#define cmsUInt32Number DWORD

#define cmsColorSpaceSignature icColorSpaceSignature
#define cmsGetHeaderRenderingIntent cmsTakeRenderingIntent

#endif /* OPJ_HAVE_LIBLCMS1 */

/*#define DEBUG_PROFILE*/
void color_apply_icc_profile(opj_image_t *image)
{
	cmsHPROFILE in_prof, out_prof;
	cmsHTRANSFORM transform;
	cmsColorSpaceSignature in_space, out_space;
	cmsUInt32Number intent, in_type, out_type, nr_samples;
	int *r, *g, *b;
	int prec, i, max, max_w, max_h;
	OPJ_COLOR_SPACE oldspace;

	in_prof = 
	 cmsOpenProfileFromMem(image->icc_profile_buf, image->icc_profile_len);
#ifdef DEBUG_PROFILE
  FILE *icm = fopen("debug.icm","wb");
  fwrite( image->icc_profile_buf,1, image->icc_profile_len,icm);
  fclose(icm);
#endif

	if(in_prof == NULL) return;

	in_space = cmsGetPCS(in_prof);
	out_space = cmsGetColorSpace(in_prof);
	intent = cmsGetHeaderRenderingIntent(in_prof);

	
	max_w = (int)image->comps[0].w;
  max_h = (int)image->comps[0].h;
	prec = (int)image->comps[0].prec;
	oldspace = image->color_space;

	if(out_space == cmsSigRgbData) /* enumCS 16 */
   {
	if( prec <= 8 )
  {
	in_type = TYPE_RGB_8;
	out_type = TYPE_RGB_8;
  }
	else
  {
	in_type = TYPE_RGB_16;
	out_type = TYPE_RGB_16;
  }
	out_prof = cmsCreate_sRGBProfile();
	image->color_space = OPJ_CLRSPC_SRGB;
   }
	else
	if(out_space == cmsSigGrayData) /* enumCS 17 */
   {
	in_type = TYPE_GRAY_8;
	out_type = TYPE_RGB_8;
	out_prof = cmsCreate_sRGBProfile();
	image->color_space = OPJ_CLRSPC_SRGB;
   }
	else
	if(out_space == cmsSigYCbCrData) /* enumCS 18 */
   {
	in_type = TYPE_YCbCr_16;
	out_type = TYPE_RGB_16;
	out_prof = cmsCreate_sRGBProfile();
	image->color_space = OPJ_CLRSPC_SRGB;
   }
	else
   {
#ifdef DEBUG_PROFILE
fprintf(stderr,"%s:%d: color_apply_icc_profile\n\tICC Profile has unknown "
"output colorspace(%#x)(%c%c%c%c)\n\tICC Profile ignored.\n",
__FILE__,__LINE__,out_space,
	(out_space>>24) & 0xff,(out_space>>16) & 0xff,
	(out_space>>8) & 0xff, out_space & 0xff);
#endif
	return;
   }

#ifdef DEBUG_PROFILE
fprintf(stderr,"%s:%d:color_apply_icc_profile\n\tchannels(%d) prec(%d) w(%d) h(%d)"
"\n\tprofile: in(%p) out(%p)\n",__FILE__,__LINE__,image->numcomps,prec,
	max_w,max_h, (void*)in_prof,(void*)out_prof);

fprintf(stderr,"\trender_intent (%u)\n\t"
"color_space: in(%#x)(%c%c%c%c)   out:(%#x)(%c%c%c%c)\n\t"
"       type: in(%u)              out:(%u)\n",
	intent,
	in_space,
	(in_space>>24) & 0xff,(in_space>>16) & 0xff,
	(in_space>>8) & 0xff, in_space & 0xff,

	out_space,
	(out_space>>24) & 0xff,(out_space>>16) & 0xff,
	(out_space>>8) & 0xff, out_space & 0xff,

	in_type,out_type
 );
#else
  (void)prec;
  (void)in_space;
#endif /* DEBUG_PROFILE */

	transform = cmsCreateTransform(in_prof, in_type,
	 out_prof, out_type, intent, 0);

#ifdef OPJ_HAVE_LIBLCMS2
/* Possible for: LCMS_VERSION >= 2000 :*/
	cmsCloseProfile(in_prof);
	cmsCloseProfile(out_prof);
#endif

	if(transform == NULL)
   {
#ifdef DEBUG_PROFILE
fprintf(stderr,"%s:%d:color_apply_icc_profile\n\tcmsCreateTransform failed. "
"ICC Profile ignored.\n",__FILE__,__LINE__);
#endif
	image->color_space = oldspace;
#ifdef OPJ_HAVE_LIBLCMS1
	cmsCloseProfile(in_prof);
	cmsCloseProfile(out_prof);
#endif
	return;
   }

	if(image->numcomps > 2)/* RGB, RGBA */
   {
	if( prec <= 8 )
  {
	unsigned char *inbuf, *outbuf, *in, *out;
	max = max_w * max_h;
	nr_samples = (cmsUInt32Number)max * 3 * (cmsUInt32Number)sizeof(unsigned char);
	in = inbuf = (unsigned char*)malloc(nr_samples);
	out = outbuf = (unsigned char*)malloc(nr_samples);

	r = image->comps[0].data;
	g = image->comps[1].data;
	b = image->comps[2].data;

	for(i = 0; i < max; ++i)
 {
	*in++ = (unsigned char)*r++;
	*in++ = (unsigned char)*g++;
	*in++ = (unsigned char)*b++;
 }

	cmsDoTransform(transform, inbuf, outbuf, (cmsUInt32Number)max);

	r = image->comps[0].data;
	g = image->comps[1].data;
	b = image->comps[2].data;

	for(i = 0; i < max; ++i)
 {
	*r++ = (int)*out++;
	*g++ = (int)*out++;
	*b++ = (int)*out++;
 }
	free(inbuf); free(outbuf);
  }
	else
  {
	unsigned short *inbuf, *outbuf, *in, *out;
	max = max_w * max_h;
	nr_samples = (cmsUInt32Number)max * 3 * (cmsUInt32Number)sizeof(unsigned short);
	in = inbuf = (unsigned short*)malloc(nr_samples);
	out = outbuf = (unsigned short*)malloc(nr_samples);

	r = image->comps[0].data;
	g = image->comps[1].data;
	b = image->comps[2].data;

	for(i = 0; i < max; ++i)
 {
	*in++ = (unsigned short)*r++;
	*in++ = (unsigned short)*g++;
	*in++ = (unsigned short)*b++;
 }

	cmsDoTransform(transform, inbuf, outbuf, (cmsUInt32Number)max);

	r = image->comps[0].data;
	g = image->comps[1].data;
	b = image->comps[2].data;

	for(i = 0; i < max; ++i)
 {
	*r++ = (int)*out++;
	*g++ = (int)*out++;
	*b++ = (int)*out++;
 }
	free(inbuf); free(outbuf);
  }
   }
	else /* GRAY, GRAYA */
   {
	unsigned char *in, *inbuf, *out, *outbuf;
	max = max_w * max_h;
	nr_samples = (cmsUInt32Number)max * 3 * sizeof(unsigned char);
	in = inbuf = (unsigned char*)malloc(nr_samples);
	out = outbuf = (unsigned char*)malloc(nr_samples);

	image->comps = (opj_image_comp_t*)
	 realloc(image->comps, (image->numcomps+2)*sizeof(opj_image_comp_t));

	if(image->numcomps == 2)
	 image->comps[3] = image->comps[1];

	image->comps[1] = image->comps[0];
	image->comps[2] = image->comps[0];

	image->comps[1].data = (int*)calloc((size_t)max, sizeof(int));
	image->comps[2].data = (int*)calloc((size_t)max, sizeof(int));

	image->numcomps += 2;

	r = image->comps[0].data;

	for(i = 0; i < max; ++i)
  {
	*in++ = (unsigned char)*r++;
  }
	cmsDoTransform(transform, inbuf, outbuf, (cmsUInt32Number)max);

	r = image->comps[0].data;
	g = image->comps[1].data;
	b = image->comps[2].data;

	for(i = 0; i < max; ++i)
  {
	*r++ = (int)*out++; *g++ = (int)*out++; *b++ = (int)*out++;
  }
	free(inbuf); free(outbuf);

   }/* if(image->numcomps */

	cmsDeleteTransform(transform);

#ifdef OPJ_HAVE_LIBLCMS1
	cmsCloseProfile(in_prof);
	cmsCloseProfile(out_prof);
#endif
}/* color_apply_icc_profile() */

void color_cielab_to_rgb(opj_image_t *image)
{
	int *row;
	int enumcs, numcomps;
	
	image->color_space = OPJ_CLRSPC_SRGB;
	
	numcomps = (int)image->numcomps;
	
	if(numcomps != 3)
	{
		fprintf(stderr,"%s:%d:\n\tnumcomps %d not handled. Quitting.\n",
						__FILE__,__LINE__,numcomps);
		return;
	}
	
	row = (int*)image->icc_profile_buf;
	enumcs = row[0];
	
	if(enumcs == 14) /* CIELab */
	{
		int *L, *a, *b, *red, *green, *blue;
		int *src0, *src1, *src2, *dst0, *dst1, *dst2;
		double rl, ol, ra, oa, rb, ob, prec0, prec1, prec2;
		double minL, maxL, mina, maxa, minb, maxb;
		unsigned int default_type;
		unsigned int i, max;
		cmsHPROFILE in, out;
		cmsHTRANSFORM transform;
		cmsUInt16Number RGB[3];
		cmsCIELab Lab;
		
		in = cmsCreateLab4Profile(NULL);
		out = cmsCreate_sRGBProfile();
		
		transform = cmsCreateTransform(in, TYPE_Lab_DBL, out, TYPE_RGB_16, INTENT_PERCEPTUAL, 0);
		
#ifdef OPJ_HAVE_LIBLCMS2
		cmsCloseProfile(in);
		cmsCloseProfile(out);
#endif
		if(transform == NULL)
		{
#ifdef OPJ_HAVE_LIBLCMS1
			cmsCloseProfile(in);
			cmsCloseProfile(out);
#endif
			return;
		}
		prec0 = (double)image->comps[0].prec;
		prec1 = (double)image->comps[1].prec;
		prec2 = (double)image->comps[2].prec;
		
		default_type = (unsigned int)row[1];
		
		if(default_type == 0x44454600)// DEF : default
		{
			rl = 100; ra = 170; rb = 200;
			ol = 0;
			oa = pow(2, prec1 - 1);
			ob = pow(2, prec2 - 2) +  pow(2, prec2 - 3);
		}
		else
		{
			rl = row[2]; ra = row[4]; rb = row[6];
			ol = row[3]; oa = row[5]; ob = row[7];
		}
		
		L = src0 = image->comps[0].data;
		a = src1 = image->comps[1].data;
		b = src2 = image->comps[2].data;
		
		max = image->comps[0].w * image->comps[0].h;
		
		red = dst0 = (int*)malloc(max * sizeof(int));
		green = dst1 = (int*)malloc(max * sizeof(int));
		blue = dst2 = (int*)malloc(max * sizeof(int));
		
		minL = -(rl * ol)/(pow(2, prec0)-1);
		maxL = minL + rl;
		
		mina = -(ra * oa)/(pow(2, prec1)-1);
		maxa = mina + ra;
		
		minb = -(rb * ob)/(pow(2, prec2)-1);
		maxb = minb + rb;
		
		for(i = 0; i < max; ++i)
		{
			Lab.L = minL + (double)(*L) * (maxL - minL)/(pow(2, prec0)-1); ++L;
			Lab.a = mina + (double)(*a) * (maxa - mina)/(pow(2, prec1)-1); ++a;
			Lab.b = minb + (double)(*b) * (maxb - minb)/(pow(2, prec2)-1); ++b;
		
			cmsDoTransform(transform, &Lab, RGB, 1);
		
			*red++ = RGB[0];
			*green++ = RGB[1];
			*blue++ = RGB[2];
		}
		cmsDeleteTransform(transform);
#ifdef OPJ_HAVE_LIBLCMS1
		cmsCloseProfile(in);
		cmsCloseProfile(out);
#endif
		free(src0); image->comps[0].data = dst0;
		free(src1); image->comps[1].data = dst1;
		free(src2); image->comps[2].data = dst2;
		
		image->color_space = OPJ_CLRSPC_SRGB;
		image->comps[0].prec = 16;
		image->comps[1].prec = 16;
		image->comps[2].prec = 16;
		
		return;
	}
	
	fprintf(stderr,"%s:%d:\n\tenumCS %d not handled. Ignoring.\n", __FILE__,__LINE__, enumcs);
}// color_apply_conversion()

#endif // OPJ_HAVE_LIBLCMS2 || OPJ_HAVE_LIBLCMS1

void color_cmyk_to_rgb(opj_image_t *image)
{
	float C, M, Y, K;
	float sC, sM, sY, sK;
	unsigned int w, h, max, i;

	w = image->comps[0].w;
	h = image->comps[0].h;

	if(image->numcomps < 4) return;

	max = w * h;
	
	sC = 1.0F / (float)((1 << image->comps[0].prec) - 1);
	sM = 1.0F / (float)((1 << image->comps[1].prec) - 1);
	sY = 1.0F / (float)((1 << image->comps[2].prec) - 1);
	sK = 1.0F / (float)((1 << image->comps[3].prec) - 1);

	for(i = 0; i < max; ++i)
	{
		/* CMYK values from 0 to 1 */
		C = (float)(image->comps[0].data[i]) * sC;
		M = (float)(image->comps[1].data[i]) * sM;
		Y = (float)(image->comps[2].data[i]) * sY;
		K = (float)(image->comps[3].data[i]) * sK;
		
		/* Invert all CMYK values */
		C = 1.0F - C;
		M = 1.0F - M;
		Y = 1.0F - Y;
		K = 1.0F - K;

		/* CMYK -> RGB : RGB results from 0 to 255 */
		image->comps[0].data[i] = (int)(255.0F * C * K); /* R */
		image->comps[1].data[i] = (int)(255.0F * M * K); /* G */
		image->comps[2].data[i] = (int)(255.0F * Y * K); /* B */
	}

	free(image->comps[3].data); image->comps[3].data = NULL;
	image->comps[0].prec = 8;
	image->comps[1].prec = 8;
	image->comps[2].prec = 8;
	image->numcomps -= 1;
	image->color_space = OPJ_CLRSPC_SRGB;
	
	for (i = 3; i < image->numcomps; ++i) {
		memcpy(&(image->comps[i]), &(image->comps[i+1]), sizeof(image->comps[i]));
	}

}// color_cmyk_to_rgb()

//
// This code has been adopted from sjpx_openjpeg.c of ghostscript
//
void color_esycc_to_rgb(opj_image_t *image)
{
	int y, cb, cr, sign1, sign2, val;
	unsigned int w, h, max, i;
	int flip_value = (1 << (image->comps[0].prec-1));
	int max_value = (1 << image->comps[0].prec) - 1;
	
	if(image->numcomps < 3) return;
	
	w = image->comps[0].w;
	h = image->comps[0].h;
	
	sign1 = (int)image->comps[1].sgnd;
	sign2 = (int)image->comps[2].sgnd;
	
	max = w * h;
	
	for(i = 0; i < max; ++i)
	{
		
		y = image->comps[0].data[i]; cb = image->comps[1].data[i]; cr = image->comps[2].data[i];
		
		if( !sign1) cb -= flip_value;
		if( !sign2) cr -= flip_value;
		
		val = (int)
		((float)y - (float)0.0000368 * (float)cb
		 + (float)1.40199 * (float)cr + (float)0.5);
		
		if(val > max_value) val = max_value; else if(val < 0) val = 0;
		image->comps[0].data[i] = val;
		
		val = (int)
		((float)1.0003 * (float)y - (float)0.344125 * (float)cb
		 - (float)0.7141128 * (float)cr + (float)0.5);
		
		if(val > max_value) val = max_value; else if(val < 0) val = 0;
		image->comps[1].data[i] = val;
		
		val = (int)
		((float)0.999823 * (float)y + (float)1.77204 * (float)cb
		 - (float)0.000008 *(float)cr + (float)0.5);
		
		if(val > max_value) val = max_value; else if(val < 0) val = 0;
		image->comps[2].data[i] = val;
	}
	image->color_space = OPJ_CLRSPC_SRGB;

}// color_esycc_to_rgb()

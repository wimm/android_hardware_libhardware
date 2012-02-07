/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *	  http://www.samsung.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 /*
** 
** @initial code siva krishna neeli(siva.neeli@samsung.com)
** @date   2009-02-27
** @s5p6442 specific changes: rama(v.meka@samaung.com)
** @date   2009-11-27
*/
#if 1
#define LOG_TAG "copybit"

#include <cutils/log.h>

#include <linux/fb.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/mman.h>

#include <hardware/copybit.h>
#include <linux/android_pmem.h>

#include "gralloc_priv.h"
#include "s5prender.h"

//------------ DEFINE ---------------------------------------------------------//
//#define USE_SINGLE_INSTANCE

#ifndef DEFAULT_FB_NUM
	#error "WHY DON'T YOU SET FB NUM.."
	//#define DEFAULT_FB_NUM 0
#endif

//#define USE_SINGLE_INSTANCE

#define MAX_RESIZING_RATIO_LIMIT   63    //64 // 4

#define S3C_TRANSP_NOP	(0xffffffff)
#define S3C_ALPHA_NOP   (0xff)

//------------ STRUCT ---------------------------------------------------------//

struct copybit_context_t {
	struct copybit_device_t device;
#if 0
	S3c2DRender*            render;
#endif
	uint8_t                 mAlpha;
	uint8_t                 mFlags;
};

//----------------------- Common hardware methods ------------------------------//

static int open_copybit (const struct hw_module_t* module, const char* name, struct hw_device_t** device);
static int close_copybit(struct hw_device_t *dev);

static int set_parameter_copybit(struct copybit_device_t *dev, int name, int value) ;
static int get_parameter_copybit(struct copybit_device_t *dev, int name);
static int stretch_copybit      (struct copybit_device_t *dev,
                                 struct copybit_image_t  const *dst,
                                 struct copybit_image_t  const *src,
                                 struct copybit_rect_t   const *dst_rect,
                                 struct copybit_rect_t   const *src_rect,
                                 struct copybit_region_t const *region);
static int stretch_copybitPP(struct copybit_device_t *dev,
                           struct copybit_image_t  const *dst,
                           struct copybit_image_t  const *src,
                           struct copybit_rect_t   const *dst_rect,
                           struct copybit_rect_t   const *src_rect,
                           struct copybit_region_t const *region,
                           int  flagPP);
static int blit_copybit         (struct copybit_device_t *dev,
                                 struct copybit_image_t  const *dst,
                                 struct copybit_image_t  const *src,
                                 struct copybit_region_t const *region);

//---------------------- Function Declarations ---------------------------------//



//---------------------- The COPYBIT Module ---------------------------------//

static struct hw_module_methods_t copybit_module_methods = {
    open:  open_copybit
};

struct copybit_module_t HAL_MODULE_INFO_SYM = {
    common:
	{
        tag:           HARDWARE_MODULE_TAG,
        version_major:  1,
        version_minor:  0,
        id:             COPYBIT_HARDWARE_MODULE_ID,
        name:           "Samsung S5P6442 COPYBIT Module",
        author:         "Samsung Electronics, Inc.",
        methods:        &copybit_module_methods,
    }
};

//------------- GLOBAL VARIABLE-------------------------------------------------//

#ifdef USE_SINGLE_INSTANCE	
int ctx_created;
struct copybit_context_t *g_ctx;
#endif


//-----------------------------------------------------------------------------//

// Open a new instance of a copybit device using name
static int open_copybit(const struct hw_module_t* module, const char* name, struct hw_device_t** device)
{
#ifdef USE_SINGLE_INSTANCE	
	int status = 0;
	if (ctx_created == 1)
	{
		*device = &g_ctx->device.common;
		status = 0;
		return status;
	}
#endif

    struct copybit_context_t *ctx = (copybit_context_t*)malloc(sizeof(struct copybit_context_t));

#ifdef USE_SINGLE_INSTANCE		
	g_ctx = ctx;
#endif

	memset(ctx, 0, sizeof(*ctx));

	ctx->device.common.tag     = HARDWARE_DEVICE_TAG;
	ctx->device.common.version = 0;
	ctx->device.common.module  = const_cast<hw_module_t*>(module);
	ctx->device.common.close   = close_copybit;
	ctx->device.set_parameter  = set_parameter_copybit;
	ctx->device.get            = get_parameter_copybit;
	ctx->device.blit           = blit_copybit;
	ctx->device.stretch        = stretch_copybit;
	ctx->device.stretchPP        = stretch_copybitPP;
	ctx->mAlpha                = S3C_ALPHA_NOP;
	ctx->mFlags                = 0;
#if 0
	ctx->render                = S3c2DRender::Get2DRender();

	if(ctx->render == NULL)
	{
		close_copybit(&ctx->device.common);
		return -1;	// Use SW Renderer instead of HW Renderer
	}
#endif
#ifdef USE_SINGLE_INSTANCE	
	if (status == 0)
#endif		
	{
		*device = &ctx->device.common;

		#ifdef USE_SINGLE_INSTANCE
			ctx_created = 1;
		#endif
	}
#ifdef USE_SINGLE_INSTANCE	
	else
	{
		close_copybit(&ctx->device.common);
	}
	return status;
#endif	
	return 0;
}

// Close the copybit device
static int close_copybit(struct hw_device_t *dev)
{
	struct copybit_context_t* ctx = (struct copybit_context_t*)dev;
	int ret = 0;
	if (ctx)
	{
#if 0
		if(ctx->render != NULL)
		S3c2DRender::Delete2DRender(ctx->render);
#endif
		free(ctx);

		#ifdef USE_SINGLE_INSTANCE
			ctx_created = 0;
		#endif
	}
	
	return ret;
}

// min of int a, b
static inline int min(int a, int b) {
    return (a<b) ? a : b;
}

// max of int a, b
static inline int max(int a, int b) {
    return (a>b) ? a : b;
}

// scale each parameter by mul/div. Assume div isn't 0
static inline void MULDIV(uint32_t *a, uint32_t *b, int mul, int div) {
    if (mul != div) {
        *a = (mul * *a) / div;
        *b = (mul * *b) / div;
    }
}

// Determine the intersection of lhs & rhs store in out
static void intersect(struct copybit_rect_t *out,
                      const struct copybit_rect_t *lhs,
                      const struct copybit_rect_t *rhs) {
    out->l = max(lhs->l, rhs->l);
    out->t = max(lhs->t, rhs->t);
    out->r = min(lhs->r, rhs->r);
    out->b = min(lhs->b, rhs->b);
}

// convert from copybit image to image structure
static int set_image(s3c_img *img, const struct copybit_image_t *rhs) 
{
	// added by RainAde : to change from cupcake to eclair
	struct private_handle_t* hnd = (private_handle_t*)rhs->handle;    
	img->width      = rhs->w;
	img->height     = rhs->h;
	img->format     = rhs->format;
	img->base       = (unsigned char *)rhs->base;

	if(!hnd)
		return -1;
	
	if(!img->base)
		img->base = (unsigned char *)hnd->base;

	img->offset     = hnd->offset;
// added by RainAde : to change from cupcake to eclair
#if 1 // eclair
	img->memory_id  = hnd->fd;
	

	if(hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER)
		img->flag = S3c2DRender::SEC2D_FLAGS_FRAMEBUFFER;
	else if(hnd->flags & private_handle_t::PRIV_FLAGS_USES_PMEM)
		img->flag = S3c2DRender::SEC2D_FLAGS_USES_PMEM;
    else if(hnd->flags & private_handle_t::PRIV_FLAGS_VIDEO_PLAY)
		img->flag = S3c2DRender::SEC2D_FLAGS_VIDEO_PLAY;
	else if(hnd->flags & private_handle_t::PRIV_FLAGS_CAMERA_PREVIEW)
		img->flag = S3c2DRender::SEC2D_FLAGS_CAMERA_PREVIEW; 
	else if(hnd->flags & private_handle_t::PRIV_FLAGS_CAMERA_PICTURE)
		img->flag = S3c2DRender::SEC2D_FLAGS_CAMERA_PICTURE;
	else 
		img->flag = 0;
#else // cupcake
	img->memory_id  = rhs->fd;
#endif
	return 0;
}

// setup rectangles
static void set_rects(struct copybit_context_t *dev,
                      s3c_img  *src_img,
                      s3c_rect *src_rect,
                      s3c_rect *dst_rect,
                      const struct copybit_rect_t *src,
                      const struct copybit_rect_t *dst,
                      const struct copybit_rect_t *scissor)
{
    struct copybit_rect_t clip;
    intersect(&clip, scissor, dst);

    dst_rect->x  = clip.l;
    dst_rect->y  = clip.t;
    dst_rect->w  = clip.r - clip.l;
    dst_rect->h  = clip.b - clip.t;

    uint32_t W, H;
    if (dev->mFlags & COPYBIT_TRANSFORM_ROT_90) {
        src_rect->x = (clip.t - dst->t) + src->t;
        src_rect->y = (dst->r - clip.r) + src->l;
        src_rect->w = (clip.b - clip.t);
        src_rect->h = (clip.r - clip.l);
        W = dst->b - dst->t;
        H = dst->r - dst->l;
    } else {
        src_rect->x  = (clip.l - dst->l) + src->l;
        src_rect->y  = (clip.t - dst->t) + src->t;
        src_rect->w  = (clip.r - clip.l);
        src_rect->h  = (clip.b - clip.t);
        W = dst->r - dst->l;
        H = dst->b - dst->t;
    }
    MULDIV(&src_rect->x, &src_rect->w, src->r - src->l, W);
    MULDIV(&src_rect->y, &src_rect->h, src->b - src->t, H);
    if (dev->mFlags & COPYBIT_TRANSFORM_FLIP_V) {
        src_rect->y = src_img->height - (src_rect->y + src_rect->h);
    }
    if (dev->mFlags & COPYBIT_TRANSFORM_FLIP_H) {
        src_rect->x = src_img->width  - (src_rect->x + src_rect->w);
    }
}

// Set a parameter to value
static int set_parameter_copybit(struct copybit_device_t *dev, int name, int value) 
{
    struct copybit_context_t* ctx = (struct copybit_context_t*)dev;
    int status = 0;
    if (ctx) {
        switch(name) {
        case COPYBIT_ROTATION_DEG:
            switch (value) {
            case 0:
                ctx->mFlags &= ~0x7;
                break;
            case 90:
                ctx->mFlags &= ~0x7;
                ctx->mFlags |= COPYBIT_TRANSFORM_ROT_90;
                break;
            case 180:
                ctx->mFlags &= ~0x7;
                ctx->mFlags |= COPYBIT_TRANSFORM_ROT_180;
                break;
            case 270:
                ctx->mFlags &= ~0x7;
                ctx->mFlags |= COPYBIT_TRANSFORM_ROT_270;
                break;
            default:
                LOGE("set_parameter_copybit::Invalid value for COPYBIT_ROTATION_DEG");
                status = -EINVAL;
                break;
            }
            break;
        case COPYBIT_PLANE_ALPHA:
            if (value < 0)      value = 0;
            if (value >= 256)   value = 255;
            ctx->mAlpha = value;
            break;
        case COPYBIT_DITHER:
            if (value == COPYBIT_ENABLE) {
                ctx->mFlags |= 0x8;
            } else if (value == COPYBIT_DISABLE) {
                ctx->mFlags &= ~0x8;
            }
            break;
        case COPYBIT_TRANSFORM:
            ctx->mFlags &= ~0x7;
            ctx->mFlags |= value & 0x7;
            break;

        default:
            status = -EINVAL;
            break;
        }
    } else {
        status = -EINVAL;
    }
    return status;
}

// Get a static info value
static int get_parameter_copybit(struct copybit_device_t *dev, int name) 
{
    struct copybit_context_t* ctx = (struct copybit_context_t*)dev;
    int value;
    if (ctx) {
        switch(name) {
        case COPYBIT_MINIFICATION_LIMIT:
	    value = MAX_RESIZING_RATIO_LIMIT;
            break;
        case COPYBIT_MAGNIFICATION_LIMIT:
            value = MAX_RESIZING_RATIO_LIMIT;
            break;
        case COPYBIT_SCALING_FRAC_BITS:
            value = 32;
            break;
        case COPYBIT_ROTATION_STEP_DEG:
            value = 90;
            break;
        default:
            value = -EINVAL;
        }
    } else {
        value = -EINVAL;
    }
    return value;
}

// do a stretch blit type operation
static int stretch_copybit(struct copybit_device_t *dev,
                           struct copybit_image_t  const *dst,
                           struct copybit_image_t  const *src,
                           struct copybit_rect_t   const *dst_rect,
                           struct copybit_rect_t   const *src_rect,
                           struct copybit_region_t const *region) 
{
		return stretch_copybitPP(dev, dst ,  src, dst_rect,  src_rect, region , 0);

}
static int stretch_copybitPP(struct copybit_device_t *dev,
                           struct copybit_image_t  const *dst,
                           struct copybit_image_t  const *src,
                           struct copybit_rect_t   const *dst_rect,
                           struct copybit_rect_t   const *src_rect,
                           struct copybit_region_t const *region,
                           int  flagPP)
{
	struct copybit_context_t* ctx = (struct copybit_context_t*)dev;
	int status = 0;

	if (ctx)
	{
		const struct copybit_rect_t bounds = { 0, 0, dst->w, dst->h };
		s3c_img src_img; 
		s3c_img dst_img;
		s3c_rect src_work_rect;
		s3c_rect dst_work_rect;

		struct copybit_rect_t clip;
		int count = 0;
		status = 0;

		while ((status == 0) && region->next(region, &clip))
		{
			intersect(&clip, &bounds, &clip);
			if(set_image(&src_img, src) < 0)
			{
				LOGW("stretch_copybit::set_image::dst fail");
				status = -EINVAL;
				continue;
			}
			
			if(set_image(&dst_img, dst) < 0)
			{
				LOGW("stretch_copybit::set_image::dst fail");
				status = -EINVAL;
				continue;
			}
			set_rects(ctx, &src_img, &src_work_rect, &dst_work_rect, src_rect, dst_rect, &clip);
			
			S3c2DRender* render = S3c2DRender::Get2DRender();
			if(render == NULL)
			{
				LOGW("stretch_copybit::render == NULL");
				status = -EINVAL;
				continue;
			}
#if 1
			render->m_src_org_rect.x  = src_rect->l;
			render->m_src_org_rect.y  = src_rect->t;
			render->m_src_org_rect.w  = src_rect->r - src_rect->l;
			render->m_src_org_rect.h  = src_rect->b - src_rect->t;

			render->m_dst_org_rect.x  = dst_rect->l;
			render->m_dst_org_rect.y  = dst_rect->t;
			render->m_dst_org_rect.w  = dst_rect->r - dst_rect->l;
			render->m_dst_org_rect.h  = dst_rect->b - dst_rect->t;
#endif						

			render->SetFlags(ctx->mAlpha, ctx->mFlags);
			if(render->Render(&src_img, &src_work_rect,		
			                      &dst_img, &dst_work_rect , flagPP) < 0)
			{
				LOGW("stretch_copybit::run_render fail");
				status = -EINVAL;
			}
		}
	}
	else 
	{
		status = -EINVAL;
	}

	return status;
}

// Perform a blit type operation
static int blit_copybit(struct copybit_device_t *dev,
                        struct copybit_image_t  const *dst,
                        struct copybit_image_t  const *src,
                        struct copybit_region_t const *region) 
{
	int ret = 0;

	struct copybit_rect_t dr = { 0, 0, dst->w, dst->h };
	struct copybit_rect_t sr = { 0, 0, src->w, src->h };

	struct copybit_context_t* ctx = (struct copybit_context_t*)dev;

	ret = stretch_copybit(dev, dst, src, &dr, &sr, region);
		
	return ret;
}











#else
#define LOG_TAG "copybit"

#include <cutils/log.h>

#include <linux/fb.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/mman.h>

#include <hardware/copybit.h>
#include <linux/android_pmem.h>

#include "gralloc_priv.h"
#include "s5prender.h"

//------------ DEFINE ---------------------------------------------------------//

#ifndef DEFAULT_FB_NUM
	#error "WHY DON'T YOU SET FB NUM.."
	//#define DEFAULT_FB_NUM 0
#endif

#define DEFAULT_FB_NUM 0

//#define USE_SINGLE_INSTANCE

#define MAX_RESIZING_RATIO_LIMIT   63    //64 // 4

#define S3C_TRANSP_NOP	(0xffffffff)
#define S3C_ALPHA_NOP   (0xff)

//------------ STRUCT ---------------------------------------------------------//

struct copybit_context_t {
	struct copybit_device_t device;

	S3c2DRender*            render;
	uint8_t                 mAlpha;
	uint8_t                 mFlags;
};

#define LOG_TAG "copybit"

#define HW_FIMC_USE

#ifndef S5P6442_EVT0     //by rama
#define HW_G2D_USE
#endif

//#define MEASURE_DURATION
//#define MEASURE_BLIT_DURATION

// added in order to support video contents zero copy
// src address includes the physical addresses of Y & CbCr
#define VIDEO_ZERO_COPY
//#define CAMERA_CAPTURE_ZERO_COPY

#include <cutils/log.h>

#include <linux/fb.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/mman.h>

#include <hardware/copybit.h>
#include <linux/android_pmem.h>

#include "utils/Timers.h"

#include "s3c_mem.h"

//#define FIMC_DEBUG 
//#define FIMC_GET

#ifdef	HW_FIMC_USE
#include "videodev2.h"
#include "s5p_fimc.h"
#endif

#include "s3c_g2d.h"
#include "s3c_lcd.h"

#include "gralloc_priv.h"

//------------ DEFINE ---------------------------------------------------------//
#ifndef DEFAULT_FB_NUM
	#error "WHY DON'T YOU SET FB NUM.."
	//#define DEFAULT_FB_NUM 0
#endif

#define USE_SINGLE_INSTANCE
#define NUM_OF_MEMORY_OBJECT 2
//#define ADJUST_PMEM
#define MAX_RESIZING_RATIO_LIMIT  63	//64 // 4

#define S3C_TRANSP_NOP	0xffffffff
#define S3C_ALPHA_NOP 0xff


//------------ STRUCT ---------------------------------------------------------//

typedef struct _s3c_rect {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
}s3c_rect;

typedef struct _s3c_img {
	uint32_t width;
	uint32_t height;
	uint32_t format;
	uint32_t offset;
	uint32_t base;
	int memory_id;
}s3c_img;

typedef struct _s3c_fb_t {
	unsigned int    width;
	unsigned int    height;
	//int             dev_fd;
	//unsigned int    phys_addr;
	//unsigned int    phys_start_addr;
}s3c_fb_t;

#ifdef	HW_FIMC_USE
typedef	struct
{
	unsigned int full_width;			// Source Image Full Width (Virtual screen size)
	unsigned int full_height;			// Source Image Full Height (Virtual screen size)
	unsigned int start_x;			// Source Image Start width offset
	unsigned int start_y;			// Source Image Start height offset
	unsigned int width;				// Source Image Width
	unsigned int height;			// Source Image Height
	unsigned int buf_addr_phy_rgb_y;		// Base Address of the Source Image (RGB or Y): Physical Address
	unsigned int buf_addr_phy_cb;		// Base Address of the Source Image (CB Component) : Physical Address
	unsigned int buf_addr_phy_cr;		// Base Address of the Source Image (CR Component) : Physical Address
	unsigned int color_space;		// Color Space of the Source Image
} s5p_pp_img_info;

typedef struct 
{
	s5p_pp_img_info	src;
	s5p_pp_img_info	dst;
} s5p_pp_params_t;

typedef struct _s5p_pp_t {
	int                	dev_fd;	
	struct fimc_buffer	out_buf;	

	s5p_pp_params_t	   params;
//	s5p_pp_mem_alloc_t alloc_info;

	int					use_ext_out_mem;
}s5p_pp_t;

struct yuv_fmt_list yuv_list[] = {
	{ "V4L2_PIX_FMT_NV12", 		"YUV420/2P/LSB_CBCR", 	V4L2_PIX_FMT_NV12, 		12, 2 },
	{ "V4L2_PIX_FMT_NV12T", 	"YUV420/2P/LSB_CBCR", 	V4L2_PIX_FMT_NV12T,		12, 2 },
	{ "V4L2_PIX_FMT_NV21", 		"YUV420/2P/LSB_CRCB", 	V4L2_PIX_FMT_NV21, 		12, 2 },
	{ "V4L2_PIX_FMT_NV21X", 	"YUV420/2P/MSB_CBCR", 	V4L2_PIX_FMT_NV21X, 	12, 2 },
	{ "V4L2_PIX_FMT_NV12X", 	"YUV420/2P/MSB_CRCB", 	V4L2_PIX_FMT_NV12X, 	12, 2 },
	{ "V4L2_PIX_FMT_YUV420", 	"YUV420/3P", 			V4L2_PIX_FMT_YUV420, 	12, 3 },
	{ "V4L2_PIX_FMT_YUYV", 		"YUV422/1P/YCBYCR", 	V4L2_PIX_FMT_YUYV, 		16, 1 },
	{ "V4L2_PIX_FMT_YVYU", 		"YUV422/1P/YCRYCB", 	V4L2_PIX_FMT_YVYU, 		16, 1 },
	{ "V4L2_PIX_FMT_UYVY", 		"YUV422/1P/CBYCRY", 	V4L2_PIX_FMT_UYVY, 		16, 1 },
	{ "V4L2_PIX_FMT_VYUY", 		"YUV422/1P/CRYCBY", 	V4L2_PIX_FMT_VYUY, 		16, 1 },
	{ "V4L2_PIX_FMT_UV12", 		"YUV422/2P/LSB_CBCR", 	V4L2_PIX_FMT_NV16, 		16, 2 },
	{ "V4L2_PIX_FMT_UV21", 		"YUV422/2P/LSB_CRCB", 	V4L2_PIX_FMT_NV61, 		16, 2 },
	{ "V4L2_PIX_FMT_UV12X", 	"YUV422/2P/MSB_CBCR", 	V4L2_PIX_FMT_NV16X, 	16, 2 },
	{ "V4L2_PIX_FMT_UV21X", 	"YUV422/2P/MSB_CRCB", 	V4L2_PIX_FMT_NV61X, 	16, 2 },
	{ "V4L2_PIX_FMT_YUV422P", 	"YUV422/3P", 			V4L2_PIX_FMT_YUV422P, 	16, 3 },
};
#endif

#ifdef HW_G2D_USE
typedef struct _s3c_g2d_t {
	int                dev_fd;
	s3c_g2d_params     params;
}s3c_g2d_t;
#endif

typedef struct _s3c_mem_t{
	int             dev_fd;   
	unsigned char * virt_addr;
	unsigned int    phys_addr;
	struct s3c_mem_alloc 		mem_alloc_info;
}s3c_mem_t;

struct copybit_context_t {
	struct copybit_device_t device;
	s3c_fb_t                s3c_fb;
	
#ifdef	HW_FIMC_USE
	s5p_pp_t                s5p_pp;
#endif
#ifdef HW_G2D_USE
	s3c_g2d_t               s3c_g2d;
#endif

	s3c_mem_t           	s3c_mem[NUM_OF_MEMORY_OBJECT];
	
	uint8_t                 mAlpha;
	uint8_t                 mFlags;

#ifdef USE_SINGLE_INSTANCE
	// the number of instances to open copybit module
	unsigned int			count;
#endif
};

//----------------------- Common hardware methods ------------------------------//

static int open_copybit (const struct hw_module_t* module, const char* name, struct hw_device_t** device);
static int close_copybit(struct hw_device_t *dev);

static int set_parameter_copybit(struct copybit_device_t *dev, int name, int value) ;
static int get_parameter_copybit(struct copybit_device_t *dev, int name);
static int stretch_copybit(struct copybit_device_t *dev,
                           struct copybit_image_t  const *dst,
                           struct copybit_image_t  const *src,
                           struct copybit_rect_t   const *dst_rect,
                           struct copybit_rect_t   const *src_rect,
                           struct copybit_region_t const *region);
static int blit_copybit(struct copybit_device_t *dev,
                        struct copybit_image_t  const *dst,
                        struct copybit_image_t  const *src,
                        struct copybit_region_t const *region);

//---------------------- Function Declarations ---------------------------------//
static int sec_stretch(struct copybit_context_t *ctx,
                           s3c_img *src_img, s3c_rect *src_rect,
                           s3c_img *dst_img, s3c_rect *dst_rect);

#ifdef	HW_FIMC_USE
static int createPP  (struct copybit_context_t *ctx);
static int destroyPP (struct copybit_context_t *ctx);
static int doPP      (struct copybit_context_t *ctx,
               	      unsigned int src_phys_addr, s3c_img *src_img, s3c_rect *src_rect,
                      unsigned int dst_phys_addr, s3c_img *dst_img, s3c_rect *dst_rect, int rotate_flag, int reset_flag);

static unsigned int get_yuv_bpp(unsigned int fmt);
static unsigned int get_yuv_planes(unsigned int fmt);
static inline int colorFormatCopybit2PP(int format);
static int fimc_v4l2_streamon(int fp);
static int fimc_v4l2_streamoff(int fp);
static int fimc_v4l2_start_overlay(int fd);
static int fimc_v4l2_stop_overlay(int fd);
static int fimc_v4l2_s_crop(int fp, int target_width, int target_height, int h_offset, int v_offset);
#endif

#ifdef HW_G2D_USE
static int createG2D (struct copybit_context_t *ctx);
static int destroyG2D(struct copybit_context_t *ctx);
static int doG2D     (struct copybit_context_t *ctx,
                      unsigned int src_phys_addr, s3c_img *src_img, s3c_rect *src_rect,
                      unsigned int dst_phys_addr, s3c_img *dst_img, s3c_rect *dst_rect,
                      int rotate_value, int alpha_value);
static int copyG2Dframe(unsigned char * src_virt_addr,
                        unsigned char * dst_virt_addr,
                        unsigned int full_width, unsigned int full_height,
                        unsigned int start_x,    unsigned int start_y,
                        unsigned int width,      unsigned int height,
                        int rotate_value,        int g2d_color_space);

static inline int colorFormatCopybit2G2D(int format);
#endif
static inline int rotateValueCopybit2G2D(unsigned char flags);

static int createMem (struct copybit_context_t *ctx, unsigned int memory_size);
static int destroyMem(struct copybit_context_t *ctx);
static int checkMem(struct copybit_context_t *ctx, unsigned int requested_size);

static inline unsigned int getFrameSize(int colorformat, int width, int height);

#if	defined(HW_FIMC_USE)
static inline int rotateValueCopybit2PP(unsigned char flags);
static inline int checkPPOutPath(struct copybit_context_t *ctx, s3c_img *dst_img, s3c_rect *dst_rest);
static inline int heightOfPP(int pp_color_format, int number);
static inline int widthOfPP(int pp_color_format, int number);
static inline int multipleOf2 (int number);
static inline int multipleOf4 (int number);
static inline int multipleOf8 (int number);
#endif

//---------------------- The COPYBIT Module ---------------------------------//

static struct hw_module_methods_t copybit_module_methods = {
    open:  open_copybit
};

struct copybit_module_t HAL_MODULE_INFO_SYM = {
    common:
	{
        tag 		:            HARDWARE_MODULE_TAG,
        version_major	: 1,
        version_minor	: 0,
        id            : COPYBIT_HARDWARE_MODULE_ID,
        name          : "Samsung S5P S5P6442 COPYBIT Module",
        author        : "Samsung Electronics, Inc.",
        methods       : &copybit_module_methods,
   	}
};

//------------- GLOBAL VARIABLE-------------------------------------------------//

#ifdef USE_SINGLE_INSTANCE	
int ctx_created;
struct copybit_context_t *g_ctx;
#endif // USE_SINGLE_INSTANCE

//-----------------------------------------------------------------------------//

// Open a new instance of a copybit device using name
static int open_copybit(const struct hw_module_t* module, const char* name, struct hw_device_t** device)
{
	int status = 0;

#ifdef USE_SINGLE_INSTANCE	
	if (ctx_created == 1)
	{
		*device = &g_ctx->device.common;
		status = 0;
		g_ctx->count++;
		return status;
	}
#endif // USE_SINGLE_INSTANCE

    struct copybit_context_t *ctx = (copybit_context_t *)malloc(sizeof(struct copybit_context_t));

#ifdef USE_SINGLE_INSTANCE		
	g_ctx = ctx;
#endif // USE_SINGLE_INSTANCE

	memset(ctx, 0, sizeof(*ctx));

	ctx->device.common.tag     = HARDWARE_DEVICE_TAG;
	ctx->device.common.version = 0;
	ctx->device.common.module  = (struct hw_module_t *)module;
	ctx->device.common.close   = close_copybit;
	ctx->device.set_parameter  = set_parameter_copybit;
	ctx->device.get            = get_parameter_copybit;
	ctx->device.blit           = blit_copybit;
	ctx->device.stretch        = stretch_copybit;
	ctx->mAlpha                = S3C_ALPHA_NOP;
	ctx->mFlags                = 0;


	// get width * height for decide virtual frame size..
	char const * const device_template[] = {
			"/dev/graphics/fb%u",
			"/dev/fb%u",
			0 };
	int fb_fd = -1;
	int i=0;
	char fb_name[64];
	struct fb_var_screeninfo info;

	while ((fb_fd==-1) && device_template[i])
	{
		snprintf(fb_name, 64, device_template[i], DEFAULT_FB_NUM);
		fb_fd = open(fb_name, O_RDONLY, 0);
		if (0 > fb_fd)
			LOGE("%s:: %s errno=%d\n", __func__, fb_name, errno);
		i++;
	}

	if (0 < fb_fd && ioctl(fb_fd, FBIOGET_VSCREENINFO, &info) >= 0)
	{
		ctx->s3c_fb.width  = info.xres;
		ctx->s3c_fb.height = info.yres;
	}
	else
	{
		LOGE("%s::%d = open(%s) fail or FBIOGET_VSCREENINFO fail.. BUT we can do..\n",
			 __func__, fb_fd, fb_name);
		
		ctx->s3c_fb.width  = SCREEN_WIDTH;
		ctx->s3c_fb.height = SCREEN_HEIGHT;
	}

	if(0 < fb_fd)
		close(fb_fd);

	//unsigned int memory_size =  ctx->s3c_fb.width * ctx->s3c_fb.height * 2;
	unsigned int memory_size =  ctx->s3c_fb.width * ctx->s3c_fb.height * 4;

#if 0
	// commented by jamie (2009.09.08)
	// move temporary mem allocation from open_copybit() to sec_stretch()
	// normally, those memories are not used because pmem is used.
	// however, I retains mem allocation code for the case not using pmem.
	// mem allocation is called when temporary memory is necessary.
	//set S3C_MEM
	if(createMem(ctx, memory_size) < 0)
	{
		LOGE("%s::createMem fail\n", __func__);
		status = -EINVAL;
	}
#endif


#if	defined(HW_FIMC_USE)
	//set PP
	if(createPP(ctx) < 0)
	{
		LOGE("%s::createPP fail\n", __func__);
		status = -EINVAL;
	}
#endif
		
#ifdef HW_G2D_USE
	//set g2d
	if(createG2D(ctx) < 0)
	{
		LOGE("%s::createG2D fail\n", __func__);
		status = -EINVAL;
	}
#endif

	if (status == 0)
	{
		*device = &ctx->device.common;

		#ifdef USE_SINGLE_INSTANCE
		ctx->count = 1;
		ctx_created = 1;
		#endif // USE_SINGLE_INSTANCE
	}
	else
	{
		close_copybit(&ctx->device.common);
	 }

#ifdef FIMC_DEBUG
	if(status == 0)
	LOGD("%s::success!!!\n", __func__);
#endif	
	return status;
}

// Close the copybit device
static int close_copybit(struct hw_device_t *dev)
{
	struct copybit_context_t* ctx = (struct copybit_context_t*)dev;
	int ret = 0;

#ifdef USE_SINGLE_INSTANCE
	ctx->count--;

	if (0 >= ctx->count)
#else
	if (ctx)
#endif
	{
		ctx->s3c_fb.width = 0;
		ctx->s3c_fb.height = 0;
        
#ifdef HW_G2D_USE
		if(destroyG2D(ctx) < 0)
		{
			LOGE("%s::destroyG2D fail\n", __func__);
			ret = -1;
		}
#endif

#if	defined(HW_FIMC_USE)
		if(destroyPP(ctx) < 0)
		{
			LOGE("%s::destroyPP fail\n", __func__);
			ret = -1;
		}
#endif
		
		if(destroyMem(ctx) < 0)
		{
			LOGE("%s::destroyMem fail\n", __func__);
			ret = -1;
		}

        free(ctx);

		#ifdef USE_SINGLE_INSTANCE
		ctx_created = 0;
		g_ctx = NULL;
		#endif // USE_SINGLE_INSTANCE
    }

    return ret;
}

// min of int a, b
static inline int min(int a, int b) {
    return (a<b) ? a : b;
}

// max of int a, b
static inline int max(int a, int b) {
    return (a>b) ? a : b;
}

// scale each parameter by mul/div. Assume div isn't 0
static inline void MULDIV(uint32_t *a, uint32_t *b, int mul, int div) {
    if (mul != div) {
        *a = (mul * *a) / div;
        *b = (mul * *b) / div;
    }
}

// Determine the intersection of lhs & rhs store in out
static void intersect(struct copybit_rect_t *out,
                      const struct copybit_rect_t *lhs,
                      const struct copybit_rect_t *rhs) {
    out->l = max(lhs->l, rhs->l);
    out->t = max(lhs->t, rhs->t);
    out->r = min(lhs->r, rhs->r);
    out->b = min(lhs->b, rhs->b);
}

// convert from copybit image to mdp image structure
static void set_image(s3c_img *img,
                      const struct copybit_image_t *rhs) 
{
	// this code is specific to MSM7K
	// Need to modify
    struct private_handle_t* hnd = (struct private_handle_t*)rhs->handle;

    img->width      = rhs->w;
    img->height     = rhs->h;
    img->format     = rhs->format;
	img->base		= (uint32_t)rhs->base;

    img->offset		= hnd->offset;
    img->memory_id 	= hnd->fd;
}

// setup rectangles
static void set_rects(struct copybit_context_t *dev,
                      s3c_img  *src_img,
                      s3c_rect *src_rect,
                      s3c_rect *dst_rect,
                      const struct copybit_rect_t *src,
                      const struct copybit_rect_t *dst,
                      const struct copybit_rect_t *scissor)
{
    struct copybit_rect_t clip;
    intersect(&clip, scissor, dst);

    dst_rect->x  = clip.l;
    dst_rect->y  = clip.t;
    dst_rect->w  = clip.r - clip.l;
    dst_rect->h  = clip.b - clip.t;

    uint32_t W, H;
    if (dev->mFlags & COPYBIT_TRANSFORM_ROT_90) {
        src_rect->x = (clip.t - dst->t) + src->t;
        src_rect->y = (dst->r - clip.r) + src->l;
        src_rect->w = (clip.b - clip.t);
        src_rect->h = (clip.r - clip.l);
        W = dst->b - dst->t;
        H = dst->r - dst->l;
    } else {
        src_rect->x  = (clip.l - dst->l) + src->l;
        src_rect->y  = (clip.t - dst->t) + src->t;
        src_rect->w  = (clip.r - clip.l);
        src_rect->h  = (clip.b - clip.t);
        W = dst->r - dst->l;
        H = dst->b - dst->t;
    }
    MULDIV(&src_rect->x, &src_rect->w, src->r - src->l, W);
    MULDIV(&src_rect->y, &src_rect->h, src->b - src->t, H);
    if (dev->mFlags & COPYBIT_TRANSFORM_FLIP_V) {
        src_rect->y = src_img->height - (src_rect->y + src_rect->h);
    }
    if (dev->mFlags & COPYBIT_TRANSFORM_FLIP_H) {
        src_rect->x = src_img->width  - (src_rect->x + src_rect->w);
    }
}

// Set a parameter to value
static int set_parameter_copybit(struct copybit_device_t *dev, int name, int value) 
{
    struct copybit_context_t* ctx = (struct copybit_context_t*)dev;
    int status = 0;
    if (ctx) {
        switch(name) {
        case COPYBIT_ROTATION_DEG:
            switch (value) {
            case 0:
                ctx->mFlags &= ~0x7;
                break;
            case 90:
                ctx->mFlags &= ~0x7;
                ctx->mFlags |= COPYBIT_TRANSFORM_ROT_90;
                break;
            case 180:
                ctx->mFlags &= ~0x7;
                ctx->mFlags |= COPYBIT_TRANSFORM_ROT_180;
                break;
            case 270:
                ctx->mFlags &= ~0x7;
                ctx->mFlags |= COPYBIT_TRANSFORM_ROT_270;
                break;
            default:
                LOGE("%s::Invalid value for COPYBIT_ROTATION_DEG", __func__);
                status = -EINVAL;
                break;
            }
            break;
        case COPYBIT_PLANE_ALPHA:
            if (value < 0)      value = 0;
            if (value >= 256)   value = 255;
            ctx->mAlpha = value;
            break;
        case COPYBIT_DITHER:
            if (value == COPYBIT_ENABLE) {
                ctx->mFlags |= 0x8;
            } else if (value == COPYBIT_DISABLE) {
                ctx->mFlags &= ~0x8;
            }
            break;
        case COPYBIT_TRANSFORM:
            ctx->mFlags &= ~0x7;
            ctx->mFlags |= value & 0x7;
            break;

        default:
            status = -EINVAL;
            break;
        }
    } else {
        status = -EINVAL;
    }

    return status;
}

// Get a static info value
static int get_parameter_copybit(struct copybit_device_t *dev, int name) 
{
    struct copybit_context_t* ctx = (struct copybit_context_t*)dev;
    int value;
    if (ctx) {
        switch(name) {
        case COPYBIT_MINIFICATION_LIMIT:
	    	value = MAX_RESIZING_RATIO_LIMIT;
            break;
        case COPYBIT_MAGNIFICATION_LIMIT:
            value = MAX_RESIZING_RATIO_LIMIT;
            break;
        case COPYBIT_SCALING_FRAC_BITS:
            value = 32;
            break;
        case COPYBIT_ROTATION_STEP_DEG:
            value = 90;
            break;
        default:
            value = -EINVAL;
        }
    } else {
        value = -EINVAL;
    }
    return value;
}

// do a stretch blit type operation
static int stretch_copybit(struct copybit_device_t *dev,
                           struct copybit_image_t  const *dst,
                           struct copybit_image_t  const *src,
                           struct copybit_rect_t   const *dst_rect,
                           struct copybit_rect_t   const *src_rect,
                           struct copybit_region_t const *region) 
{
	struct copybit_context_t* ctx = (struct copybit_context_t*)dev;
	int status = 0;
	nsecs_t   before1, after1;



#ifndef HW_G2D_USE
#if 0
	switch(src->format)
	{
		case COPYBIT_FORMAT_RGB_565:
		case COPYBIT_FORMAT_RGBA_8888:
		case COPYBIT_FORMAT_BGRA_8888:
		case COPYBIT_FORMAT_RGBA_5551:
		case COPYBIT_FORMAT_RGBA_4444:
			return -1;
	}
#else
	if((src->format != COPYBIT_FORMAT_YCbCr_420_P)&&
		(src->format != COPYBIT_FORMAT_YCbCr_420_SP))
		return -1;
#endif
#endif

#ifdef FIMC_DEBUG
        LOGD("%s::start!!!src_fmt::%d dst_fmt::%d\n", __func__, src->format, dst->format);
#endif
	
	if (ctx)
	{
		const struct copybit_rect_t bounds = { 0, 0, dst->w, dst->h };
		s3c_img src_img; 
		s3c_img dst_img;
		s3c_rect src_work_rect;
		s3c_rect dst_work_rect;

		struct copybit_rect_t clip;
		int count = 0;
		status = 0;

		while ((status == 0) && region->next(region, &clip))
		{
			count++;
			intersect(&clip, &bounds, &clip);
			//LOGD("###dst_base::%x left::%d right::%d top::%d bottom::%d\n", dst->base, clip.l, clip.r, clip.t, clip.b);
			set_image(&src_img, src);
			set_image(&dst_img, dst);
			set_rects(ctx, &src_img,&src_work_rect, &dst_work_rect, src_rect, dst_rect, &clip);
			//LOGD("INSIDE WHILE LOOP ITERATION = %d, calling sec_stretch fun.",count);

			#ifdef	MEASURE_DURATION
			before1  = systemTime(SYSTEM_TIME_MONOTONIC);
			#endif
			if(sec_stretch(ctx, &src_img, &src_work_rect, &dst_img, &dst_work_rect) < 0)
			{
				LOGE("%s::sec_stretch fail\n", __func__);
				status = -EINVAL;
			}
			#ifdef	MEASURE_DURATION
			after1  = systemTime(SYSTEM_TIME_MONOTONIC);
			LOGD("sec_stretch[%d]: duration=%lld", count, ns2us(after1-before1));
			#endif
		}
	}
	else 
	{
		status = -EINVAL;
	}

#ifdef USE_SINGLE_INSTANCE 
	// re-initialize
	ctx->mAlpha                = S3C_ALPHA_NOP;
	ctx->mFlags                = 0;
#endif

#ifdef FIMC_DEBUG
        if(status == 0)
        LOGD("%s::success!!!\n", __func__);
#endif

	return status;
}

// Perform a blit type operation
static int blit_copybit(struct copybit_device_t *dev,
                        struct copybit_image_t  const *dst,
                        struct copybit_image_t  const *src,
                        struct copybit_region_t const *region) 
{
	int ret = 0;

	struct copybit_rect_t dr = { 0, 0, dst->w, dst->h };
	struct copybit_rect_t sr = { 0, 0, src->w, src->h };

	struct copybit_context_t* ctx = (struct copybit_context_t*)dev;

	nsecs_t   before, after;
	
#ifdef MEASURE_BLIT_DURATION
	before  = systemTime(SYSTEM_TIME_MONOTONIC);
	LOGD("[%lld us] blit_copybit", ns2us(before));
#endif

	ret = stretch_copybit(dev, dst, src, &dr, &sr, region);

#ifdef MEASURE_BLIT_DURATION
	after  = systemTime(SYSTEM_TIME_MONOTONIC);
	LOGD("[%lld us] blit_copybit-end (duration=%lld us)", ns2us(after), ns2us(after-before));
#endif
		
	return ret;
}


//-----------------------------------------------------------------------------//
void CbCrSwap(unsigned char *DstCbCrFrm, unsigned char *SrcCbCrFrm, int size)
{
	int cnt;
	unsigned char cb, cr;
//	LOGD("###CbCrSwap::size %d", size);
	for(cnt=0; cnt < size; cnt+=2){
	  cb = *SrcCbCrFrm++;
	  cr = *SrcCbCrFrm++;
          *DstCbCrFrm++ = cr;
	  *DstCbCrFrm++ = cb;	
//	 *DstbCrFrm++ = 128;
//	 *DstCbCrFrm++ = 128;
	}

	return;
}

void FIMC_WA_framecopy(unsigned char *dstBuf, unsigned char *SrcBuf, s3c_rect *rect_org, s3c_rect *rect, int F_W)
{
	int Cnt;
	unsigned char *Dst = dstBuf;

	LOGD("###########FIMC_WA_framecopy\n");
	if(!Dst)
	   return;
	
	Dst += (rect_org->y * F_W) + rect_org->x;
	for(Cnt = 0; Cnt < rect_org->h; Cnt++){
		memcpy(Dst, SrcBuf, rect_org->w);
		Dst += F_W;
		SrcBuf += rect->w;
	}
	return;
}

int fimc_workaround(int colorformat, s3c_rect *rect,  s3c_rect *rect_tmp, int full_width, int full_height)
{
	
	int ext_width;
	int ext_height;
	switch(colorformat)
	{
		case COPYBIT_FORMAT_RGB_565:  		
			//LOGD("#####fimc_workaround::COPYBIT_FORMAT_RGB_565\n");
			rect_tmp->w = ((rect->w + 7) >> 3) << 3;
			rect_tmp->h = ((rect->h + 7) >> 3) << 3;
			break;

		
		case COPYBIT_FORMAT_YCbCr_420_P: 
		case COPYBIT_FORMAT_YCbCr_420_SP: 
			//LOGD("#####fimc_workaround::COPYBIT_FORMAT_YCbCr_420_P\n");
			rect_tmp->w = ((rect->w + 15) >> 4) << 4;
			rect_tmp->h = ((rect->h + 15) >> 4) << 4;			
			break;		

		default :  
			//LOGD("#####fimc_workaround::default\n");
			rect_tmp->w = rect->w;
			rect_tmp->h = rect->h;
			break;
	}

	if((rect_tmp->w != rect->w) ||(rect_tmp->h != rect->h)){
		ext_width = rect_tmp->w - rect->w;
		ext_height = rect_tmp->h - rect->h;
		rect_tmp->x = max((rect->x - (ext_width >> 1)), 0);
		rect_tmp->y = max((rect->y - (ext_height >> 1)), 0);
		if((rect_tmp->x + rect_tmp->w) > full_width)
		{
		     rect_tmp->x = max((rect->x - ext_width), 0);	
		     rect_tmp->w = min(rect_tmp->w,  (full_width - rect_tmp->x));				 
		}

		if((rect_tmp->y + rect_tmp->h) > full_height)
		{
		     rect_tmp->y = max((rect->y - ext_height), 0);	
		     rect_tmp->h = min(rect_tmp->h,  (full_height- rect_tmp->y));				 
		}
		
	}
	else
		return 0;
	
	//temporary solution......software padding is not possible because the dst_img->base is nulll !!!!!!!!!!!!!!!!
	rect->w = rect_tmp->w;
	rect->h = rect_tmp->h;
	rect->x = rect_tmp->x;
	rect->y = rect_tmp->y;
	

	return 1;


	
}

static int sec_stretch(struct copybit_context_t *ctx,
                           s3c_img *src_img, s3c_rect *src_rect,
                           s3c_img *dst_img, s3c_rect *dst_rect)
{
#ifdef	HW_FIMC_USE
	s5p_pp_t *  s5p_pp   = &ctx->s5p_pp;
#endif
#ifdef HW_G2D_USE
	s3c_g2d_t * s3c_g2d  = &ctx->s3c_g2d;
#endif
	s3c_rect dst_rect_tmp;
       int FIMC_WA_FLAG = 0;	
	int ExtWidth, ExtHeight;   

	s3c_mem_t * s3c_mem  = &ctx->s3c_mem[0];
	s3c_mem_t * s3c_mem2 = &ctx->s3c_mem[1];

	unsigned int 	src_phys_addr  	= 0;
	unsigned int 	dst_phys_addr  	= 0;	
	unsigned int 	src_frame_size 	= 0;
	unsigned int 	dst_frame_size 	= 0;
	int          	rotate_value   	= 0;
	unsigned int	thru_g2d_path	= 0;
	int          	flag_force_memcpy = 0;
	struct pmem_region 	region;
	s3c_fb_next_info_t 	fb_info;

	nsecs_t   before, after;

#ifdef FIMC_DEBUG
        LOGD("%s::start!!!!!!!!!!!!!!!!!!!\n", __func__);
#endif

	// 1 : source address and size
	// becase of tearing issue...very critical..

#ifdef ADJUST_PMEM
	if((src_img->format != COPYBIT_FORMAT_CUSTOM_YCbCr_420_SP)
#ifdef CAMERA_CAPTURE_ZERO_COPY
		&& (src_img->format != COPYBIT_FORMAT_CUSTOM_YCbCr_422_I)
		&& (src_img->format != COPYBIT_FORMAT_CUSTOM_CbYCr_422_I) //Kamat
#endif
		&& (ioctl(src_img->memory_id, PMEM_GET_PHYS, &region) >= 0))
	{
		src_phys_addr = (unsigned int)region.offset + src_img->offset;
		LOGD("####PMEM:src_phys_addr::%x \n", src_phys_addr);
	}
	else
#endif // ADJUST_PMEM
	if(ioctl(src_img->memory_id, S3C_FB_GET_CURR_FB_INFO, &fb_info) >= 0)
	{
		src_phys_addr  = fb_info.phy_start_addr + src_img->offset;
		LOGD("####CURR_FB:src_phys_addr::%x \n", src_phys_addr);
	}
	else
	{
#ifdef	VIDEO_ZERO_COPY
		if (COPYBIT_FORMAT_CUSTOM_YCbCr_420_SP == src_img->format)
		{
			// sw workaround for video content zero copy
			memcpy(&s5p_pp->params.src.buf_addr_phy_rgb_y, (void*)((unsigned int)src_img->base), 4);
			memcpy(&s5p_pp->params.src.buf_addr_phy_cb, (void*)((unsigned int)src_img->base+4), 4);
			src_phys_addr = s5p_pp->params.src.buf_addr_phy_rgb_y;
#ifdef FIMC_DEBUG
			 LOGD("#### VIDEO_ZERO_COPY:src_phys_addr::%x \n", src_phys_addr);
#endif
		}
		else
#endif
#ifdef CAMERA_CAPTURE_ZERO_COPY
		if (COPYBIT_FORMAT_CUSTOM_YCbCr_422_I == src_img->format || COPYBIT_FORMAT_CUSTOM_CbYCr_422_I == src_img->format) //Kamat
		{
			// sw workaround for camera capture zero copy
			memcpy(&s5p_pp->params.src.buf_addr_phy_rgb_y, (void*)((unsigned int)src_img->base + src_img->offset), 4);
			src_phys_addr = s5p_pp->params.src.buf_addr_phy_rgb_y;
		}
		else
#endif
		{
			src_frame_size = getFrameSize(src_img->format, src_img->width, src_img->height);

			if(src_frame_size == 0)
			{
				LOGD("%s::getFrameSize fail \n", __func__);
				return -1;
			}

			if (0 > checkMem(ctx, src_frame_size))
			{
				LOGD("%s::check_mem fail \n", __func__);
				return -1;
			}

			#ifdef	MEASURE_DURATION
			before  = systemTime(SYSTEM_TIME_MONOTONIC);
			#endif

			if(src_img->format != COPYBIT_FORMAT_YCbCr_420_SP){
			memcpy(s3c_mem2->virt_addr, (void*)((unsigned int)src_img->base), src_frame_size);
			}
			else{
			memcpy(s3c_mem2->virt_addr, (void*)((unsigned int)src_img->base), (src_img->width * src_img->height));
			CbCrSwap(s3c_mem2->virt_addr + (src_img->width * src_img->height), 
				(unsigned char *)(src_img->base + (src_img->width * src_img->height)), (src_img->width * src_img->height >> 1));
			}			

			src_phys_addr = s3c_mem2->phys_addr;
#ifdef FIMC_DEBUG
			LOGD("#### 3:src_phys_addr::%x \n", src_phys_addr);
#endif
			#ifdef	MEASURE_DURATION
			after  = systemTime(SYSTEM_TIME_MONOTONIC);
			LOGD("src_memcpy: duration=%lld (src_frame_size=%d, id=%d)", ns2us(after-before), src_frame_size, src_img->memory_id);
			#endif
		}
	}


      FIMC_WA_FLAG = fimc_workaround(dst_img->format, dst_rect,  &dst_rect_tmp, dst_img->width, dst_img->height);
	//LOGD("#########dst_img->base %x   dst_img->offset %x \n", dst_img->base, dst_img->offset);   
	// 2 : destination address and size
	// ask this is fb
	//


	if( (ioctl(dst_img->memory_id, S3C_FB_GET_CURR_FB_INFO, &fb_info) >= 0) )	  //work around......dst_img->base is nulll !!!!!!!!!!!!!!!!
	{
		dst_phys_addr  = fb_info.phy_start_addr + dst_img->offset;
//		dst_var_addr = fb_info.var_start_addr + dst_img->offset;
//		isItFB = 1;
#ifdef FIMC_DEBUG
		LOGD("#### FB:phy_start_addr::%x dst_phys_addr::%x \n",fb_info.phy_start_addr, dst_phys_addr);
#endif
	}
#ifdef ADJUST_PMEM
	else if (ioctl(dst_img->memory_id, PMEM_GET_PHYS, &region) >= 0)
	{
		dst_phys_addr = (unsigned int)region.offset + dst_img->offset;
#ifdef FIMC_DEBUG
		LOGD("#### PMEM:region.offset::%x dst_phys_addr::%x \n",region.offset, dst_phys_addr);
#endif
	}
#endif // ADJUST_PMEM
	else
	{
		dst_frame_size = getFrameSize(dst_img->format, dst_img->width, dst_img->height);
		if(dst_frame_size == 0)
		{
			LOGE("%s::getFrameSize fail \n", __func__);
			return -1;
		}

		dst_phys_addr = s3c_mem->phys_addr;
		ExtWidth = dst_rect_tmp.w;
		ExtHeight = dst_rect_tmp.h;
		memcpy(&dst_rect_tmp, dst_rect, sizeof(s3c_rect));
		dst_rect->x = 0;
		dst_rect->y = 0;
		dst_rect->w =ExtWidth;
		dst_rect->h =ExtHeight;
		// memcpy trigger...
		flag_force_memcpy = 1;
#ifdef FIMC_DEBUG
		 LOGD("#### s3c_mem:dst_phys_addr::%x \n", dst_phys_addr);
#endif
	}

	rotate_value = rotateValueCopybit2G2D(ctx->mFlags);    
    
	if(   src_img->format == COPYBIT_FORMAT_YCbCr_420_SP
	   || src_img->format == COPYBIT_FORMAT_CUSTOM_YCbCr_420_SP
	   || src_img->format == COPYBIT_FORMAT_YCbCr_420_P
	   || src_img->format == COPYBIT_FORMAT_YCbCr_422_I
	   || src_img->format == COPYBIT_FORMAT_YCbCr_422_SP
	   || src_img->format == COPYBIT_FORMAT_CUSTOM_YCbCr_422_I
	   || src_img->format == COPYBIT_FORMAT_CUSTOM_CbYCr_422_I) //Kamat
	{
#if	defined(HW_FIMC_USE)
		// YUV case
		if (dst_img->format == COPYBIT_FORMAT_RGB_565)
		{
			// check the path to handle the source YUV data

			thru_g2d_path	= checkPPOutPath(ctx, dst_img, dst_rect);

			if (0 == thru_g2d_path)
			{
				// PATH : src img -> FIMC -> ext output dma (FB or mem)
#ifdef FIMC_DEBUG
				LOGD("### PATH : src img -> FIMC -> ext output dma\n");
#endif
				#ifdef	MEASURE_DURATION
				before  = systemTime(SYSTEM_TIME_MONOTONIC);
				#endif
				if(doPP(ctx, src_phys_addr, src_img, src_rect,
					         dst_phys_addr, dst_img, dst_rect, ctx->mFlags, 1) < 0) 
				{
					LOGE("%s::doPP fail\n", __func__);
					return -1;
				}
				#ifdef	MEASURE_DURATION
				after  = systemTime(SYSTEM_TIME_MONOTONIC);
				LOGD("[fimc->fb] doPP: duration=%lld (ext_out_mem=%d dst_phys_addr=%x)", ns2us(after-before), ctx->s5p_pp.use_ext_out_mem, dst_phys_addr);
				#endif
			}
			else
			{
#ifdef HW_G2D_USE
				// PATH : srg img -> FIMC -> internal output dma -> G2D -> dst (FB or mem)
			
				s3c_img  temp_img;
				s3c_rect temp_rect;
			
				temp_img.format    = COPYBIT_FORMAT_RGB_565;
				temp_img.memory_id = dst_img->memory_id;
				temp_img.offset    = dst_img->offset;
				temp_img.base      = dst_img->base;
				temp_rect.x = 0;
				temp_rect.y = 0;
	
				if(   rotate_value == S3C_G2D_ROTATOR_90
				   || rotate_value == S3C_G2D_ROTATOR_270)
				{
#if 0
					temp_img.width  = dst_img->height;
					temp_img.height = dst_img->width;
#else
					temp_img.width  = dst_rect->h;
					temp_img.height = dst_rect->w;
#endif
					temp_rect.w     = dst_rect->h;
					temp_rect.h     = dst_rect->w;
				}
				else // if(rotate_value == S3C_G2D_ROTATOR_0 || rotate_value == S3C_G2D_ROTATOR_180) .. etc..
				{
#if 0
					temp_img.width  = dst_img->width;
					temp_img.height = dst_img->height;
#else
					temp_img.width  = dst_rect->w;
					temp_img.height = dst_rect->h;
#endif
					temp_rect.w     = dst_rect->w;
					temp_rect.h     = dst_rect->h;
				}
				
				#ifdef	MEASURE_DURATION
				before  = systemTime(SYSTEM_TIME_MONOTONIC);
				#endif
				if(doPP(ctx, src_phys_addr, src_img, src_rect,
							 0, &temp_img, &temp_rect, 0, (ctx->s5p_pp.use_ext_out_mem?1:0)) < 0) 
				{
					LOGE("%s::doPP fail\n", __func__);
					return -1;
				}
				#ifdef	MEASURE_DURATION
				after  = systemTime(SYSTEM_TIME_MONOTONIC);
				LOGD("doPP: duration=%lld (ext_out_mem=%d)", ns2us(after-before), ctx->s5p_pp.use_ext_out_mem);
				#endif
	
				#ifdef	MEASURE_DURATION 
				before  = systemTime(SYSTEM_TIME_MONOTONIC);
				#endif
				if(doG2D(ctx, s5p_pp->out_buf.phys_addr, &temp_img, &temp_rect,
								dst_phys_addr, dst_img,  dst_rect,
				                rotate_value, ctx->mAlpha) < 0)
				{
					LOGE("%s::doG2D fail\n", __func__);
					return -1;
				}
				#ifdef	MEASURE_DURATION
				after  = systemTime(SYSTEM_TIME_MONOTONIC);
				LOGD("doG2D: duration=%lld", ns2us(after-before));
				#endif
#else
				return -1;
#endif
			}
		}
		else if(dst_img->format == COPYBIT_FORMAT_YCbCr_420_SP
		   || dst_img->format == COPYBIT_FORMAT_YCbCr_420_P
		   || dst_img->format == COPYBIT_FORMAT_YCbCr_422_SP
		   || dst_img->format == COPYBIT_FORMAT_YCbCr_422_I)
		{
			LOGD("##########doPP::2\n");
			if(doPP(ctx, src_phys_addr, src_img, src_rect,
				         dst_phys_addr, dst_img, dst_rect, ctx->mFlags, (ctx->s5p_pp.use_ext_out_mem?0:1)) < 0) 
			{
				LOGE("%s::doPP fail\n", __func__);
				return -1;
			}
		}
		else
		{
			LOGE("%s: not support destination color format (0x%x)\n", __func__, dst_img->format);
			return -1;
		}		
#else
		LOGE("%s: not support source color format (0x%x)\n", __func__, src_img->format);
		return -1;
#endif
	}
	else
	{
#ifdef HW_G2D_USE
		#ifdef	MEASURE_DURATION 
		before  = systemTime(SYSTEM_TIME_MONOTONIC);
		#endif
		if(doG2D(ctx, src_phys_addr, src_img, src_rect,
		              dst_phys_addr, dst_img, dst_rect,
		              rotate_value, ctx->mAlpha) < 0) 
		{
			LOGE("%s::doG2D fail\n", __func__);
			return -1;
		}		
		#ifdef	MEASURE_DURATION 
		after  = systemTime(SYSTEM_TIME_MONOTONIC);
		LOGD("doG2D: duration=%lld", ns2us(after-before));
		#endif
#else
		if (src_img->format == COPYBIT_FORMAT_RGB_565)
		{
			// consider blit from FB to FB
			
			#ifdef	MEASURE_DURATION
			before  = systemTime(SYSTEM_TIME_MONOTONIC);
			#endif
			if(doPP(ctx, src_phys_addr, src_img, src_rect,
				         dst_phys_addr, dst_img, dst_rect, ctx->mFlags, 1) < 0) 
			{
				LOGE("%s::doPP fail\n", __func__);
				return -1;
			}
			#ifdef	MEASURE_DURATION
			after  = systemTime(SYSTEM_TIME_MONOTONIC);
			LOGD("[fimc->fb] doPP: duration=%lld (ext_out_mem=%d)", ns2us(after-before), ctx->s5p_pp.use_ext_out_mem);
			#endif
		}
		else
		{
			LOGE("%s: not support source color format (0x%x)\n", __func__, src_img->format);
		}
#endif
	}
	
	if(flag_force_memcpy == 1)
	{
		LOGD("#####flag_force_memcpy: dst_img->base:: %x s3c_mem->virt_addr:: %x, dst_frame_size:: %d\n", dst_img->base, s3c_mem->virt_addr, dst_frame_size);
#ifndef HW_G2D_USE
#if 0
		memcpy((void*)((unsigned int)dst_img->base),
				s3c_mem->virt_addr,
				dst_frame_size);
#else
		FIMC_WA_framecopy((unsigned char *) (dst_img->base), (unsigned char *) (s3c_mem->virt_addr), &dst_rect_tmp, dst_rect, dst_img->width);
#endif
#else
		int g2d_color_space = colorFormatCopybit2G2D(dst_img->format);
		if(g2d_color_space < 0)
		{
			LOGE("%s::colorFormatCopybit2G2D fail\n", __func__);
			return -1;
		}

		if(copyG2Dframe(s3c_mem->virt_addr, (unsigned char * )((unsigned int)dst_img->base),
		                dst_img->width, dst_img->height, 
		                dst_rect->x, dst_rect->y,
		                dst_rect->w, dst_rect->h,
		                rotate_value, g2d_color_space) < 0)
		{
			LOGE("%s::copyG2Dframe fail\n", __func__);
			return -1;
		}
#endif
	}
	return 0;
}

#ifdef	HW_FIMC_USE
int fimc_v4l2_set_src(int fd, s5p_pp_img_info *src)
{
	struct v4l2_format			fmt;
	struct v4l2_cropcap			cropcap;
	struct v4l2_crop			crop;
	struct v4l2_requestbuffers	req;    
	int			ret_val;

	/* 
	 * To set size & format for source image (DMA-INPUT) 
	 */

	fmt.type 				= V4L2_BUF_TYPE_VIDEO_OUTPUT;
	fmt.fmt.pix.width 		= src->full_width;
	fmt.fmt.pix.height 		= src->full_height;
	fmt.fmt.pix.pixelformat	= src->color_space;
	fmt.fmt.pix.field 		= V4L2_FIELD_NONE; 

	ret_val = ioctl (fd, VIDIOC_S_FMT, &fmt);
	if (ret_val < 0) {
    	LOGE("VIDIOC_S_FMT failed : ret=%d\n", ret_val);
		return -1;
	}

#ifdef FIMC_GET
	/* To see modified values*/
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	ret_val= ioctl(fd, VIDIOC_G_FMT, &fmt);
	if (ret_val < 0) {
		printf("[%s] Error in video VIDIOC_G_FMT\n", __FUNCTION__);
		return -1;
	}

#endif

	/* 
	 * crop input size
	 */

	crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	crop.c.left   = 0;
	crop.c.top    = 0;
	crop.c.width  = src->width;
	crop.c.height = src->height;

	ret_val = ioctl(fd, VIDIOC_S_CROP, &crop);
	if (ret_val < 0) {
		LOGE("Error in video VIDIOC_S_CROP (%d)\n",ret_val);
		return -1;
	}

	/* 
	 * input buffer type
	 */

	req.count       = 1;
	req.type        = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	req.memory      = V4L2_MEMORY_USERPTR;

 	ret_val = ioctl (fd, VIDIOC_REQBUFS, &req);
 	if (ret_val < 0) 
	{
		LOGE("Error in VIDIOC_REQBUFS (%d)\n", ret_val);
		return -1;
	}

	return ret_val;
}

int fimc_v4l2_set_dst(int fd, s5p_pp_img_info *dst, int rotation, unsigned int addr)
{
	struct v4l2_format		sFormat;
	struct v4l2_control		vc;
	struct v4l2_framebuffer	fbuf;
	int				ret_val;

	/* 
	 * set rotation configuration
	 */

	vc.id = V4L2_CID_ROTATION;
	vc.value = rotation;

	ret_val = ioctl(fd, VIDIOC_S_CTRL, &vc);
	if (ret_val < 0) {
		LOGE("Error in video VIDIOC_S_CTRL - rotation (%d)\n",ret_val);
		return -1;
	}

	/*
	 * set size, format & address for destination image (DMA-OUTPUT)
	 */
	ret_val = ioctl (fd, VIDIOC_G_FBUF, &fbuf);
	if (ret_val < 0){
		LOGE("Error in video VIDIOC_G_FBUF (%d)\n", ret_val);
		return -1;
	}

	fbuf.base 				= (void *)addr;
	fbuf.fmt.width 			= dst->full_width;
	fbuf.fmt.height 		= dst->full_height;
	fbuf.fmt.pixelformat 	= dst->color_space;

	ret_val = ioctl (fd, VIDIOC_S_FBUF, &fbuf);
	if (ret_val < 0)  {
		LOGE("Error in video VIDIOC_S_FBUF (%d)\n",ret_val);        
		return -1;
	}

	/* 
	 * set destination window
	 */

	sFormat.type 				= V4L2_BUF_TYPE_VIDEO_OVERLAY;
	sFormat.fmt.win.w.left 		= dst->start_x;
	sFormat.fmt.win.w.top 		= dst->start_y;
	sFormat.fmt.win.w.width 	= dst->width;
	sFormat.fmt.win.w.height 	= dst->height;

	ret_val = ioctl(fd, VIDIOC_S_FMT, &sFormat);
	if (ret_val < 0) {
		LOGE("Error in video VIDIOC_S_FMT (%d)\n",ret_val);
		return -1;
	}
#ifdef FIMC_GET
	sFormat.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	ret_val= ioctl(fd, VIDIOC_G_FMT, &sFormat);
	if (ret_val < 0) {
		printf("[%s] Error in video VIDIOC_G_FMT\n", __FUNCTION__);
		return -1;
	}
#endif


	return 0;
}

int fimc_v4l2_stream_on(int fd, enum v4l2_buf_type type)
{
	if (-1 == ioctl (fd, VIDIOC_STREAMON, &type)) {
		LOGE("Error in VIDIOC_STREAMON\n");
		return -1;
	}

	return 0;
}

int fimc_v4l2_queue(int fd, struct fimc_buf *fimc_buf)
{
	struct v4l2_buffer	buf;

	buf.type        = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	buf.memory      = V4L2_MEMORY_USERPTR;
	buf.m.userptr	= (unsigned long)fimc_buf;
	buf.length		= 0;
	buf.index       = 0;

	int	ret_val;

	ret_val = ioctl (fd, VIDIOC_QBUF, &buf);
	if (0 > ret_val) {
		LOGE("Error in VIDIOC_QBUF : (%d) \n", ret_val);
		return -1;
	}

	return 0;
}

int fimc_v4l2_dequeue(int fd)
{
	struct v4l2_buffer          buf;

	buf.type        = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	buf.memory      = V4L2_MEMORY_USERPTR;

	if (-1 == ioctl (fd, VIDIOC_DQBUF, &buf)) {
		LOGE("Error in VIDIOC_DQBUF\n");
		return -1;
	}

	return buf.index;
}

int fimc_v4l2_stream_off(int fd)
{
	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    
	if (-1 == ioctl (fd, VIDIOC_STREAMOFF, &type)) {
		LOGE("Error in VIDIOC_STREAMOFF\n");
		return -1;
	}

	return 0;
}

int fimc_v4l2_clr_buf(int fd)
{
	struct v4l2_requestbuffers req;

	req.count   = 0;
	req.type    = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	req.memory  = V4L2_MEMORY_USERPTR;

	if (ioctl (fd, VIDIOC_REQBUFS, &req) == -1) {
		LOGE("Error in VIDIOC_REQBUFS");
	}

	return 0;
}

int fimc_handle_oneshot(int fd, struct fimc_buf *fimc_buf)
{
	int		ret;

	ret = fimc_v4l2_stream_on(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	if(ret < 0) {
		LOGE("Fail : v4l2_stream_on()");
		return -1;
	}

	ret = fimc_v4l2_queue(fd, fimc_buf);  
	if(ret < 0) {
		LOGE("Fail : v4l2_queue()\n");
		return -1;
	}

#if 0
	ret = fimc_v4l2_dequeue(fd);
	if(ret < 0) {
		LOGE("Fail : v4l2_dequeue()\n");
		return -1;
	}
#endif

	ret = fimc_v4l2_stream_off(fd);    
	if(ret < 0) {
		LOGE("Fail : v4l2_stream_off()");
		return -1;
	}

	ret = fimc_v4l2_clr_buf(fd);
	if(ret < 0) {
		LOGE("Fail : v4l2_clr_buf()");
		return -1;
	}

	return 0;
}

static int createPP(struct copybit_context_t* ctx)
{	
	struct v4l2_capability 		cap;
	struct v4l2_format			fmt;
	s5p_pp_t *s5p_pp 			= &ctx->s5p_pp;
	s5p_pp_params_t * params 	= &(s5p_pp->params);
	s3c_fb_t *s3c_fb 			= &ctx->s3c_fb;
	int		ret, index;

	#define  PP_DEVICE_DEV_NAME  "/dev/video1"
	
	//return -1;
	// open device file
	if(s5p_pp->dev_fd == 0)
		s5p_pp->dev_fd = open(PP_DEVICE_DEV_NAME, O_RDWR);
	if (0 > s5p_pp->dev_fd)
	{
		if (EACCES == errno || EBUSY == errno)
		{
			// egl_window_surface_v2_t is initialized when eglCreateWindowSurface() is called.
			// Eclair loads copybit lib whenever egl_window_surface_v2_t is initialized.
			// however, fimc cannot be opened multiply.
			// So, fimc just can be open at only one time 
			// when egl_window_surface_v2_t is initialized by system, it means when Surfaceflinger is initialized.
			LOGD("%s::post processor already open, using only G2D\n", __func__);
			return 0;
		}
		else
		{
			LOGE("%s::Post processor open error (%d)\n", __func__,  errno);
			return -1;
		}
	}

	// check capability
	ret = ioctl(s5p_pp->dev_fd, VIDIOC_QUERYCAP, &cap);
	if (ret < 0) {
		LOGE("VIDIOC_QUERYCAP failed\n");
		return -1;
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		LOGE("%d has no streaming support\n", s5p_pp->dev_fd);
		return -1;
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
		LOGE("%d is no video output\n", s5p_pp->dev_fd);
		return -1;
	}

	/*
	 * malloc fimc_outinfo structure
	 */
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ret = ioctl(s5p_pp->dev_fd, VIDIOC_G_FMT, &fmt);
    if (ret < 0) {
        LOGE("[%s] Error in video VIDIOC_G_FMT\n", __FUNCTION__);
        return -1;
    }

	s5p_pp->use_ext_out_mem = 0;

	return 0;
}

static int destroyPP(struct copybit_context_t *ctx)
{
	s5p_pp_t *s5p_pp = &ctx->s5p_pp;
	
	if(s5p_pp->out_buf.virt_addr != NULL)
	{
		s5p_pp->out_buf.virt_addr = NULL;
		s5p_pp->out_buf.length = 0;
	}

	// close
	if(s5p_pp->dev_fd != 0)
	{
		close(s5p_pp->dev_fd);
		s5p_pp->dev_fd = 0;
	}
	
	return 0;
}


static int doPP(struct copybit_context_t *ctx,
                unsigned int src_phys_addr, s3c_img *src_img, s3c_rect *src_rect,
                unsigned int dst_phys_addr, s3c_img *dst_img, s3c_rect *dst_rect,
				int rotate_flag, int reset_flag)
{
	s5p_pp_t        * s5p_pp = &ctx->s5p_pp;
	s5p_pp_params_t * params = &(s5p_pp->params);

	unsigned int      frame_size = 0;

	int		src_color_space, dst_color_space;
	int		src_bpp, src_planes;

	struct fimc_buf		fimc_src_buf;

	int		rotate_value = rotateValueCopybit2PP(rotate_flag);

	nsecs_t   before, after;

#ifdef FIMC_DEBUG
        LOGD("%s::start!!!\n", __func__);
#endif


	// set post processor configuration
	src_color_space     	= colorFormatCopybit2PP(src_img->format);
	params->src.full_width  = src_img->width;
	params->src.full_height = src_img->height;
	params->src.start_x     = src_rect->x;
	params->src.start_y     = src_rect->y;
	params->src.width       = widthOfPP(src_color_space, src_rect->w);
	params->src.height      = heightOfPP(src_color_space, src_rect->h);
	params->src.color_space = src_color_space;
#ifdef FIMC_DEBUG	
	LOGD("doPP::f_w=%d f_h=%d x=%d y=%d w=%d h=%d (ow=%d oh=%d) format=0x%x\n", 
			params->src.full_width, params->src.full_height, params->src.start_x, params->src.start_y,
                        params->src.width, params->src.height, src_rect->w, src_rect->h, params->src.color_space);
#endif

#if 1
	if (src_rect->w < 16 || src_rect->h < 8) 
	{
		LOGE("%s src size is not supported by fimc : f_w=%d f_h=%d x=%d y=%d w=%d h=%d (ow=%d oh=%d) format=0x%x", __func__,
			params->src.full_width, params->src.full_height, params->src.start_x, params->src.start_y, 
			params->src.width, params->src.height, src_rect->w, src_rect->h, params->src.color_space);
		return -1;
	}
#else
	if ((src_rect->w & 0xF) || (src_rect->h & 0x7)) 
        {
                LOGE("%s src size is not supported by fimc : f_w=%d f_h=%d x=%d y=%d w=%d h=%d (ow=%d oh=%d) format=0x%x", __func__,
                        params->src.full_width, params->src.full_height, params->src.start_x, params->src.start_y, 
                        params->src.width, params->src.height, src_rect->w, src_rect->h, params->src.color_space);
                return -1;
        }

#endif
	
	if (COPYBIT_FORMAT_CUSTOM_YCbCr_420_SP != src_img->format
#ifdef CAMERA_CAPTURE_ZERO_COPY
		&& COPYBIT_FORMAT_CUSTOM_YCbCr_422_I != src_img->format
		&& COPYBIT_FORMAT_CUSTOM_CbYCr_422_I != src_img->format //Kamat
#endif
		)
		params->src.buf_addr_phy_rgb_y 	= src_phys_addr;

	dst_color_space     	= colorFormatCopybit2PP(dst_img->format);

	if (90 == rotate_value || 270 == rotate_value)
	{
#ifdef FIMC_DEBUG
		LOGD("#####doPP:rotate_value 90 or 270\n");
#endif
		params->dst.full_width  = dst_img->height;
		params->dst.full_height = dst_img->width;

		params->dst.start_x     = dst_rect->y;
		params->dst.start_y     = dst_rect->x;

		//params->dst.width       = heightOfPP(dst_color_space, dst_rect->h);
		params->dst.width       = widthOfPP(dst_color_space, dst_rect->h);
		//params->dst.height      = heightOfPP(dst_color_space, dst_rect->w);
		params->dst.height      = widthOfPP(dst_color_space, dst_rect->w);

		// work-around
		params->dst.start_y     += (dst_rect->w - params->dst.height);
#ifdef FIMC_DEBUG
		LOGD("DST###f_w::%d f_h::%d s_x::%d s_y::%d d_w::%d d_h::%d r_w::%d r_h::%d\n", 
			params->dst.full_width,  params->dst.full_height,  params->dst.start_x,  params->dst.start_y, 
			 params->dst.width, params->dst.height, dst_rect->w, dst_rect->h); 	        
#endif
	}
	else
	{
		params->dst.full_width  = dst_img->width;
		params->dst.full_height = dst_img->height;

		params->dst.start_x     = dst_rect->x;
		params->dst.start_y     = dst_rect->y;

		params->dst.width       = widthOfPP(dst_color_space, dst_rect->w);
		params->dst.height      = heightOfPP(dst_color_space, dst_rect->h);

	}
	params->dst.color_space = dst_color_space;

	if (src_rect->w < 8 || src_rect->h < 8) 
	{
		LOGE("%s dst size is not supported by fimc : f_w=%d f_h=%d x=%d y=%d w=%d h=%d (ow=%d oh=%d) format=0x%x", __func__, 
			params->dst.full_width, params->dst.full_height, params->dst.start_x, params->dst.start_y, 
			params->dst.width, params->dst.height, dst_rect->w, dst_rect->h, params->dst.color_space);
		return -1;
	}

	if (0 == dst_phys_addr) 
			s5p_pp->use_ext_out_mem = 0;
		else	
			s5p_pp->use_ext_out_mem = 1;

	/* set configuration related to source (DMA-INPUT)
	 *   - set input format & size 
	 *   - crop input size 
	 *   - set input buffer
	 *   - set buffer type (V4L2_MEMORY_USERPTR)
	 */
	if (fimc_v4l2_set_src(s5p_pp->dev_fd, &params->src) < 0) 
	{
		return -1;
	}

	/* set configuration related to destination (DMA-OUT)
	 *   - set input format & size 
	 *   - crop input size 
	 *   - set input buffer
	 *   - set buffer type (V4L2_MEMORY_USERPTR)
	 */
	if (fimc_v4l2_set_dst(s5p_pp->dev_fd, &params->dst, rotate_value, dst_phys_addr) < 0) 
	{
		return -1;
	}

	// set input dma address (Y/RGB, Cb, Cr)
	if (COPYBIT_FORMAT_CUSTOM_YCbCr_420_SP == src_img->format)
	{
		// for video contents zero copy case
		fimc_src_buf.base[0]	= params->src.buf_addr_phy_rgb_y;
		fimc_src_buf.base[1]	= params->src.buf_addr_phy_cb; 
	}
	else
#ifdef CAMERA_CAPTURE_ZERO_COPY
	if (COPYBIT_FORMAT_CUSTOM_YCbCr_422_I == src_img->format || COPYBIT_FORMAT_CUSTOM_CbYCr_422_I == src_img->format) //Kamat
	{
		// for camera capture zero copy case
		fimc_src_buf.base[0]	= params->src.buf_addr_phy_rgb_y;
	}
	else
#endif
	{
		// set source image
		src_bpp		= get_yuv_bpp(src_color_space);
		src_planes 	= get_yuv_planes(src_color_space);

		fimc_src_buf.base[0]	= params->src.buf_addr_phy_rgb_y;
#ifdef FIMC_DEBUG
		LOGD("###doPP::Y: %x \n",  params->src.buf_addr_phy_rgb_y);
#endif
		if (2 == src_planes)
		{
			frame_size = params->src.full_width * params->src.full_height;
			params->src.buf_addr_phy_cb		= params->src.buf_addr_phy_rgb_y + frame_size;

			fimc_src_buf.base[1]	= params->src.buf_addr_phy_cb;
#ifdef FIMC_DEBUG
			 LOGD("###doPP::cb: %x \n",  params->src.buf_addr_phy_cb); 
#endif
		}
		else if (3 == src_planes)
		{
			frame_size = params->src.full_width * params->src.full_height;
			params->src.buf_addr_phy_cb		= params->src.buf_addr_phy_rgb_y + frame_size;

			if (12 == src_bpp)
				 params->src.buf_addr_phy_cr = params->src.buf_addr_phy_cb + (frame_size >> 2);
			else
				 params->src.buf_addr_phy_cr = params->src.buf_addr_phy_cb + (frame_size >> 1);
	
			fimc_src_buf.base[1]	= params->src.buf_addr_phy_cb; 
			fimc_src_buf.base[2]	= params->src.buf_addr_phy_cr;
#ifdef FIMC_DEBUG 
			 LOGD("###doPP::cb:%x cr:%x \n",  params->src.buf_addr_phy_cb, params->src.buf_addr_phy_cr);
#endif
		}
	}


	if (fimc_handle_oneshot(s5p_pp->dev_fd, &fimc_src_buf) < 0)
	{
		fimc_v4l2_clr_buf(s5p_pp->dev_fd);
		return -1;
	}

#ifdef FIMC_DEBUG
        LOGD("%s::success!!!\n", __func__);
#endif


	return 0;
}
#endif	// HW_FIMC_USE

#ifdef HW_G2D_USE
static int createG2D(struct copybit_context_t *ctx)
{
	#define S3C_G2D_DEV_NAME  "/dev/s3c-g2d"
	
	s3c_g2d_t *s3c_g2d = &ctx->s3c_g2d;
	
	if(s3c_g2d->dev_fd == 0)
		s3c_g2d->dev_fd = open(S3C_G2D_DEV_NAME, O_RDONLY);

	if(s3c_g2d->dev_fd < 0)
	{
		LOGE("%s::open(%s) fail\n", __func__, S3C_G2D_DEV_NAME);
		return -1;
	}

	return 0;
}

static int destroyG2D(struct copybit_context_t *ctx)
{
	if(ctx->s3c_g2d.dev_fd != 0)
		close(ctx->s3c_g2d.dev_fd);
	ctx->s3c_g2d.dev_fd = 0;
	
	return 0;
}

static int doG2D(struct copybit_context_t *ctx,
                 unsigned int src_phys_addr, s3c_img *src_img, s3c_rect *src_rect,
                 unsigned int dst_phys_addr, s3c_img *dst_img, s3c_rect *dst_rect,
                 int rotate_value, int alpha_value)
{
	s3c_g2d_t *      s3c_g2d = &ctx->s3c_g2d;
	s3c_g2d_params * params  = &(s3c_g2d->params);
	
	int bpp_src = 0;
	int bpp_dst = 0;

	params->src_base_addr   = src_phys_addr;
	params->src_full_width  = src_img->width;
	params->src_full_height = src_img->height;

	params->src_start_x     = src_rect->x;
	params->src_start_y     = src_rect->y;
	params->src_work_width  = src_rect->w;
	params->src_work_height = src_rect->h;
		
	params->dst_base_addr   = dst_phys_addr;
	params->dst_full_width  = dst_img->width;
	params->dst_full_height = dst_img->height;

	params->dst_start_x     = dst_rect->x;
	params->dst_start_y     = dst_rect->y;	
	params->dst_work_width  = dst_rect->w;
	params->dst_work_height = dst_rect->h;
		
	//initialize clipping window
	params->cw_x1 = 0;
	params->cw_y1 = 0;
	params->cw_x2 = params->dst_full_width - 1;
	params->cw_y2 = params->dst_full_height - 1;
	
	bpp_src = colorFormatCopybit2G2D(src_img->format);
	bpp_dst = colorFormatCopybit2G2D(dst_img->format);	

	if(bpp_src == -1 || bpp_dst == -1)
	{
		LOGE("%s::colorFormatCopybit2G2D(src_img->format(%d)/dst_img->format(%d)) fail\n",
			  __func__, src_img->format, dst_img->format);
		return -1;
	}

	params->bpp_dst = bpp_dst;
	params->bpp_src = bpp_src;

	if (G2D_RGBA32 == bpp_src)
		params->src_big_endian_flag = 1;
	else
		params->src_big_endian_flag = 0;

	if (255 > alpha_value)
	{
		params->alpha_val      = alpha_value;
		params->alpha_mode     = 1;
	}
	else
	{
		params->alpha_val      = 0;
		params->alpha_mode     = 0;
	}

	params->color_key_mode = 0;
	params->color_key_val  = 0;

	if(ioctl(s3c_g2d->dev_fd, rotate_value, params) < 0)
	{
		LOGE("%s::S3C_G2D_ROTATOR_%d fail\n", __func__, rotate_value);
		return -1;
	}

	return 0;
}

// this func assumes G2D_RGB16, G2D_RGBA16, G2D_RGBA32 ...
static int copyG2Dframe(unsigned char * src_virt_addr,
                        unsigned char * dst_virt_addr,
						unsigned int full_width, unsigned int full_height,
						unsigned int start_x,    unsigned int start_y,
						unsigned int width,      unsigned int height,
                        int rotate_value,        int g2d_color_space)
{
	unsigned int x;
	unsigned int y;

	unsigned char * current_src_addr  = src_virt_addr;
	unsigned char * current_dst_addr  = dst_virt_addr;

	unsigned int full_frame_size  = 0;

	unsigned int real_full_width;
	unsigned int real_full_height;
	unsigned int real_start_x;
	unsigned int real_width;
						
	if(full_width == width && full_height == height)
	{
		unsigned int full_frame_size  = 0;

		if(g2d_color_space == G2D_RGBA32)
		{
			full_frame_size = ((full_width * full_height) << 2);
		}
		else // RGB16
			full_frame_size = ((full_width * full_height) << 1);

		memcpy(dst_virt_addr, src_virt_addr, full_frame_size);
	}
	else
	{
		if(g2d_color_space == G2D_RGBA32)
		{
			// 32bit
			real_full_width  = full_width  << 2;
			real_full_height = full_height << 2;
			real_start_x     = start_x     << 2;
			real_width       = width       << 2;
		}
		else
		{
			// 16bit
			real_full_width  = full_width  << 1;
			real_full_height = full_height << 1;
			real_start_x     = start_x     << 1;
			real_width       = width       << 1;
		}

		current_src_addr += (real_full_width * start_y);
		current_dst_addr += (real_full_width * start_y);

		if(full_width == width)
		{
			memcpy(current_dst_addr, current_src_addr, real_width * height);
		}
		else
		{
			for(y = 0; y < height; y++)
			{
				memcpy(current_dst_addr + real_start_x,
				       current_src_addr + real_start_x,
				       real_width);

				current_src_addr += real_full_width;
				current_dst_addr += real_full_width;
			}
		}
	}

	return 0;
}
#endif // HW_G2D_USE

static int createMem(struct copybit_context_t *ctx, unsigned int memory_size)
{
	int i = 0;

	for(i = 0; i < NUM_OF_MEMORY_OBJECT; i++)
	{
		s3c_mem_t  * s3c_mem = &ctx->s3c_mem[i];
		
		#define S3C_MEM_DEV_NAME "/dev/s3c-mem"

		if(s3c_mem->dev_fd == 0)
			s3c_mem->dev_fd = open(S3C_MEM_DEV_NAME, O_RDWR);
		if(s3c_mem->dev_fd < 0)
		{
			LOGE("%s::open(%s) fail(%s)\n", __func__, S3C_MEM_DEV_NAME, strerror(errno));
			s3c_mem->dev_fd = 0;
			return -1;
		}
		
		// kcoolsw
		s3c_mem->mem_alloc_info.size = memory_size;

		if(ioctl(s3c_mem->dev_fd, S3C_MEM_CACHEABLE_ALLOC, &s3c_mem->mem_alloc_info) < 0)
		{	
			LOGE("%s::S3C_MEM_ALLOC(size : %d)  fail\n", __func__, s3c_mem->mem_alloc_info.size);
			return -1;
		}

		s3c_mem->phys_addr =                 s3c_mem->mem_alloc_info.phy_addr;
		s3c_mem->virt_addr = (unsigned char*)s3c_mem->mem_alloc_info.vir_addr;
	}
		
	return 0;
}

static int destroyMem(struct copybit_context_t *ctx)
{
	int i = 0;
	for(i = 0; i < NUM_OF_MEMORY_OBJECT; i++)
	{
		s3c_mem_t  * s3c_mem = &ctx->s3c_mem[i];

		if(0 < s3c_mem->dev_fd)
		{
			if (ioctl(s3c_mem->dev_fd, S3C_MEM_FREE, &s3c_mem->mem_alloc_info) < 0)
			{
				LOGE("%s::S3C_MEM_FREE fail\n", __func__);
				return -1;
			}
			
			close(s3c_mem->dev_fd);
			s3c_mem->dev_fd = 0;
		}
		s3c_mem->phys_addr = 0;
		s3c_mem->virt_addr = NULL;
	}

	return 0;
}

#ifdef	HW_FIMC_USE
static unsigned int get_yuv_bpp(unsigned int fmt)
{
	int i, sel = -1;

	for (i = 0; i < (int)(sizeof(yuv_list) / sizeof(struct yuv_fmt_list)); i++) {
		if (yuv_list[i].fmt == fmt) {
			sel = i;
			break;
		}
	}

	if (sel == -1)
		return sel;
	else
		return yuv_list[sel].bpp;
}

static unsigned int get_yuv_planes(unsigned int fmt)
{
	int i, sel = -1;

	for (i = 0; i < (int)(sizeof(yuv_list) / sizeof(struct yuv_fmt_list)); i++) {
		if (yuv_list[i].fmt == fmt) {
			sel = i;
			break;
		}
	}

	if (sel == -1)
		return sel;
	else
		return yuv_list[sel].planes;
}

static inline int colorFormatCopybit2PP(int format)
{
	switch (format) 
	{
		case COPYBIT_FORMAT_RGBA_8888:    			return V4L2_PIX_FMT_RGB32;
		case COPYBIT_FORMAT_RGB_565:      			return V4L2_PIX_FMT_RGB565;		
		//case COPYBIT_FORMAT_YCbCr_422_SP:			return V4L2_PIX_FMT_NV16;
		case COPYBIT_FORMAT_YCbCr_422_SP:			return V4L2_PIX_FMT_NV61;
		case COPYBIT_FORMAT_CUSTOM_CbYCr_422_I:		return V4L2_PIX_FMT_UYVY; //Kamat
		case COPYBIT_FORMAT_CUSTOM_YCbCr_422_I:		return V4L2_PIX_FMT_YUYV;
		//case COPYBIT_FORMAT_YCbCr_420_SP: 			return V4L2_PIX_FMT_NV21;
		case COPYBIT_FORMAT_YCbCr_420_SP: 			return V4L2_PIX_FMT_NV12;
		case COPYBIT_FORMAT_CUSTOM_YCbCr_420_SP: 	return V4L2_PIX_FMT_NV12T;
		case COPYBIT_FORMAT_YCbCr_420_P: 			return V4L2_PIX_FMT_YUV420;
		default :
			LOGE("%s::not matched frame format : %d\n", __func__, format);
			break;
	}
    return -1;
}

static int fimc_v4l2_streamon(int fp)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int ret;

	ret = ioctl(fp, VIDIOC_STREAMON, &type);
	if (ret < 0) {
    		LOGE("VIDIOC_STREAMON failed\n");
    		return -1;
  	}

	return ret;
}

static int fimc_v4l2_streamoff(int fp)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int ret;

	ret = ioctl(fp, VIDIOC_STREAMOFF, &type);
	if (ret < 0) {
    		LOGE("VIDIOC_STREAMOFF failed\n");
    		return -1;
  	}

	return ret;
}

static int fimc_v4l2_start_overlay(int fd)
{
    int ret, start = 1;

    ret = ioctl(fd, VIDIOC_OVERLAY, &start);
    if (ret < 0)
    {
        LOGE("fimc_v4l2_start_overlay() VIDIOC_OVERLAY failed\n");
        return -1;
    }

    LOGV("fimc_v4l2_start_overlay() success\n");
    return ret;
}

static int fimc_v4l2_stop_overlay(int fd)
{
    int ret, start = 0;

    ret = ioctl(fd, VIDIOC_OVERLAY, &start);
    if (ret < 0)
    {
        LOGE("fimc_v4l2_stop_overlay() VIDIOC_OVERLAY failed\n");
        return -1;
    }

    LOGV("fimc_v4l2_stop_overlay() success\n");
    return ret;
}

static int fimc_v4l2_s_crop(int fp, int target_width, int target_height, int h_offset, int v_offset)
{
	struct v4l2_crop crop;
	int ret;

	crop.type 		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	crop.c.left 	= h_offset;
	crop.c.top 		= v_offset;
	crop.c.width 	= target_width;
	crop.c.height 	= target_height;
		

	ret = ioctl(fp, VIDIOC_S_CROP, &crop);
	if (ret < 0) {
		LOGE("VIDIOC_S_crop failed\n");
		return -1;
	}

	return ret;
}
#endif

#ifdef HW_G2D_USE
static inline int colorFormatCopybit2G2D(int format)
{
	switch (format)
	{
		case COPYBIT_FORMAT_RGBA_8888 :  return G2D_RGBA32;
		case COPYBIT_FORMAT_RGB_565   :  return G2D_RGB16;
		case COPYBIT_FORMAT_BGRA_8888 :  return G2D_ARGB32;
		case COPYBIT_FORMAT_RGBA_5551 :  return G2D_RGBA16;
		case COPYBIT_FORMAT_RGBA_4444 :  return G2D_RGBA16;
		default :
			LOGE("%s::not matched frame format : %d\n", __func__, format);
			break;
    }
    return -1;
}
#endif

static inline int rotateValueCopybit2G2D(unsigned char flags)
{
	int rotate_value  = S3C_G2D_ROTATOR_0;
	
	if      (flags & COPYBIT_TRANSFORM_ROT_90)
		rotate_value = S3C_G2D_ROTATOR_90;

	else if (flags & COPYBIT_TRANSFORM_ROT_180)
		rotate_value = S3C_G2D_ROTATOR_180;

	else if (flags & COPYBIT_TRANSFORM_ROT_270)
		rotate_value = S3C_G2D_ROTATOR_270;

	else if (flags & COPYBIT_TRANSFORM_FLIP_V)
		rotate_value = S3C_G2D_ROTATOR_X_FLIP;

	else if (flags & COPYBIT_TRANSFORM_FLIP_H)
		rotate_value = S3C_G2D_ROTATOR_Y_FLIP;	

	else
		rotate_value = S3C_G2D_ROTATOR_0;

	return rotate_value;
}


static inline unsigned int getFrameSize(int colorformat, int width, int height)
{
	unsigned int frame_size = 0;
	unsigned int size = width * height;

	switch(colorformat)
	{
		case COPYBIT_FORMAT_RGBA_8888: 
		case COPYBIT_FORMAT_BGRA_8888: 		
			frame_size = width * height * 4;
			break;

		case COPYBIT_FORMAT_YCbCr_420_SP: 
		case COPYBIT_FORMAT_CUSTOM_YCbCr_420_SP: 
		case COPYBIT_FORMAT_YCbCr_420_P: 
			//frame_size = width * height * 3 / 2;
			frame_size = size + (2 * ( size / 4));
			break;

		case COPYBIT_FORMAT_RGB_565: 
		case COPYBIT_FORMAT_RGBA_5551: 
		case COPYBIT_FORMAT_RGBA_4444: 
		case COPYBIT_FORMAT_YCbCr_422_I :
		case COPYBIT_FORMAT_YCbCr_422_SP :
		case COPYBIT_FORMAT_CUSTOM_YCbCr_422_I :
		case COPYBIT_FORMAT_CUSTOM_CbYCr_422_I : //Kamat
			frame_size = width * height * 2;
			break;

		default :  // hell.~~
			LOGE("%s::no matching source colorformat(%d), width(%d), height(%d) \n",
			     __func__, colorformat, width, height);
			frame_size = 0;
			break;
	}
	return frame_size;
}

#if	defined(HW_FIMC_USE)
static inline int rotateValueCopybit2PP(unsigned char flags)
{
	int rotate_value  = 0;
	
	if (flags & COPYBIT_TRANSFORM_ROT_90)
		rotate_value = 90;

	else if (flags & COPYBIT_TRANSFORM_ROT_180)
		rotate_value = 180;

	else if (flags & COPYBIT_TRANSFORM_ROT_270)
		rotate_value = 270;

#if 0
	else if (flags & COPYBIT_TRANSFORM_FLIP_V)
		rotate_value = V4L2_CID_ROTATE_90_VFLIP;

	else if (flags & COPYBIT_TRANSFORM_FLIP_H)
		rotate_value = V4L2_CID_ROTATE_90_HFLIP;	
#endif
	else
		rotate_value = 0;

	return rotate_value;
}

static inline int checkPPOutPath(struct copybit_context_t *ctx, s3c_img *dst_img, s3c_rect *dst_rect)
{
	int	color_space, width;

	// need alpha blending ? 
	if (ctx->mAlpha < 255) return 1;

#ifdef HW_G2D_USE
	// need rotation 90 or 270 ?
	if (ctx->mFlags & COPYBIT_TRANSFORM_ROT_90) return 1;
	if (ctx->mFlags & COPYBIT_TRANSFORM_ROT_270) return 1;

	// when the destination width's constraints is violated ?
	color_space = colorFormatCopybit2PP(dst_img->format);
	width		= widthOfPP(color_space, dst_rect->w);
	if (dst_rect->w != width) return 1;
#endif

	return 0;
}

static inline int heightOfPP(int pp_color_format, int number)
{
	switch(pp_color_format)
	{
		case V4L2_PIX_FMT_RGB565:
		case V4L2_PIX_FMT_YUYV:
			return number;
			break;

		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV12T:
		case V4L2_PIX_FMT_YUV420:
			return multipleOf2(number);
			break;

		default :
			return number;
			break;
	}
	return number;
}

static inline int widthOfPP(int pp_color_format, int number)
{
	int tmp;

	switch(pp_color_format)
	{
		case V4L2_PIX_FMT_RGB565:
		//	return multipleOf4(number);
			return multipleOf8(number);
		//	return number;
			break;

		case V4L2_PIX_FMT_YUYV:
			return multipleOf2(number);
			break;

		case V4L2_PIX_FMT_NV12T:
		case V4L2_PIX_FMT_NV21:
//		case V4L2_PIX_FMT_YUV420:
			return multipleOf8(number);
			break;
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_YUV420:
			tmp = (number - (number & 0xF));
			if(tmp < 0)
				tmp = 0;
			return tmp;

		default :
			return number;
			break;
	}
	return number;
}

static inline int multipleOf2(int number)
{
	if(number % 2 == 1)
		return (number - 1);
	else
		return number;
}

static inline int multipleOf4(int number)
{
	int remain_number = number % 4;

	if(remain_number != 0)
		return (number - remain_number);
	else
		return number;
}

static inline int multipleOf8(int number)
{
	int remain_number = number % 8;

	if(remain_number != 0)
		return (number - remain_number);
	else
		return number;
}
#endif

int checkMem(struct copybit_context_t *ctx, unsigned int requested_size)
{
	s3c_mem_t * s3c_mem  = &ctx->s3c_mem[0];

	if (0 == s3c_mem->virt_addr)
	{
		// memory is not allocated. need to allocate
	
		unsigned int memory_size = ctx->s3c_fb.width * ctx->s3c_fb.height * 2;

		if (memory_size < requested_size)
			memory_size = requested_size;

		if(createMem(ctx, memory_size) < 0)
		{
			LOGE("%s::createMem fail (size=%d)\n", __func__, memory_size);
			return -1;
		} 
	}
	else if ((unsigned int)s3c_mem->mem_alloc_info.size < requested_size)
	{
		// allocated mem is smaller than the requested. 
		// need to free previously-allocated mem and reallocate

		if(destroyMem(ctx) < 0)
		{
			LOGE("%s::destroyMem fail\n", __func__);
			return -1;
		}

		if(createMem(ctx, requested_size) < 0)
		{
			LOGE("%s::createMem fail (size=%d)\n", __func__, requested_size);
			return -1;
		} 
	}

	return 0;
}
#endif

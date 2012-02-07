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
 * @author Sangwoo, Park(sw5771.park@samsung.com)
 * @date   2009-11-28
 */

#ifndef S3C_2D_RENDER_H_
#define S3C_2D_RENDER_H_

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

#include "s5p_fimc.h"
#include "s3c_g2d.h"
#include "s3c_lcd.h"
#include "videodev2.h"

#ifdef DITHERING_ON_CAMERA_PREVIEW_FRAME
	#include "s3c-cmm.h"
	#include "simgp.h" // dithering api
#endif

//------------------------------------------------//
// define                                         //
//------------------------------------------------//
#define NUM_OF_S3C2D_OBJECT (1)

#define PP_RGB_ALIGNED_PHYS_ADDR  (0x4)
#define PP_YUV_ALIGNED_PHYS_ADDR  (0x8)
#define PP_YUYV_ALIGNED_PHYS_ADDR (0x4)
#define PP_WIDTH_STEP_VALUE       (0xF)  //(0x4)
#define PP_HEIGHT_STEP_VALUE     (0xF)  //(0x2)

#define PMEM_PREVIEW_DEV_NAME             ("/dev/pmem_preview")
#define PMEM_PICTURE_DEV_NAME             ("/dev/pmem_picture")
#define PMEM_DEV_NAME                	  ("/dev/pmem")

#define PMEM_STREAM_DEV_NAME              ("/dev/pmem_stream2")
#define PMEM_RENDER_DEV_NAME              ("/dev/pmem_render")
#define PMEM_RENDER_PARTITION_NUM         (2)
#define PMEM_RENDER_PARTITION_NUM_SHIFT   (1)

#ifdef DITHERING_ON_CAMERA_PREVIEW_FRAME
	#define DITHERING_ON_CAMERA_PICTURE_FRAME
	#define NUM_OF_MEMORY_OBJECT (2)
	#define FORCE_SW_MEM
#endif


#define FIMC1_SCL_MAX_IN_HSIZE		854

#define PMEM_STREAM

//------------------------------------------------//
// struct                                         //
//------------------------------------------------//
typedef struct _s3c_rect {
	uint32_t x;
	uint32_t y;
	uint32_t w;	
	uint32_t h;
}s3c_rect;

typedef struct _s3c_img {
	uint32_t         width;
	uint32_t         height;
	uint32_t         format;
	unsigned char *  base;
	uint32_t         offset;
	int              memory_id;
	int              flag;
}s3c_img;

//------------------------------------------------//
// class S3c2DRender                              //
//------------------------------------------------//
class S3c2DRender
{
public :

	enum SEC2D_FLAGS
	{
		SEC2D_FLAGS_FRAMEBUFFER = 0x00000001,
		SEC2D_FLAGS_USES_PMEM = 0x00000002,
		SEC2D_FLAGS_VIDEO_PLAY = 0x00000004,
		SEC2D_FLAGS_CAMERA_PREVIEW = 0x00000008,
		SEC2D_FLAGS_CAMERA_PICTURE = 0x00000010
	};
	s3c_rect     m_src_org_rect;
    s3c_rect     m_dst_org_rect;
protected :

#ifdef DITHERING_ON_CAMERA_PREVIEW_FRAME
	enum MEMORY_TYPE
	{
		MEMORY_TYPE_BASE = 0,
		MEMORY_TYPE_HW_MEM,
		MEMORY_TYPE_SW_MEM,
		MEMORY_TYPE_MAX,
	};
#endif

	
	struct s3c_pp_t
	{
		int                dev_fd;	
		unsigned char *    virt_addr;
		unsigned int       phys_addr;
		int                buf_size;
	};

	struct s3c_g2d_t
	{
		int                dev_fd;
	};
	
#ifdef DITHERING_ON_CAMERA_PREVIEW_FRAME
	struct mem_t
	{
		int             dev_fd;
		int             type;
		unsigned int    size;
		unsigned int    phys_addr;
		unsigned char * virt_addr;
		
		CODEC_MEM_ALLOC_ARG mem_info;
	};
#endif

	
	/***************************************
	 * static member variables             *
	 ***************************************/
protected :
	static          bool   m_isInit;
	static S3c2DRender*    m_2DInstance[NUM_OF_S3C2D_OBJECT];
	static          int    m_current_instance;
	static          int    m_num_of_instance;

	static		bool   m_isPmemInit;
	static unsigned int    m_pmem_preview_phys_addr;
	static unsigned int    m_pmem_picture_phys_addr;
	static unsigned int    m_pmem_phys_addr;

#ifdef PMEM_STREAM
	static          int    m_pmem_stream_fd;
	static unsigned int    m_pmem_stream_size;
	static unsigned int    m_pmem_stream_phys_addr;
	static unsigned char * m_pmem_stream_virt_addr;
#endif

	static          int    m_pmem_render_fd;
	static unsigned int    m_pmem_render_size;
	static unsigned int    m_pmem_render_partition_size;
	static unsigned int    m_pmem_render_phys_addrs[PMEM_RENDER_PARTITION_NUM];
	static unsigned char * m_pmem_render_virt_addrs[PMEM_RENDER_PARTITION_NUM];
	static unsigned int    m_current_pmem;

	static unsigned int    m_fb_start_phy_addr;

#ifdef DITHERING_ON_CAMERA_PREVIEW_FRAME
	static mem_t           m_dither_mem[NUM_OF_MEMORY_OBJECT];
#endif
	/****************************************/

	s3c_pp_t     m_pp;
	s3c_g2d_t    m_g2d;
	uint8_t      m_alpha;
	uint8_t      m_flags;
	bool         m_isDeviceOpen;

	/***************************************
	 * static member functions             *
	 ***************************************/
public:
	static void Init2DRender(void);
	static void InitializePmem(void);
	static void DeinitalizePmem(void);
	static S3c2DRender* Get2DRender(void);
	static void Delete2DRender(S3c2DRender* render);
	/***************************************/
	

	/***************************************
	 * class member functions              *
	 ***************************************/
public:
	int Render(s3c_img *src_img, s3c_rect *src_rect,
	           s3c_img *dst_img, s3c_rect *dst_rect , int flagPP);
	int SetFlags(unsigned char alpha, unsigned char flags);
protected:
	S3c2DRender();
	~S3c2DRender();
private:
	int OpenDevice(void);
	int CloseDevice(void);
	bool IsDeviceOpen(void) { return m_isDeviceOpen; }

	// for Rendering
	int JustdoPP(s3c_img *src_img, s3c_rect *src_rect,
	             s3c_img *dst_img, s3c_rect *dst_rect);
	int DoCameraPicture(s3c_img *src_img, s3c_rect *src_rect,
	                    s3c_img *dst_img, s3c_rect *dst_rect,
	                    unsigned int pp_src_phys_addr);
	int DoPPDoG2D(s3c_img *src_img, s3c_rect *src_rect,
	               s3c_img *dst_img, s3c_rect *dst_rect);
	int JustdoG2D(s3c_img *src_img, s3c_rect *src_rect,
	              s3c_img *dst_img, s3c_rect *dst_rect);
				  
	int OpenPP (void);
	int ClosePP(void);
	int DoPP     (unsigned int src_phys_addr, s3c_img *src_img, s3c_rect *src_rect,
	              unsigned int dst_phys_addr, s3c_img *dst_img, s3c_rect *dst_rect);
	inline int ControlPP(s3c_pp_params_t * params);
	int DoPPWorkAround(unsigned int src_phys_addr, unsigned char * src_virt_addr,
	                   s3c_img *src_img, s3c_rect *src_rect, 
	                   s3c_img *dst_img, s3c_rect *dst_rect );
	unsigned int DoFIMC1WorkAround(unsigned char *src_buf , s3c_img *src_img, s3c_rect *src_rect, unsigned int src_physAddr);
	unsigned int DoImageSclDownYUV422I(unsigned char *src_buf , s3c_img *src_img, s3c_rect *src_rect, int scl_down_fact);
	
	int CopyPPSrcframe(unsigned char * src_virt_addr, s3c_pp_params_t * src_params, 
	                   unsigned char * dst_virt_addr, s3c_pp_params_t * dst_params,
	                   unsigned int pad_width, unsigned int pad_height);
	int CopyPPDstframe(unsigned char * src_virt_addr, s3c_pp_params_t * src_params, 
	                   unsigned char * dst_virt_addr, s3c_pp_params_t * dst_params,
	                   int pad_width, int pad_height);
	int OpenG2D (void);
	int CloseG2D(void);
	inline int DoG2D(unsigned int src_phys_addr, s3c_img *src_img, s3c_rect *src_rect, int src_flag_android_order,
	                 unsigned int dst_phys_addr, s3c_img *dst_img, s3c_rect *dst_rect, int dst_flag_android_order,
	                 int rotate_value, int alpha_value);
	int CopyG2DFrame(unsigned char * src_virt_addr,
	                 unsigned char * dst_virt_addr,
	                 unsigned int full_width, unsigned int full_height,
	                 unsigned int start_x,    unsigned int start_y,
	                 unsigned int width,      unsigned int height,
	                 int rotate_value,        int g2d_color_space);
#if 1//G2D Fix
	int CopyG2DFrame_2dworkaround(unsigned char * src_virt_addr,
                              unsigned char * dst_virt_addr,
                              unsigned int full_width, unsigned int full_height,
                              unsigned int src_start_x,    unsigned int src_start_y,
                              unsigned int start_x,    unsigned int start_y,
                              unsigned int width,      unsigned int height,
                              int rotate_value,        int g2d_color_space);
	inline int 	CopyRgb_2dworkaround(s3c_img *img, s3c_rect *src_rect, s3c_rect *rect,
                                unsigned char * src_virt_addr,
                                unsigned char * dst_virt_addr,
                                int flag_android_order);
#endif
	inline int CopyRgb(s3c_img *img, s3c_rect *rect,
	                   unsigned char * src_virt_addr,
	                   unsigned char * dst_virt_addr,
					   int flag_android_order);
	inline void MakeMidImgRect(s3c_img * dst_img, s3c_rect * dst_rect, 
	                               s3c_img * mid_img, s3c_rect * mid_rect,
	                               int rotate_value);
	inline void SetS3cPPParams(s3c_pp_params_t *params, 
	         s3c_img *src_img, s3c_rect *src_rect, int src_color_space, unsigned int src_phys_addr,
			 s3c_img *dst_img, s3c_rect *dst_rect, int dst_color_space, unsigned int dst_phys_addr,
			 int out_path, int workaround);
	inline void         SetS3cPPParamsCbCr(s3c_pp_params_t *params);
	inline int          ColorFormatCopybit2PP (int format);
	inline int          ColorFormatCopybit2G2D(int format, int flag_android_order);
	inline int          RotateValueCopybit2G2D(unsigned char flags);
	inline char *       GetFormatString       (int colorformat);
	inline unsigned int GetFrameSize          (int colorformat, unsigned int width, unsigned int height);
	inline unsigned int GetAlignedStartPosOfPP(unsigned int   phy_start_addr, int pp_color_format,
	                                           unsigned int   start_x,      unsigned int   start_y,
	                                           unsigned int   full_width,   unsigned int   full_height,
	                                           unsigned int * aligned_factor);
	inline unsigned int GetFactorNumber(unsigned int num, unsigned int limit_num);
	inline unsigned int GetWidthOfPP (int pp_color_format, unsigned int number);
	inline unsigned int GetHeightOfPP(int pp_color_format, unsigned int number);
	inline void PPSrcDstWidthHeightCheck(s3c_img *src_img, s3c_rect *src_rect, s3c_img *dst_img, s3c_rect *dst_rect, int src_color_format, int dst_color_format);
	
	inline unsigned int WidthOfPP    (int pp_color_format, unsigned int number);
	inline unsigned int HeightOfPP   (int pp_color_format, unsigned int number);
#ifdef DITHERING_ON_CAMERA_PREVIEW_FRAME
	int ditheringYuv2Rgb(unsigned char * src_data,
	                     unsigned char * dst_data, 
	                     unsigned int width, unsigned int height,
	                     int color_format);
	int createMem (mem_t * mem, unsigned int mem_size, int mem_type);
	static int destroyMem(mem_t * mem);
	int flushMem  (mem_t * mem);

#endif

	#if 0
	inline unsigned int convertABGR2RGBA(int copybit_color_space, 
												unsigned char * src_addr, unsigned char *dst_addr,
												unsigned int width,       unsigned int height);
	#endif

	//-----------------------------------------------//
	// for Pmem
	static void OpenPmem(const char*     dev_name, 
                         int*            fd, 
                         unsigned char** virt_addr, 
                         unsigned int*   phys_addr, 
                         unsigned int*   size);
	static void ClosePmem(int fd, unsigned char* virt_addr, int size);

	inline void            ResetPmemPartition(void);
	inline void            GoNextPmemPartition(void);
	inline void            GoPrevPmemPartition(void);
	inline int             GetPmemPhysAddr    (void);
	inline unsigned char * GetPmemVirtAddr    (void);
	inline int             CheckFrameSize      (unsigned int frame_size, const char *func_name);
};

#endif /* S3C_2D_RENDER_H_ */

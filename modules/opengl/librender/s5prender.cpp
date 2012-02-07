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
** @initial code Sangwoo, Park(sw5771.park@samsung.com)
** @date   2009-11-28
**@s5p6442 specific changes: rama(v.meka@samaung.com)
** @date   2009-12-23
*/

#define LOG_TAG "libs5prender"

#include "s5prender.h"
#ifdef  HW_HDMI_USE
#include "hdmi_lib.h"
#endif
//#define RENDER_DEBUG

#ifdef RENDER_DEBUG
	#define RENDER_DEBUG_FIMC_2D
	//#define RENDER_DEBUG_2D
	#define RENDER_DEBUG_FIMC
#endif

#ifdef ZERO_COPY
	#define ZERO_COPY_FLAG  0x12345678
#endif
//TV params
#define HDMI_GRAPHIC_LAYER_FOR_UI       0
/*************************************************
 * static member variables
 *************************************************/
bool           S3c2DRender::m_isInit = false;
S3c2DRender*   S3c2DRender::m_2DInstance[NUM_OF_S3C2D_OBJECT];
int            S3c2DRender::m_num_of_instance = 0;
int            S3c2DRender::m_current_instance = 0;

bool           S3c2DRender::m_isPmemInit = false;
unsigned int   S3c2DRender::m_pmem_preview_phys_addr;
unsigned int   S3c2DRender::m_pmem_picture_phys_addr;
unsigned int   S3c2DRender::m_pmem_phys_addr;
#ifdef PMEM_STREAM
int            S3c2DRender::m_pmem_stream_fd;
unsigned int   S3c2DRender::m_pmem_stream_size;
unsigned int   S3c2DRender::m_pmem_stream_phys_addr;
unsigned char* S3c2DRender::m_pmem_stream_virt_addr;
#endif

int            S3c2DRender::m_pmem_render_fd;
unsigned int   S3c2DRender::m_pmem_render_size;
unsigned int   S3c2DRender::m_pmem_render_partition_size;
unsigned int   S3c2DRender::m_pmem_render_phys_addrs[PMEM_RENDER_PARTITION_NUM];
unsigned char* S3c2DRender::m_pmem_render_virt_addrs[PMEM_RENDER_PARTITION_NUM];
unsigned int   S3c2DRender::m_current_pmem;

unsigned int   S3c2DRender::m_fb_start_phy_addr = 0;

#ifdef DITHERING_ON_CAMERA_PREVIEW_FRAME
S3c2DRender::mem_t          S3c2DRender::m_dither_mem[NUM_OF_MEMORY_OBJECT];
#endif
/*************************************************/


/*************************************************
 * static member functions
 *************************************************/
void S3c2DRender::Init2DRender(void)
{
	int i = 0;
	
	if(m_isInit)
		return;

	/*
	 * create instances of 2D Render
	 */
	for(i = 0; i < NUM_OF_S3C2D_OBJECT; i++)
	{
		if(m_2DInstance[i] == NULL)
		{
			m_2DInstance[i] = new S3c2DRender;

			if(m_2DInstance[i]->IsDeviceOpen())
			{
				m_num_of_instance++;
				LOGV("created instance is %d", m_num_of_instance);
			}
			else
			{
				LOGE("Get2DRender(%d)::IsDeviceOpen() fail", i);
				delete m_2DInstance[i];
				m_2DInstance[i] = NULL;
			}
		}
	}

	/*
	 * Get virtual and physical address of Pmem for 2D rendering
	 */
	if(!m_isPmemInit)
		InitializePmem();

	if(m_num_of_instance == NUM_OF_S3C2D_OBJECT)
		m_isInit = true;
}

void S3c2DRender::InitializePmem(void)
{
	int            pmem_fd;
	unsigned int   		pmem_size 			= 0;
	unsigned char* 	pmem_virt_addr		= NULL;
#if 0
	if(m_isInit)
	{
		LOGV("S3C2DRender is already initialized");
		return;
	}

	m_num_of_instance = 0;
#endif

#ifdef DITHERING_ON_CAMERA_PREVIEW_FRAME
	{
		int i = 0;
		for(i = 0; i < NUM_OF_MEMORY_OBJECT; i++)
			memset(&m_dither_mem[i], 0, sizeof(mem_t));
	}
#endif

	OpenPmem(PMEM_RENDER_DEV_NAME,
			 &m_pmem_render_fd,
			 &m_pmem_render_virt_addrs[0],
			 &m_pmem_render_phys_addrs[0],
			 &m_pmem_render_size);
	if(m_pmem_render_virt_addrs[0] == MAP_FAILED)
	{
		LOGE("OpenPmem(%s) fail", PMEM_RENDER_DEV_NAME);
		return;
	}

	m_pmem_render_partition_size = m_pmem_render_size / PMEM_RENDER_PARTITION_NUM;
	for (int i = 1; i < PMEM_RENDER_PARTITION_NUM; i++)
	{
		m_pmem_render_virt_addrs[i] = m_pmem_render_virt_addrs[i-1] + m_pmem_render_partition_size;
		m_pmem_render_phys_addrs[i] = m_pmem_render_phys_addrs[i-1] + m_pmem_render_partition_size;
	}

#ifdef PMEM_STREAM
	OpenPmem(PMEM_STREAM_DEV_NAME,
			 &m_pmem_stream_fd,
			 &m_pmem_stream_virt_addr,
			 &m_pmem_stream_phys_addr,
			 &m_pmem_stream_size);
	if(m_pmem_stream_virt_addr == MAP_FAILED)
	{
		LOGE("OpenPmem(%s) fail", PMEM_STREAM_DEV_NAME);
		return;
	}
#endif
	OpenPmem(PMEM_PICTURE_DEV_NAME,
			 &pmem_fd,
			 &pmem_virt_addr,
			 &m_pmem_picture_phys_addr,
			 &pmem_size);
	ClosePmem(pmem_fd, pmem_virt_addr, pmem_size);

	OpenPmem(PMEM_PREVIEW_DEV_NAME,
			 &pmem_fd,
			 &pmem_virt_addr,
			 &m_pmem_preview_phys_addr,
			 &pmem_size);
	ClosePmem(pmem_fd, pmem_virt_addr, pmem_size);
	OpenPmem(PMEM_DEV_NAME,
			 &pmem_fd,
			 &pmem_virt_addr,
			 &m_pmem_phys_addr,
			 &pmem_size);
	ClosePmem(pmem_fd, pmem_virt_addr, pmem_size);
#if 0
	m_isInit = true;
#else
	m_isPmemInit = true;
#endif
}

void S3c2DRender::DeinitalizePmem(void)
{
#ifdef DITHERING_ON_CAMERA_PREVIEW_FRAME
	int i = 0, ret;
	for(i = 0; i < NUM_OF_MEMORY_OBJECT; i++)
	{
		if(destroyMem(&m_dither_mem[i]) < 0)
		{
			LOGE("%s::destroyMem fail\n", __func__);
			ret = -1;
		}
	}
#endif

#ifdef PMEM_STREAM
	ClosePmem(m_pmem_stream_fd, m_pmem_stream_virt_addr, m_pmem_stream_size);
#endif

	ClosePmem(m_pmem_render_fd, m_pmem_render_virt_addrs[0], m_pmem_render_size);
#if 0
	m_num_of_instance = 0;
	m_isInit = false;
#endif
}

void S3c2DRender::OpenPmem(const char*     dev_name, 
                           int*            fd, 
                           unsigned char** virt_addr, 
                           unsigned int*   phys_addr, 
                           unsigned int*   size)
{
	*fd = open (dev_name, O_RDWR);

	if (*fd > 0)
	{
		struct pmem_region region;

		if (ioctl(*fd, PMEM_GET_TOTAL_SIZE, &region)<0)
		{
			LOGE("Something wrong on %s\n", dev_name);
			close (*fd);
			return ;
		}

		*size = region.len;

		*virt_addr = (unsigned char*)mmap(0, region.len,
					PROT_READ|PROT_WRITE, MAP_SHARED, *fd, 0);

		if (*virt_addr == MAP_FAILED)
		{
			LOGW("It's already mmaped: %s\n", dev_name);
		}
					
		if (ioctl(*fd, PMEM_GET_PHYS, &region)<0)
		{
			LOGE("Something wrong get physical address on %s\n", dev_name);
			close (*fd);
			return ;
		}
		*phys_addr = region.offset;
		LOGV("OpenPmem:: devname=%s, fd=%d, physaddr=0x%x", dev_name, *fd, *phys_addr);
	}
	else
	{
		*fd = 0;
		LOGW("%s is not opened", dev_name);
	}
}

void S3c2DRender::ClosePmem(int fd, unsigned char* virt_addr, int size)
{
	if (virt_addr != MAP_FAILED)
		munmap(virt_addr, size);

	if(fd > 0)
		close (fd);
}

S3c2DRender* S3c2DRender::Get2DRender(void)
{
	//if(!m_isInit)
	//	S3c2DRender::Init2DRender();

	m_current_instance++;

	if(m_current_instance == NUM_OF_S3C2D_OBJECT)
		m_current_instance = 0;

	//LOGV("m_2DInstance[%d] is used", m_current_instance);

	return m_2DInstance[m_current_instance];
}

void S3c2DRender::Delete2DRender(S3c2DRender* render)
{
	if(render != NULL)
	{
	delete render;
	--m_num_of_instance;
	}

	LOGV("remained instance is %d", m_num_of_instance);
}
/*************************************************/


/*************************************************
 * class member functions
 *************************************************/
S3c2DRender::S3c2DRender()
             : m_alpha               (0xFF),
			   m_flags               (0),
			   m_isDeviceOpen        (false)
{
	memset(&m_pp,  0, sizeof(s3c_pp_t));
	memset(&m_g2d, 0, sizeof(s3c_g2d_t));

	if(OpenDevice() < 0)
	{
		LOGE("%s::OpenDevice() fail \n", __func__);
	}
	else
		m_isDeviceOpen = true;
}

S3c2DRender::~S3c2DRender()
{
	if(CloseDevice() < 0)
	{
		LOGE("%s::CloseDevice() failed", __func__);
	}
}

int S3c2DRender::Render(s3c_img *src_img, s3c_rect *src_rect,
                        s3c_img *dst_img, s3c_rect *dst_rect , int flagPP)
{
	ResetPmemPartition();	// yj: Is this call usefull?

	if(    src_img->format == COPYBIT_FORMAT_YCbCr_420_P
	    || src_img->format == COPYBIT_FORMAT_YCbCr_422_P
		|| src_img->format == COPYBIT_FORMAT_YCbCr_420_SP
	    || src_img->format == COPYBIT_FORMAT_YCbCr_422_SP
	    || src_img->format == COPYBIT_FORMAT_YCbCr_422_I)
	{
		if(   dst_img->format == COPYBIT_FORMAT_YCbCr_420_P
		   || dst_img->format == COPYBIT_FORMAT_YCbCr_422_P
		   || dst_img->format == COPYBIT_FORMAT_YCbCr_420_SP
		   || dst_img->format == COPYBIT_FORMAT_YCbCr_422_SP
		   || dst_img->format == COPYBIT_FORMAT_YCbCr_422_I)
		{
			return JustdoPP(src_img, src_rect, dst_img, dst_rect);
		}
		else
		{
			if(flagPP  == 1)
			{ //PP+G3D path
				return JustdoPP(src_img, src_rect, dst_img, dst_rect);
			}
			else 
			{
#if 0
				if(!((src_rect->x == 0)&&(src_rect->y==0)))
				{//Use (PP+G3D) here as only part of the screen needs to be redrawn
					 return -1;
				}
				else
#endif					
				{
					//Use PP+G2D 
					return  DoPPDoG2D(src_img, src_rect, dst_img, dst_rect);
				}
			}
		}
	}
	else
	{
		return JustdoG2D(src_img, src_rect, dst_img, dst_rect);
	}

	return 0;
}

int S3c2DRender::SetFlags(unsigned char alpha, unsigned char flags)
{
	m_alpha = alpha;
	m_flags = flags;

	return 0;
}

int S3c2DRender::OpenDevice(void)
{	
	if(OpenPP() < 0)
		return -1;
		
	if(OpenG2D() < 0)
		return -1;

	return 0;
}

int S3c2DRender::CloseDevice(void)
{
	CloseG2D();
	ClosePP();

	return 0;
}


int S3c2DRender::JustdoPP(s3c_img *src_img, s3c_rect *src_rect,
                    s3c_img *dst_img, s3c_rect *dst_rect)
{
	unsigned int    pp_src_phys_addr = 0;
	unsigned char * pp_src_virt_addr = NULL;
	unsigned int    pp_dst_phys_addr = 0;
	unsigned char * pp_dst_virt_addr = NULL;
	unsigned char * dst_virt_addr    = NULL;

	unsigned int src_frame_size = GetFrameSize(src_img->format, src_img->width, src_img->height);
#if 1
	if((src_rect->w < 1) || (src_rect->h < 1))
	{
		return 0;//SUCCESS
	}
#endif
	if(CheckFrameSize(src_frame_size, __func__) < 0 )
		return -1;

	pp_src_phys_addr = GetPmemPhysAddr();
	pp_src_virt_addr = GetPmemVirtAddr();
	GoNextPmemPartition();

	memcpy(pp_src_virt_addr, src_img->base, src_frame_size);

	if(DoPPWorkAround(pp_src_phys_addr, pp_src_virt_addr, src_img, src_rect,
	                                                      dst_img, dst_rect) < 0) 
		return -1;

	pp_dst_phys_addr = GetPmemPhysAddr();
	pp_dst_virt_addr = GetPmemVirtAddr();

	dst_virt_addr = dst_img->base;

	s3c_pp_params_t   params;
	int src_color_space = ColorFormatCopybit2PP(src_img->format);
	int dst_color_space = ColorFormatCopybit2PP(dst_img->format);

	SetS3cPPParams(&params, src_img, src_rect, src_color_space, 0, 
	                        dst_img, dst_rect, dst_color_space, 0, 
	                        0 , 1);

	return CopyPPDstframe(pp_dst_virt_addr, &params,
	                      dst_virt_addr,    &params,
	                      0, 0);
}

int S3c2DRender::DoCameraPicture(s3c_img *src_img, s3c_rect *src_rect,
                                 s3c_img *dst_img, s3c_rect *dst_rect,
                                 unsigned int pp_src_phys_addr)
{
	struct pmem_region region;
	s3c_fb_next_info_t fb_info;
	unsigned int g2d_src_phys_addr = 0;
	unsigned int g2d_dst_phys_addr = 0;
	s3c_img      mid_img;
	s3c_rect     mid_rect;
	unsigned int mid_frame_size = 0;
	int          ret = 0;

	int          dst_flag_android_order = 0;
	
#ifdef RENDER_DEBUG_FIMC_2D
	LOGD("##############DoCameraPicture-1:: src_width %d  src_height %d src_fmt %d  dst_width %d  dst_height %d dst_fmt %d\n", 
					src_img->width, src_img->height, src_img->format, dst_img->width, dst_img->height, dst_img->format);
#endif

	switch(dst_img->flag)
	{
		case SEC2D_FLAGS_FRAMEBUFFER:
			if(m_fb_start_phy_addr)
				g2d_dst_phys_addr = m_fb_start_phy_addr + dst_img->offset;
			else
			{
				if(ioctl(dst_img->memory_id, S3C_FB_GET_CURR_FB_INFO, &fb_info) >= 0)
				{
					m_fb_start_phy_addr = fb_info.phy_start_addr;
					g2d_dst_phys_addr = m_fb_start_phy_addr + dst_img->offset;
				}
				else
				{
					LOGE("%s::ioctl(S3C_FB_GET_CURR_FB_INFO) error\n", __func__);
					return -1;
				}
			}

			dst_flag_android_order = 0;
			break;

		case SEC2D_FLAGS_USES_PMEM:
			g2d_dst_phys_addr = m_pmem_phys_addr + dst_img->offset;
			dst_flag_android_order = 1;
			break;

		default:
			LOGE("%s::destnation buffer should be fb or pmem\n", __func__);
			dst_flag_android_order = 1;
			return -1;
			break;
	}

	g2d_src_phys_addr = GetPmemPhysAddr();

	int rotate_value = RotateValueCopybit2G2D(m_flags);

	// set dst data
	MakeMidImgRect(dst_img, dst_rect, &mid_img, &mid_rect, rotate_value);

#ifdef RENDER_DEBUG_FIMC_2D
	LOGD("##############DoCameraPicture-2 \n");
#endif
	
#ifdef S5P6442_EVT0
	if(src_rect->x & 0x01)
	{
		src_rect->x--;
		src_rect->w++;
	}
#endif

#ifdef DITHERING_ON_CAMERA_PICTURE_FRAME

	int          flag_dithering_yuv2rgb = 0;
	unsigned int src_frame_size         = 0;
	
	if(mid_img.format == COPYBIT_FORMAT_RGB_565)
	{
		flag_dithering_yuv2rgb = 1;

		mid_frame_size = GetFrameSize(mid_img.format, mid_img.width, mid_img.height);

		if(createMem(&m_dither_mem[1], mid_frame_size, MEMORY_TYPE_SW_MEM) < 0)		
		{
			LOGE("%s::createMem(%d) on 1 fail\n", __func__, mid_frame_size);
			flag_dithering_yuv2rgb = 0;
		}
		else
		{
			// yuyv -> yuv420p
			mid_img.format = COPYBIT_FORMAT_YCbCr_420_P;
			
			src_frame_size = GetFrameSize(mid_img.format, mid_img.width, mid_img.height);

			if(createMem(&m_dither_mem[0], src_frame_size, MEMORY_TYPE_SW_MEM) < 0)
			{
				LOGE("%s::createMem(%d) on 0 fail\n", __func__, src_frame_size);
				flag_dithering_yuv2rgb = 0;
			}
			else
			{
				unsigned char * pp_src_virt_addr  = src_img->base;

				if(DoPPWorkAround(pp_src_phys_addr, pp_src_virt_addr,  src_img,  src_rect,
				                                                      &mid_img, &mid_rect) < 0) 
				{
					LOGE("DoPPWorkAround fail");
					flag_dithering_yuv2rgb = 0;
				}
				else
				{
					unsigned char * pp_dst_virt_addr = GetPmemVirtAddr();
					memcpy(m_dither_mem[0].virt_addr, pp_dst_virt_addr, src_frame_size);
				}
			}
		}

		if(flag_dithering_yuv2rgb == 1)
		{
			// only color conversion
			if(ditheringYuv2Rgb(m_dither_mem[0].virt_addr,
								m_dither_mem[1].virt_addr,
								mid_img.width, mid_img.height, mid_img.format) < 0)
			{
				LOGE("%s::ditheringYuv2Rgb fail\n", __func__);
				flag_dithering_yuv2rgb = 0;
			}
			else
			{
				if(m_dither_mem[1].type == MEMORY_TYPE_HW_MEM)
				{
					if(flushMem(&m_dither_mem[1]) < 0)
					{
						LOGE("%s::flushMem fail\n", __func__);
						flag_dithering_yuv2rgb = 0;
					}
					else
						g2d_src_phys_addr = m_dither_mem[1].phys_addr;
				}
				else
				{
					if(mid_frame_size == 0)
						mid_frame_size = GetFrameSize(dst_img->format, mid_img.width, mid_img.height);

					unsigned char * g2d_src_virt_addr = GetPmemVirtAddr();
					                g2d_src_phys_addr = GetPmemPhysAddr();

					memcpy(g2d_src_virt_addr, m_dither_mem[1].virt_addr, mid_frame_size);					
				}
			}
		}

		mid_img.format = dst_img->format;
	}

	if(flag_dithering_yuv2rgb == 0)
#endif // DITHERING_ON_CAMERA_PICTURE_FRAME
	{
		unsigned char * pp_src_virt_addr  = src_img->base;

		//////////////////////////////////////////////////////////////////////////
		if(DoPPWorkAround(pp_src_phys_addr, pp_src_virt_addr,  src_img,  src_rect,
			                                                  &mid_img, &mid_rect) < 0) 
		{
			ret = -1;
			goto doCameraPicture_done;
		}

		g2d_src_phys_addr = GetPmemPhysAddr();
	}
#ifdef RENDER_DEBUG_FIMC_2D
	LOGD("##############DoCameraPicture-3 \n");
#endif

	if(DoG2D(g2d_src_phys_addr, &mid_img, &mid_rect, 0,
	         g2d_dst_phys_addr,  dst_img,  dst_rect, dst_flag_android_order,
	         rotate_value, m_alpha) < 0)
	{
		ret = -1;
		goto doCameraPicture_done;
	}

#ifdef RENDER_DEBUG_FIMC_2D
	LOGD("##############DoCameraPicture-4 \n");
#endif

doCameraPicture_done :

	#ifdef DITHERING_ON_CAMERA_PICTURE_FRAME
	{
		int i = 0;
		for(i = 0; i < NUM_OF_MEMORY_OBJECT; i++)
		{
			if(destroyMem(&m_dither_mem[i]) < 0)
				LOGE("DoCameraPicture::destroyMem fail");
		}
	}
	#endif

	return ret;
}

unsigned int S3c2DRender::DoImageSclDownYUV422I(unsigned char *src_buf , s3c_img *src_img, s3c_rect *src_rect, int scl_down_fact)
{
	unsigned int dst_phys_addr = 0;
	unsigned int *dst_virt_addr;
	unsigned int *src_virt_addr;
	unsigned char *src_virt_addr2;
	unsigned int frame_size;
	unsigned int new_width;
	unsigned int new_height;
	unsigned int R0, R1;
	unsigned int offset;
	unsigned int mask1, mask2;
	int loop, loop2;

	if(scl_down_fact <= 2)
                scl_down_fact = 2;
	if((scl_down_fact > 2) && (scl_down_fact < 5))
			scl_down_fact = 4;
	else
			scl_down_fact = ((scl_down_fact + 3) >> 2) << 2;


	new_width = (src_rect->w /scl_down_fact);
	new_height = (src_rect->h /scl_down_fact);
	frame_size =  new_width *  new_height * 2;
	src_virt_addr = (unsigned int *)(src_buf + (src_rect->y * src_img->width ) + src_rect->x);	
	src_virt_addr = (unsigned int *)((((unsigned int)src_virt_addr) >> 2) << 2);
	src_virt_addr2 = (unsigned char *)src_virt_addr;
	
	offset = scl_down_fact >> 1;

#ifdef RENDER_DEBUG_FIMC_2D
	LOGD("#########DoImageSclDownYUV422I:: src_virt_addr %x  src_buf %x  offset %d scl_down_fact %d \n", src_virt_addr, src_buf, offset, scl_down_fact);
#endif

	mask1 = 0xFF00FFFF;
	mask2 = 0xFF;
	
	
	if ( CheckFrameSize(frame_size, __func__) == 0 ){
		dst_phys_addr = GetPmemPhysAddr();
		dst_virt_addr = (unsigned int *)GetPmemVirtAddr();
		GoNextPmemPartition();

		src_img->base = (unsigned char *) dst_virt_addr;

		for(loop2=0; loop2 < new_height; loop2++){
			for(loop=0; loop < new_width; loop= loop+2){
				R0 = *src_virt_addr;
				src_virt_addr += offset;
				R1 = *src_virt_addr;
				src_virt_addr += offset;

				R0 &= mask1;
				R1 &= mask2;
				R1 = R1 << 16;
				R0 |= R1;
			
				*dst_virt_addr++ = R0;			
			}

			src_virt_addr2 +=  (scl_down_fact * src_img->width * 2);
#ifdef RENDER_DEBUG
	//LOGD("#########DoImageSclDownYUV422I:: src_virt_addr %x  src_virt_addr2 %x  dst_virt_addr %x \n", src_virt_addr, src_virt_addr2, dst_virt_addr);
#endif
			src_virt_addr = (unsigned int *)src_virt_addr2;

		}
		
		
		src_rect->w = new_width;
		src_rect->h = new_height;
		src_rect->x = 0;
		src_rect->y = 0;

		src_img->width  = new_width;
		src_img->height = new_height;	
		
		
	}

	return dst_phys_addr;	
	
	
}

unsigned int S3c2DRender::DoFIMC1WorkAround(unsigned char *src_buf , s3c_img *src_img, s3c_rect *src_rect, unsigned int src_physAddr)
{
	int scl_down_fact;	
	
	
	unsigned int src_phys_addr     = src_physAddr;

	if(src_rect->w <= FIMC1_SCL_MAX_IN_HSIZE)
		return src_phys_addr;
	
	switch(src_img->format)    //to do
	{
		case COPYBIT_FORMAT_YCbCr_422_I:
			scl_down_fact = (int)((src_rect->w  + (FIMC1_SCL_MAX_IN_HSIZE - 1))/FIMC1_SCL_MAX_IN_HSIZE);
			
			if(scl_down_fact > 1)
				src_phys_addr = DoImageSclDownYUV422I(src_buf, src_img, src_rect, scl_down_fact);

			break;

		default:
			break;
	}

	return src_phys_addr;

}

int S3c2DRender::DoPPDoG2D(s3c_img *src_img, s3c_rect *src_rect,
                      s3c_img *dst_img, s3c_rect *dst_rect)
{
	struct pmem_region region;
	s3c_fb_next_info_t fb_info;
	unsigned int    src_phys_addr     = 0;
	unsigned char * src_virt_addr     = NULL;
	unsigned int    pp_src_phys_addr  = 0;
	unsigned char * pp_src_virt_addr  = NULL;
	unsigned int    g2d_src_phys_addr = 0;
	unsigned char * g2d_src_virt_addr = NULL;
	unsigned int    g2d_dst_phys_addr = 0;
	unsigned char * g2d_dst_virt_addr = NULL;
	unsigned int    dst_phys_addr     = 0;
	unsigned char * dst_virt_addr     = NULL;
	unsigned int    flag_need_src_memcpy = 0;
	unsigned int    flag_need_dst_memcpy = 0;

	unsigned int    src_frame_size    = 0;
	unsigned int    mid_frame_size    = 0;
	unsigned int 	is_full_screen   = 1;

	int             dst_flag_android_order = 1;
#ifdef ZERO_COPY
	unsigned int zeroCopyData[3];
#endif

#ifdef DITHERING_ON_CAMERA_PREVIEW_FRAME
	int flag_dithering_yuv2rgb     = 0;
	int flag_dithering_yuv2rgb_2   = 0;
#endif
	s3c_img  *   real_src_img  = src_img;
	s3c_rect *   real_src_rect = src_rect;
	s3c_img      temp_src_img;
	s3c_rect     temp_src_rect;
	s3c_img      mid_img;
	s3c_rect     mid_rect;
	s3c_rect     dst_org_rect;
	
	int rotate_value = RotateValueCopybit2G2D(m_flags);
	

#if 1//Handle PP limitation of input starting at odd boundary:Change to even boundary
	if(src_rect->x || src_rect->y)
	{
	   src_rect->x = m_src_org_rect.x;
	   src_rect->y = m_src_org_rect.y;
	   src_rect->w = m_src_org_rect.w;
	   src_rect->h = m_src_org_rect.h;
	   is_full_screen = 0;
          memcpy(&dst_org_rect, dst_rect, sizeof(s3c_rect));
	   dst_rect->x = m_dst_org_rect.x;
	   dst_rect->y = m_dst_org_rect.y;
          dst_rect->w = m_dst_org_rect.w;
      	   dst_rect->h = m_dst_org_rect.h; 	
	}
	if(src_rect->x & 01)
	{
		src_rect->x--;
	}
#endif

	MakeMidImgRect(dst_img, dst_rect, &mid_img, &mid_rect, rotate_value);

	mid_frame_size = GetFrameSize(mid_img.format, mid_img.width, mid_img.height);

	if ( CheckFrameSize(mid_frame_size, __func__) < 0 )
		return -1;

	/*
	 * set dst data
	 */
	switch(dst_img->flag)
	{
	    case SEC2D_FLAGS_FRAMEBUFFER:
			if(m_fb_start_phy_addr)
				g2d_dst_phys_addr = m_fb_start_phy_addr + dst_img->offset;
			else
			{
				if(ioctl(dst_img->memory_id, S3C_FB_GET_CURR_FB_INFO, &fb_info) >= 0)
				{
					m_fb_start_phy_addr = fb_info.phy_start_addr;
					g2d_dst_phys_addr = m_fb_start_phy_addr + dst_img->offset; 
				}
				else
					flag_need_dst_memcpy = 1;
			}
			g2d_dst_virt_addr  = dst_img->base;

#ifdef DITHERING_ON_CAMERA_PREVIEW_FRAME
			flag_dithering_yuv2rgb_2 = 1;
#endif
			dst_flag_android_order = 0;
			break;

		 case SEC2D_FLAGS_USES_PMEM:
			g2d_dst_phys_addr = m_pmem_phys_addr + dst_img->offset;
			g2d_dst_virt_addr = dst_img->base;

			dst_flag_android_order = 1;
			break;

		default:
			flag_need_dst_memcpy = 1;
			break;
	}

	/*
	 * set src data
	 */
	switch(src_img->flag)
	{
#ifdef PMEM_STREAM
		case SEC2D_FLAGS_VIDEO_PLAY:
#ifdef ZERO_COPY
			memcpy(&(zeroCopyData[0]), (void *)(m_pmem_stream_virt_addr + src_img->offset), 12);		
			if(zeroCopyData[0] != ZERO_COPY_FLAG)
			{
				pp_src_phys_addr = m_pmem_stream_phys_addr + src_img->offset;
				pp_src_virt_addr = m_pmem_stream_virt_addr + src_img->offset;
			}
			else
			{
				pp_src_phys_addr = zeroCopyData[1];			
				pp_src_virt_addr = (unsigned char *) (zeroCopyData[2]);				
			}
#else
			pp_src_phys_addr = m_pmem_stream_phys_addr + src_img->offset;
			pp_src_virt_addr = m_pmem_stream_virt_addr + src_img->offset;
#endif
	  		break;
#endif			
		case SEC2D_FLAGS_CAMERA_PREVIEW:
		
			pp_src_phys_addr = m_pmem_preview_phys_addr + src_img->offset;
			pp_src_virt_addr  = src_img->base;

#ifdef DITHERING_ON_CAMERA_PREVIEW_FRAME
			if(  (src_img->format == COPYBIT_FORMAT_YCbCr_420_P || src_img->format == COPYBIT_FORMAT_YCbCr_422_P)
				  && dst_img->format == COPYBIT_FORMAT_RGB_565)
				flag_dithering_yuv2rgb = 1;
			else
				flag_dithering_yuv2rgb = 0;
#endif
			break;

		case SEC2D_FLAGS_CAMERA_PICTURE:
			pp_src_phys_addr = m_pmem_picture_phys_addr + src_img->offset;

			pp_src_phys_addr = DoFIMC1WorkAround(src_img->base, src_img, src_rect, m_pmem_picture_phys_addr + src_img->offset);

			//return -1;

			if(!pp_src_phys_addr)
				return -1;
		
			return DoCameraPicture(src_img, src_rect,  
			                       dst_img, dst_rect, pp_src_phys_addr);
			break;

		case SEC2D_FLAGS_FRAMEBUFFER:	        
		
			if(m_fb_start_phy_addr)
				pp_src_phys_addr = m_fb_start_phy_addr + src_img->offset;
			else
			{
				if(ioctl(src_img->memory_id, S3C_FB_GET_CURR_FB_INFO, &fb_info) >= 0)
				{
					m_fb_start_phy_addr = fb_info.phy_start_addr;
					pp_src_phys_addr = m_fb_start_phy_addr + src_img->offset; 
				}
				else
					flag_need_src_memcpy = 1;
			}

			pp_src_virt_addr  = src_img->base;
			break;

		default:
			flag_need_src_memcpy = 1;
			break;
	}
	
	if (flag_need_src_memcpy)
	{
		src_frame_size = GetFrameSize(src_img->format, src_img->width, src_img->height);
		if(CheckFrameSize(src_frame_size, __func__) < 0 )
			return -1;

		src_virt_addr = src_img->base;

		pp_src_virt_addr = GetPmemVirtAddr();
		pp_src_phys_addr = GetPmemPhysAddr();
		GoNextPmemPartition();

		memcpy(pp_src_virt_addr, src_virt_addr, src_frame_size);
	}


#ifdef DITHERING_ON_CAMERA_PREVIEW_FRAME	
	flag_dithering_yuv2rgb &= flag_dithering_yuv2rgb_2;

	if(flag_dithering_yuv2rgb == 1)
	{
		if(src_frame_size == 0)
			src_frame_size = GetFrameSize(src_img->format, src_img->width, src_img->height);

		mid_frame_size = GetFrameSize(dst_img->format, src_img->width, src_img->height);

#ifdef FORCE_SW_MEM
		if(createMem(&m_dither_mem[0], src_frame_size, MEMORY_TYPE_SW_MEM) < 0)
#else
		if(createMem(&m_dither_mem[0], src_frame_size, MEMORY_TYPE_HW_MEM) < 0)
#endif
		{
			LOGE("%s::createMem(%d) fail\n", __func__, src_frame_size);
			flag_dithering_yuv2rgb = 0;
		}

		if(    flag_dithering_yuv2rgb == 1
#ifdef FORCE_SW_MEM
		   && createMem(&m_dither_mem[1], mid_frame_size, MEMORY_TYPE_SW_MEM) < 0)
#else
		   && createMem(&m_dither_mem[1], mid_frame_size, MEMORY_TYPE_HW_MEM) < 0)
#endif 
		{
			LOGE("%s::createMem(%d) fail\n", __func__, src_frame_size);
			flag_dithering_yuv2rgb = 0;
		}

		if(flag_dithering_yuv2rgb == 1)
		{
			memcpy(m_dither_mem[0].virt_addr, pp_src_virt_addr, src_frame_size);

			// only color conversion
			if(ditheringYuv2Rgb(m_dither_mem[0].virt_addr,
								m_dither_mem[1].virt_addr,
								src_img->width, src_img->height, src_img->format) < 0)
			{
				LOGE("%s::ditheringYuv2Rgb fail\n", __func__);
				flag_dithering_yuv2rgb = 0;
			}
			else
			{
				if(m_dither_mem[1].type == MEMORY_TYPE_HW_MEM)
				{
					if(flushMem(&m_dither_mem[1]) < 0)
					{
						LOGE("%s::flushMem fail\n", __func__);
						flag_dithering_yuv2rgb = 0;
					}
					else
					{
						if(   src_rect->w == mid_rect.w
						   && src_rect->h == mid_rect.h)
						{
							g2d_src_phys_addr = m_dither_mem[1].phys_addr;
							g2d_src_virt_addr = m_dither_mem[1].virt_addr;
						}
						else
						{
							pp_src_phys_addr = m_dither_mem[1].phys_addr;
							pp_src_virt_addr = m_dither_mem[1].virt_addr;

							flag_dithering_yuv2rgb = 2;
						}
					}
				}
				else // if(m_dither_mem[1].type == MEMORY_TYPE_SW_MEM)
				{
					if(CheckFrameSize(mid_frame_size, __func__) < 0 )
					{
						flag_dithering_yuv2rgb = 0;
					}
					else
					{						
						if(   src_rect->w == mid_rect.w
						   && src_rect->h == mid_rect.h)
						{
							g2d_src_phys_addr = GetPmemPhysAddr();
							g2d_src_virt_addr = GetPmemVirtAddr();
							GoNextPmemPartition();

							memcpy(g2d_src_virt_addr, m_dither_mem[1].virt_addr, mid_frame_size);
						}
						else
						{
							pp_src_phys_addr = GetPmemPhysAddr();
							pp_src_virt_addr = GetPmemVirtAddr();
							GoNextPmemPartition();

							memcpy(pp_src_virt_addr, m_dither_mem[1].virt_addr, mid_frame_size);

							flag_dithering_yuv2rgb = 2;
						}
					}
				}

				// ALL SUCCEED ON DITHERING..
				if(flag_dithering_yuv2rgb == 1)  // -> g2d : src -> dst
				{
					memcpy(&mid_img,  src_img,  sizeof(s3c_img));
					memcpy(&mid_rect, src_rect, sizeof(s3c_rect));
					mid_img.format = dst_img->format;
				}
				else if(flag_dithering_yuv2rgb == 2)  // -> pp : src -> src
				{
					memcpy(&temp_src_img,  src_img,  sizeof(s3c_img));
					memcpy(&temp_src_rect, src_rect, sizeof(s3c_rect));

					temp_src_img.format = dst_img->format;

					real_src_img  = &temp_src_img;
					real_src_rect = &temp_src_rect;	
				}
			}
		}
	}
	
	if(   flag_dithering_yuv2rgb == 0
	   || flag_dithering_yuv2rgb == 2)
#endif // DITHERING_ON_CAMERA_PREVIEW_FRAME	
	{
		// color conversion + resize
		if(DoPPWorkAround(pp_src_phys_addr, pp_src_virt_addr, real_src_img,  real_src_rect,
		                                                      &mid_img,      &mid_rect) < 0)
		{
			LOGE("%s::DoPPWorkAround fail\n", __func__);
			return -1;
		}
		
		// set dst data
		g2d_src_phys_addr = GetPmemPhysAddr();
		//g2d_src_virt_addr = GetPmemVirtAddr();
		GoNextPmemPartition();
	}

	if((flag_need_dst_memcpy) || (is_full_screen == 0))
	{
		flag_need_dst_memcpy = 1;
		g2d_dst_phys_addr = GetPmemPhysAddr();
		g2d_dst_virt_addr = GetPmemVirtAddr();
		
		dst_virt_addr = dst_img->base;
	}
	
	
	if(DoG2D(g2d_src_phys_addr, &mid_img, &mid_rect, 0,
	         g2d_dst_phys_addr,  dst_img,  dst_rect, dst_flag_android_order,
	         rotate_value, m_alpha) < 0)
	{
		LOGE("%s::DoG2D fail\n", __func__);
		return -1;
	}

	if(is_full_screen == 0){
		memcpy(dst_rect, &dst_org_rect, sizeof(s3c_rect));
	}
	// need to copy to dest buffer, because it's not FB
	if (flag_need_dst_memcpy)
	{
		return CopyRgb(dst_img, dst_rect, g2d_dst_virt_addr, dst_virt_addr, dst_flag_android_order);
	}

/*
#ifdef DITHERING_ON_CAMERA_PREVIEW_FRAME
	{
		//if(   flag_dithering_yuv2rgb == 0
	    //   || flag_dithering_yuv2rgb == 2)
		int i = 0;
		for(i = 0; i < NUM_OF_MEMORY_OBJECT; i++)
		{
			if(destroyMem(&m_dither_mem[i]) < 0)
				LOGE("%s::destroyMem fail\n", __func__);
		}
	}
#endif
*/
	return 0;
}

int S3c2DRender::JustdoG2D(s3c_img *src_img, s3c_rect *src_rect,
                           s3c_img *dst_img, s3c_rect *dst_rect)
{
	struct pmem_region region;
	s3c_fb_next_info_t fb_info;
	unsigned int	src_frame_size    = 0;
	unsigned int    src_phys_addr     = 0;
	unsigned char * src_virt_addr     = src_img->base;
	unsigned int	g2d_src_phys_addr = 0;
	unsigned char * g2d_src_virt_addr = NULL;
	unsigned int	g2d_dst_phys_addr = 0;
	unsigned char * g2d_dst_virt_addr = NULL;
	unsigned int	dst_phys_addr     = 0;
	unsigned char * dst_virt_addr     = NULL;
	int 	        rotate_value      = 0;
	unsigned int	flag_need_dst_memcpy = 0;
	int             src_flag_android_order = 1;
	int             dst_flag_android_order = 1;
	int             w_h_1_case = 0;
	s3c_rect 		dst_mid_rect;
#ifdef ZERO_COPY
	unsigned int zeroCopyData[3];
#endif

	src_frame_size = GetFrameSize(src_img->format, src_img->width, src_img->height);
	if(CheckFrameSize(src_frame_size, __func__) < 0 )
		return -1;

	g2d_src_phys_addr = GetPmemPhysAddr();
	g2d_src_virt_addr = GetPmemVirtAddr();
	GoNextPmemPartition();

#ifdef RENDER_DEBUG_2D
	LOGD("########JustdoG2D-1:: src_width %d  src_height %d src_fmt %d  dst_width %d  dst_height %d dst_fmt %d rect_x %d rect_y %d rect_w  %d rect_h %d\n",
	      src_img->width, src_img->height, src_img->format, dst_img->width, dst_img->height, dst_img->format, src_rect->x, src_rect->y, src_rect->w, src_rect->h);
#endif

	/*
	 * set src address
	 */
	switch(src_img->flag)
	{
		case SEC2D_FLAGS_USES_PMEM:
#ifdef RENDER_DEBUG_2D
			LOGD("########JustdoG2D-2:: src_width %d  src_height %d src_fmt %d  dst_width %d  dst_height %d dst_fmt %d rect_x %d rect_y %d rect_w  %d rect_h %d\n",
                                        src_img->width, src_img->height, src_img->format, dst_img->width, dst_img->height, dst_img->format, src_rect->x, src_rect->y, src_rect->w, src_rect->h);
#endif
			g2d_src_phys_addr = m_pmem_phys_addr + src_img->offset;
			cacheflush((unsigned int)src_virt_addr, (unsigned int)src_virt_addr + src_frame_size - 1, 0);

			src_flag_android_order = 1;
			break;
#ifdef PMEM_STREAM
		case SEC2D_FLAGS_VIDEO_PLAY:
#ifdef ZERO_COPY
			memcpy(&(zeroCopyData[0]), (void *)(m_pmem_stream_virt_addr + src_img->offset), 12);			
			if(zeroCopyData[0] != ZERO_COPY_FLAG)
			{
				g2d_src_phys_addr = m_pmem_stream_phys_addr + src_img->offset;	
			}
			else
			{
				g2d_src_phys_addr = zeroCopyData[1];
				g2d_src_virt_addr = (unsigned char *)zeroCopyData[2];
			}
#else
			g2d_src_phys_addr = m_pmem_stream_phys_addr + src_img->offset;
#endif
			src_flag_android_order = 0;
			break;
#endif			
		case SEC2D_FLAGS_FRAMEBUFFER:	
#ifdef RENDER_DEBUG_2D
			LOGD("########JustdoG2D-3:: src_width %d  src_height %d src_fmt %d  dst_width %d  dst_height %d dst_fmt %d rect_x %d rect_y %d rect_w  %d rect_h %d\n",
                                        src_img->width, src_img->height, src_img->format, dst_img->width, dst_img->height, dst_img->format, src_rect->x, src_rect->y, src_rect->w, src_rect->h);
#endif
			if(m_fb_start_phy_addr)
				g2d_src_phys_addr = m_fb_start_phy_addr + src_img->offset;
			else
			{
				if(ioctl(src_img->memory_id, S3C_FB_GET_CURR_FB_INFO, &fb_info) >= 0)
				{
					m_fb_start_phy_addr = fb_info.phy_start_addr;
					g2d_src_phys_addr = m_fb_start_phy_addr + src_img->offset; 
				}
				else
				{
					if(g2d_src_virt_addr && src_virt_addr)
						memcpy(g2d_src_virt_addr, src_virt_addr, src_frame_size);
				}
			}
			src_flag_android_order = 0;
			break;
			
		default:
			if(g2d_src_virt_addr && src_virt_addr)
				memcpy(g2d_src_virt_addr, src_virt_addr, src_frame_size);	

			src_flag_android_order = 1;
			break;
	}

	/*
	 * set dst address
	 */
	switch(dst_img->flag)
	{
		case SEC2D_FLAGS_FRAMEBUFFER:
#ifdef RENDER_DEBUG_2D
			LOGD("########JustdoG2D-4:: src_width %d  src_height %d src_fmt %d  dst_width %d  dst_height %d dst_fmt %d rect_x %d rect_y %d rect_w  %d rect_h %d\n",
			                            src_img->width, src_img->height, src_img->format, dst_img->width, dst_img->height, dst_img->format, src_rect->x, src_rect->y, src_rect->w, src_rect->h);
#endif
#if 0//1:x
			if((src_rect->w == 1) || (src_rect->h == 1) || (dst_rect->w == 1) || (dst_rect->h == 1))
			{//Handle G2d Limitation: can't process 1:x or x:1 input rect:so NO direct FB copy 
				flag_need_dst_memcpy = 1;
       			w_h_1_case = 1;
			}
			else
#endif
			{
			if(m_fb_start_phy_addr)
				g2d_dst_phys_addr = m_fb_start_phy_addr + dst_img->offset;
			else
			{
				if(ioctl(dst_img->memory_id, S3C_FB_GET_CURR_FB_INFO, &fb_info) >= 0)
				{
					m_fb_start_phy_addr = fb_info.phy_start_addr;
					g2d_dst_phys_addr = m_fb_start_phy_addr + dst_img->offset; 
				}
				else
				{
					flag_need_dst_memcpy = 1;
				}
			}
			}
			dst_flag_android_order = 0;
			break;
		case SEC2D_FLAGS_USES_PMEM:	
#ifdef RENDER_DEBUG_2D
			LOGD("########JustdoG2D-5:: src_width %d  src_height %d src_fmt %d  dst_width %d  dst_height %d dst_fmt %d rect_x %d rect_y %d rect_w  %d rect_h %d\n",
			                            src_img->width, src_img->height, src_img->format, dst_img->width, dst_img->height, dst_img->format, src_rect->x, src_rect->y, src_rect->w, src_rect->h);
#endif
			g2d_dst_phys_addr = m_pmem_phys_addr + dst_img->offset;
			dst_flag_android_order = 1;
			break;
		default:
	      	flag_need_dst_memcpy = 1;
       		break;
	}

	if(flag_need_dst_memcpy)
	{
			g2d_dst_phys_addr = GetPmemPhysAddr();
			g2d_dst_virt_addr = GetPmemVirtAddr();

		//don't reset "flag_need_dst_memcpy" yet
#ifdef RENDER_DEBUG_2D
		LOGE("G2D path: (%d,%d):(%d,%d) & (%d,%d,%d,%d):(%d,%d,%d,%d)",
                   src_img->width, src_img->height, 
                   dst_img->width, dst_img->height,
                   src_rect->x, src_rect->y, src_rect->w, src_rect->h,
                   dst_rect->x, dst_rect->y, dst_rect->w, dst_rect->h);			
#endif
	}

	// execute G2D 
	rotate_value = RotateValueCopybit2G2D(m_flags);
#ifdef RENDER_DEBUG_2D
        LOGD("########JustdoG2D-6:: src_width %d  src_height %d src_fmt %d  dst_width %d  dst_height %d dst_fmt %d rect_x %d rect_y %d rect_w  %d rect_h %d\n",
                                        src_img->width, src_img->height, src_img->format, dst_img->width, dst_img->height, dst_img->format, src_rect->x, src_rect->y, src_rect->w, src_rect->h);
#endif

	if(DoG2D(g2d_src_phys_addr, src_img, src_rect, src_flag_android_order,
	         g2d_dst_phys_addr, dst_img, dst_rect, dst_flag_android_order,
	         rotate_value, m_alpha) < 0)
		return -1;
#ifdef RENDER_DEBUG_2D
        LOGD("########JustdoG2D-7:: src_width %d  src_height %d src_fmt %d  dst_width %d  dst_height %d dst_fmt %d rect_x %d rect_y %d rect_w  %d rect_h %d\n",
                                        src_img->width, src_img->height, src_img->format, dst_img->width, dst_img->height, dst_img->format, src_rect->x, src_rect->y, src_rect->w, src_rect->h);
#endif
	// copy back to memory while destnation buffer is not the FB 
	if (flag_need_dst_memcpy)
	{
		dst_virt_addr = dst_img->base;

		if(!(g2d_dst_virt_addr && dst_virt_addr))
			return -1;
#if 0//G2D Fix
		if(w_h_1_case)
		{
			memcpy(&dst_mid_rect, dst_rect, sizeof(s3c_rect));
			dst_mid_rect.x =0;
			dst_mid_rect.y = 0;                
			if((rotate_value == S3C_G2D_ROTATOR_0) || (rotate_value == S3C_G2D_ROTATOR_180))
			{				
		            if(((src_rect->w == 1) || (dst_rect->w == 1))&& (src_rect->x + src_rect->w == src_img->width))
		            {
						dst_mid_rect.x = 1;                    		   

		            }       
			     if(((src_rect->h == 1) || (dst_rect->h == 1))&& (src_rect->y + src_rect->h == src_img->height))
			     {                
						dst_mid_rect.y = 1;
		            }

	        	}
		  	else 
		    	{
				if((src_rect->w == 1) || (dst_rect->h == 1))
				{
					if(src_rect->x + src_rect->w == src_img->width)
					{
						dst_mid_rect.y = 1;                    		   
					}
					else
					{
						dst_mid_rect.y = 0; //default                    		   
					}
		   		}       
				if((src_rect->h == 1) || (dst_rect->w == 1))
				{                
					if(src_rect->y + src_rect->h == src_img->height)
					{
		                   		dst_mid_rect.x = 0; //default
					}
					else
					{
						dst_mid_rect.x = 1;
					}
		       	 }		
			}
			return CopyRgb_2dworkaround(dst_img, &dst_mid_rect, dst_rect, g2d_dst_virt_addr, dst_virt_addr, dst_flag_android_order);
	 	}
#endif
		return CopyRgb(dst_img, dst_rect, g2d_dst_virt_addr, dst_virt_addr, dst_flag_android_order);
	}
	//Start the HDMI now

#ifdef  HW_HDMI_USE
        {
                int ret;

#ifdef ROTATE_90
                ret = hdmi_gl_set_param(HDMI_GRAPHIC_LAYER_FOR_UI,g2d_dst_phys_addr,dst_img->height, dst_img->width);
                //LOGD("hdmi_gl_set_format : layer=%d addr=0x%x w=%d h=%d (ret=%d)",HDMI_GRAPHIC_LAYER_FOR_UI,g2d_dst_phys_addr,dst_img->height, dst_img->width, ret);
#else
                ret = hdmi_gl_set_param(HDMI_GRAPHIC_LAYER_FOR_UI,g2d_dst_phys_addr,dst_img->width, dst_img->height);
                //LOGD("hdmi_gl_set_format : layer=%d addr=0x%x w=%d h=%d (ret=%d)",HDMI_GRAPHIC_LAYER_FOR_UI,g2d_dst_phys_addr, dst_img->width, dst_img->height, ret);
#endif

                ret = hdmi_gl_streamon(HDMI_GRAPHIC_LAYER_FOR_UI);
                //LOGD("hdmi_gl_streamon(%d) : ret=%d", ret, HDMI_GRAPHIC_LAYER_FOR_UI);
        }
#endif
	
	return 0;
}

int S3c2DRender::OpenPP(void)
{
	struct v4l2_capability  cap;
	struct v4l2_format      fmt;
	//s5p_pp_t * s5p_pp        = &m_pp;
	//s5p_pp_params_t * params = &(s5p_pp->params);

	int  ret, index;
	//LOGD("#########################OpenPP \n");
	
	#define  PP_DEVICE_DEV_NAME  "/dev/video1"
	if(m_pp.dev_fd <= 0){

		m_pp.dev_fd = open(PP_DEVICE_DEV_NAME, O_RDWR);
		if(m_pp.dev_fd > 0){
			// check capability
			ret = ioctl(m_pp.dev_fd, VIDIOC_QUERYCAP, &cap);
			if (ret < 0) {
				LOGE("%s::VIDIOC_QUERYCAP failed\n", __func__);
				return -1;
			}

	        if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
				LOGE("%s::%d has no streaming support\n", __func__, m_pp.dev_fd);
                return -1;
       		}

			if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
				LOGE("%s::%d is no video output\n", __func__, m_pp.dev_fd);
				return -1;
			}

			//malloc fimc_outinfo structure
			fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

			ret = ioctl(m_pp.dev_fd, VIDIOC_G_FMT, &fmt);
			if (ret < 0) {
				LOGE("%s::Error in video VIDIOC_G_FMT\n", __func__);
				return -1;
			}
		}
	}
	if(m_pp.dev_fd < 0)
	{
		if (EACCES == errno || EBUSY == errno)
		{
			// egl_window_surface_v2_t is initialized when eglCreateWindowSurface() is called.
			// Eclair loads copybit lib whenever egl_window_surface_v2_t is initialized.
			// however, fimc cannot be opened multiply.
			// So, fimc just can be open at only one time 
			// when egl_window_surface_v2_t is initialized by system, it means when Surfaceflinger is initialized.
			LOGD("%s:: post processor already open, using only G2D\n", __func__);
			return 0;
		}
		else
		{
			LOGE("%s::Post processor open error (%d)\n", __func__,  errno);
			return -1;
		}
	}	
	#ifdef  HW_HDMI_USE
        	{
                int ret;

                ret = hdmi_gl_initialize(HDMI_GRAPHIC_LAYER_FOR_UI);
		if(ret<0)
			LOGE("[ERROR]TV init failed...");
                LOGD("hdmi_gl_initialize(%d) (ret=%d)", HDMI_GRAPHIC_LAYER_FOR_UI, ret);
        	}
	#endif
	//s5p_pp->use_ext_out_mem = 0;

	return 0;
}

int S3c2DRender::ClosePP(void)
{	
	if(m_pp.virt_addr != NULL)
	{
		munmap(m_pp.virt_addr, m_pp.buf_size);
		
		m_pp.virt_addr = NULL;
		m_pp.phys_addr = 0;
		m_pp.buf_size  = 0;
	}

	if(m_pp.dev_fd != 0)
		close(m_pp.dev_fd);
	m_pp.dev_fd = 0;

	return 0;
}

int S3c2DRender::DoPP(unsigned int src_phys_addr, s3c_img *src_img, s3c_rect *src_rect,
                unsigned int dst_phys_addr, s3c_img *dst_img, s3c_rect *dst_rect)
{
	s3c_pp_params_t   params;
	unsigned int      frame_size = 0;

	int src_color_space = ColorFormatCopybit2PP(src_img->format);
	int dst_color_space = ColorFormatCopybit2PP(dst_img->format);

	SetS3cPPParams(&params, src_img, src_rect, src_color_space, src_phys_addr, 
	                           dst_img, dst_rect, dst_color_space, dst_phys_addr, 
	                           0 , 0);
	
	SetS3cPPParamsCbCr(&params);

	if(ControlPP(&params) < 0)
	{
		LOGE("ControlPP fail");
		return -1;
	}

	return 0;
}

int fimc_v4l2_set_src(int fd, s3c_pp_params_t  *params)
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
	fmt.fmt.pix.width 		= params->src_full_width;
	fmt.fmt.pix.height 		= params->src_full_height;
	fmt.fmt.pix.pixelformat	= params->src_color_space;
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
	crop.c.left   = params->src_start_x;
	crop.c.top    = params->src_start_y;
	crop.c.width  = params->src_width;
	crop.c.height = params->src_height;

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

int fimc_v4l2_set_dst(int fd, s3c_pp_params_t  *params)
{
	struct v4l2_format		sFormat;
	struct v4l2_control		vc;
	struct v4l2_framebuffer	fbuf;
	int				ret_val;

	/* 
	 * set rotation configuration
	 */

	vc.id = V4L2_CID_ROTATION;
	vc.value = 0; //rotation;

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

	fbuf.base 				= (void *)params->dst_buf_addr_phy_rgb_y;
	fbuf.fmt.width 			= params->dst_full_width;
	fbuf.fmt.height 		= params->dst_full_height;
	fbuf.fmt.pixelformat 	= params->dst_color_space;

	ret_val = ioctl (fd, VIDIOC_S_FBUF, &fbuf);
	if (ret_val < 0)  {
		LOGE("Error in video VIDIOC_S_FBUF (%d)\n",ret_val);        
		return -1;
	}

	/* 
	 * set destination window
	 */

	sFormat.type 				= V4L2_BUF_TYPE_VIDEO_OVERLAY;
	sFormat.fmt.win.w.left 		= params->dst_start_x;
	sFormat.fmt.win.w.top 		= params->dst_start_y;
	sFormat.fmt.win.w.width 	= params->dst_width;
	sFormat.fmt.win.w.height 	= params->dst_height;

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
	struct v4l2_buffer      buf;

	buf.type        = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	buf.memory      = V4L2_MEMORY_USERPTR;
	buf.m.userptr   = (unsigned long)fimc_buf;
	buf.length      = 0;
	buf.index       = 0;

	int     ret_val;

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

	if (-1 == ioctl (fd, VIDIOC_DQBUF, &buf)){
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



inline int S3c2DRender::ControlPP(s3c_pp_params_t * params)
{
	int             ret;
	struct fimc_buf         fimc_src_buf;

	fimc_v4l2_set_src(m_pp.dev_fd, params);
	fimc_v4l2_set_dst(m_pp.dev_fd, params);
	
	fimc_src_buf.base[0] = params->src_buf_addr_phy_rgb_y;
	fimc_src_buf.base[1] = params->src_buf_addr_phy_cb;
	fimc_src_buf.base[2] = params->src_buf_addr_phy_cr;

    ret = fimc_v4l2_stream_on(m_pp.dev_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT);
    if(ret < 0) {
		LOGE("Fail : v4l2_stream_on()");
		return -1;
    }

	ret = fimc_v4l2_queue(m_pp.dev_fd, &fimc_src_buf);
	if(ret < 0) {
		LOGE("Fail : v4l2_queue()\n");
		return -1;
	}

#if 0

	ret = fimc_v4l2_dequeue(m_pp.dev_fd);
	if(ret < 0) {
		LOGE("Fail : v4l2_dequeue()\n");
		return -1;
	}

#endif

	ret = fimc_v4l2_stream_off(m_pp.dev_fd);
	if(ret < 0) {
		LOGE("Fail : v4l2_stream_off()");
		return -1;
	}

	ret = fimc_v4l2_clr_buf(m_pp.dev_fd);
	if(ret < 0) {
		LOGE("Fail : v4l2_clr_buf()");
		return -1;
	}

	return 0;

}

int S3c2DRender::DoPPWorkAround(unsigned int src_phys_addr, unsigned char * src_virt_addr,
                          s3c_img * src_img, s3c_rect * src_rect, 
                          s3c_img * dst_img, s3c_rect * dst_rect )
{
	s3c_pp_params_t   org_params;
	s3c_pp_params_t   new_params;
	
	int             flag_strip = 0;
	unsigned int    src_pad_width     = 0;
	unsigned int    dst_pad_width     = 0;
	unsigned int    src_pad_height    = 0;
	unsigned int    dst_pad_height    = 0;	
	unsigned int    src_pad_start_x   = 0;
	unsigned int    dst_pad_start_x   = 0;
	unsigned int    src_pad_start_x_align_factor   = 0;
	unsigned int    dst_pad_start_x_align_factor   = 0;
	
	unsigned int	pp_src_phys_addr  = 0;
	unsigned char * pp_src_virt_addr  = NULL;
	unsigned int	pp_dst_phys_addr  = 0;
	unsigned char * pp_dst_virt_addr  = NULL;
	unsigned int	strip_phys_addr   = 0;
	unsigned char * strip_virt_addr   = NULL;

	int src_color_space = ColorFormatCopybit2PP(src_img->format);
	int dst_color_space = ColorFormatCopybit2PP(dst_img->format);

	SetS3cPPParams(&org_params, src_img, src_rect, src_color_space, src_phys_addr, 
	                               dst_img, dst_rect, dst_color_space, 0, 
	                               0 , 1);

	memcpy(&new_params, &org_params, sizeof(s3c_pp_params_t));

	//////////
	// src
	src_pad_width   = GetWidthOfPP (src_color_space, src_rect->w)  - src_rect->w;
	src_pad_height  = GetHeightOfPP(src_color_space, src_rect->h)  - src_rect->h;
	
	src_pad_start_x = GetAlignedStartPosOfPP(src_phys_addr,             src_color_space,
	                                         new_params.src_start_x,    new_params.src_start_y,
	                                         new_params.src_full_width, new_params.src_full_height,
	                                         &src_pad_start_x_align_factor);
	
	if(src_pad_width != 0 || src_pad_height != 0 || src_pad_start_x != 0)
	{
		unsigned int src_frame_size = GetFrameSize(src_img->format, src_img->width, src_img->height);
#ifdef RENDER_DEBUG
		LOGD("#####DoPPWorkAround::  src_pad_width %d src_pad_height %d src_pad_start_x %d\n", src_pad_width, src_pad_height, src_pad_start_x);
#endif
		// add pad..
		if(CheckFrameSize(src_frame_size, __func__) == 0)
		{
			new_params.src_start_x  = 0;
			new_params.src_start_y  = 0;
			new_params.src_width   += src_pad_width;
			new_params.src_height  += src_pad_height;

			if(new_params.src_full_width < new_params.src_width)
				new_params.src_full_width  = new_params.src_width;

			if(new_params.src_full_height < new_params.src_height)
				new_params.src_full_height  = new_params.src_height;
								
			pp_src_phys_addr = GetPmemPhysAddr();
			pp_src_virt_addr = GetPmemVirtAddr();
			GoNextPmemPartition();

			

			// 2. copy and add pad
			CopyPPSrcframe(src_virt_addr,    &org_params, 
						   pp_src_virt_addr, &new_params,
						   src_pad_width,    src_pad_height);

			// 3. adjust address of PP's src hw
			new_params.src_buf_addr_phy_rgb_y = pp_src_phys_addr;
		}
		// cut pad..
		else
		{			
			new_params.src_width    = WidthOfPP (src_color_space, src_rect->w);
			new_params.src_height   = HeightOfPP(src_color_space, src_rect->h);

			pp_src_phys_addr = src_phys_addr;
			pp_src_virt_addr = src_virt_addr;
		}
	}
	else
	{
		pp_src_phys_addr = src_phys_addr;
		pp_src_virt_addr = src_virt_addr;
	}
	
	//////////
	// dst
	dst_pad_width   = GetWidthOfPP (dst_color_space, dst_rect->w) - dst_rect->w;
	dst_pad_height  = GetHeightOfPP(dst_color_space, dst_rect->h) - dst_rect->h;

	dst_pad_start_x = GetAlignedStartPosOfPP(GetPmemPhysAddr(),      dst_color_space,
	                                         new_params.dst_start_x,    new_params.dst_start_y,
	                                         new_params.dst_full_width, new_params.dst_full_height,
	                                         &dst_pad_start_x_align_factor);

	if(dst_pad_width != 0 || dst_pad_height != 0 || dst_pad_start_x != 0)
	{
		new_params.dst_start_x  = 0;
		new_params.dst_start_y  = 0;
		new_params.dst_width   += dst_pad_width;
		new_params.dst_height  += dst_pad_height;

		if(new_params.dst_full_width < new_params.dst_width)
			new_params.dst_full_width = new_params.dst_width;

		if(new_params.dst_full_height < new_params.dst_height)
			new_params.dst_full_height = new_params.dst_height;
		
		flag_strip = 1;
	}
	
	pp_dst_phys_addr = GetPmemPhysAddr();
	pp_dst_virt_addr = GetPmemVirtAddr();
	
	new_params.dst_buf_addr_phy_rgb_y = pp_dst_phys_addr;
	
	SetS3cPPParamsCbCr(&new_params);
	
#if 0
	if(   src_pad_start_x != 0 || src_pad_width != 0 || src_pad_height != 0
	   || dst_pad_start_x != 0 || dst_pad_width != 0 || dst_pad_height != 0)
	{
		LOGD("==== PP_SW_WORKAROUND SRC === \n");
		LOGD("src_img->width (%d)\n", src_img->width);
		LOGD("src_img->height(%d)\n", src_img->height);
		LOGD("src_rect->x    (%d)\n", src_rect->x);
		LOGD("src_rect->y    (%d)\n", src_rect->y);
		LOGD("src_rect->w    (%d)\n", src_rect->w);
		LOGD("src_rect->h    (%d)\n", src_rect->h);
		LOGD("src_pad_start_x(%d)\n", src_pad_start_x);
		LOGD("src_pad_width  (%d)\n", src_pad_width);
		LOGD("src_pad_height (%d)\n", src_pad_height);
		LOGD("\n");

		LOGD("==== PP_SW_WORKAROUND DST === \n");
		LOGD("dst_img->width (%d)\n", dst_img->width);
		LOGD("dst_img->height(%d)\n", dst_img->height);
		LOGD("dst_rect->x    (%d)\n", dst_rect->x);
		LOGD("dst_rect->y    (%d)\n", dst_rect->y);
		LOGD("dst_rect->w    (%d)\n", dst_rect->w);
		LOGD("dst_rect->h    (%d)\n", dst_rect->h);
		LOGD("dst_pad_start_x(%d)\n", dst_pad_start_x);
		LOGD("dst_pad_width  (%d)\n", dst_pad_width);
		LOGD("dst_pad_height (%d)\n", dst_pad_height);
		
		LOGD("\n");

		LOGD("==== PP PARAM            === \n");
		LOGD("new_params.src_color_space        (%d)\n", new_params.src_color_space);
		LOGD("new_params.src_full_width         (%d)\n", new_params.src_full_width);
		LOGD("new_params.src_full_height        (%d)\n", new_params.src_full_height);
		LOGD("new_params.src_start_x            (%d)\n", new_params.src_start_x);
		LOGD("new_params.src_start_y            (%d)\n", new_params.src_start_y);
		LOGD("new_params.src_width              (%d)\n", new_params.src_width);
		LOGD("new_params.src_height             (%d)\n", new_params.src_height);
		LOGD("new_params.src_buf_addr_phy_rgb_y (%d)\n", new_params.src_buf_addr_phy_rgb_y);
		LOGD("new_params.src_buf_addr_phy_cb    (%d)\n", new_params.src_buf_addr_phy_cb);
		LOGD("new_params.src_buf_addr_phy_cr    (%d)\n", new_params.src_buf_addr_phy_cr);

		LOGD("new_params.dst_color_space        (%d)\n", new_params.dst_color_space);
		LOGD("new_params.dst_full_width         (%d)\n", new_params.dst_full_width);
		LOGD("new_params.dst_full_height        (%d)\n", new_params.dst_full_height);
		LOGD("new_params.dst_start_x            (%d)\n", new_params.dst_start_x);
		LOGD("new_params.dst_start_y            (%d)\n", new_params.dst_start_y);
		LOGD("new_params.dst_width              (%d)\n", new_params.dst_width);
		LOGD("new_params.dst_height             (%d)\n", new_params.dst_height);
		LOGD("new_params.dst_buf_addr_phy_rgb_y (%d)\n", new_params.dst_buf_addr_phy_rgb_y);
		LOGD("new_params.dst_buf_addr_phy_cb    (%d)\n", new_params.dst_buf_addr_phy_cb);
		LOGD("new_params.dst_buf_addr_phy_cr    (%d)\n", new_params.dst_buf_addr_phy_cr);
		LOGD("\n");
	}
#endif
	
	if(ControlPP(&new_params) < 0)
	{
		LOGE("%s::ControlPP() fail\n", __func__);
		return -1;
	}

	if(flag_strip == 1)
	{
		GoNextPmemPartition();
		strip_virt_addr = GetPmemVirtAddr();

		CopyPPDstframe(pp_dst_virt_addr, &new_params,
		               strip_virt_addr,  &org_params,
		               dst_pad_width,    dst_pad_height);
	}
	return 0;
}

// this func assumes only YUV420, YUV422
int S3c2DRender::CopyPPSrcframe(unsigned char * src_virt_addr, s3c_pp_params_t * src_params, 
                                unsigned char * dst_virt_addr, s3c_pp_params_t * dst_params,
                                unsigned int pad_width, unsigned int pad_height)
{
	int plane_count;
	unsigned int x;
	unsigned int y;	
	unsigned int i;

	unsigned int src_frame_size = 0;
	unsigned int dst_frame_size = 0;
	unsigned int src_temp_index = 0;
	unsigned int dst_temp_index = 0;
	char last_pixel[4] = {0,};
	unsigned int num_of_pixel   = 0;

	unsigned char * current_src_addr  = src_virt_addr;
	unsigned char * current_dst_addr  = dst_virt_addr;
	unsigned char * previous_src_addr = src_virt_addr;
	unsigned char * previous_dst_addr = dst_virt_addr;

	unsigned int src_full_width  = src_params->src_full_width;
	unsigned int src_full_height = src_params->src_full_height;
	unsigned int src_start_x     = src_params->src_start_x;
	unsigned int src_start_y     = src_params->src_start_y;

	unsigned int dst_full_width  = dst_params->src_full_width;
	unsigned int dst_full_height = dst_params->src_full_height;
	unsigned int dst_start_x     = dst_params->src_start_x;
	unsigned int dst_start_y     = dst_params->src_start_y;	

	unsigned int width           = src_params->src_width;
	unsigned int height          = src_params->src_height;
	unsigned int real_pad_width  = pad_width;
	unsigned int real_pad_height = pad_height;

	for(plane_count = 0; plane_count < 3; plane_count++)
	{
		switch(src_params->src_color_space)
		{	
			case V4L2_PIX_FMT_YUV420 : // 8bit
				// y plane
				if(plane_count == 0)
				{
					// just use default size
				}
				else // cb cr
				{
					src_full_width  = (src_params->src_full_width  + 1) >> 1;
					src_full_height = (src_params->src_full_height + 1) >> 1;
					src_start_x     = (src_params->src_start_x        ) >> 1;
					src_start_y     = (src_params->src_start_y        ) >> 1;					

					dst_full_width  = (dst_params->src_full_width  + 1) >> 1;
					dst_full_height = (dst_params->src_full_height + 1) >> 1;
					dst_start_x     = (dst_params->src_start_x        ) >> 1;
					dst_start_y     = (dst_params->src_start_y        ) >> 1;

					//if((src_params->src_start_x & 0x01) && ~(src_params->src_width & 0x01))
					//	width           = (src_params->src_width       + 2) >> 1;
					//else
					//	width           = (src_params->src_width       + 1) >> 1;					
					width           = (src_params->src_width  + 1 + (src_params->src_start_x & 0x01))  >> 1;
					height          = (src_params->src_height + 1 + (src_params->src_start_y & 0x01))  >> 1;

					real_pad_width  = (pad_width                   + 1) >> 1;
					real_pad_height = (pad_height                  + 1) >> 1;
				}

				num_of_pixel = 1;
				break;
#if 0				
			case YC422 : // 8bit
				// y plane
				if(plane_count == 0)
				{
					// just use default size
				}
				else // cb cr
				{
					src_full_width  = (src_params->src_full_width  + 1) >> 1;
					src_full_height =  src_params->src_full_height;
					src_start_x     = (src_params->src_start_x        ) >> 1;
					src_start_y     =  src_params->src_start_y;
					
					dst_full_width  = (dst_params->src_full_width  + 1) >> 1;
					dst_full_height =  dst_params->src_full_height;
					dst_start_x     = (dst_params->src_start_x        ) >> 1;
					dst_start_y     =  dst_params->src_start_y;
					
					//if((src_params->src_start_x & 0x01) && ~(src_params->src_width & 0x01))
					//	width           = (src_params->src_width       + 2) >> 1;
					//else
					//	width           = (src_params->src_width       + 1) >> 1;					
					width           = (src_params->src_width  + 1 + (src_params->src_start_x & 0x01))  >> 1;
					height          =  src_params->src_height;

					real_pad_width  = (pad_width                   + 1) >> 1;
					real_pad_height =  pad_height;
				}

				num_of_pixel = 1;
				break;
#endif
			case V4L2_PIX_FMT_YUYV: // 16bit
				src_full_width  = src_params->src_full_width      << 1;
				src_full_height = src_params->src_full_height;
				src_start_x     = src_params->src_start_x         << 1;
				src_start_y     = src_params->src_start_y;				

				dst_full_width  = dst_params->src_full_width      << 1;
				dst_full_height = dst_params->src_full_height;
				dst_start_x     = dst_params->src_start_x         << 1;
				dst_start_y     = dst_params->src_start_y;				

				width           = src_params->src_width           << 1;
				height          = src_params->src_height;
				real_pad_width  = pad_width                       << 1;
				real_pad_height = pad_height;

				num_of_pixel = 2;
				break;

			case V4L2_PIX_FMT_RGB565: // 16bit
				src_full_width  = src_params->src_full_width       << 1;
				src_full_height = src_params->src_full_height;
				src_start_x     = src_params->src_start_x          << 1;
				src_start_y     = src_params->src_start_y;
			
				dst_full_width  = dst_params->src_full_width       << 1;
				dst_full_height = dst_params->src_full_height;
				dst_start_x     = dst_params->src_start_x          << 1;
				dst_start_y     = dst_params->src_start_y;				

				width           = src_params->src_width            << 1;
				height          = src_params->src_height;
				real_pad_width  = pad_width                        << 1;
				real_pad_height = pad_height;

				num_of_pixel = 2;
				break;

			case V4L2_PIX_FMT_RGB32 : // 32bit
				src_full_width  = src_params->src_full_width       << 2;
				src_full_height = src_params->src_full_height;
				src_start_x     = src_params->src_start_x          << 2;
				src_start_y     = src_params->src_start_y;

				dst_full_width  = dst_params->src_full_width       << 2;
				dst_full_height = dst_params->src_full_height;
				dst_start_x     = dst_params->src_start_x          << 2;
				dst_start_y     = dst_params->src_start_y;				

				width           = src_params->src_width            << 2;
				height          = src_params->src_height;
				real_pad_width  = pad_width                        << 2;
				real_pad_height = pad_height;

				num_of_pixel = 4;
				break;

			default :
				LOGE("%s::improper operation(color space : %d) \n",
					 __func__, src_params->src_color_space);
				return -1;
				// just use default size
				break;
		}

		current_src_addr = previous_src_addr;
		current_dst_addr = previous_dst_addr;
	
		current_src_addr += (src_full_width * src_start_y);
		current_dst_addr += (dst_full_width * dst_start_y);

		///////////
		// inner side and right side pad 
		src_temp_index = src_start_x + width - num_of_pixel;
		dst_temp_index = dst_start_x + width;

		for(y = 0; y < height; y++)
		{
			/////////////
			// copy valid inner side
			memcpy(current_dst_addr + dst_start_x, current_src_addr + src_start_x, width);
						
			///////////
			// right side pad
			memcpy(current_dst_addr + dst_temp_index, current_src_addr + src_temp_index, real_pad_width);
			/*
			for(x = 0; x < real_pad_width; x++)
				current_dst_addr[dst_temp_index + x] = current_src_addr[src_temp_index];
		*/
					current_src_addr += src_full_width;
					current_dst_addr += dst_full_width;
				}

		// we are here on 2 x 2
		// 0 0 0    0 0 0
		// 0 0 0    0 0 0
		// 1 0 0    1 0 0
		
		// kcoolsw : we don't have height constrain.. 2009.5.22
		// kcoolsw : we have height constrain........ 2009.8.5

		///////////
		// bottom side pad
		current_src_addr -= src_full_width;
		// we are here on 2 x 2
		// 0 0 0   0 0 0
		// 1 0 0   0 0 0
		// 0 0 0   1 0 0

		for(y = 0; y < real_pad_height; y++)
		{
			memcpy(current_dst_addr + dst_start_x, current_src_addr + src_start_x, width);
			current_dst_addr += dst_full_width;
		}
		// we are here on 2 x 2
		// 0 0 0   0 0 0
		// 1 0 0   0 0 0
		// 0 0 0   0 0 0
		//         1

		///////////
		// right bottom side pad
		current_src_addr += (src_start_x + width - num_of_pixel);
		current_dst_addr -= (dst_full_width * real_pad_height);
		current_dst_addr += (dst_start_x + width);
		// we are here on 2 x 2
		// 0 0 0   0 0 0
		// 0 1 0   0 0 0
		// 0 0 0   0 0 1

		memcpy(last_pixel, current_src_addr, num_of_pixel);
		
		for(y = 0; y < real_pad_height; y++)
		{
			for(x = 0; x < real_pad_width; x += num_of_pixel)
			{
				for(i = 0; i < num_of_pixel; i++)
					*current_dst_addr++ = last_pixel[i];				
			}
			current_dst_addr += (dst_full_width - real_pad_width);
		}
		
		switch(src_params->src_color_space)
		{
			case V4L2_PIX_FMT_RGB565:
			case V4L2_PIX_FMT_RGB32:
			case V4L2_PIX_FMT_YUYV: 
				// these guys copy full plane
				return 0;
				break;

			default :
				break;			
		}
		
		src_frame_size = src_full_width * src_full_height;
		dst_frame_size = dst_full_width * dst_full_height;

		previous_src_addr += (src_frame_size);
		previous_dst_addr += (dst_frame_size);
	}


	return 0;
}

// this func assumes YUV420, YUV422, RGB16, RGB24
int S3c2DRender::CopyPPDstframe(unsigned char * src_virt_addr, s3c_pp_params_t * src_params, 
                                unsigned char * dst_virt_addr, s3c_pp_params_t * dst_params,
                                int pad_width, int pad_height)
{
	int plane_count;
	unsigned int x;
	unsigned int y;	

	unsigned int src_frame_size = 0;
	unsigned int dst_frame_size = 0;
	unsigned int temp_value = 0;
	
	unsigned char * current_src_addr  = src_virt_addr;
	unsigned char * current_dst_addr  = dst_virt_addr;
	unsigned char * previous_src_addr = src_virt_addr;
	unsigned char * previous_dst_addr = dst_virt_addr;
	
	unsigned int src_full_width  = src_params->dst_full_width;
	unsigned int src_full_height = src_params->dst_full_height;
	unsigned int src_start_x     = src_params->dst_start_x;
	unsigned int src_start_y     = src_params->dst_start_y;
	
	unsigned int dst_full_width  = dst_params->dst_full_width;
	unsigned int dst_full_height = dst_params->dst_full_height;
	unsigned int dst_start_x     = dst_params->dst_start_x;
	unsigned int dst_start_y     = dst_params->dst_start_y;
	
	unsigned int width           = dst_params->dst_width;
	unsigned int height          = dst_params->dst_height;
		
	for(plane_count = 0; plane_count < 3; plane_count++)
	{
		switch(src_params->dst_color_space)
		{
			case V4L2_PIX_FMT_YUV420:
				
				if(plane_count == 0) // y plane
				{
					// just use default size
				}
				else // cb cr
				{
					src_full_width  = (src_params->dst_full_width  + 1) >> 1;
					src_full_height = (src_params->dst_full_height + 1) >> 1;
					src_start_x     = (src_params->dst_start_x        ) >> 1;
					src_start_y     = (src_params->dst_start_y        ) >> 1;
					
					dst_full_width  = (dst_params->dst_full_width  + 1) >> 1;
					dst_full_height = (dst_params->dst_full_height + 1) >> 1;
					dst_start_x     = (dst_params->dst_start_x        ) >> 1;
					dst_start_y     = (dst_params->dst_start_y        ) >> 1;
					
					//if((dst_params->dst_start_x & 0x01) && ~(dst_params->dst_width & 0x01))
					//	width           = (dst_params->dst_width       + 2) >> 1;
					//else
					//	width           = (dst_params->dst_width       + 1) >> 1;					
					width           = (dst_params->dst_width  + 1 + (dst_params->dst_start_x & 0x01))  >> 1;
					height          = (dst_params->dst_height + 1 + (dst_params->dst_start_y & 0x01))  >> 1;
				}						
				break;
#if 0				
			case YC422 :
				if(plane_count == 0) // y plane
				{
					// just use default size
				}
				else // cb cr
				{
					src_full_width  = (src_params->dst_full_width  + 1) >> 1;
					src_full_height =  src_params->dst_full_height;
					src_start_x     = (src_params->dst_start_x        ) >> 1;
					src_start_y     =  src_params->dst_start_y;
					
					dst_full_width  = (dst_params->dst_full_width  + 1) >> 1;
					dst_full_height =  dst_params->dst_full_height;
					dst_start_x     = (dst_params->dst_start_x        ) >> 1;
					dst_start_y     =  dst_params->dst_start_y;
					
					//if((dst_params->dst_start_x & 0x01) && ~(dst_params->dst_width & 0x01))
					//	width           = (dst_params->dst_width       + 2) >> 1;
					//else
					//	width           = (dst_params->dst_width       + 1) >> 1;
					width           = (dst_params->dst_width  + 1 + (dst_params->dst_start_x & 0x01))  >> 1;
					height          =  dst_params->dst_height;
				}
				break;
#endif

			case V4L2_PIX_FMT_YUYV:
				src_full_width  = src_params->dst_full_width       << 1;
				src_full_height = src_params->dst_full_height;
				src_start_x     = src_params->dst_start_x          << 1;
				src_start_y     = src_params->dst_start_y;				

				dst_full_width  = dst_params->dst_full_width       << 1;
				dst_full_height = dst_params->dst_full_height;
				dst_start_x     = dst_params->dst_start_x          << 1;
				dst_start_y     = dst_params->dst_start_y;				
				
				width           = dst_params->dst_width            << 1;
				height          = dst_params->dst_height;		
				break;

			case V4L2_PIX_FMT_RGB565: // 16bit
				src_full_width  = src_params->dst_full_width       << 1;
				src_full_height = src_params->dst_full_height;
				src_start_x     = src_params->dst_start_x          << 1;
				src_start_y     = src_params->dst_start_y;
				
				dst_full_width  = dst_params->dst_full_width       << 1;
				dst_full_height = dst_params->dst_full_height;
				dst_start_x     = dst_params->dst_start_x          << 1;
				dst_start_y     = dst_params->dst_start_y;				

				width           = dst_params->dst_width            << 1;
				height          = dst_params->dst_height;				
				break;

			case V4L2_PIX_FMT_RGB32: // 32bit
				src_full_width  = src_params->dst_full_width       << 2;
				src_full_height = src_params->dst_full_height;
				src_start_x     = src_params->dst_start_x          << 2;
				src_start_y     = src_params->dst_start_y;
								
				dst_full_width  = dst_params->dst_full_width       << 2;
				dst_full_height = dst_params->dst_full_height;
				dst_start_x     = dst_params->dst_start_x          << 2;
				dst_start_y     = dst_params->dst_start_y;

				width           = dst_params->dst_width            << 2;
				height          = dst_params->dst_height;
				break;
			
			default :
				LOGE("%s::improper operation(color space : %d) \n",
					 __func__, src_params->dst_color_space);
				return -1;
				break;			
		}
		
		current_src_addr = previous_src_addr;
		current_dst_addr = previous_dst_addr;
		
		current_src_addr += (src_full_width * src_start_y);
		current_dst_addr += (dst_full_width * dst_start_y);

		if(width == src_full_width && src_full_width == dst_full_width)
			memcpy(current_dst_addr, current_src_addr, width * height);
		else
		{	
			// strip -> copy inner side
			for(y = 0; y < height; y++)
			{
				/////////////
				// copy valid inner side
				memcpy(current_dst_addr + dst_start_x, current_src_addr + src_start_x, width);
				
				current_src_addr += src_full_width;
				current_dst_addr += dst_full_width;
			}
		}

		switch(src_params->dst_color_space)
		{
			case V4L2_PIX_FMT_RGB565:
			case V4L2_PIX_FMT_RGB32:
			case V4L2_PIX_FMT_YUYV:
				// these guys copy full plane
				return 0;
				break;

			default :
				break;
		}

		src_frame_size = src_full_width * src_full_height;
		dst_frame_size = dst_full_width * dst_full_height;

		previous_src_addr += (src_frame_size);
		previous_dst_addr += (dst_frame_size);
	}

	return 0;
}

int S3c2DRender::OpenG2D(void)
{
	#define S3C_G2D_DEV_NAME  "/dev/s3c-g2d"
	
	if(m_g2d.dev_fd == 0)
		m_g2d.dev_fd = open(S3C_G2D_DEV_NAME, O_RDWR);

	if(m_g2d.dev_fd < 0)
	{
		LOGE("%s::open(%s) fail\n", __func__, S3C_G2D_DEV_NAME);
		return -1;
	}

	return 0;
}

int S3c2DRender::CloseG2D(void)
{
	if(m_g2d.dev_fd != 0)
		close(m_g2d.dev_fd);
	m_g2d.dev_fd = 0;
	
	return 0;
}

inline int S3c2DRender::DoG2D(unsigned int src_phys_addr, s3c_img *src_img, s3c_rect *src_rect, int src_flag_android_order,
                              unsigned int dst_phys_addr, s3c_img *dst_img, s3c_rect *dst_rect, int dst_flag_android_order,
                              int rotate_value, int alpha_value)
{
	s5p_g2d_params params;
	
	int 		bpp_src = 0;
	int 		bpp_dst = 0;
	int 		alpha_mode = 1;

	if(!(src_phys_addr && dst_phys_addr))
		return -1;
	
	params.src_base_addr   = src_phys_addr;
	params.src_full_width  = src_img->width;
	params.src_full_height = src_img->height;

	params.src_start_x     = src_rect->x;
	params.src_start_y     = src_rect->y;
	params.src_work_width  = src_rect->w;
	params.src_work_height = src_rect->h;
	params.src_select      = G2D_NORM_MODE;
		
	params.dst_base_addr   = dst_phys_addr;
	params.dst_full_width  = dst_img->width;
	params.dst_full_height = dst_img->height;

	params.dst_start_x     = dst_rect->x;
	params.dst_start_y     = dst_rect->y;	
	params.dst_work_width  = dst_rect->w;
	params.dst_work_height = dst_rect->h;
	params.dst_select = G2D_NORM_MODE;
		
	//initialize clipping window
	params.cw_x1 = 0;
	params.cw_y1 = 0;
	params.cw_x2 = params.dst_full_width;
	params.cw_y2 = params.dst_full_height;
	bpp_src = ColorFormatCopybit2G2D(src_img->format, src_flag_android_order);
	bpp_dst = ColorFormatCopybit2G2D(dst_img->format, dst_flag_android_order);	

	if(bpp_src == -1 || bpp_dst == -1)
	{
		LOGE("%s::ColorFormatCopybit2G2D(src_img->format(%d of %d)/dst_img->format(%d of %d)) fail\n",
		          __func__,
		          src_img->format, src_flag_android_order,
		          dst_img->format, dst_flag_android_order);
		return -1;
	}

	params.src_colorfmt = (G2D_COLOR_FMT)bpp_src;
	params.dst_colorfmt = (G2D_COLOR_FMT)bpp_dst;	

	/*
	// kcoolsw
	if(255 <= alpha_value && !alpha_mode)
	{
		params.alpha_mode    = 0;
		params.alpha_val     = 0;
		//LOGI("Zero alpha alpha_value = %d\n",alpha_value);
	}
	else
	{
		params.alpha_mode     = 1;
		params.alpha_val      = alpha_value;
	}
	*/
	params.alpha_mode     = 1;
	params.alpha_val      = alpha_value;
	params.color_key_mode = 0;
	params.color_key_val  = 0;
	
//	LOGD ("%s:: w=%d, h=%d, color=%d\n", params.dst_work_width, params.dst_work_height, params.bpp_dst);

	if(ioctl(m_g2d.dev_fd, rotate_value, &params) < 0)
	{
		LOGE("%s::S3C_G2D_ROTATOR_%d fail\n", __func__, rotate_value);
		return -1;
	}

	return 0;
}

int S3c2DRender::CopyG2DFrame(unsigned char * src_virt_addr,
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

		// (100311 / kcoolsw ) support 24bpp & optimization
		switch(g2d_color_space)
		{
			case G2D_RGBA_8888 :
			case G2D_RGBX_8888 :
			case G2D_ARGB_8888 :
			case G2D_XRGB_8888 :
			case G2D_BGRA_8888 :
			case G2D_BGRX_8888 :
			case G2D_ABGR_8888 :
			case G2D_XBGR_8888 :
			case G2D_RGB_888   :  // mem size is same with RGBA
			case G2D_BGR_888   :  // mem size is same with BGRA
				// 32bit
				full_frame_size = ((full_width * full_height) << 2);
				break;
			
			case G2D_RGB_565   :
			case G2D_BGR_565   :
			case G2D_RGBA_5551 :
			case G2D_ARGB_5551 :
			case G2D_RGBA_4444 :
			case G2D_ARGB_4444 :
			default:
				// 16bit
				full_frame_size = ((full_width * full_height) << 1);
				break;
		}

		memcpy(dst_virt_addr, src_virt_addr, full_frame_size);
	}
	else
	{
		// (100311 / kcoolsw ) support 24bpp & optimization
		switch(g2d_color_space)
		{
			case G2D_RGBA_8888 :
			case G2D_RGBX_8888 :
			case G2D_ARGB_8888 :
			case G2D_XRGB_8888 :
			case G2D_BGRA_8888 :
			case G2D_BGRX_8888 :
			case G2D_ABGR_8888 :
			case G2D_XBGR_8888 :
			case G2D_RGB_888   :  // mem size is same with RGBA
			case G2D_BGR_888   :  // mem size is same with BGRA
				// 32bit
				real_full_width  = full_width  << 2;
				real_full_height = full_height << 2;
				real_start_x     = start_x     << 2;
				real_width       = width       << 2;
				break;
			
			case G2D_RGB_565   :
			case G2D_BGR_565   :
			case G2D_RGBA_5551 :
			case G2D_ARGB_5551 :
			case G2D_RGBA_4444 :
			case G2D_ARGB_4444 :
			default:
				// 16bit
				real_full_width  = full_width  << 1;
				real_full_height = full_height << 1;
				real_start_x     = start_x     << 1;
				real_width       = width       << 1;
				break;
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


inline int S3c2DRender::CopyRgb(s3c_img *img, s3c_rect *rect,
                                unsigned char * src_virt_addr,
                                unsigned char * dst_virt_addr,
                                int flag_android_order)
{
	int g2d_color_space = ColorFormatCopybit2G2D(img->format, flag_android_order);
	int rotate_value    = RotateValueCopybit2G2D(m_flags);

	if(g2d_color_space < 0)
	{
		LOGE("%s::ColorFormatCopybit2G2D(%d, %d) fail \n", __func__, img->format, flag_android_order);
		return -1;
	}
	if(CopyG2DFrame(src_virt_addr, dst_virt_addr,
	                img->width, img->height, rect->x, rect->y, rect->w, rect->h,
	                rotate_value, g2d_color_space) < 0)
	{
		LOGE("%s::CopyG2DFrame fail \n", __func__);
		return -1;
	}
	return 0;
}

int S3c2DRender::CopyG2DFrame_2dworkaround(unsigned char * src_virt_addr,
                              unsigned char * dst_virt_addr,
                              unsigned int full_width, unsigned int full_height,
                              unsigned int src_start_x,    unsigned int src_start_y,
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
	unsigned int real_src_start_x;
	unsigned int real_width;
						
	if(full_width == width && full_height == height)
	{
		unsigned int full_frame_size  = 0;

		// (100311 / kcoolsw ) support 24bpp & optimization
		switch(g2d_color_space)
		{
			case G2D_RGBA_8888 :
			case G2D_RGBX_8888 :
			case G2D_ARGB_8888 :
			case G2D_XRGB_8888 :
			case G2D_BGRA_8888 :
			case G2D_BGRX_8888 :
			case G2D_ABGR_8888 :
			case G2D_XBGR_8888 :
			case G2D_RGB_888   :  // mem size is same with RGBA
			case G2D_BGR_888   :  // mem size is same with BGRA
				// 32bit
				full_frame_size = ((full_width * full_height) << 2);
				break;
			
			case G2D_RGB_565   :
			case G2D_BGR_565   :
			case G2D_RGBA_5551 :
			case G2D_ARGB_5551 :
			case G2D_RGBA_4444 :
			case G2D_ARGB_4444 :
			default:
				// 16bit
				full_frame_size = ((full_width * full_height) << 1);
				break;
		}

		memcpy(dst_virt_addr, src_virt_addr, full_frame_size);
	}
	else
	{
		// (100311 / kcoolsw ) support 24bpp & optimization
		switch(g2d_color_space)
		{
			case G2D_RGBA_8888 :
			case G2D_RGBX_8888 :
			case G2D_ARGB_8888 :
			case G2D_XRGB_8888 :
			case G2D_BGRA_8888 :
			case G2D_BGRX_8888 :
			case G2D_ABGR_8888 :
			case G2D_XBGR_8888 :
			case G2D_RGB_888   :  // mem size is same with RGBA
			case G2D_BGR_888   :  // mem size is same with BGRA
				// 32bit
				real_full_width  = full_width  << 2;
				real_full_height = full_height << 2;
				real_src_start_x     = src_start_x     << 2;
				real_start_x     = start_x     << 2;
				real_width       = width       << 2;
				break;
			
			case G2D_RGB_565   :
			case G2D_BGR_565   :
			case G2D_RGBA_5551 :
			case G2D_ARGB_5551 :
			case G2D_RGBA_4444 :
			case G2D_ARGB_4444 :
			default:
				// 16bit
				real_full_width  = full_width  << 1;
				real_full_height = full_height << 1;
				real_start_x     = start_x     << 1;
				real_src_start_x     = src_start_x     << 1;
				real_width       = width       << 1;
				break;
		}


		current_src_addr += (real_full_width * src_start_y);
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
				       current_src_addr + real_src_start_x,
				       real_width);

				current_src_addr += real_full_width;
				current_dst_addr += real_full_width;
			}
		}
	}

	return 0;
}

inline int S3c2DRender::CopyRgb_2dworkaround(s3c_img *img, s3c_rect *src_rect, s3c_rect *rect,
                                unsigned char * src_virt_addr,
                                unsigned char * dst_virt_addr,
                                int flag_android_order)
{
	int g2d_color_space = ColorFormatCopybit2G2D(img->format, flag_android_order);
	int rotate_value    = RotateValueCopybit2G2D(m_flags);

	if(g2d_color_space < 0)
	{
		LOGE("%s::ColorFormatCopybit2G2D(%d, %d) fail \n", __func__, img->format, flag_android_order);
		return -1;
	}
	if(CopyG2DFrame_2dworkaround(src_virt_addr, dst_virt_addr,
	                img->width, img->height, src_rect->x, src_rect->y, rect->x, rect->y, rect->w, rect->h,
	                rotate_value, g2d_color_space) < 0)
	{
		LOGE("%s::CopyG2DFrame_2dworkaround fail \n", __func__);
		return -1;
	}
	return 0;
}

inline void S3c2DRender::MakeMidImgRect(s3c_img * dst_img, s3c_rect * dst_rect, 
                                        s3c_img * mid_img, s3c_rect * mid_rect,
                                        int rotate_value)
{
	// (100311 / kcoolsw ) support 24bpp & optimization
	switch(dst_img->format)
	{
		case COPYBIT_FORMAT_RGBA_8888:
		case COPYBIT_FORMAT_BGRA_8888:
			mid_img->format = COPYBIT_FORMAT_RGBX_8888;
			break;

		default:
			mid_img->format    = dst_img->format;
			break;
	}

	mid_img->base      = dst_img->base;
	mid_img->offset    = dst_img->offset;
	mid_img->memory_id = dst_img->memory_id;
	mid_img->flag      = dst_img->flag;
	
	mid_rect->x = 0;
	mid_rect->y = 0;

	if(   rotate_value == S3C_G2D_ROTATOR_90
	   || rotate_value == S3C_G2D_ROTATOR_270)
	{
		mid_img->width  = dst_img->height;
		mid_img->height = dst_img->width;
		mid_rect->w     = dst_rect->h;
		mid_rect->h     = dst_rect->w;
	}
	else
	{
		mid_img->width  = dst_img->width;
		mid_img->height = dst_img->height;
		mid_rect->w     = dst_rect->w;
		mid_rect->h     = dst_rect->h;
	}
}

inline void S3c2DRender::SetS3cPPParams(s3c_pp_params_t *params, 
                   s3c_img *src_img, s3c_rect *src_rect, int src_color_space, unsigned int src_phys_addr, 
                   s3c_img *dst_img, s3c_rect *dst_rect, int dst_color_space, unsigned int dst_phys_addr,
                   int out_path, int workaround)
{

	PPSrcDstWidthHeightCheck(src_img, src_rect, dst_img, dst_rect, src_color_space, dst_color_space);
    
	   
	params->src_full_width         = src_img->width;
	params->src_full_height        = src_img->height;
	params->src_height             = src_rect->h;
	params->src_start_x            = src_rect->x;
	params->src_start_y            = src_rect->y;
	params->src_color_space        = src_color_space;
	params->src_buf_addr_phy_rgb_y = src_phys_addr;

	params->dst_full_width         = dst_img->width;
	params->dst_full_height        = dst_img->height;
	params->dst_height             = dst_rect->h;
	params->dst_start_x            = dst_rect->x;
	params->dst_start_y            = dst_rect->y;
	params->dst_color_space        = dst_color_space;
	params->dst_buf_addr_phy_rgb_y = dst_phys_addr;
	params->src_width              = src_rect->w;
	params->dst_width              = dst_rect->w;

//	params->out_path               = (s3c_pp_out_path_t)out_path; // DMA_ONESHOT;
#if 0
	if (workaround == 1)
	{
		params->src_width              = src_rect->w;
		params->dst_width              = dst_rect->w;
	}
	else
	{
		params->src_width              = WidthOfPP(src_color_space, src_rect->w);
		params->dst_width              = WidthOfPP(dst_color_space, dst_rect->w);
	}
#endif
}

inline void S3c2DRender::SetS3cPPParamsCbCr(s3c_pp_params_t *params)
{
	unsigned int src_frame_size = params->src_full_width * params->src_full_height;
	unsigned int dst_frame_size = src_frame_size;

	switch(params->src_color_space)
	{
		case V4L2_PIX_FMT_YUV420 :
			params->src_buf_addr_phy_cb = params->src_buf_addr_phy_rgb_y + (src_frame_size);
			params->src_buf_addr_phy_cr = params->src_buf_addr_phy_cb    + (src_frame_size >> 2);
			break;
			
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
			params->src_buf_addr_phy_cb = params->src_buf_addr_phy_rgb_y + (src_frame_size);
			params->src_buf_addr_phy_cr = 0;
			break;

		case V4L2_PIX_FMT_NV61 :	//have to check
		case V4L2_PIX_FMT_NV16 :	
			params->src_buf_addr_phy_cb = params->src_buf_addr_phy_rgb_y + (src_frame_size);
			params->src_buf_addr_phy_cr = 0;
			break;
			
		default:
			params->src_buf_addr_phy_cb = 0;
			params->src_buf_addr_phy_cr = 0;
			break;
	}
	if(   params->src_full_width  != params->dst_full_width
	   || params->src_full_height != params->dst_full_height)
	{
		dst_frame_size = params->dst_full_width * params->dst_full_height;
	}
	
	switch(params->dst_color_space)
	{
		case V4L2_PIX_FMT_YUV420 :
			params->dst_buf_addr_phy_cb = params->dst_buf_addr_phy_rgb_y + (dst_frame_size);
			params->dst_buf_addr_phy_cr = params->dst_buf_addr_phy_cb    + (dst_frame_size >> 2);
			break;

		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
			params->dst_buf_addr_phy_cb = params->dst_buf_addr_phy_rgb_y + (dst_frame_size);
			params->dst_buf_addr_phy_cr = 0;
			break;

		case V4L2_PIX_FMT_NV61 :	//have to check
		case V4L2_PIX_FMT_NV16 :	
			params->dst_buf_addr_phy_cb = params->dst_buf_addr_phy_rgb_y + (dst_frame_size);
			params->dst_buf_addr_phy_cr = 0;
			break;

		default:
			break;
	}
}

inline int S3c2DRender::ColorFormatCopybit2PP(int format)
{
#ifdef	RENDER_DEBUG
	LOGD("#######ColorFormatCopybit2PP::%d\n", format);
#endif 
	switch (format) 
	{
		case COPYBIT_FORMAT_RGBA_8888:                  return V4L2_PIX_FMT_RGB32;
		case COPYBIT_FORMAT_RGBX_8888:    			    return V4L2_PIX_FMT_RGB32;
		case COPYBIT_FORMAT_RGB_888  :    			    return V4L2_PIX_FMT_RGB32;
		case COPYBIT_FORMAT_RGB_565:                    return V4L2_PIX_FMT_RGB565;
		case COPYBIT_FORMAT_BGRA_8888:                  return V4L2_PIX_FMT_RGB32;
		case COPYBIT_FORMAT_RGBA_5551:    				return V4L2_PIX_FMT_RGB32;
		case COPYBIT_FORMAT_RGBA_4444:    				return V4L2_PIX_FMT_RGB32;	
		//case COPYBIT_FORMAT_YCbCr_422_SP:               return V4L2_PIX_FMT_NV16;
		case COPYBIT_FORMAT_YCbCr_422_SP:               return V4L2_PIX_FMT_NV61;
		case COPYBIT_FORMAT_CUSTOM_CbYCr_422_I:         return V4L2_PIX_FMT_UYVY; //Kamat
		case COPYBIT_FORMAT_CUSTOM_YCbCr_422_I:         return V4L2_PIX_FMT_YUYV;
		case COPYBIT_FORMAT_YCbCr_420_SP:               return V4L2_PIX_FMT_NV21;
		//case COPYBIT_FORMAT_YCbCr_420_SP:               return V4L2_PIX_FMT_NV12;
		case COPYBIT_FORMAT_YCbCr_422_I:                return V4L2_PIX_FMT_YUYV;   
		case COPYBIT_FORMAT_CUSTOM_YCbCr_420_SP:        return V4L2_PIX_FMT_NV12T;
		case COPYBIT_FORMAT_YCbCr_420_P:                return V4L2_PIX_FMT_YUV420;				

		default :
			LOGD("%s::not matched frame format : %d\n", __func__, format);
			break;
	}
	return -1;
}

inline int S3c2DRender::ColorFormatCopybit2G2D(int format, int flag_android_order)
{
#ifdef RENDER_DEBUG_2D
	LOGD("########### %s::%d \n", __func__, format);
#endif
	// (100311 / kcoolsw ) support 24bpp
	// To match color format among
	// android color-format-order (android\system\core\libpixelflinger\format.cpp)
	// FB      color-format-order (s3cfb.c)
	// g2d     color-format-order (g2d_dev.c)
	// The byte order is very different between android and hw.
	// So, Be cautious..

	switch(format)
	{
		case COPYBIT_FORMAT_RGBA_8888 :  
			if(flag_android_order == 1)
				return G2D_ABGR_8888;
			else
				return G2D_ARGB_8888;
			break;

		case COPYBIT_FORMAT_RGBX_8888 :
			if(flag_android_order == 1)
				return G2D_XBGR_8888;
			else
				return G2D_XRGB_8888;				
			break;

		case COPYBIT_FORMAT_RGB_888   :
			if(flag_android_order == 1)
				return G2D_BGR_888;
			else
				return G2D_RGB_888;
			break;

		// rgb is same..
		case COPYBIT_FORMAT_RGB_565   :
			/*
			if(flag_android_order == 1)
				return G2D_GBR_565;
			else
				return G2D_RGB_565;
			break;
			*/
			return G2D_RGB_565;
			break;

		case COPYBIT_FORMAT_BGRA_8888 :
			if(flag_android_order == 1)
				return G2D_ARGB_8888;
			else				
				return G2D_ABGR_8888;
			break;

		// be care of RGBA_5551..
		// android platform's bite order is RGBA
		// FB's               bite order is ARGB
		case COPYBIT_FORMAT_RGBA_5551 :
			if(flag_android_order == 1)
				return G2D_RGBA_5551;
			else				
				return G2D_ARGB_5551;
			break;

		case COPYBIT_FORMAT_RGBA_4444 :
			if(flag_android_order == 1)
				return G2D_RGBA_4444;
			else				
				return G2D_ARGB_4444;
			break;

		default :
			LOGE("%s::not matched frame format : %d \n", __func__, format);
			break;
	}
	return -1;
}

inline int S3c2DRender::RotateValueCopybit2G2D(unsigned char flags)
{
	int rotate_value  = S3C_G2D_ROTATOR_0;

	// ROT_270 = ROT_180|ROT_90 : in Transform.h
	switch(flags & 0x07)
	{
		case COPYBIT_TRANSFORM_ROT_90: 
			rotate_value = S3C_G2D_ROTATOR_90;		
			break;

		case COPYBIT_TRANSFORM_ROT_270: 
			rotate_value = S3C_G2D_ROTATOR_270;
			break;			

		case COPYBIT_TRANSFORM_ROT_180: 
			rotate_value = S3C_G2D_ROTATOR_180;
			break;

		case COPYBIT_TRANSFORM_FLIP_V: 
			rotate_value = S3C_G2D_ROTATOR_X_FLIP;
			break;

		case COPYBIT_TRANSFORM_FLIP_H: 
			rotate_value = S3C_G2D_ROTATOR_Y_FLIP;
			break;

		default :
			rotate_value = S3C_G2D_ROTATOR_0;
			break;
	}
	
	return rotate_value;
}


inline char *S3c2DRender::GetFormatString(int colorformat)
{
	switch(colorformat)
	{
		case COPYBIT_FORMAT_RGBA_8888: 
			return (char *)"RGBA_8888";
		case COPYBIT_FORMAT_RGBX_8888:
			return (char *)"RGBX_8888";
		case COPYBIT_FORMAT_RGB_888:
			return (char *)"RGB_888";
		case COPYBIT_FORMAT_BGRA_8888: 		
			return (char *)"BGRA_8888";
		case COPYBIT_FORMAT_RGB_565: 
			return (char *)"RGB_565";
		case COPYBIT_FORMAT_RGBA_5551: 
			return (char *)"RGBA_5551";
		case COPYBIT_FORMAT_RGBA_4444: 
			return (char *)"RGBA_4444";
		case COPYBIT_FORMAT_YCbCr_420_P: 
			return (char *)"YCbCr_420_P";
		case COPYBIT_FORMAT_YCbCr_422_P :
			return (char *)"YCbCr_422_P";
		case COPYBIT_FORMAT_YCbCr_420_SP: 
			return (char *)"YCbCr_420_SP";
		case COPYBIT_FORMAT_YCbCr_422_SP :
			return (char *)"YCbCr_422_SP";
		case COPYBIT_FORMAT_YCbCr_422_I  :
			return (char *)"YCbCr_422_I";
		default :  // hell.~~
			return NULL;
	}

}

inline unsigned int S3c2DRender::GetFrameSize(int colorformat, unsigned int width, unsigned int height)
{
	       unsigned int frame_size  = 0;
	static unsigned int planar_size = 0;
//	static unsigned int old_width   = 0;
//	static unsigned int old_height  = 0;

	switch(colorformat)
	{
		case COPYBIT_FORMAT_RGBA_8888:
		case COPYBIT_FORMAT_RGBX_8888:
		case COPYBIT_FORMAT_RGB_888  :
		case COPYBIT_FORMAT_BGRA_8888:

//			if(old_width != width || old_height != height)
				planar_size = width * height;

			frame_size = (planar_size << 2);
			break;

		case COPYBIT_FORMAT_YCbCr_420_P: 
		case COPYBIT_FORMAT_YCbCr_420_SP:
			
//			if(old_width != width || old_height != height)
				planar_size = width * height;

			//frame_size  = (width * height * 3) >> 1;
			frame_size  = ((planar_size << 1) + planar_size) >> 1;	
			break;

		case COPYBIT_FORMAT_RGB_565: 
		case COPYBIT_FORMAT_RGBA_5551: 
		case COPYBIT_FORMAT_RGBA_4444: 
		case COPYBIT_FORMAT_YCbCr_422_P: 
		case COPYBIT_FORMAT_YCbCr_422_SP :
		case COPYBIT_FORMAT_YCbCr_422_I  :

//			if(old_width != width || old_height != height)
				planar_size = width * height;

			frame_size = (planar_size << 1);
			break;

		default :  // hell.~~
			LOGE("%s::no matching source colorformat(%d), width(%d), height(%d) \n",
			     __func__, colorformat, width, height);
			frame_size = 0;
			break;
	}

//	old_width   = width;
//	old_height  = height;
	
	return frame_size;
}

inline unsigned int S3c2DRender::GetAlignedStartPosOfPP(unsigned int   phy_start_addr, int pp_color_format,
                                                  unsigned int   start_x,      unsigned int   start_y,
                                                  unsigned int   full_width,   unsigned int   full_height,
                                                  unsigned int * aligned_factor)
{
#ifdef S5P6442_EVT0
	unsigned int alinged_size = PP_YUV_ALIGNED_PHYS_ADDR; // 0x08

	switch(pp_color_format)
	{
		case V4L2_PIX_FMT_RGB565 :
		case V4L2_PIX_FMT_RGB32 :		
			alinged_size = PP_RGB_ALIGNED_PHYS_ADDR;
			break;
		case V4L2_PIX_FMT_YUYV :
			alinged_size = PP_YUYV_ALIGNED_PHYS_ADDR;
			break;
		default :
			break;
	}

	*aligned_factor = alinged_size;

	unsigned int gap = ((phy_start_addr + (full_width * start_y) + start_x) & (alinged_size - 1));

	if(gap == 0)
		return 0;
	else
		return (alinged_size - gap);
#else
	return 0;
#endif
}
						 
inline unsigned int S3c2DRender::GetFactorNumber(unsigned int num, unsigned int limit_num)
{
	unsigned int factor_number = 0;

	switch(limit_num)
	{
		case 2 :
			factor_number = ((num + (limit_num -1)) >> 1) << 1;
			break;

		case 4 :
			factor_number = ((num + (limit_num -1)) >> 2) << 2;
			break;

		case 8 :
			factor_number = ((num + (limit_num -1)) >> 3) << 3;
			break;

		case 16 :
			factor_number = ((num + (limit_num -1)) >> 4) << 4;
			break;

		case 32 :
			factor_number = ((num + (limit_num -1)) >> 5) << 5;
			break;

		default :
			factor_number = ((num + (limit_num -1)) / limit_num) * limit_num;
			break;
	}

	return factor_number;
}


inline unsigned int S3c2DRender::GetWidthOfPP(int pp_color_format, unsigned int number)
{

	unsigned int factor_number = number;

	//number     = 4; // ok
	//number     = 3; // pp_wait
	//number     = 2; // pp_wait
	//number     = 1;  // die

#ifdef S5P6442_EVT0
	switch(pp_color_format)
	{
		case V4L2_PIX_FMT_RGB565:

			// 원래 RGB16은 2의 배수조건이나,
			// PreScale_H(horizontal)_Ratio 로 인해서 4의 배수가 맞음
			/*
			if(factor_number & 0x01) // odd number
					factor_number = GetFactorNumber(factor_number, 2);
			*/
			if(factor_number & 0x03)
					factor_number = GetFactorNumber(factor_number, 4);

			break;

		case V4L2_PIX_FMT_YUV420:
		//case YC422:
			//8의배수는 뒤의 세 자리가 8의 배수거나 000이면 이 수는 8의 배수가 됩니다
			if(factor_number & 0x07)
				factor_number = GetFactorNumber(factor_number, 8);

			break;

		case V4L2_PIX_FMT_YUYV:
			if(factor_number & 0x03)
			factor_number = GetFactorNumber(factor_number, 4);

			break;

		default :
			break;
        }
#endif

	return factor_number;
}

inline unsigned int S3c2DRender::GetHeightOfPP(int pp_color_format, unsigned int number)
{	
	unsigned int factor_number = number;
	
#ifdef S5P6442_EVT0
	if(factor_number & 0x01) // odd number
		factor_number = GetFactorNumber(factor_number, PP_HEIGHT_STEP_VALUE);
#endif
	return factor_number;
}

inline void S3c2DRender::PPSrcDstWidthHeightCheck(s3c_img *src_img, s3c_rect *src_rect, s3c_img *dst_img, s3c_rect *dst_rect, int src_color_format, int dst_color_format)
{
	unsigned int factor_number;
	int pad, diff;

	if(src_rect->w < 16){
		factor_number = (16 + src_rect->w - 1)/src_rect->w;
		pad = ((factor_number * src_rect->w) - src_rect->w);
		if(src_rect->x >= pad){
			src_rect->x = src_rect->x - pad;
			src_rect->w = src_rect->w + pad;
		}
		else if (src_rect->x + src_rect->w + pad <= src_img->width){
			src_rect->w = src_rect->w + pad;
		}
		else{
			diff = pad - src_rect->x;
			src_rect->x = 0;
			src_rect->w = ((src_rect->w + diff) > src_img->width) ? src_img->width: (src_rect->w + diff);
			}

		pad = ((factor_number * dst_rect->w) - dst_rect->w);

		if(dst_rect->x >= pad){
			dst_rect->x = dst_rect->x - pad;
			dst_rect->w = dst_rect->w + pad;
		}
		else if (dst_rect->x + dst_rect->w + pad <= dst_img->width){
			dst_rect->w = dst_rect->w + pad;
		}
		else{
			diff = pad - dst_rect->x;
			dst_rect->x = 0;
			dst_rect->w = ((dst_rect->w + diff) > dst_img->width) ? dst_img->width: (dst_rect->w + diff);
			}
	}
		
	if(src_rect->h < 8)
	{
		factor_number = (8 + src_rect->h - 1)/src_rect->h;
		pad = ((factor_number * src_rect->h) - src_rect->h);
		
		if(src_rect->y >= pad){
			src_rect->y = src_rect->y - pad;
			src_rect->h = src_rect->h + pad;
		}
		else if (src_rect->y + src_rect->h + pad <= src_img->height){
			src_rect->h = src_rect->h + pad;
		}
		else{
			diff = pad - src_rect->y;
			src_rect->y = 0;
			src_rect->h = ((src_rect->h + diff) > src_img->height) ? src_img->height: (src_rect->h + diff);
			}

		pad = ((factor_number * dst_rect->h) - dst_rect->h);
		
		if(dst_rect->y >= pad){
			dst_rect->y = dst_rect->y - pad;
			dst_rect->h = dst_rect->h + pad;
		}
		else if (dst_rect->y + dst_rect->h + pad <= dst_img->height){
			dst_rect->h = dst_rect->h + pad;
		}
		else{
			diff = pad - dst_rect->y;
			dst_rect->y = 0;
			dst_rect->h = ((dst_rect->h + diff) > dst_img->height) ? dst_img->height: (dst_rect->h + diff);
			}				
	}
}

inline unsigned int S3c2DRender::WidthOfPP(int pp_color_format, unsigned int number)
{
	unsigned int new_number = number;
	
#ifdef S5P6442_EVT0
	switch(pp_color_format)
	{
		case V4L2_PIX_FMT_RGB565:
			new_number = (number - (number & 0x07));
			//new_number = (number - (number & 0x01));
			break;
		case V4L2_PIX_FMT_YUV420:
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:	
			//new_number = (number - (number % 0x10));
			new_number = (number - (number & 0x0F));
			break;
		case V4L2_PIX_FMT_YUYV:
			//new_number = (number - (number % 0x02));
			new_number = (number - (number & 0x01));
			break;
			
		default :
			break;
	}
#endif

	return new_number;
}

inline unsigned int S3c2DRender::HeightOfPP(int pp_color_format, unsigned int number)
{
#ifdef S5P6442_EVT0
	switch(pp_color_format)
	{
		case V4L2_PIX_FMT_RGB565:
		case V4L2_PIX_FMT_YUYV:
			return number;
			break;

		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV12T:
		case V4L2_PIX_FMT_NV21:
		case V4L2_PIX_FMT_YUV420:
			return (number - (number & 0x01));
			break;

		default :
			return number;
			break;
        }
#endif
	return number;
}


#ifdef DITHERING_ON_CAMERA_PREVIEW_FRAME

int S3c2DRender::ditheringYuv2Rgb(unsigned char * src_data, unsigned char * dst_data,
                                  unsigned int width, unsigned int height,
                                  int color_format)
{
	       unsigned int a_plane_size  = 0;
	static unsigned int   plane_size  = 0;
	static unsigned int old_width     = 0;
	static unsigned int old_height    = 0;
	
	SCMN_IMGB dither;
	SCMN_IMGB dither2;

	switch(color_format)
	{
		case COPYBIT_FORMAT_YCbCr_420_P :

			if(old_width != width || old_height != height)		
				plane_size = width * height;
			a_plane_size = plane_size;

			dither.w[0]               =  width;
			dither.w[1] = dither.w[2] = (width >> 1);
			dither.h[0]               =  height;
			dither.h[1] = dither.h[2] = (height >> 1);
			dither.s[0]               =  width;
			dither.s[1] = dither.s[2] = (width >> 1);
			
			dither.a[0] = src_data;
			dither.a[1] = src_data + a_plane_size;
			dither.a[2] = src_data + a_plane_size + (a_plane_size >> 2);

			dither.cs   = SCMN_CS_YUV420;
			
			dither2.w[0]                =  width;
			dither2.h[0]                =  height;
			dither2.s[0]                = (width << 1);
			
			dither2.a[0] = dst_data;
		
			dither2.cs = SCMN_CS_RGB565;
					
			break;

		case COPYBIT_FORMAT_YCbCr_422_P :

			if(old_width != width || old_height != height)
				a_plane_size = width * height;
			a_plane_size = plane_size;

			dither.w[0]                             =  width;
			dither.w[1] = dither.w[2]               = (width >> 1);
			dither.h[0] = dither.h[1] = dither.h[2] =  height;
			dither.s[0]                             =  width;
			dither.s[1] = dither.s[2]               = (width >> 1);

			dither.a[0] = src_data;
			dither.a[1] = src_data + a_plane_size;
			dither.a[2] = src_data + a_plane_size + (a_plane_size >> 1);

			dither.cs = SCMN_CS_YUV422;

			dither2.w[0]                =  width;
			dither2.h[0]                =  height;
			dither2.s[0]                = (width << 1);
			
			dither2.a[0] = dst_data;
		
			dither2.cs = SCMN_CS_RGB565;

			break;
			
		//case COPYBIT_FORMAT_RGB_565 :
		//case COPYBIT_FORMAT_YCbCr_420_SP :
		//case COPYBIT_FORMAT_YCbCr_422_SP :
		//case COPYBIT_FORMAT_YCbCr_422_I :
		default:

			return -1;
			break;
	}
	
	if(simgp_csc_dither(&dither, &dither2) < 0)
	{
		LOGE("%s::simgp_csc_dither() fail\n", __func__);
		return -1;
	}

	old_width  = width;
	old_height = height;

	return 0;
}

int S3c2DRender::createMem(mem_t * mem, unsigned int mem_size, int mem_type)
{
	int          flag_hw_bad      = 0;
	int          flag_new_malloc  = 0;
	int          real_memory_type = mem_type;
	unsigned int memory_size      = mem_size;	

	if(    (mem->type == MEMORY_TYPE_HW_MEM && mem_type == MEMORY_TYPE_SW_MEM)
		|| (mem->type == MEMORY_TYPE_SW_MEM && mem_type == MEMORY_TYPE_HW_MEM))
	{
		//LOGD("########## BUFFER CHANGE %d -> %d  \n", mem->type, mem_type);

		flag_new_malloc = 1;
	}
	
	//#define  MINIMUM_SIZE (0x40000) // 256 K
	#define  MINIMUM_SIZE (0x20000) // 128 K

	if(memory_size < MINIMUM_SIZE)
		memory_size = MINIMUM_SIZE;
	
	if(   ((mem->size < memory_size) && (mem->size != 0))
	   || (flag_new_malloc == 1))
	{
		switch(mem->type)
		{
			case MEMORY_TYPE_HW_MEM :
			{
				if(0 < mem->dev_fd)
				{
					CODEC_MEM_FREE_ARG codec_mem_free_arg;
					codec_mem_free_arg.u_addr = mem->mem_info.out_addr;

					if(ioctl(mem->dev_fd, IOCTL_CODEC_MEM_FREE, &codec_mem_free_arg) < 0)
					{
						LOGE("%s::IOCTL_CODEC_MEM_FREE fail \n", __func__);
						close(mem->dev_fd);
						mem->dev_fd = 0;
					}
				}
				break;
			}
			case MEMORY_TYPE_SW_MEM :
			{
				if(mem->virt_addr != NULL)
					free(mem->virt_addr);

				break;
			}
			default:
				break;
		}

		mem->size      = 0;
		mem->phys_addr = 0;
		mem->virt_addr = NULL;

		flag_new_malloc = 1;
	}

	switch(real_memory_type)
	{
		case MEMORY_TYPE_HW_MEM :
		{		
			if(mem->dev_fd == 0)
			{
				#define HW_MEM_DEV_NAME  "/dev/s3c-cmm"
			
				mem->dev_fd = open(HW_MEM_DEV_NAME, O_RDWR);		
				if(mem->dev_fd < 0)
				{
					LOGE("%s::open(%s) fail(%s)\n", __func__, HW_MEM_DEV_NAME, strerror(errno));
					mem->dev_fd = 0;

					real_memory_type = MEMORY_TYPE_SW_MEM;
				}

				mem->size      = 0;
				mem->phys_addr = 0;
				mem->virt_addr = NULL;

				flag_new_malloc  = 1;
			}
			break;
		}
		case MEMORY_TYPE_SW_MEM :
		{
			if(mem->size < memory_size)
				flag_new_malloc = 1;

			break;
		}
		default:
		{
			LOGE("%s::unmatched real_memory_type(%d)\n", __func__, real_memory_type);
			return -1;
			break;
		}
	}

	if(flag_new_malloc == 1)
	{
		mem->size = memory_size;

		//LOGD("########## NEW ALLOC %d~~ \n", memory_size);
		
		if(real_memory_type == MEMORY_TYPE_HW_MEM)
		{
			flag_hw_bad = 1;

			unsigned char	*cached_addr;
			unsigned char	*non_cached_addr;
			CODEC_GET_PHY_ADDR_ARG	codec_get_phy_addr_arg;

			// Mapping cacheable memory area
			cached_addr = (unsigned char *)mmap(0, CODEC_CACHED_MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem->dev_fd, 0);
			if(cached_addr == NULL) 
			{
				LOGE("%s::CMM driver buffer mapping failed\n", __func__);
				goto createMem_hw_fail;
			}

			// Mapping non-cacheable memory area
			non_cached_addr = (unsigned char *)mmap(0, CODEC_NON_CACHED_MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem->dev_fd, CODEC_CACHED_MEM_SIZE);
			if(non_cached_addr == NULL)
			{
				LOGE("%s::CMM driver buffer mapping failed\n", __func__);
				goto createMem_hw_fail;
			}

			mem->mem_info.buffSize               = memory_size;
			mem->mem_info.cached_mapped_addr     = (unsigned int)cached_addr;
			mem->mem_info.non_cached_mapped_addr = (unsigned int)non_cached_addr;
			mem->mem_info.cacheFlag              = 1; // 1: cacheable, 0: non-cacheable

			if (ioctl(mem->dev_fd, IOCTL_CODEC_MEM_ALLOC, &(mem->mem_info)) < 0)
			{
				LOGE("%s::IOCTL_CODEC_MEM_ALLOC fail \n", __func__);
				goto createMem_hw_fail;
			}

			flag_hw_bad = 2;

			// Get physical address
			codec_get_phy_addr_arg.u_addr = mem->mem_info.out_addr;
			if(ioctl(mem->dev_fd, IOCTL_CODEC_GET_PHY_ADDR, &codec_get_phy_addr_arg) < 0)
			{
				LOGE("%s::IOCTL_CODEC_GET_PHY_ADDR fail \n", __func__);
				goto createMem_hw_fail;
			}

			mem->phys_addr = (unsigned int)  codec_get_phy_addr_arg.p_addr;
			mem->virt_addr = (unsigned char*)codec_get_phy_addr_arg.u_addr;
			mem->type      = MEMORY_TYPE_HW_MEM;

			flag_hw_bad = 0;

createMem_hw_fail :

			if(flag_hw_bad != 0)
			{
				if(0 < mem->dev_fd)
				{
					if(flag_hw_bad == 2)
					{
						CODEC_MEM_FREE_ARG codec_mem_free_arg;
						codec_mem_free_arg.u_addr = mem->mem_info.out_addr;

						if(ioctl(mem->dev_fd, IOCTL_CODEC_MEM_FREE, &codec_mem_free_arg) < 0)
						{
							LOGE("%s::IOCTL_CODEC_MEM_FREE fail \n", __func__);
							return -1;
						}	
					}

					close(mem->dev_fd);
					mem->dev_fd = 0;
				}
				real_memory_type = MEMORY_TYPE_SW_MEM;
			}
		}

		if(real_memory_type == MEMORY_TYPE_SW_MEM)
		{
			if(mem->virt_addr != NULL)
			{
				//LOGD("%s::mem->virt_addr != NULL, so force free()\n", __func__);
				free(mem->virt_addr);
				mem->virt_addr = NULL;
			}
			mem->phys_addr = 0;
			mem->virt_addr = (unsigned char *)malloc(memory_size);

			if(mem->virt_addr == NULL)
			{
				mem->size = 0;
				LOGE("%s::malloc(%d) fail \n", __func__, memory_size);
				return -1;				
			}

			mem->type = MEMORY_TYPE_SW_MEM;
		}
	}

	return 0;
}

static int S3c2DRender::destroyMem(mem_t * mem)
{
	if(mem == NULL)
	{
		//LOGD("%s::mem == NULL \n", __func__);
		return 0;
	}

	switch(mem->type)
	{
		case MEMORY_TYPE_HW_MEM :
		{
			if(0 < mem->dev_fd)
			{
				//LOGD("########## FREE HW MEM~~ \n");

				CODEC_MEM_FREE_ARG codec_mem_free_arg;
				codec_mem_free_arg.u_addr = mem->mem_info.out_addr;

				if(ioctl(mem->dev_fd, IOCTL_CODEC_MEM_FREE, &codec_mem_free_arg) < 0)
					LOGE("%s::IOCTL_CODEC_MEM_FREE fail \n", __func__);
				
				close(mem->dev_fd);
				mem->dev_fd = 0;
			}
			break;
		}
		case MEMORY_TYPE_SW_MEM :
		{
			//LOGD("########## FREE SW MEM~~ \n");
			
			if(mem->virt_addr != NULL)
				free(mem->virt_addr);

			break;
		}
		default:
			LOGE("%s::unmatched mem->type(%d) fail \n", __func__, mem->type);
			break;
	}
	
	mem->virt_addr = NULL;
	mem->phys_addr = 0;
	mem->size      = 0;	

	return 0;
}

int S3c2DRender::flushMem(mem_t * mem)
{
	switch(mem->type)
	{
		case MEMORY_TYPE_HW_MEM :
		{
			if(0 < mem->dev_fd)
			{
				CODEC_CACHE_FLUSH_ARG	codec_cache_flush_arg;

				codec_cache_flush_arg.u_addr = mem->mem_info.out_addr;
				codec_cache_flush_arg.size   = mem->mem_info.buffSize;

				if(ioctl(mem->dev_fd, IOCTL_CODEC_CACHE_FLUSH, &codec_cache_flush_arg) < 0)
				{
					LOGE("%s::IOCTL_CODEC_CACHE_FLUSH fail \n", __func__);
					return -1;
				}
			}
			break;		
		}
		case MEMORY_TYPE_SW_MEM :
		default :
			break;
	}

	return 0;
}

#endif //DITHERING_ON_CAMERA_PREVIEW_FRAME

#if 0
inline unsigned int S3c2DRender::convertABGR2RGBA(int copybit_color_space, 
                                            unsigned char * src_addr, unsigned char *dst_addr,
                                            unsigned int width,       unsigned int height)
{
	unsigned int* src = (unsigned int*)(src_addr);
	unsigned int* dst = (unsigned int*)(dst_addr);
	
	unsigned int i = 0;
	unsigned int frame_size         =  (width * height);
	unsigned int aligned_frame_size =  frame_size >> 3 ;
	unsigned int remain_frame_size  =  frame_size  & 0x7;

	for(i = 0; i < aligned_frame_size; i++)
	{
		// 0
		*dst  =((*src>>24)&0xff)|((*src>>8)&0xff00)|((*src<<8)&0xff0000)|((*src<<24)&0xff000000);		//ABGR->RGBA
		src++; dst++;

		// 1
		*dst  =((*src>>24)&0xff)|((*src>>8)&0xff00)|((*src<<8)&0xff0000)|((*src<<24)&0xff000000);		//ABGR->RGBA
		src++; dst++;

		// 2
		*dst  =((*src>>24)&0xff)|((*src>>8)&0xff00)|((*src<<8)&0xff0000)|((*src<<24)&0xff000000);		//ABGR->RGBA
		src++; dst++;

		// 3
		*dst  =((*src>>24)&0xff)|((*src>>8)&0xff00)|((*src<<8)&0xff0000)|((*src<<24)&0xff000000);		//ABGR->RGBA
		src++; dst++;

		// 4
		*dst  =((*src>>24)&0xff)|((*src>>8)&0xff00)|((*src<<8)&0xff0000)|((*src<<24)&0xff000000);		//ABGR->RGBA
		src++; dst++;

		// 5
		*dst  =((*src>>24)&0xff)|((*src>>8)&0xff00)|((*src<<8)&0xff0000)|((*src<<24)&0xff000000);		//ABGR->RGBA
		src++; dst++;

		// 6
		*dst  =((*src>>24)&0xff)|((*src>>8)&0xff00)|((*src<<8)&0xff0000)|((*src<<24)&0xff000000);		//ABGR->RGBA
		src++; dst++;

		// 7
		*dst  =((*src>>24)&0xff)|((*src>>8)&0xff00)|((*src<<8)&0xff0000)|((*src<<24)&0xff000000);		//ABGR->RGBA
		src++; dst++;
	}

	for(i = (frame_size - remain_frame_size); i < frame_size; i++)
	{
		*dst  =((*src>>24)&0xff)|((*src>>8)&0xff00)|((*src<<8)&0xff0000)|((*src<<24)&0xff000000);		//ABGR->RGBA
		src++; dst++;
	}
	//dst[i]=(src[i]&0xff00ff00)|((src[i]>>16)&0xff)|((src[i]<<16)&0xff0000);									//ABGR->ARGB

	return 0;
}
#endif


inline void S3c2DRender::ResetPmemPartition(void)
{
	m_current_pmem = 0;
}

inline void S3c2DRender::GoNextPmemPartition(void)
{
	m_current_pmem = (m_current_pmem + 1) & PMEM_RENDER_PARTITION_NUM_SHIFT;
}

inline void S3c2DRender::GoPrevPmemPartition(void)
{
	m_current_pmem = (m_current_pmem - 1) & PMEM_RENDER_PARTITION_NUM_SHIFT;
}

inline int S3c2DRender::GetPmemPhysAddr(void)
{
	return m_pmem_render_phys_addrs[m_current_pmem];
}

inline unsigned char * S3c2DRender::GetPmemVirtAddr(void)
{
	return m_pmem_render_virt_addrs[m_current_pmem];
}

inline int S3c2DRender::CheckFrameSize(unsigned int frame_size, const char *func_name)
{
	if(frame_size == 0)
	{
		LOGE("%s::frame_size == 0 fail", func_name);
		return -1;
	}
	if(m_pmem_render_partition_size < frame_size)
	{
		LOGE("%s::FrameSize(%d) is too bigger than m_pmem_render_partition_size(%d)", func_name, frame_size, m_pmem_render_partition_size);
		return -1;
	}
	return 0;
}

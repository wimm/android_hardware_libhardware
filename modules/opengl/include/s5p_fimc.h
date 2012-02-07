/*
 * Test application for Samsung Camera Interface (FIMC) driver
 *
 * Jinsung Yang, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _S5P_FIMC_H_
#define _S5P_FIMC_H_

#include "videodev2.h"
/*
 * G E N E R A L S
 *
*/
#define MIN(x, y)		((x < y) ? x : y)

/*
 * P I X E L   F O R M A T   G U I D E
 *
 * The 'x' means 'DO NOT CARE'
 * The '*' means 'FIMC SPECIFIC'
 * For some fimc formats, we couldn't find equivalent format in the V4L2 FOURCC.
 *
 * FIMC TYPE	PLANES	ORDER		V4L2_PIX_FMT
 * ---------------------------------------------------------
 * RGB565	x	x		V4L2_PIX_FMT_RGB565
 * RGB888	x	x		V4L2_PIX_FMT_RGB24
 * YUV420	2	LSB_CBCR	V4L2_PIX_FMT_NV12
 * YUV420	2	LSB_CRCB	V4L2_PIX_FMT_NV21
 * YUV420	2	MSB_CBCR	V4L2_PIX_FMT_NV21X*
 * YUV420	2	MSB_CRCB	V4L2_PIX_FMT_NV12X*
 * YUV420	3	x		V4L2_PIX_FMT_YUV420
 * YUV422	1	YCBYCR		V4L2_PIX_FMT_YUYV
 * YUV422	1	YCRYCB		V4L2_PIX_FMT_YVYU
 * YUV422	1	CBYCRY		V4L2_PIX_FMT_UYVY
 * YUV422	1	CRYCBY		V4L2_PIX_FMT_VYUY*
 * YUV422	2	LSB_CBCR	V4L2_PIX_FMT_NV16*
 * YUV422	2	LSB_CRCB	V4L2_PIX_FMT_NV61*
 * YUV422	2	MSB_CBCR	V4L2_PIX_FMT_NV16X*
 * YUV422	2	MSB_CRCB	V4L2_PIX_FMT_NV61X*
 * YUV422	3	x		V4L2_PIX_FMT_YUV422P
 *
*/

/*
 * V 4 L 2   F I M C   E X T E N S I O N S
 *
*/
#define V4L2_PIX_FMT_YVYU		v4l2_fourcc('Y', 'V', 'Y', 'U')

/* FOURCC for FIMC specific */
#define V4L2_PIX_FMT_NV12X		v4l2_fourcc('N', '1', '2', 'X')
#define V4L2_PIX_FMT_NV21X		v4l2_fourcc('N', '2', '1', 'X')
#define V4L2_PIX_FMT_VYUY		v4l2_fourcc('V', 'Y', 'U', 'Y')
#define V4L2_PIX_FMT_NV16		v4l2_fourcc('N', 'V', '1', '6')
#define V4L2_PIX_FMT_NV61		v4l2_fourcc('N', 'V', '6', '1')
#define V4L2_PIX_FMT_NV16X		v4l2_fourcc('N', '1', '6', 'X')
#define V4L2_PIX_FMT_NV61X		v4l2_fourcc('N', '6', '1', 'X')

#define V4L2_PIX_FMT_NV12T    	v4l2_fourcc('T', 'V', '1', '2') /* 12  Y/CbCr 4:2:0 64x32 macroblocks */

/* CID extensions */
#define V4L2_CID_ROTATION		(V4L2_CID_PRIVATE_BASE + 0)
#define V4L2_CID_RESERVED_MEM_BASE_ADDR     (V4L2_CID_PRIVATE_BASE + 5)

/*
 * U S E R   D E F I N E D   T Y P E S
 *
*/

typedef unsigned int dma_addr_t;

struct fimc_buf {
	dma_addr_t	base[3];
 	size_t		length[3];
};

struct fimc_buffer {
	void	*virt_addr;
	void	*phys_addr;
	size_t	length;
};

struct yuv_fmt_list {
	const char 		*name;
	const char 		*desc;
	unsigned int 	fmt;
	int				bpp;
	int				planes;
};

struct img_offset {
    int y_h;
    int y_v;
    int cb_h;
    int cb_v;
    int cr_h;
    int cr_v;
};

// Structure type for IOCTL commands S3C_PP_SET_PARAMS, S3C_PP_SET_INPUT_BUF_START_ADDR_PHY, 
typedef	struct
{
	unsigned int full_width;			// Source Image Full Width (Virtual screen size)
	unsigned int full_height;			// Source Image Full Height (Virtual screen size)
	unsigned int start_x;				// Source Image Start width offset
	unsigned int start_y;				// Source Image Start height offset
	unsigned int width;					// Source Image Width
	unsigned int height;				// Source Image Height
	unsigned int buf_addr_phy_rgb_y;	// Base Address of the Source Image (RGB or Y): Physical Address
	unsigned int buf_addr_phy_cb;		// Base Address of the Source Image (CB Component) : Physical Address
	unsigned int buf_addr_phy_cr;		// Base Address of the Source Image (CR Component) : Physical Address
	unsigned int color_space;			// Color Space of the Source Image
} s5p_fimc_img_info;
//      S3C_PP_SET_INPUT_BUF_NEXT_START_ADDR_PHY, S3C_PP_SET_OUTPUT_BUF_START_ADDR_PHY.

typedef struct {

        unsigned int src_full_width;                    // Source Image Full Width (Virtual screen size)

        unsigned int src_full_height;                   // Source Image Full Height (Virtual screen size)

        unsigned int src_start_x;                       // Source Image Start width offset

        unsigned int src_start_y;                       // Source Image Start height offset

        unsigned int src_width;                         // Source Image Width

        unsigned int src_height;                        // Source Image Height

        unsigned int src_buf_addr_phy_rgb_y;            // Base Address of the Source Image (RGB or Y): Physical Address

        unsigned int src_buf_addr_phy_cb;               // Base Address of the Source Image (CB Component) : Physical Address

        unsigned int src_buf_addr_phy_cr;               // Base Address of the Source Image (CR Component) : Physical Address

        unsigned int src_next_buf_addr_phy_rgb_y;       // Base Address of Source Image (RGB or Y) to be displayed next time in FIFO_FREERUN Mode

        unsigned int src_next_buf_addr_phy_cb;          // Base Address of Source Image (CB Component) to be displayed next time in FIFO_FREERUN Mode

        unsigned int src_next_buf_addr_phy_cr;          // Base Address of Source Image (CR Component) to be displayed next time in FIFO_FREERUN Mode

        unsigned int src_color_space;              // Color Space of the Source Image



        unsigned int dst_full_width;                    // Destination Image Full Width (Virtual screen size)

        unsigned int dst_full_height;                   // Destination Image Full Height (Virtual screen size)

        unsigned int dst_start_x;                       // Destination Image Start width offset

        unsigned int dst_start_y;                       // Destination Image Start height offset

        unsigned int dst_width;                         // Destination Image Width

        unsigned int dst_height;                        // Destination Image Height

        unsigned int dst_buf_addr_phy_rgb_y;            // Base Address of the Destination Image (RGB or Y) : Physical Address

        unsigned int dst_buf_addr_phy_cb;               // Base Address of the Destination Image (CB Component) : Physical Address

        unsigned int dst_buf_addr_phy_cr;               // Base Address of the Destination Image (CR Component) : Physical Address

        unsigned int dst_color_space;              // Color Space of the Destination Image
	unsigned int dst_rotate;


     //   s3c_pp_out_path_t out_path;                     // output and run mode (DMA_ONESHOT or FIFO_FREERUN)

//        s3c_pp_scan_mode_t scan_mode;                   // INTERLACE_MODE, PROGRESSIVE_MODE

} s3c_pp_params_t;

typedef struct 
{
	s5p_fimc_img_info	src;
	s5p_fimc_img_info	dst;
} s5p_fimc_params_t;

typedef struct _s5p_fimc_t {
    int                dev_fd;
    struct fimc_buffer  in_buf;
    struct fimc_buffer  out_buf;

    s5p_fimc_params_t   params;

    int                 use_ext_out_mem;
    unsigned int        hw_ver;
}s5p_fimc_t;

//------------------------  functions for v4l2 ------------------------------//
int fimc_v4l2_set_src(int fd, s5p_fimc_img_info *src);
int fimc_v4l2_set_dst(int fd, s5p_fimc_img_info *dst, int rotation, unsigned int addr);
int fimc_v4l2_stream_on(int fd, enum v4l2_buf_type type);
int fimc_v4l2_queue(int fd, struct fimc_buf *fimc_buf);
int fimc_v4l2_dequeue(int fd);
int fimc_v4l2_stream_off(int fd);
int fimc_v4l2_clr_buf(int fd);
int fimc_handle_oneshot(int fd, struct fimc_buf *fimc_buf);
#endif

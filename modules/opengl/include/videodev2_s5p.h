#ifndef __VIDEODEV2_S5P_H__
#define __VIDEODEV2_S5P_H__

/* ------------- Input ------------------*/
// type
#define V4L2_INPUT_TYPE_MSDMA			3
#define V4L2_INPUT_TYPE_FIFO			4
// fmt

/* ------------- Output -----------------*/
// type
#define V4L2_OUTPUT_TYPE_MSDMA			4
#define V4L2_OUTPUT_TYPE_COMPOSITE		5
#define V4L2_OUTPUT_TYPE_SVIDEO			6
#define V4L2_OUTPUT_TYPE_YPBPR_INERLACED	7
#define V4L2_OUTPUT_TYPE_YPBPR_PROGRESSIVE	8
#define V4L2_OUTPUT_TYPE_RGB_PROGRESSIVE	9
#define V4L2_OUTPUT_TYPE_DIGITAL		10
// fmt

/* ------------- STD -------------------*/
#define V4L2_STD_PAL_BDGHI V4L2_STD_PAL_B|V4L2_STD_PAL_D|V4L2_STD_PAL_G|V4L2_STD_PAL_H|V4L2_STD_PAL_I

#define V4L2_STD_480P_60_16_9		((v4l2_std_id)0x04000000)
#define V4L2_STD_480P_60_4_3		((v4l2_std_id)0x05000000)
#define V4L2_STD_576P_50_16_9		((v4l2_std_id)0x06000000)
#define V4L2_STD_576P_50_4_3		((v4l2_std_id)0x07000000)
#define V4L2_STD_720P_60		((v4l2_std_id)0x08000000)
#define V4L2_STD_720P_50		((v4l2_std_id)0x09000000)
#define V4L2_STD_1080P_60		((v4l2_std_id)0x0a000000)
#define V4L2_STD_1080P_50		((v4l2_std_id)0x0b000000)
/* ------------- ext. param -------------------*/

#define V4L2_FBUF_FLAG_PRE_MULTIPLY			0x0040
#define V4L2_FBUF_CAP_PRE_MULTIPLY			0x0080

#define VIDIOC_HDCP_ENABLE _IOWR('V', 100, unsigned int)	//Control HDCP flag 1: enable, 0: disable


struct v4l2_window_s5p_tvout
{
	__u32			capability;
	__u32			flags;
	unsigned int		priority;
	struct v4l2_window	win;	
};

struct v4l2_pix_format_s5p_tvout
{
	void *base_y;
	void *base_c;
	unsigned short src_img_endian;
	struct v4l2_pix_format	pix_fmt;
};

#endif //__VIDEODEV2_S5P_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

//#include <linux/videodev2.h>
#include "videodev2.h"
#include "videodev2_s5p.h"
#include "hdmi_api.h"
#include "hdmi_lib.h"

#define LOG_NDEBUG 0
#define LOG_TAG "libhdmi"
#include <cutils/log.h>


int tvout_v4l2_cropcap(int fp, struct v4l2_cropcap *a)
{
	struct v4l2_cropcap *cropcap = a;
	int ret;

	ret = ioctl(fp, VIDIOC_CROPCAP, cropcap);

	if (ret < 0) {
		LOGE("tvout_v4l2_cropcap" "VIDIOC_CROPCAP failed %d\n", errno);
		return ret;
	}

	//LOGV("tvout_v4l2_cropcap" "bound width : %d, bound height : %d,\n", 
	//		cropcap->bounds.width, cropcap->bounds.height);
	return ret;
 
}

int tvout_v4l2_querycap(int fp)
{
	struct v4l2_capability cap;
	int ret;

	ret = ioctl(fp, VIDIOC_QUERYCAP, &cap);

	if (ret < 0) {
		LOGE("tvout_v4l2_querycap" "VIDIOC_QUERYCAP failed %d\n", errno);
		return ret;
	}

	//if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
	//	printf("no overlay devices\n");
	//	return ret;
	//}

	//LOGV("tvout_v4l2_querycap" "DRIVER : %s, CARD : %s, CAP.: 0x%08x\n", 
	//		cap.driver, cap.card, cap.capabilities);


	return ret;
}

/*
ioctl VIDIOC_G_STD, VIDIOC_S_STD
To query and select the current video standard applications use the VIDIOC_G_STD and
VIDIOC_S_STD ioctls which take a pointer to a v4l2_std_id type as argument. VIDIOC_G_STD can
return a single flag or a set of flags as in struct v4l2_standard field id
*/

int tvout_v4l2_g_std(int fp, v4l2_std_id *std_id)
{
	int ret;

	ret = ioctl(fp, VIDIOC_G_STD, std_id);
	if (ret < 0) {
		LOGE("tvout_v4l2_g_std" "VIDIOC_G_STD failed %d\n", errno);
		return ret;
	}

	return ret;
}

int tvout_v4l2_s_std(int fp, v4l2_std_id std_id)
{
	int ret;

	ret = ioctl(fp, VIDIOC_S_STD, &std_id);
	if (ret < 0) {
		LOGE("tvout_v4l2_s_std" "VIDIOC_S_STD failed %d\n", errno);
		return ret;
	}

	return ret;
}

/*
ioctl VIDIOC_ENUMSTD
To query the attributes of a video standard, especially a custom (driver defined) one, applications
initialize the index field of struct v4l2_standard and call the VIDIOC_ENUMSTD ioctl with a pointer
to this structure. Drivers fill the rest of the structure or return an EINVAL error code when the index
is out of bounds.
*/
int tvout_v4l2_enum_std(int fp, struct v4l2_standard *std, v4l2_std_id std_id)
{
	
	std->index = 0;
	while (0 == ioctl (fp, VIDIOC_ENUMSTD, std)) {
		if (std->id & std_id) {
			LOGV("tvout_v4l2_s_std" "Current video standard: %s\n", std->name);
		}
		std->index++;
	}
	
	return 0;
}

/*
ioctl VIDIOC_ENUMOUTPUT
To query the attributes of a video outputs applications initialize the index field of struct v4l2_output
and call the VIDIOC_ENUMOUTPUT ioctl with a pointer to this structure. Drivers fill the rest of the
structure or return an EINVAL error code when the index is out of bounds
*/
int tvout_v4l2_enum_output(int fp, struct v4l2_output *output)
{
	int ret;

	ret = ioctl(fp, VIDIOC_ENUMOUTPUT, output);

	if(ret >=0){
		LOGV("tvout_v4l2_enum_output" "enum. output [index = %d] :: type : 0x%08x , name = %s\n",
			output->index,output->type,output->name);
	}
	
	return ret;
}

/*
ioctl VIDIOC_G_OUTPUT, VIDIOC_S_OUTPUT
To query the current video output applications call the VIDIOC_G_OUTPUT ioctl with a pointer to an
integer where the driver stores the number of the output, as in the struct v4l2_output index field.
This ioctl will fail only when there are no video outputs, returning the EINVAL error code
*/
int tvout_v4l2_s_output(int fp, int index)
{
	int ret;

	ret = ioctl(fp, VIDIOC_S_OUTPUT, &index);
	if (ret < 0) {
		LOGE("tvout_v4l2_s_output" "VIDIOC_S_OUTPUT failed %d\n", errno);
		return ret;
	}

	return ret;
}

int tvout_v4l2_g_output(int fp, int *index)
{
	int ret;

	ret = ioctl(fp, VIDIOC_G_OUTPUT, index);
	if (ret < 0) {
		LOGE("tvout_v4l2_g_output" "VIDIOC_G_OUTPUT failed %d\n", errno);
		return ret;
	}else{
		LOGV("tvout_v4l2_g_output" "Current output index %d\n", *index);
	}

	return ret;
}

/*
ioctl VIDIOC_ENUM_FMT
To enumerate image formats applications initialize the type and index field of struct v4l2_fmtdesc
and call the VIDIOC_ENUM_FMT ioctl with a pointer to this structure. Drivers fill the rest of the
structure or return an EINVAL error code. All formats are enumerable by beginning at index zero
and incrementing by one until EINVAL is returned.
*/
int tvout_v4l2_enum_fmt(int fp, struct v4l2_fmtdesc *desc)
{
	desc->index = 0;
	while (0 == ioctl(fp, VIDIOC_ENUM_FMT, desc)) {
		LOGV("tvout_v4l2_enum_fmt" "enum. fmt [id : 0x%08x] :: type = 0x%08x, name = %s, pxlfmt = 0x%08x\n",
			desc->index, 
			desc->type, 
			desc->description, 
			desc->pixelformat);			
		desc->index++;
	}

	
	return 0;
}

int tvout_v4l2_g_fmt(int fp, int buf_type, void* ptr)
{
	int ret;
	struct v4l2_format format;
	struct v4l2_pix_format_s5p_tvout *fmt_param = (struct v4l2_pix_format_s5p_tvout*)ptr;
	
	format.type = (enum v4l2_buf_type)buf_type;

	ret = ioctl(fp, VIDIOC_G_FMT, &format);
	if (ret < 0) {
		LOGE("tvout_v4l2_g_fmt" "type : %d, VIDIOC_G_FMT failed %d\n", buf_type, errno);
		return ret;
	}else{
		memcpy(fmt_param, format.fmt.raw_data, sizeof(struct v4l2_pix_format_s5p_tvout));
		LOGV("tvout_v4l2_g_fmt" "get. fmt [base_c : 0x%08x], [base_y : 0x%08x] type = 0x%08x, width = %d, height = %d\n",
			fmt_param->base_c, 
			fmt_param->base_y,
			fmt_param->pix_fmt.pixelformat, 
			fmt_param->pix_fmt.width,
			fmt_param->pix_fmt.height);
	}
	

	return 0;
}

int tvout_v4l2_s_fmt(int fp, int buf_type, void *ptr)
{
	struct v4l2_format format;
	int ret;

	format.type = (enum v4l2_buf_type)buf_type;
	switch(buf_type)
	{
		case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		{
			struct v4l2_pix_format_s5p_tvout *fmt_param = (struct v4l2_pix_format_s5p_tvout*)ptr;
			memcpy(format.fmt.raw_data, fmt_param, sizeof(struct v4l2_pix_format_s5p_tvout));
			break;
		}
		default:
			break;
	}
	ret = ioctl(fp, VIDIOC_S_FMT, &format);
	if (ret < 0) {
		LOGE("tvout_v4l2_s_fmt" "[tvout_v4l2_s_fmt] : type : %d, VIDIOC_S_FMT failed %d\n", buf_type, errno);
		return ret;
	}
	return 0;
}

int tvout_v4l2_g_parm(int fp, int buf_type, void *ptr)
{
	int ret;
	struct v4l2_streamparm parm;
	struct v4l2_window_s5p_tvout *vparm = (struct v4l2_window_s5p_tvout*)ptr;
	

	parm.type = (enum v4l2_buf_type)buf_type;
	
	ret = ioctl(fp, VIDIOC_G_PARM, &parm);

	if (ret < 0) {
		LOGE("tvout_v4l2_g_parm" "type : %d, VIDIOC_G_PARM failed %d\n", buf_type, errno);
		return ret;
	}else{
		memcpy(vparm, parm.parm.raw_data, sizeof(struct v4l2_pix_format_s5p_tvout));
		LOGV("tvout_v4l2_g_parm" "get. param : width  = %d, height = %d\n",
			vparm->win.w.width, 
			vparm->win.w.height);
	}
	return 0;
}

int tvout_v4l2_s_parm(int fp, int buf_type, void *ptr)
{
	struct v4l2_streamparm parm;
	struct v4l2_window_s5p_tvout *vparm = (struct v4l2_window_s5p_tvout*)ptr;
	int ret;

	parm.type = (enum v4l2_buf_type)buf_type;
	memcpy(parm.parm.raw_data, vparm, sizeof(struct v4l2_window_s5p_tvout));

	ret = ioctl(fp, VIDIOC_S_PARM, &parm);
	if (ret < 0) {
		LOGE("tvout_v4l2_s_parm" "VIDIOC_S_PARM failed %d\n", errno);
		return ret;
	}

	return 0;
}

int tvout_v4l2_g_fbuf(int fp, struct v4l2_framebuffer *frame)
{
	int ret;

	ret = ioctl(fp, VIDIOC_G_FBUF, frame);
	if (ret < 0) {
		LOGE("tvout_v4l2_g_fbuf" "VIDIOC_STREAMON failed %d\n", errno);
		return ret;
	}

	LOGV("tvout_v4l2_g_fbuf" "get. fbuf: base = 0x%08X, pixel format = %d\n",
			frame->base, 
			frame->fmt.pixelformat);
	return 0;
}

int tvout_v4l2_s_fbuf(int fp, struct v4l2_framebuffer *frame)
{
	int ret;

	ret = ioctl(fp, VIDIOC_S_FBUF, frame);
	if (ret < 0) {
		LOGE("tvout_v4l2_s_fbuf" "VIDIOC_STREAMON failed %d\n", errno);
		return ret;
	}
	return 0;
}

int tvout_v4l2_g_crop(int fp, unsigned int type, struct v4l2_rect *rect)
{
	int ret;
	struct v4l2_crop crop;
	crop.type = (enum v4l2_buf_type)type;
	ret = ioctl(fp, VIDIOC_G_CROP, &crop);
	if (ret < 0) {
		LOGE("tvout_v4l2_s_crop" "VIDIOC_S_CROP failed %d\n", errno);
		return ret;
	}

	rect->left	= crop.c.left;
	rect->top	= crop.c.top;
	rect->width	= crop.c.width;
	rect->height	= crop.c.height;
/*
	LOGV("tvout_v4l2_g_crop" "get. crop : left = %d, top = %d, width  = %d, height = %d\n",
			rect->left, 
			rect->top,
			rect->width,
			rect->height);
			*/
	return 0;
}

int tvout_v4l2_s_crop(int fp, unsigned int type, struct v4l2_rect *rect)
{
	struct v4l2_crop crop;
	int ret;

	crop.type = (enum v4l2_buf_type)type;

	crop.c.left 		= rect->left;
	crop.c.top 	= rect->top;
	crop.c.width 	= rect->width;
	crop.c.height 	= rect->height;

	ret = ioctl(fp, VIDIOC_S_CROP, &crop);
	if (ret < 0) {
		LOGE("tvout_v4l2_s_crop" "VIDIOC_S_CROP failed %d\n", errno);
		return ret;
	}

	return 0;
}

int tvout_v4l2_streamon(int fp, unsigned int type)
{
	int ret;

	ret = ioctl(fp, VIDIOC_STREAMON, &type);
	if (ret < 0) {
		LOGE("tvout_v4l2_streamon" "VIDIOC_STREAMON failed %d\n", errno);
		return ret;
	}

	LOGV("tvout_v4l2_streamon" "requested streamon buftype[id : 0x%08x]\n", type);
	return 0;
}

int tvout_v4l2_streamoff(int fp, unsigned int type)
{
	int ret;

	ret = ioctl(fp, VIDIOC_STREAMOFF, &type);
	if (ret < 0) {
		LOGE("tvout_v4l2_streamoff""VIDIOC_STREAMOFF failed \n");
		return ret;
	}

	LOGV("tvout_v4l2_streamoff" "requested streamon buftype[id : 0x%08x]\n", type);
	return 0;
}


int tvout_v4l2_start_overlay(int fp)
{
	int ret, start = 1;

	ret = ioctl(fp, VIDIOC_OVERLAY, &start);
	if (ret < 0) {
		LOGE("tvout_v4l2_start_overlay" "VIDIOC_OVERLAY failed\n");
		return ret;
  	}

	return ret;
}

int tvout_v4l2_stop_overlay(int fp)
{
	int ret, stop =0;

	ret = ioctl(fp, VIDIOC_OVERLAY, &stop);
	if (ret < 0) 
	{
		LOGE("tvout_v4l2_stop_overlay" "VIDIOC_OVERLAY failed\n");
		return ret;
  	}

	return ret;
}

struct v4l2_s5p_video_param
{
    unsigned short b_win_blending;
    unsigned int ui_alpha;
    unsigned int ui_priority;
    unsigned int ui_top_y_address;
    unsigned int ui_top_c_address;
    struct v4l2_rect rect_src;
    unsigned short src_img_endian;
};

struct tvout_param {
    struct v4l2_pix_format_s5p_tvout    tvout_src;
    struct v4l2_window_s5p_tvout        tvout_rect;
    struct v4l2_rect            tvout_dst;
};

struct overlay_param {
	struct v4l2_framebuffer 		overlay_frame;
	struct v4l2_window_s5p_tvout 	overlay_rect;
	struct v4l2_rect				overlay_dst;	
};

#define TVOUT_DEV   "/dev/video14"
#define TVOUT_DEV_G0	"/dev/video21"
#define TVOUT_DEV_G1	"/dev/video22"

static int fp_tvout = -1;
static int fp_tvout_g0 = -1;
static int fp_tvout_g1 = -1;

struct tvout_param tv_param;
static int f_vp_stream_on = 0;

unsigned int init_tv(void)
{
    //unsigned int fp_tvout;
    int ret;
    struct v4l2_output output;
    struct v4l2_standard std;
    v4l2_std_id std_g_id;
    v4l2_std_id std_id = t_std_id;
    unsigned int matched=0, i=0;
    int output_index;

    // It was initialized already
    if(fp_tvout != -1)
        return fp_tvout;

    fp_tvout = open(TVOUT_DEV, O_RDWR);
    if (fp_tvout < 0) {
        LOGE("tvout video drv open failed\n");
        return fp_tvout;
    }

        // hdcp on
        // if(hdcp_flag)
        //ioctl(fp_tvout, VIDIOC_HDCP_ENABLE, 1);

    /* ============== query capability============== */
    tvout_v4l2_querycap(fp_tvout);

    tvout_v4l2_enum_std(fp_tvout, &std, std_id);

    // set std
    tvout_v4l2_s_std(fp_tvout, std_id);
    tvout_v4l2_g_std(fp_tvout, &std_g_id);

    i = 0;

    do
    {
        output.index = i;
        ret = tvout_v4l2_enum_output(fp_tvout, &output);
        if(output.type == output_type)
        {
            matched = 1;
            break;
        }
        i++;
        
    }while(ret >=0);

    if(!matched)
    {
        LOGE("no matched output type [type : 0x%08x]\n", output_type);
        return ret;
    }

    // set output
    tvout_v4l2_s_output(fp_tvout, output.index);
    output_index = 0;
    tvout_v4l2_g_output(fp_tvout, &output_index);

    return fp_tvout;
}

int hdmi_set_v_param(unsigned int ui_top_y_address, unsigned int ui_top_c_address, int i_width, int i_height)
{
    tv_param.tvout_src.base_y = (void *)ui_top_y_address;
    tv_param.tvout_src.base_c = (void *)ui_top_c_address;
    tv_param.tvout_src.pix_fmt.width = i_width;
    tv_param.tvout_src.pix_fmt.height = i_height;
    //tv_param.tvout_src.pix_fmt.field = V4L2_FIELD_NONE;
    tvout_v4l2_s_fmt(fp_tvout, V4L2_BUF_TYPE_VIDEO_OUTPUT, &tv_param.tvout_src);

    tv_param.tvout_rect.win.w.width = i_width;
    tv_param.tvout_rect.win.w.height = i_height;
    tvout_v4l2_s_parm(fp_tvout, V4L2_BUF_TYPE_VIDEO_OUTPUT, &tv_param.tvout_rect);

    if ( i_width * HDMI_HEIGHT >= i_height * HDMI_WIDTH )
    {
	    tv_param.tvout_dst.left = 0;
	    tv_param.tvout_dst.top = (HDMI_HEIGHT - ((HDMI_WIDTH * i_height) / i_width)) / 2;
	    tv_param.tvout_dst.width = HDMI_WIDTH;
	    tv_param.tvout_dst.height = ((HDMI_WIDTH * i_height) / i_width);
    }
    else
    {
	    tv_param.tvout_dst.left = (HDMI_WIDTH - ((HDMI_HEIGHT * i_width) / i_height)) / 2;
	    tv_param.tvout_dst.top = 0;
	    tv_param.tvout_dst.width = ((HDMI_HEIGHT * i_width) / i_height);
	    tv_param.tvout_dst.height = HDMI_HEIGHT;
    }
    tvout_v4l2_s_crop(fp_tvout, V4L2_BUF_TYPE_VIDEO_OUTPUT, &tv_param.tvout_dst);

	LOGD("hdmi_set_v_param() - yaddress: [0x%08x], caddress[0x%08x], %d\n", ui_top_y_address, ui_top_c_address, f_vp_stream_on);
    if (!f_vp_stream_on)
    {
        tvout_v4l2_streamon(fp_tvout, V4L2_BUF_TYPE_VIDEO_OUTPUT);
        f_vp_stream_on = 1;
    }

	LOGV("hdmi_set_v_param() - yaddress: [0x%08x], caddress[0x%08x]\n", ui_top_y_address, ui_top_c_address);

    return 0;
}

int hdmi_initialize(int i_width, int i_height)
{
    struct tvout_param tv_g_param;
    struct v4l2_fmtdesc desc;
    printf("%s\n",__func__);
    fp_tvout = init_tv();

    //set fmt param
    tv_param.tvout_src.base_y = (void *)0x26000000;
    tv_param.tvout_src.base_c = (void *)0x27200000;
    // video_src.base_y = 0x26000000;
    // video_src.base_c = 0x27200000;
    tv_param.tvout_src.pix_fmt.width = i_width;
    tv_param.tvout_src.pix_fmt.height = i_height;
    tvout_v4l2_s_fmt(fp_tvout, V4L2_BUF_TYPE_VIDEO_OUTPUT, &tv_param.tvout_src);
    tvout_v4l2_g_fmt(fp_tvout, V4L2_BUF_TYPE_VIDEO_OUTPUT, &tv_g_param.tvout_src);
    desc.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    tvout_v4l2_enum_fmt(fp_tvout, &desc);

    //set window param
    tv_param.tvout_rect.flags = 0;      //video_rect.flags = 0;
    tv_param.tvout_rect.priority = 1;   //video_rect.priority = 1;
    tv_param.tvout_rect.win.w.left = 0;     //video_rect.win.w.left = 0;
    tv_param.tvout_rect.win.w.top = 0;  //video_rect.win.w.top = 0;
    tv_param.tvout_rect.win.w.width = i_width;  //video_rect.win.w.width = 800;
    tv_param.tvout_rect.win.w.height = i_height; //video_rect.win.w.height = 480;

    tvout_v4l2_s_parm(fp_tvout, V4L2_BUF_TYPE_VIDEO_OUTPUT, &tv_param.tvout_rect);
    tvout_v4l2_g_parm(fp_tvout, V4L2_BUF_TYPE_VIDEO_OUTPUT, &tv_g_param.tvout_rect);

    if ( i_width * HDMI_HEIGHT >= i_height * HDMI_WIDTH )
    {
	    tv_param.tvout_dst.left = 0;
	    tv_param.tvout_dst.top = (HDMI_HEIGHT - ((HDMI_WIDTH * i_height) / i_width)) / 2;
	    tv_param.tvout_dst.width = HDMI_WIDTH;
	    tv_param.tvout_dst.height = ((HDMI_WIDTH * i_height) / i_width);
    }
    else
    {
	    tv_param.tvout_dst.left = (HDMI_WIDTH - ((HDMI_HEIGHT * i_width) / i_height)) / 2;
	    tv_param.tvout_dst.top = 0;
	    tv_param.tvout_dst.width = ((HDMI_HEIGHT * i_width) / i_height);
	    tv_param.tvout_dst.height = HDMI_HEIGHT;
    }
    tvout_v4l2_s_crop(fp_tvout, V4L2_BUF_TYPE_VIDEO_OUTPUT, &tv_param.tvout_dst);

    f_vp_stream_on = 0;

    return 0;
}

int hdmi_deinitialize()
{
    if(fp_tvout != -1)
    {
        if(f_vp_stream_on == 1)
        {
            tvout_v4l2_streamoff(fp_tvout, V4L2_BUF_TYPE_VIDEO_OUTPUT);
            f_vp_stream_on = 0;
        }
        close(fp_tvout);
        fp_tvout = -1;
    }
    return 0;
}

//=============================
// 

static int open_tvout(char *fp_name)
{
	int fp;
	
	fp = open(fp_name, O_RDWR);
	if (fp < 0) 
	{
		LOGE("tvout graphic drv open failed!!\n");
		return fp;
	}

	return fp;
}

static int set_overlay_param(int layer, void *param)
{
	int fp_tvout;

	struct overlay_param *vo_param = param;
	struct overlay_param vo_g_param;
	struct v4l2_cropcap cropcap;
	
	if(layer == 0)
		fp_tvout = fp_tvout_g0;
	else
		fp_tvout = fp_tvout_g1;

	tvout_v4l2_s_fbuf(fp_tvout, &(vo_param->overlay_frame));
	//tvout_v4l2_g_fbuf(fp_tvout, &(vo_g_param.overlay_frame));
	
	tvout_v4l2_s_parm(fp_tvout, V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY, &(vo_param->overlay_rect));
	//tvout_v4l2_g_parm(fp_tvout, V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY, &(vo_g_param.overlay_rect));	//test get param

	cropcap.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
	tvout_v4l2_cropcap(fp_tvout, &cropcap);
	if(vo_param->overlay_dst.width <= cropcap.bounds.width && vo_param->overlay_dst.height <= cropcap.bounds.height){
		tvout_v4l2_s_crop(fp_tvout, V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY,&(vo_param->overlay_dst));
		tvout_v4l2_g_crop(fp_tvout, V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY,&(vo_g_param.overlay_dst));
	}
	else{
		LOGE("[%s] invalid crop size dst.w=%d bounds.w=%d dst.h=%d bounds.h=%d\n", __func__, vo_param->overlay_dst.width, cropcap.bounds.width, vo_param->overlay_dst.height, cropcap.bounds.height);
		return -1;;
	}
	
	return 1;
}

int hdmi_gl_initialize(int layer)
{
    fp_tvout = init_tv();
	if(layer == 0)
	{
		if(fp_tvout_g0 < 0)
			{
				fp_tvout_g0 	= open_tvout(TVOUT_DEV_G0);
			}

		return fp_tvout_g0;	
	}
	else
	{
		if(fp_tvout_g1 < 0)
			{
				fp_tvout_g1 	= open_tvout(TVOUT_DEV_G1);
			}

		return fp_tvout_g1;	
	}
}

int hdmi_gl_deinitialize(int layer)
{
	if(layer == 0)
	{
		if (fp_tvout_g0 >= 0)
		{
			close(fp_tvout_g0);
			fp_tvout_g0 = -1;
		}
	}
	else
	{
		if (fp_tvout_g1 >= 0)
		{
			close(fp_tvout_g1);
			fp_tvout_g1 = -1;
		}
	}

	return 0;
}

int hdmi_gl_set_param(int layer, unsigned int ui_base_address, int i_width, int i_height)
{
	struct overlay_param ov_param;

	ov_param.overlay_frame.fmt.pixelformat = V4L2_PIX_FMT_RGB565;
	ov_param.overlay_frame.base = ui_base_address;

	ov_param.overlay_rect.flags 	= 0;
	ov_param.overlay_rect.priority 	= 0x02;
	ov_param.overlay_rect.win.w.left 	= 0;
	ov_param.overlay_rect.win.w.top 	= 0;
	ov_param.overlay_rect.win.w.width 	= i_width;
	ov_param.overlay_rect.win.w.height 	= i_height;
	ov_param.overlay_rect.win.global_alpha 	= 0;

	if ( i_width ==  HDMI_WIDTH )
	{
		ov_param.overlay_dst.left   = 0;
		ov_param.overlay_dst.top    = (HDMI_HEIGHT - i_height) / 2;
	}
	else
	{
		ov_param.overlay_dst.left   = (HDMI_WIDTH - i_width) / 2;
		ov_param.overlay_dst.top    = 0;
	}
	ov_param.overlay_dst.width  = i_width;
	ov_param.overlay_dst.height = i_height;

	return set_overlay_param(layer, &ov_param);
}

int hdmi_gl_streamon(int layer)
{
	int ret = 0;
	if (0 == layer)
		ret = tvout_v4l2_start_overlay(fp_tvout_g0);
	else
		ret = tvout_v4l2_start_overlay(fp_tvout_g1);

	return ret;
}

int hdmi_gl_streamoff(int layer)
{
	int ret = 0;
	if (0 == layer)
		ret = tvout_v4l2_stop_overlay(fp_tvout_g0);
	else
		ret = tvout_v4l2_stop_overlay(fp_tvout_g1);

	return ret;
}

int hdmi_v_streamon()
{
    int ret = 0;
    if (fp_tvout !=-1 && !f_vp_stream_on)
    {
        ret = tvout_v4l2_streamon(fp_tvout, V4L2_BUF_TYPE_VIDEO_OUTPUT);
        f_vp_stream_on = 1;
    }
    return ret;
}


int hdmi_v_streamoff()
{
    int ret = 0;
    if(fp_tvout != -1)
    {
        if(f_vp_stream_on == 1)
        {
            ret = tvout_v4l2_streamoff(fp_tvout, V4L2_BUF_TYPE_VIDEO_OUTPUT);
            f_vp_stream_on = 0;
        }
    }
    return ret;
}


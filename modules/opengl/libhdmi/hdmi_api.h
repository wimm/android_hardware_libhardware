#ifndef __HDMI_API_H__
#define __HDMI_API_H__

#ifdef __cplusplus
extern "C" {
#endif

int tvout_v4l2_querycap(int fp);
int tvout_v4l2_g_std(int fp, v4l2_std_id *std_id);
int tvout_v4l2_s_std(int fp, v4l2_std_id std_id);
int tvout_v4l2_enum_std(int fp, struct v4l2_standard *std, v4l2_std_id std_id);
int tvout_v4l2_enum_output(int fp, struct v4l2_output *output);
int tvout_v4l2_s_output(int fp, int index);
int tvout_v4l2_g_output(int fp, int *index);
int tvout_v4l2_enum_fmt(int fp, struct v4l2_fmtdesc *desc);
int tvout_v4l2_g_fmt(int fp, int buf_type, void* ptr);
int tvout_v4l2_s_fmt(int fp, int buf_type, void *ptr);
int tvout_v4l2_g_parm(int fp, int buf_type, void *ptr);
int tvout_v4l2_s_parm(int fp, int buf_type, void *ptr);
int tvout_v4l2_g_fbuf(int fp, struct v4l2_framebuffer *frame);
int tvout_v4l2_s_fbuf(int fp, struct v4l2_framebuffer *frame);
int tvout_v4l2_g_crop(int fp, unsigned int type, struct v4l2_rect *rect);
int tvout_v4l2_s_crop(int fp, unsigned int type, struct v4l2_rect *rect);
int tvout_v4l2_streamon(int fp, unsigned int type);
int tvout_v4l2_streamoff(int fp, unsigned int type);
int tvout_v4l2_start_overlay(int fp);
int tvout_v4l2_stop_overlay(int fp);
int tvout_v4l2_cropcap(int fp, struct v4l2_cropcap *a);

//Kamat added
int hdmi_initialize(int i_width, int i_height);
int hdmi_set_v_param(unsigned int ui_top_y_address, unsigned int ui_top_c_address, int i_width, int i_height);
int hdmi_deinitialize();
int hdmi_gl_initialize(int layer);
int hdmi_gl_streamon(int layer);
int hdmi_gl_streamoff(int layer);
int hdmi_v_streamon();
int hdmi_v_streamoff();
int hdmi_cable_status();
#ifdef __cplusplus
}
#endif


#endif //__HDMI_API_H__


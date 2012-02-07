#ifndef __SAMSUNG_SYSLSI_APDEV_HDMILIB_H__
#define __SAMSUNG_SYSLSI_APDEV_HDMILIB_H__

#ifdef __cplusplus
extern "C" {
#endif

#define ANALOG_OUT

#if defined(STD_480P)
#define output_type     V4L2_OUTPUT_TYPE_DIGITAL
#define t_std_id        V4L2_STD_480P_60_16_9
#define HDMI_WIDTH  720
#define HDMI_HEIGHT 480
#elif defined(STD_720P)
#if defined(ANALOG_OUT)
#define output_type     V4L2_OUTPUT_TYPE_COMPOSITE
#define t_std_id        V4L2_STD_NTSC_M
#define HDMI_WIDTH  720
#define HDMI_HEIGHT 480
#else
#define output_type     V4L2_OUTPUT_TYPE_DIGITAL
#define t_std_id        V4L2_STD_720P_60
#define HDMI_WIDTH  1280
#define HDMI_HEIGHT 720
#endif
#else
#define output_type     V4L2_OUTPUT_TYPE_DIGITAL
#define t_std_id        V4L2_STD_1080P_60
#define HDMI_WIDTH  1920
#define HDMI_HEIGHT 1080
#endif

// functions for video layer
extern int hdmi_initialize(int i_width, int i_height);
extern int hdmi_deinitialize();
extern int hdmi_set_v_param(unsigned int ui_top_y_address, unsigned int ui_top_c_address, int i_width, int i_height);
// functions for graphic layer
extern int hdmi_gl_initialize(int layer);
extern int hdmi_gl_deinitialize(int layer);
extern int hdmi_gl_set_param(int layer, unsigned int ui_base_address, int i_width, int i_height);
extern int hdmi_gl_streamon(int layer);
extern int hdmi_gl_streamoff(int layer);
//extern int hdmi_set_g0_format(unsigned int ui_base_address, int i_width, int i_height);
//extern int hdmi_set_g1_format(unsigned int ui_base_address, int i_width, int i_height);
//extern int hdmi_stream_switch_g0();
//extern int hdmi_stream_switch_g1();
//extern int hdmi_streamon_g0();
//extern int hdmi_streamoff_g0();
//extern int hdmi_streamon_g1();
//extern int hdmi_streamoff_g1();
//extern int hdmi_streamon_v();
//extern int hdmi_streamoff_v();
#ifdef __cplusplus
}
#endif

#endif //__SAMSUNG_SYSLSI_APDEV_HDMILIB_H__

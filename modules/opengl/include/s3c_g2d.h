/* s3c_g2d.h
 **
 ** Copyright (c) 2010 Samsung Electronics Co., Ltd.
 **	  http://www.samsung.com/
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License version 2 as
 ** published by the Free Software Foundation.
 */
 
#ifndef _S3C_G2D_H_
#define _S3C_G2D_H_

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned long  u32;

#define TRUE    1
#define FALSE   0


#define S3C_G2D_DEV_NAME		"/dev/s3c-g2d"

#define G2D_IOCTL_MAGIC	'G'

#define S3C_G2D_ROTATOR_0			_IO(G2D_IOCTL_MAGIC,0)
#define S3C_G2D_ROTATOR_90			_IO(G2D_IOCTL_MAGIC,1)
#define S3C_G2D_ROTATOR_180			_IO(G2D_IOCTL_MAGIC,2)
#define S3C_G2D_ROTATOR_270			_IO(G2D_IOCTL_MAGIC,3)
#define S3C_G2D_ROTATOR_X_FLIP		_IO(G2D_IOCTL_MAGIC,4)
#define S3C_G2D_ROTATOR_Y_FLIP		_IO(G2D_IOCTL_MAGIC,5)

#define G2D_ALPHA_ON	1
#define G2D_ALPHA_OFF	0
#define G2D_ALPHA_SRC_BITMAP 1
#define G2D_ALPHA_SRC_BITMAP 1
#define G2D_COLOR_KEY_ON	1
#define G2D_COLOR_KEY_OFF	0

typedef enum
{
	ROT_0,
	ROT_90,
	ROT_180,
	ROT_270,
	ROT_X_FLIP,
	ROT_Y_FLIP
} ROT_DEG;


typedef enum
{
	ROP_DST_ONLY,
	ROP_SRC_ONLY,
	ROP_3RD_OPRND_ONLY,
	ROP_SRC_AND_DST,
	ROP_SRC_AND_3RD_OPRND,
	ROP_SRC_OR_DST,
	ROP_SRC_OR_3RD_OPRND,
	ROP_DST_OR_3RD,
	ROP_SRC_XOR_3RD_OPRND

} G2D_ROP_TYPE;

typedef enum
{
	G2D_NO_ALPHA_BLEND_MODE,
	G2D_EN_ALPHA_BLEND_MODE,
	G2D_EN_ALPHA_BLEND_CONST_ALPHA,
	G2D_EN_ALPHA_BLEND_PERPIXEL_ALPHA,
	G2D_EN_FADING_MODE
} G2D_ALPHA_BLENDING_MODE;

typedef enum
{
	G2D_DIS_COLKEY,
	G2D_EN_SRC_COLORKEY,
	G2D_EN_DST_COLORKEY,
	G2D_EN_SRC_DST_COLORKEY
}G2D_COLKEY_MODE;

typedef enum
{
	G2D_BLACK   = 0,
	G2D_RED     = 1,
	G2D_GREEN   = 2,
	G2D_BLUE    = 3,
	G2D_WHITE   = 4, 
	G2D_YELLOW  = 5,
	G2D_CYAN    = 6,
	G2D_MAGENTA = 7
} G2D_COLOR;

/*
typedef enum
{
	G2D_PAL1,  G2D_PAL2,  G2D_PAL4,  G2D_PAL8,
	G2D_RGB8,  G2D_ARGB8, G2D_RGB16, G2D_ARGB16,
	G2D_RGB18, G2D_RGB24, G2D_RGB30, G2D_ARGB24,G2D_RGBA16,G2D_RGBX24, G2D_RGBA24,
	G2D_YC420, G2D_YC422, // Non-interleave
	G2D_CRYCBY, G2D_CBYCRY, G2D_YCRYCB, G2D_YCBYCR, G2D_YUV444 // Interleave
} G2D_COLOR_SPACE;
*/

typedef enum
{
	G2D_RGBA_8888 = 1,
	G2D_RGBX_8888 = 2,
	G2D_ARGB_8888 = 3,
	G2D_XRGB_8888 = 4,
	G2D_BGRA_8888 = 5,
	G2D_BGRX_8888 = 6,
	G2D_ABGR_8888 = 7,
	G2D_XBGR_8888 = 8,
	G2D_RGB_888   = 9,
	G2D_BGR_888   = 10,
	G2D_RGB_565   = 11,
	G2D_BGR_565   = 12,
	G2D_RGBA_5551 = 13,
	G2D_ARGB_5551 = 14,
	G2D_RGBA_4444 = 15,
	G2D_ARGB_4444 = 16
} G2D_COLOR_FMT;

typedef enum
{
	G2D_PATTERN=0, G2D_FG_COLOR, g2D_BG_COLOR
}G2D_THIRD_OPERAND;

typedef enum
{
	G2D_NORM_MODE=0, G2D_FOREGROUND, G2D_BACKGROUND
}G2D_IMG_SEL;

typedef struct
{
	u32	src_base_addr;          //Base address of the source image
	u32	src_full_width;         //source image full width
	u32	src_full_height;        //source image full height
	u32	src_start_x;            //coordinate start x of source image
	u32	src_start_y;            //coordinate start y of source image
	u32	src_work_width;         //source image width for work
	u32 src_work_height;        //source image height for work
	G2D_COLOR_FMT src_colorfmt; //source color format
	u32 src_select;             //source selection

	u32	dst_base_addr;          //Base address of the destination image	
	u32	dst_full_width;         //destination screen full width
	u32	dst_full_height;        //destination screen full width
	u32	dst_start_x;            //coordinate start x of destination screen
	u32	dst_start_y;            //coordinate start y of destination screen
	u32	dst_work_width;         //destination screen width for work
	u32 dst_work_height;        //destination screen height for work
	G2D_COLOR_FMT dst_colorfmt; //destination color format
	u32 dst_select;             //destination selection

	// Coordinate (X, Y) of clipping window
	u32  cw_x1, cw_y1;
	u32  cw_x2, cw_y2;

	u32  color_val[8];
	u32 FGColor;                //Foreground Color
	u32 BGColor;                //Background Color
	u32 BSCOlor;                //BlueScreen Color
	//ROP 
	u32 Masked_select;          //THIRD_OPERAND
	u32 UnMasked_select;
	u32 MaskedROP3;             //Raster Operation Value
	u32 UnMaskedROP3;
	u32 transparent_mode;

	u32	alpha_mode;             //true : enable, false : disable
	u32	alpha_val;
	u32 fading_offset;
	u32	color_key_mode;         //true : enable, false : disable
	u32	color_key_val;          //transparent color value

}s5p_g2d_params;

#endif /*_S3C_G2D_H_*/

/*
 * Copyright (C) Guangzhou FriendlyARM Computer Tech. Co., Ltd.
 * (http://www.friendlyarm.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>
#include <linux/videodev2_nxp_media.h>
#include <linux/nxp_ion.h>

#include <ion.h>
#include <nx_fourcc.h>
#include <libnxjpeghw.h>
#include <nxp-v4l2.h>

// --------------------------------------------------------
static int debug = 1;

#define dbg(level, fmt, arg...)		\
	do {							\
		if (debug >= (level))		\
			fprintf(stderr, "D/%.3d: " fmt, __LINE__, ## arg);	\
	} while (0)

#define err(fmt, arg...)			\
	fprintf(stderr, "E/%.3d: " fmt, __LINE__, ## arg)

static struct timespec tpstart;

static void time_start(void)
{
	clock_gettime(CLOCK_REALTIME, &tpstart);
}

static long time_us_delta(void)
{
	struct timespec tpend;
	clock_gettime(CLOCK_REALTIME, &tpend);
	long usec = ((tpend.tv_sec - tpstart.tv_sec) * 1000000 +
				 (tpend.tv_nsec - tpstart.tv_nsec) / 1000);
	tpstart = tpend;
	return usec;
}

// --------------------------------------------------------

/*
 * See system/core/init/util.c
 * Copyright (C) 2008 The Android Open Source Project
 */
static char hardware[32];
static unsigned revision = 0;

static void get_hardware_name(char *hardware, unsigned int *revision)
{
	const char *cpuinfo = "/proc/cpuinfo";
	char *data = NULL;
	size_t len = 0, limit = 1024;
	int fd, n;
	char *x, *hw, *rev;

	/* Hardware string was provided on kernel command line */
	if (hardware[0])
		return;

	fd = open(cpuinfo, O_RDONLY);
	if (fd < 0) return;

	for (;;) {
		x = (char *)realloc(data, limit);
		if (!x) {
			err("Failed to allocate memory to read %s\n", cpuinfo);
			goto done;
		}
		data = x;

		n = read(fd, data + len, limit - len);
		if (n < 0) {
			err("Failed reading %s: %s (%d)\n", cpuinfo, strerror(errno), errno);
			goto done;
		}
		len += n;

		if (len < limit)
			break;

		/* We filled the buffer, so increase size and loop to read more */
		limit *= 2;
	}

	data[len] = 0;
	hw = strstr(data, "\nHardware");
	rev = strstr(data, "\nRevision");

	if (hw) {
		x = strstr(hw, ": ");
		if (x) {
			x += 2;
			n = 0;
			while (*x && *x != '\n') {
				if (!isspace(*x))
					hardware[n++] = tolower(*x);
				x++;
				if (n == 31) break;
			}
			hardware[n] = 0;
		}
	}

	if (rev) {
		x = strstr(rev, ": ");
		if (x) {
			*revision = strtoul(x + 2, 0, 16);
		}
	}

done:
	close(fd);
	free(data);
}

static inline bool cpu_is_s5p4418(void)
{
	return !strcmp(hardware, "nanopi2");
}

static inline bool cpu_is_s5p6818(void)
{
	return !strcmp(hardware, "nanopi3");
}

// --------------------------------------------------------
#ifndef ALIGN
#define ALIGN(x, a)		(((x) + (a) - 1) & ~((a) - 1))
#endif

static inline int get_size(int format, int num, int width, int height)
{
	int size;

	width = ALIGN(width, 128);
	height = ALIGN(height, 128);

	switch (format) {
		case V4L2_PIX_FMT_YUYV:
			if (num > 0) return 0;
			size = (width * height) * 2;
			break;
		case V4L2_PIX_FMT_YUV420M:
		case V4L2_PIX_FMT_YUV422P:
			if (num == 0)
				size = width * height;
			else
				size = (width * height) >> 1;
			break;
		case V4L2_PIX_FMT_YUV444:
			size = width * height;
			break;
		default:
			size = width * height * 2;
			break;
	}

	return size;
}

int alloc_buffers(int ion_fd, int count, struct nxp_vid_buffer *bufs,
		int width, int height, int format)
{
	struct nxp_vid_buffer *vb;
	int plane_num;
	int i, j;
	int ret;

	if (format == V4L2_PIX_FMT_YUYV || format == V4L2_PIX_FMT_RGB565)
		plane_num = 1;
	else
		plane_num = 3;

	int size[plane_num];
	for (j = 0; j < plane_num; j++)
		size[j] = get_size(format, j, width, height);

	dbg(3, "Alloc buffer: count(%d), plane(%d), format(%x)\n", count, plane_num, format);
	for (i = 0; i < count; i++) {
		vb = &bufs[i];
		vb->plane_num = plane_num;
		dbg(3, "[Buffer %d] --->\n", i);
		for (j = 0; j < plane_num; j++) {
			ret = ion_alloc_fd(ion_fd, size[j], 0,
					ION_HEAP_NXP_CONTIG_MASK, 0, &vb->fds[j]);
			if (ret < 0) {
				err("failed to ion alloc %d\n", size[j]);
				return ret;
			}
			vb->virt[j] = (char *)mmap(NULL, size[j],
					PROT_READ | PROT_WRITE, MAP_SHARED, vb->fds[j], 0);
			if (!vb->virt[j]) {
				err("failed to mmap\n");
				return -1;
			}
			ret = ion_get_phys(ion_fd, vb->fds[j], &vb->phys[j]);
			if (ret < 0) {
				err("failed to get phys\n");
				return ret;
			}
			vb->sizes[j] = size[j];
			dbg(3, "\tplane %d: fd(%d), size(%d), phys(0x%lx), virt(%p)\n",
					j, vb->fds[j], vb->sizes[j], vb->phys[j], vb->virt[j]);
		}
	}

	return 0;
}

// --------------------------------------------------------
#define MAX_BUFFER_COUNT	4
#define FMT_SENDOR			V4L2_MBUS_FMT_YUYV8_2X8

#define __V4L2_S(cmd)		\
	do {					\
		int ret = cmd;		\
		if (ret < 0) {		\
			fprintf(stderr, "%.3d: `%s' = %d\n", __LINE__, #cmd, ret);	\
			return ret;		\
		}					\
	} while (0)

static int init_preview(int width, int height, int format)
{
	__V4L2_S(v4l2_set_format(nxp_v4l2_decimator0, width, height, format));

	__V4L2_S(v4l2_set_crop_with_pad(nxp_v4l2_decimator0, 2, 0, 0, width, height));
	__V4L2_S(v4l2_set_crop(nxp_v4l2_decimator0, 0, 0, width, height));

	// Set to default format (800x600) at first
	if (width != 800)
		__V4L2_S(v4l2_set_format(nxp_v4l2_sensor0, 800, 600, FMT_SENDOR));
	__V4L2_S(v4l2_set_format(nxp_v4l2_sensor0, width, height, FMT_SENDOR));

	__V4L2_S(v4l2_reqbuf(nxp_v4l2_decimator0, MAX_BUFFER_COUNT));
	return 0;
}

/*
 * Note:
 *  - S5P4418: clipper0 + YUV422P, or decimator0 + YUV420M
 *  - S5P6818: clipper0 + YUV420M
 */
static int init_capture(int width, int height)
{
	int format = V4L2_PIX_FMT_YUV422P;
	if (cpu_is_s5p6818())
		format = V4L2_PIX_FMT_YUV420M;

	__V4L2_S(v4l2_set_format(nxp_v4l2_clipper0, width, height, format));
	__V4L2_S(v4l2_set_crop(nxp_v4l2_clipper0, 0, 0, width, height));

	// Set to default format (800x600) at first
	if (width != 800)
		__V4L2_S(v4l2_set_format(nxp_v4l2_sensor0, 800, 600, FMT_SENDOR));
	__V4L2_S(v4l2_set_format(nxp_v4l2_sensor0, width, height, FMT_SENDOR));

	__V4L2_S(v4l2_reqbuf(nxp_v4l2_clipper0, MAX_BUFFER_COUNT));
	return 0;
}

static int init_display(int width, int height, int format)
{
	__V4L2_S(v4l2_set_format(nxp_v4l2_mlc0_video, width, height, format));

	// Required for S5P6818
	__V4L2_S(v4l2_set_crop_with_pad(nxp_v4l2_mlc0_video, 2, 0, 0, width, height));

	// Adapt to LCD 1024x600
	if (width == 1280 && height == 720) {
		width  = 1024;
		height =  720;
	} else if (height > 1600) {
		width  =  640;
		height =  480;
	} else if (height > 600) {
		width  =  800;
		height =  600;
	}
	dbg(3, "display crop [%d, %d]\n", width, height);
	__V4L2_S(v4l2_set_crop(nxp_v4l2_mlc0_video, 0, 0, width, height));

	__V4L2_S(v4l2_set_ctrl(nxp_v4l2_mlc0_video, V4L2_CID_MLC_VID_PRIORITY, 0));
	__V4L2_S(v4l2_reqbuf(nxp_v4l2_mlc0_video, MAX_BUFFER_COUNT));
	return 0;
}

static int save_handle_to_jpeg(struct nxp_vid_buffer *buf,
		char *name, int width, int height)
{
	int ret = -1;
	int align = cpu_is_s5p6818() ? 128 : 16;
	int stride = ALIGN(width, align);

	int fd = creat(name, 0644);
	if (fd < 0) {
		err("failed to create `%s', errno = %d\n", name, errno);
		return ret;
	}

	int size = buf->sizes[0] + buf->sizes[1] + buf->sizes[2];
	void *dst = malloc(size);
	if (!dst) {
		err("failed to malloc(%d), errno = %d\n", size, errno);
		goto error;
	}

	ret = NX_JpegHWEncoding(dst, size, width, height, FOURCC_MVS0,
			(unsigned int)buf->phys[0], 0, stride,
			(unsigned int)buf->phys[1], 0, stride >> 1,
			(unsigned int)buf->phys[2], 0, stride >> 1, true);
	if (ret < 0) {
		err("failed to encoding JPEG, ret = %d\n", ret);
	} else {
		ret = write(fd, dst, ret);
		if (ret <= 0)
			err("failed to write file, errno = %d\n", errno);
	}

	free(dst);
error:
	close(fd);
	return ret;
}

static int do_preview(struct nxp_vid_buffer *bufs, int width, int height,
		int preview_frames)
{
	struct nxp_vid_buffer *buf;
	int i;

	for (i = 0; i < MAX_BUFFER_COUNT; i++) {
		buf = &bufs[i];
		dbg(3, "queue buffer %d\n", i);
		v4l2_qbuf(nxp_v4l2_decimator0, buf->plane_num, i, buf, -1, NULL);
	}

	__V4L2_S(v4l2_streamon(nxp_v4l2_decimator0));

	// fill all buffer for display
	int index = 0;
	for (i = 0; i < MAX_BUFFER_COUNT; i++) {
		buf = &bufs[index];
		dbg(2, "load buffer %d (%p)\n", index, buf);
		v4l2_dqbuf(nxp_v4l2_decimator0, buf->plane_num, &index, NULL);
		v4l2_qbuf(nxp_v4l2_decimator0, buf->plane_num, index, buf, -1, NULL);
		index++;
	}

	int cap_index = 0;
	int out_index = 0;
	int out_dq_index = 0;
	bool out_started = false;

	dbg(1, "start preview %dx%d (%d frames)\n", width, height, preview_frames);
	time_start();

	for (i = 0; i < preview_frames; i++) {
		buf = &bufs[cap_index];
		v4l2_dqbuf(nxp_v4l2_decimator0, buf->plane_num, &cap_index, NULL);
		v4l2_qbuf(nxp_v4l2_mlc0_video, buf->plane_num, out_index, buf, -1, NULL);

		dbg(3, "[ %ld ] switch buffer %d\n", time_us_delta(), cap_index);
		++out_index %= MAX_BUFFER_COUNT;

		if (!out_started) {
			if (v4l2_streamon(nxp_v4l2_mlc0_video) < 0)
				goto __exit;
			out_started = true;
		}

		v4l2_qbuf(nxp_v4l2_decimator0, buf->plane_num, cap_index, buf, -1, NULL);
		if (i >= (MAX_BUFFER_COUNT - 1))
			v4l2_dqbuf(nxp_v4l2_mlc0_video, buf->plane_num, &out_dq_index, NULL);
	}

	// stop preview
	__V4L2_S(v4l2_streamoff(nxp_v4l2_mlc0_video));
__exit:
	__V4L2_S(v4l2_streamoff(nxp_v4l2_decimator0));
	return 0;
}

int do_capture(struct nxp_vid_buffer *bufs, int width, int height,
		int skip_frames, char *jpeg_file)
{
	struct nxp_vid_buffer *buf;
	int i;

	dbg(1, "capture image %dx%d --> %s\n", width, height, jpeg_file);

	for (i = 0; i < MAX_BUFFER_COUNT; i++) {
		buf = &bufs[i];
		dbg(3, "queue buffer %d\n", i);
		v4l2_qbuf(nxp_v4l2_clipper0, buf->plane_num, i, buf, -1, NULL);
	}

	__V4L2_S(v4l2_streamon(nxp_v4l2_clipper0));

	int index = 0;
	for (i = 0; i < skip_frames; i++) {
		v4l2_dqbuf(nxp_v4l2_clipper0, buf->plane_num, &index, NULL);
		buf = &bufs[index];
		v4l2_qbuf(nxp_v4l2_clipper0, buf->plane_num, index, buf, -1, NULL);
		dbg(2, "skip buffer %d (%p)\n", index, buf);
	}

	__V4L2_S(v4l2_streamoff(nxp_v4l2_clipper0));

	return save_handle_to_jpeg(buf, jpeg_file, width, height);
}

// --------------------------------------------------------

/* MLC/video works improperly for YUV420M @800x600,1600x1200 */
#define FMT_PREVIEW		V4L2_PIX_FMT_YUV422P

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)	(int)(sizeof(arr) / sizeof((arr)[0]))
#endif

/* Supported frame size for sensor - OV5640 */
static int sensor_frmsize_list[][2] = {
	{  640,  480, },
	{  800,  600, },
	{ 1280,  720, },
	{ 1600, 1200, },
	{ 2592, 1944, },
};

static void show_sensor_frmsize(void)
{
	fprintf(stderr, "Supported SIZE for preview/capture:\n");
	for (int i = 0; i < ARRAY_SIZE(sensor_frmsize_list); i++) {
		fprintf(stderr, "  %d - %dx%d\n", i,
				sensor_frmsize_list[i][0], sensor_frmsize_list[i][1]);
	}
}

static void get_preview_size(int n, int *width, int *height)
{
	if (n < 0 || n > 2) n = 0;

	*width = sensor_frmsize_list[n][0];
	*height = sensor_frmsize_list[n][1];
}

static void get_capture_size(int n, int *width, int *height)
{
	if (n < 0 || n >= ARRAY_SIZE(sensor_frmsize_list))
		n = ARRAY_SIZE(sensor_frmsize_list) - 1;

	*width = sensor_frmsize_list[n][0];
	*height = sensor_frmsize_list[n][1];
}

// --------------------------------------------------------

static void show_usage(char *name)
{
	fprintf(stderr, "Usage: %s [OPTION]...\n\n", name);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -p SIZE     enable preview in size\n");
	fprintf(stderr, "  -c SIZE     select capture size\n");
	fprintf(stderr, "  -o FILE     save JPEG image to file\n");
	fprintf(stderr, "  -n COUNT    show given number of frames then capture\n");
	fprintf(stderr, "  -h          display this help and exit\n\n");
	show_sensor_frmsize();
	fprintf(stderr, "\n");
	fprintf(stderr, "Camera demo for S5P4418/S5P6818 boards\n");
	fprintf(stderr, "Copyright (C) FriendlyARM <http://www.friendlyarm.com>\n");
}

int main(int argc, char *argv[])
{
	char *jpeg_file = NULL;
	int width_p = 0, height_p = 0, count_p = 30;
	int width_c = 0, height_c = 0;
	int opt;

	while (-1 != (opt = getopt(argc, argv, "p:c:n:o:d:h"))) {
		switch (opt) {
			case 'p': get_preview_size(atoi(optarg), &width_p, &height_p); break;
			case 'c': get_capture_size(atoi(optarg), &width_c, &height_c); break;
			case 'o': jpeg_file = optarg;     break;
			case 'n': count_p = atoi(optarg); break;
			case 'd': debug = atoi(optarg);   break;
			case '?': fprintf(stderr, "\n");
			case 'h': show_usage(argv[0]);  exit(0); break;
			default:
					  break;
		}
	}

	get_hardware_name(hardware, &revision);
	dbg(2, "Board: %s, Rev %04x\n", hardware, revision);

	if (jpeg_file && !width_c)
		get_capture_size(-1, &width_c, &height_c);

	if (width_p == 0 && (width_c == 0 || !jpeg_file)) {
		dbg(1, "NO preview or capture request. Aborting...\n");
		dbg(1, "try `%s -h' for help\n", argv[0]);
		return 1;
	}

	int ion_fd = ion_open();
	if (ion_fd < 0) {
		err("failed to ion_open, errno = %d\n", errno);
		return -EINVAL;
	}

	struct V4l2UsageScheme s;
	memset(&s, 0, sizeof(s));
	s.useDecimator0 = true;
	s.useClipper0 = true;
	s.useMlc0Video = true;

	int ret = v4l2_init(&s);
	if (ret < 0) {
		err("initialize V4L2 failed, %d\n", ret);
		close(ion_fd);
		return ret;
	}

	struct nxp_vid_buffer bufs[MAX_BUFFER_COUNT];
	ret = alloc_buffers(ion_fd, MAX_BUFFER_COUNT, bufs,
			(width_c > width_p) ? width_c : width_p,
			(height_c > height_p) ? height_c : height_p, FMT_PREVIEW);
	if (ret < 0)
		goto exit;

	if (width_p > 0) {
		init_preview(width_p, height_p, FMT_PREVIEW);
		init_display(width_p, height_p, FMT_PREVIEW);
		if ((ret = do_preview(bufs, width_p, height_p, count_p)))
			goto exit;
	}

	if (jpeg_file && !init_capture(width_c, height_c)) {
		ret = do_capture(bufs, width_c, height_c, 3, jpeg_file);
	}

exit:
	v4l2_exit();
	close(ion_fd);

	return ret;
}


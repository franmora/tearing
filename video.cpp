#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <libdrm/drm_fourcc.h>
#include <chrono>

#include "video.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

using namespace std;

unsigned CreateSourceImage(int dmaFd, int width, int height, int fourcc);
void SelectTexture(unsigned index);

static const char* DriverName = "unicam";

SVideo::SVideo() :
    V4lFd(-1),
    IspFd(-1),
    SourceWidth(1280),
    SourceHeight(720),
    DmaBuffers(1),
    IspOutputBufferSize(0),
    QueueDesc({
        { -1, V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_MEMORY_MMAP }, // eQN_V4lCapture
        { -1, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_DMABUF }, // eQN_IspOutput
        { -1, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP }} ), // eQN_IspCapture
    V4lName("/dev/video0"),
    IspName("/dev/video12"),
    V4lDmaFd(),
    IspDmaFd(),
    Texture()
{
}

bool SVideo::Create()
{
    bool result = true;

    V4lFd = open(V4lName.c_str(), O_RDWR);
    IspFd = open(IspName.c_str(), O_RDWR);

    if (V4lFd >= 0 && IspFd >= 0)
    {
        result &= SetupV4lCaptureFormat();
        result &= SetupIspOutputFormat();
        result &= SetupIspCaptureFormat();
        result &= SetupV4lCaptureQueue();
        result &= SetupIspOutputQueue();
        result &= SetupIspCaptureQueue();

        int type;
        int retVal;

        // Start video capture
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        retVal = ioctl(V4lFd, VIDIOC_STREAMON, &type);
        if (retVal != 0)
        {
            result = false;
            printf("VIDIOC_STREAMON: %s\n", strerror(retVal));
        }

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        retVal = ioctl(IspFd, VIDIOC_STREAMON, &type);
        if (retVal != 0)
        {
            result = false;
            printf("VIDIOC_STREAMON: %s\n", strerror(retVal));
        }

        type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        retVal = ioctl(IspFd, VIDIOC_STREAMON, &type);
        if (retVal != 0)
        {
            result = false;
            printf("VIDIOC_STREAMON: %s\n", strerror(retVal));
        }
    }
    else
    {
        if (V4lFd < 0)
        {
            printf("Could not open %s\n", V4lName.c_str());
        }
        if (IspFd < 0)
        {
            printf("Could not open %s\n", IspName.c_str());
        }
        Destroy();
    }
    return result;
}

void SVideo::Destroy()
{
    // Stop video capture
    int retVal;
    int type;
    if (V4lFd >= 0)
    {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        retVal = ioctl(V4lFd, VIDIOC_STREAMOFF, &type);
        if (retVal != 0)
        {
            printf("VIDIOC_STREAMON: %s\n", strerror(retVal));
        }
        close(V4lFd);
    }
    if (IspFd >= 0)
    {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        retVal = ioctl(IspFd, VIDIOC_STREAMOFF, &type);
        if (retVal != 0)
        {
            printf("VIDIOC_STREAMON: %s\n", strerror(retVal));
        }
        type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        retVal = ioctl(IspFd, VIDIOC_STREAMOFF, &type);
        if (retVal != 0)
        {
            printf("VIDIOC_STREAMON: %s\n", strerror(retVal));
        }
        close(IspFd);
    }
}

bool SVideo::SetupV4lCaptureFormat()
{
    bool result = true;
    int retVal;

    ListFormats(V4lFd, "V4L2 capture", V4L2_BUF_TYPE_VIDEO_CAPTURE, "V4L2_BUF_TYPE_VIDEO_CAPTURE");

    // Check the correct loaded driver. 
    // "no dtoverlay" or "dtoverlay=tc358743"
    // "bm2835 mmal" or "unicam"
    // "mmal service 16.1" or "unicam"
    struct v4l2_capability cap;
    CLEAR(cap);
    // video0
    // 0x04000000 V4L2_CAP_STREAMING The device supports the streaming I/O method.
    // 0x01000000 V4L2_CAP_READWRITE The device supports the read() and/or write() I/O methods.
    // 0x00800000 V4L2_CAP_META_CAPTURE The device supports the Metadata Interface capture interface.
    // 0x00200000 V4L2_CAP_EXT_PIX_FORMAT The device supports the struct v4l2_pix_format extended fields.
    // 0x00000001 V4L2_CAP_VIDEO_CAPTURE The device supports the single-planar API through the Video Capture interface.
    retVal = ioctl(V4lFd, VIDIOC_QUERYCAP, &cap);
    if (retVal != 0)
    {
        result = false;
        printf("VIDIOC_QUERYCAP: %s\n", strerror(retVal));
    }
    if (strncmp((const char*)cap.driver, DriverName, strlen(DriverName)) != 0)
    {
        result = false;
        printf("Wrong driver. Set dtoverlay=tc358743 in /boot/config.txt\n");
    }

    struct v4l2_dv_timings tmg;
    CLEAR(tmg);
    retVal = ioctl(V4lFd, VIDIOC_QUERY_DV_TIMINGS, &tmg);
    if (retVal != 0)
    {
        result = false;
        printf("VIDIOC_QUERY_DV_TIMINGS: %s\n", strerror(retVal));
    }
    else
    {
        unsigned fps = (unsigned)(tmg.bt.pixelclock / ((tmg.bt.width + tmg.bt.hsync) * (tmg.bt.height + tmg.bt.vsync)));
        printf("HDMI input Width/Height: %u/%u@%uHz\n", tmg.bt.width, tmg.bt.height, fps);
        retVal = ioctl(V4lFd, VIDIOC_S_DV_TIMINGS, &tmg);
        if (retVal != 0)
        {
            result = false;
            printf("VIDIOC_S_DV_TIMINGS: %s\n", strerror(retVal));
        }
        else
        {
            SourceWidth = tmg.bt.width;
            SourceHeight = tmg.bt.height;
        }
    }

    struct v4l2_format fmt;
    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    retVal = ioctl(V4lFd, VIDIOC_G_FMT, &fmt);
    if (retVal != 0)
    {
        result = false;
        printf("VIDIOC_G_FMT: %s\n", strerror(retVal));
    }
    else
    {
        fmt.fmt.pix.width = SourceWidth;
        fmt.fmt.pix.height = SourceHeight;
        // v4l2-ctl -d /dev/video0 --list-formats
        // [0]: 'RGB3' (24-bit RGB 8-8-8)
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24; // v4l2_fourcc('R', 'G', 'B', '3') 24  RGB-8-8-8
        fmt.fmt.pix.field = V4L2_FIELD_NONE;
        retVal = ioctl(V4lFd, VIDIOC_S_FMT, &fmt);
        if (retVal != 0)
        {
            result = false;
            printf("VIDIOC_S_FMT: %s\n", strerror(retVal));
        }
        else
        {
            retVal = ioctl(V4lFd, VIDIOC_G_FMT, &fmt);
            if (retVal != 0)
            {
                result = false;
                printf("VIDIOC_G_FMT: %s\n", strerror(retVal));
            }
            else
            {
                printf("V4L capture (final): width = %u, height = %u, 4cc = %.4s\n",
                    fmt.fmt.pix.width, fmt.fmt.pix.height,
                    (char*)&fmt.fmt.pix.pixelformat);
            }
        }
    }
    return result;
}

bool SVideo::SetupIspOutputFormat()
{
    bool result = true;
    int retVal;

    ListFormats(IspFd, "ISP output", V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, "V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE");

    struct v4l2_capability cap;
    CLEAR(cap);
    // video12
    // 0x04000000 V4L2_CAP_STREAMING The device supports the streaming I / O method.
    // 0x00200000 V4L2_CAP_EXT_PIX_FORMAT The device supports the struct v4l2_pix_format extended fields.
    // 0x00004000 V4L2_CAP_VIDEO_M2M_MPLANE The device supports the multiplanar API through the Video Memory-To-Memory interface.
    retVal = ioctl(IspFd, VIDIOC_QUERYCAP, &cap);
    if (retVal != 0)
    {
        result = false;
        printf("VIDIOC_QUERYCAP: %s\n", strerror(retVal));
    }

    struct v4l2_format fmt;
    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    retVal = ioctl(IspFd, VIDIOC_G_FMT, &fmt);
    if (retVal != 0)
    {
        result = false;
        printf("VIDIOC_G_FMT: %s\n", strerror(retVal));
    }
    else
    {
        fmt.fmt.pix.width = SourceWidth;
        fmt.fmt.pix.height = SourceHeight;
        // v4l2-ctl -d /dev/video12 --list-formats
        // [31]: 'RGB3' (24-bit RGB 8-8-8)
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24; // v4l2_fourcc('B', 'G', 'R', '4') 32  BGR-8-8-8-8
        fmt.fmt.pix.field = V4L2_FIELD_NONE;
        fmt.fmt.pix_mp.width = fmt.fmt.pix.width;
        fmt.fmt.pix_mp.height = fmt.fmt.pix.height;
        fmt.fmt.pix_mp.pixelformat = fmt.fmt.pix.pixelformat;
        retVal = ioctl(IspFd, VIDIOC_S_FMT, &fmt);
        if (retVal != 0)
        {
            result = false;
            printf("VIDIOC_S_FMT: %s\n", strerror(retVal));
        }
        else
        {
            retVal = ioctl(IspFd, VIDIOC_G_FMT, &fmt);
            if (retVal != 0)
            {
                result = false;
                printf("VIDIOC_G_FMT: %s\n", strerror(retVal));
            }
            else
            {
                printf("ISP output (final): width = %u, height = %u, 4cc = %.4s\n",
                    fmt.fmt.pix.width, fmt.fmt.pix.height,
                    (char*)&fmt.fmt.pix.pixelformat);
            }
        }
    }

    return result;
}

bool SVideo::SetupIspCaptureFormat()
{
    bool result = true;
    int retVal;

    ListFormats(IspFd, "ISP capture", V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, "V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE");

    struct v4l2_capability cap;
    CLEAR(cap);
    // video12
    // 0x04000000 V4L2_CAP_STREAMING The device supports the streaming I / O method.
    // 0x00200000 V4L2_CAP_EXT_PIX_FORMAT The device supports the struct v4l2_pix_format extended fields.
    // 0x00004000 V4L2_CAP_VIDEO_M2M_MPLANE The device supports the multiplanar API through the Video Memory-To-Memory interface.
    retVal = ioctl(IspFd, VIDIOC_QUERYCAP, &cap);
    if (retVal != 0)
    {
        result = false;
        printf("VIDIOC_QUERYCAP: %s\n", strerror(retVal));
    }

    struct v4l2_format fmt;
    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    retVal = ioctl(IspFd, VIDIOC_G_FMT, &fmt);
    if (retVal != 0)
    {
        result = false;
        printf("VIDIOC_G_FMT: %s\n", strerror(retVal));
    }
    else
    {
        fmt.fmt.pix.width = SourceWidth;
        fmt.fmt.pix.height = SourceHeight;
        // v4l2-ctl -d /dev/video12 --list-formats
        // [8]: 'BGR4' (32-bit BGRA/X 8-8-8-8)
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR32; // v4l2_fourcc('B', 'G', 'R', '4') 32  BGR-8-8-8-8
        fmt.fmt.pix.field = V4L2_FIELD_NONE;
        fmt.fmt.pix_mp.width = fmt.fmt.pix.width;
        fmt.fmt.pix_mp.height = fmt.fmt.pix.height;
        fmt.fmt.pix_mp.pixelformat = fmt.fmt.pix.pixelformat;
        retVal = ioctl(IspFd, VIDIOC_S_FMT, &fmt);
        if (retVal != 0)
        {
            result = false;
            printf("VIDIOC_S_FMT: %s\n", strerror(retVal));
        }
        else
        {
            retVal = ioctl(IspFd, VIDIOC_G_FMT, &fmt);
            if (retVal != 0)
            {
                result = false;
                printf("VIDIOC_G_FMT: %s\n", strerror(retVal));
            }
            else
            {
                printf("ISP capture (final): width = %u, height = %u, 4cc = %.4s\n",
                    fmt.fmt.pix.width, fmt.fmt.pix.height,
                    (char*)&fmt.fmt.pix.pixelformat);
            }
        }
    }

    return result;
}

bool SVideo::SetupV4lCaptureQueue()
{
    bool result = true;
    int retVal;
    const tstQueueDesc& qd = QueueDesc[eQN_V4lCapture];

    // Export
    struct v4l2_requestbuffers req;
    CLEAR(req);
    req.type = qd.Type;
    req.memory = qd.Memory;
    req.count = DmaBuffers;
    retVal = ioctl(V4lFd, VIDIOC_REQBUFS, &req);
    if (retVal != 0)
    {
        result = false;
        printf("VIDIOC_REQBUFS: %s\n", strerror(retVal));
    }
    printf("VIDIOC_REQBUFS export: num %d, %s, %s\n", req.count, req.type == V4L2_BUF_TYPE_VIDEO_CAPTURE ? "V4L2_BUF_TYPE_VIDEO_CAPTURE" : "type error", req.memory == V4L2_MEMORY_MMAP ? "V4L2_MEMORY_MMAP" : "memory error");
    DmaBuffers = req.count;

    Texture.resize(req.count);
    for (unsigned i = 0; i < req.count; i++)
    {
        struct v4l2_buffer buf;
        CLEAR(buf);
        buf.index = i;
        buf.type = qd.Type;
        buf.memory = qd.Memory;
        retVal = ioctl(V4lFd, VIDIOC_QUERYBUF, &buf); // length
        if (retVal != 0)
        {
            result = false;
            printf("VIDIOC_QUERYBUF: %s\n", strerror(retVal));
        }

        struct v4l2_exportbuffer expbuf;
        CLEAR(expbuf);
        expbuf.type = qd.Type;
        expbuf.index = i;
        retVal = ioctl(V4lFd, VIDIOC_EXPBUF, &expbuf);
        if (retVal != 0)
        {
            result = false;
            printf("VIDIOC_EXPBUF: %s\n", strerror(retVal));
        }
        printf("VIDIOC_EXPBUF DMA fd %d for buffer index %d\n", expbuf.fd, i);

        V4lDmaFd.push_back(expbuf.fd);

        EnQueueV4lCapture(i);
    }
    return result;
}

bool SVideo::SetupIspOutputQueue()
{
    bool result = true;
    int retVal;
    const tstQueueDesc& qd = QueueDesc[eQN_IspOutput];

    // Import
    struct v4l2_requestbuffers req;
    CLEAR(req);
    req.type = qd.Type;
    req.memory = qd.Memory;
    req.count = DmaBuffers;
    retVal = ioctl(IspFd, VIDIOC_REQBUFS, &req);
    if (retVal != 0)
    {
        result = false;
        printf("VIDIOC_REQBUFS: %s\n", strerror(retVal));
    }
    printf("VIDIOC_REQBUFS import: num %d, %s, %s\n", req.count, req.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ? "V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE" : "type error", req.memory == V4L2_MEMORY_DMABUF ? "V4L2_MEMORY_DMABUF" : "memory error");

    for (unsigned i = 0; i < req.count; i++)
    {
        struct v4l2_buffer buf;
        struct v4l2_plane planes[VIDEO_MAX_PLANES];

        CLEAR(buf);
        buf.index = i;
        buf.type = qd.Type;
        buf.memory = qd.Memory;
        buf.length = VIDEO_MAX_PLANES;
        buf.m.planes = planes;
        retVal = ioctl(IspFd, VIDIOC_QUERYBUF, &buf); // length
        if (retVal != 0)
        {
            result = false;
            printf("VIDIOC_QUERYBUF: %s\n", strerror(retVal));
        }
        IspOutputBufferSize = buf.m.planes[0].length;
    }
    return result;
}

bool SVideo::SetupIspCaptureQueue()
{
    bool result = true;
    int retVal;
    const tstQueueDesc& qd = QueueDesc[eQN_IspCapture];

    // Export
    struct v4l2_requestbuffers req;
    CLEAR(req);
    req.type = qd.Type;
    req.memory = qd.Memory;
    req.count = DmaBuffers;
    retVal = ioctl(IspFd, VIDIOC_REQBUFS, &req);
    if (retVal != 0)
    {
        result = false;
        printf("VIDIOC_REQBUFS: %s\n", strerror(retVal));
    }
    printf("VIDIOC_REQBUFS export: num %d, %s, %s\n", req.count, req.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ? "V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE" : "type error", req.memory == V4L2_MEMORY_MMAP ? "V4L2_MEMORY_MMAP" : "memory error");

    Texture.resize(req.count);
    for (unsigned i = 0; i < req.count; i++)
    {
        struct v4l2_buffer buf;
        struct v4l2_plane planes[VIDEO_MAX_PLANES];

        CLEAR(buf);
        buf.index = i;
        buf.type = qd.Type;
        buf.memory = qd.Memory;
        buf.length = VIDEO_MAX_PLANES;
        buf.m.planes = planes;
        retVal = ioctl(IspFd, VIDIOC_QUERYBUF, &buf); // length
        if (retVal != 0)
        {
            result = false;
            printf("VIDIOC_QUERYBUF: %s\n", strerror(retVal));
        }

        struct v4l2_exportbuffer expbuf;
        CLEAR(expbuf);
        expbuf.type = qd.Type;
        expbuf.index = i;
        retVal = ioctl(IspFd, VIDIOC_EXPBUF, &expbuf);
        if (retVal != 0)
        {
            result = false;
            printf("VIDIOC_EXPBUF: %s\n", strerror(retVal));
        }
        printf("VIDIOC_EXPBUF DMA fd %d for buffer index %d\n", expbuf.fd, i);

        IspDmaFd.push_back(expbuf.fd);

        // Supported 32 bit formats: 
        // DRM_FORMAT_XRGB8888 ('X', 'R', '2', '4'), DRM_FORMAT_XBGR8888 ('X', 'B', '2', '4'), DRM_FORMAT_ARGB8888 ('A', 'R', '2', '4'), DRM_FORMAT_ABGR8888 ('A', 'B', '2', '4')
        Texture[i] = CreateSourceImage(expbuf.fd, SourceWidth, SourceHeight, DRM_FORMAT_ARGB8888); // A channel needed for R

        EnQueueIspCapture(i);
    }
    return result;
}

void SVideo::ListFormats(int fd, string devStr, unsigned int type, string typeStr)
{
    int retVal;
    struct v4l2_fmtdesc fmt;
    int i = 0;
    do {
        CLEAR(fmt);
        fmt.index = i++;
        fmt.type = type;
        retVal = ioctl(fd, VIDIOC_ENUM_FMT, &fmt);
        if (retVal == 0)
        {
            printf("%s: [%u] %.4s\n", devStr.c_str(), fmt.index, (char*)&fmt.pixelformat);
        }
    } while (retVal == 0);
}

// Put buffer into V4L queue
void SVideo::EnQueueV4lCapture(int index)
{
    const tstQueueDesc& qd = QueueDesc[eQN_V4lCapture];
    struct v4l2_buffer buf;
    CLEAR(buf);
    buf.type = qd.Type;
    buf.memory = qd.Memory;
    buf.index = index;
    int retVal = ioctl(V4lFd, VIDIOC_QBUF, &buf); // bytesused and length
    if (retVal != 0)
    {
        printf("VIDIOC_QBUF: %s\n", strerror(retVal));
    }
}

// Get buffer from V4L queue
int SVideo::DeQueueV4lCapture()
{
    const tstQueueDesc& qd = QueueDesc[eQN_V4lCapture];
    struct v4l2_buffer buf;
    CLEAR(buf);
    buf.type = qd.Type;
    buf.memory = qd.Memory;
    int retVal = ioctl(V4lFd, VIDIOC_DQBUF, &buf);
    if (retVal != 0)
    {
        printf("VIDIOC_DQBUF: %s\n", strerror(retVal));
    }
    return buf.index;
}

// Put buffer into ISP output queue
void SVideo::EnQueueIspOutput(int index)
{
    const tstQueueDesc& qd = QueueDesc[eQN_IspOutput];
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    CLEAR(planes);
    struct v4l2_buffer buf;
    CLEAR(buf);
    buf.type = qd.Type;
    buf.memory = qd.Memory;
    buf.index = index;
    buf.length = 1; // Only one plane used
    buf.m.planes = planes; // Shorten to used planes
    for (int k = 0; k < VIDEO_MAX_PLANES; k++) // loop can be shortended to buf.length
    {
        buf.m.planes[k].m.fd = V4lDmaFd[index];
        buf.m.planes[k].bytesused = IspOutputBufferSize;
        buf.m.planes[k].length = IspOutputBufferSize;
    }
    int retVal = ioctl(IspFd, VIDIOC_QBUF, &buf);
    if (retVal != 0)
    {
        printf("VIDIOC_QBUF: %s\n", strerror(retVal));
    }
}

// Get buffer from ISP output queue
int SVideo::DeQueueIspOutput()
{
    const tstQueueDesc& qd = QueueDesc[eQN_IspOutput];
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    CLEAR(planes);
    struct v4l2_buffer buf;
    CLEAR(buf);
    buf.type = qd.Type;
    buf.memory = qd.Memory;
    buf.length = VIDEO_MAX_PLANES;
    CLEAR(planes);
    buf.m.planes = planes;
    int retVal = ioctl(IspFd, VIDIOC_DQBUF, &buf);
    if (retVal != 0)
    {
        printf("VIDIOC_DQBUF: %s\n", strerror(retVal));
    }
    return buf.index;
}

// Put buffer into ISP capture queue
void SVideo::EnQueueIspCapture(int index)
{
    const tstQueueDesc& qd = QueueDesc[eQN_IspCapture];
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    CLEAR(planes);
    struct v4l2_buffer buf;
    CLEAR(buf);
    buf.type = qd.Type;
    buf.memory = qd.Memory;
    buf.index = index;
    buf.length = VIDEO_MAX_PLANES;
    buf.m.planes = planes;
    for (int k = 0; k < VIDEO_MAX_PLANES; k++)
    {
        buf.m.planes[k].m.fd = V4lDmaFd[index];
    }
    int retVal = ioctl(IspFd, VIDIOC_QBUF, &buf); // bytesused and length
    if (retVal != 0)
    {
        printf("VIDIOC_QBUF: %s\n", strerror(retVal));
    }
}

// Get buffer from ISP capture queue
int SVideo::DeQueueIspCapture()
{
    const tstQueueDesc& qd = QueueDesc[eQN_IspCapture];
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    CLEAR(planes);
    struct v4l2_buffer buf;
    CLEAR(buf);
    buf.type = qd.Type;
    buf.memory = qd.Memory;
    buf.length = VIDEO_MAX_PLANES;
    CLEAR(planes);
    buf.m.planes = planes;
    int retVal = ioctl(IspFd, VIDIOC_DQBUF, &buf);
    if (retVal != 0)
    {
        printf("VIDIOC_DQBUF: %s\n", strerror(retVal));
    }
    return buf.index;
}

int SVideo::ProcessQueueV4lCapture()
{
    int& lastBufferIndex = QueueDesc[eQN_V4lCapture].LastBufferIndex;
    int index = DeQueueV4lCapture();
    if (lastBufferIndex >= 0)
    {
        EnQueueV4lCapture(lastBufferIndex);
    }
    lastBufferIndex = index;
    return index;
}

void SVideo::ProcessQueueIspOutput(int index)
{
    int& lastBufferIndex = QueueDesc[eQN_IspOutput].LastBufferIndex;
    EnQueueIspOutput(index);
    if (lastBufferIndex >= 0)
    {
        DeQueueIspOutput();
    }
    lastBufferIndex = index;
}

int SVideo::ProcessQueueIspCapture()
{
    int& lastBufferIndex = QueueDesc[eQN_IspCapture].LastBufferIndex;
    int index = DeQueueIspCapture();
    if (lastBufferIndex >= 0)
    {
        EnQueueIspCapture(lastBufferIndex);
    }
    lastBufferIndex = index;
    return index;
}

int SVideo::ProcessQueues()
{
    int index;

    index = ProcessQueueV4lCapture();
    ProcessQueueIspOutput(index);
    index = ProcessQueueIspCapture();

    return index;
}

// Cyclic called from main.
void SVideo::FrameProcessing()
{
    int index = ProcessQueues();
    SelectTexture(index);
}

#pragma once

#include <string>
#include <vector>

class SVideo
{
public:
    SVideo();
    bool Create();
    void Destroy();

    void FrameProcessing();

protected:

private:
    enum EQueueName
    {
        eQN_V4lCapture,
        eQN_IspOutput,
        eQN_IspCapture,
        eQN_Last
    };

    struct tstQueueDesc
    {
        tstQueueDesc(int lbi, unsigned type, unsigned memory) : LastBufferIndex(lbi), Type(type), Memory(memory)
        {
        };
        int LastBufferIndex;
        const unsigned Type;
        const unsigned Memory;
    };

    bool SetupV4lCaptureFormat();
    bool SetupIspCaptureFormat();
    bool SetupIspOutputFormat();
    bool SetupV4lCaptureQueue();
    bool SetupIspOutputQueue();
    bool SetupIspCaptureQueue();
    void ListFormats(int fd, std::string devStr, unsigned int type, std::string fmtStr);
    void EnQueueV4lCapture(int index);
    int DeQueueV4lCapture();
    void EnQueueIspOutput(int index);
    int DeQueueIspOutput();
    void EnQueueIspCapture(int index);
    int DeQueueIspCapture();
    int ProcessQueues();
    int ProcessQueueV4lCapture();
    void ProcessQueueIspOutput(int index);
    int ProcessQueueIspCapture();

    int V4lFd;
    int IspFd;
    unsigned SourceWidth;
    unsigned SourceHeight;
    unsigned DmaBuffers; // Finally requested DMA buffers for each queue
    unsigned IspOutputBufferSize;
    std::vector<tstQueueDesc> QueueDesc;
    std::string V4lName;
    std::string IspName;
    std::vector<int> V4lDmaFd; // DMA file descriptor associated to buffer index
    std::vector<int> IspDmaFd; // DMA file descriptor associated to buffer index
    std::vector<unsigned> Texture; // Texture name index of the created image
};

#pragma once

#include <cstddef>
#include <cstdint>

class WavReader
{
public:
    typedef size_t (*TellCallback)(void *file_context);
    typedef bool (*SeekCallback)(void *file_context, size_t offset);
    typedef size_t (*ReadCallback)(void *file_context, uint8_t *buffer, size_t length);

    enum class Format : unsigned int
    {
        PCM = 1
    };

    static const unsigned int MAX_CHANNELS = 2;

    static const unsigned int MAX_FRAME_SIZE = 16;

public:
    WavReader(void *file_context,
              TellCallback tell_callback,
              SeekCallback seek_callback,
              ReadCallback read_callback);

    bool open();

    size_t decodeToI16(int16_t *buffer, size_t frames);

    bool opened()
    {
        return opened_;
    }

    Format format()
    {
        return format_;
    }

    unsigned int channels()
    {
        return channels_;
    }

    unsigned long samplingRate()
    {
        return sampling_rate_;
    }

    unsigned long bytesPerSecond()
    {
        return bytes_per_second_;
    }

    size_t blockAlignment()
    {
        return block_alignment_;
    }

    unsigned int bitsPerSample()
    {
        return bits_per_sample_;
    }

    size_t frameSize()
    {
        return frame_size_;
    }

private:
    size_t tell();
    bool seek(size_t offset);
    size_t read(uint8_t *buffer, size_t length);

    bool readU16(uint16_t *value);
    bool readU32(uint32_t *value);
    bool readCharBuffer(char *buffer, size_t length);

    bool decodeNextFrame();
    bool decodeNextPcmFrame();

    size_t decodeUxToI16(int16_t *buffer, size_t frames);
    size_t decodeIxToI16(int16_t *buffer, size_t frames);

private:
    bool opened_;

    void *file_context_;

    TellCallback tell_callback_;
    SeekCallback seek_callback_;
    ReadCallback read_callback_;

    size_t file_size_;

    Format format_;
    unsigned int channels_;
    unsigned long sampling_rate_;
    unsigned long bytes_per_second_;
    size_t block_alignment_;
    unsigned int bits_per_sample_;
    size_t frame_size_;
    size_t channel_size_;

    size_t next_data_chunk_offset_;
    size_t current_data_chunk_frames_;

    uint8_t frame_[MAX_FRAME_SIZE];

    bool silence_;
};

#include "wavreader.h"

#include <cstring>
#include <endian.h>

WavReader::WavReader(TellCallback tell_callback,
                     SeekCallback seek_callback,
                     ReadCallback read_callback)
    : opened_(false),
      tell_callback_(tell_callback),
      seek_callback_(seek_callback),
      read_callback_(read_callback)
{
}

bool WavReader::open(void *file_context)
{
    char chunk_id[4];
    uint32_t chunk_size;
    size_t next_chunk_offset;

    opened_ = false;

    file_context_ = file_context;

    if (!seek(0)) {
        return false;
    }

    if (!readCharBuffer(chunk_id, sizeof(chunk_id))) {
        return false;
    }

    if (memcmp(chunk_id, "RIFF", sizeof(chunk_id)) != 0) {
        return false;
    }

    if (!readU32(&chunk_size)) {
        return false;
    }

    file_size_ = chunk_size;

    char riff_type[4];

    if (!readCharBuffer(riff_type, sizeof(riff_type))) {
        return false;
    }

    if (memcmp(riff_type, "WAVE", sizeof(riff_type)) != 0) {
        return false;
    }

    if (!readCharBuffer(chunk_id, sizeof(chunk_id))) {
        return false;
    }

    if (memcmp(chunk_id, "fmt ", sizeof(chunk_id)) != 0) {
        return false;
    }

    if (!readU32(&chunk_size)) {
        return false;
    }

    next_chunk_offset = tell() + chunk_size;
    if ((next_chunk_offset & 1) != 0) {
        next_chunk_offset++;
    }

    uint16_t format;

    if (!readU16(&format)) {
        return false;
    }

    switch (format) {
    case static_cast<uint16_t>(Format::PCM):
        format_ = Format::PCM;
        break;
    default:
        return false;
    }

    uint16_t channels;

    if (!readU16(&channels)) {
        return false;
    }

    if (channels > MAX_CHANNELS) {
        return false;
    }

    channels_ = channels;

    uint32_t sampling_rate;

    if (!readU32(&sampling_rate)) {
        return false;
    }

    sampling_rate_ = sampling_rate;

    uint32_t bytes_per_second;

    if (!readU32(&bytes_per_second)) {
        return false;
    }

    bytes_per_second_ = bytes_per_second;

    uint16_t block_alignment;

    if (!readU16(&block_alignment)) {
        return false;
    }

    block_alignment_ = block_alignment;

    if (format_ == Format::PCM) {
        uint16_t bits_per_sample;

        if (!readU16(&bits_per_sample)) {
            return false;
        }

        bits_per_sample_ = bits_per_sample;

        frame_size_ = block_alignment_;

        if (frame_size_ > MAX_FRAME_SIZE) {
            return false;
        }

        channel_size_ = frame_size_ / channels_;
    } else {
        bits_per_sample_ = 0;

        frame_size_ = 0;

        channel_size_ = 0;
    }

    while (true) {
        if (!seek(next_chunk_offset)) {
            return false;
        }

        if (!readCharBuffer(chunk_id, sizeof(chunk_id))) {
            return false;
        }

        if (memcmp(chunk_id, "data", sizeof(chunk_id)) == 0) {
            current_data_chunk_frames_ = 0;
            next_data_chunk_offset_ = next_chunk_offset;
            if ((next_data_chunk_offset_ & 1) != 0) {
                next_data_chunk_offset_++;
            }

            break;
        }

        if (memcmp(chunk_id, "LIST", sizeof(chunk_id)) == 0) {
            if (!readU32(&chunk_size)) {
                return false;
            }

            char list_type[4];

            if (!readCharBuffer(list_type, sizeof(list_type))) {
                return false;
            }

            if (memcmp(list_type, "wavl", sizeof(list_type)) != 0) {
                return false;
            }

            current_data_chunk_frames_ = 0;
            next_data_chunk_offset_ = tell();
            if ((next_data_chunk_offset_ & 1) != 0) {
                next_data_chunk_offset_++;
            }

            break;
        }

        if (!readU32(&chunk_size)) {
            return false;
        }

        next_chunk_offset = tell() + chunk_size;
        if ((next_chunk_offset & 1) != 0) {
            next_chunk_offset++;
        }
    };

    memset(frame_, 0, sizeof(frame_));

    opened_ = true;

    return true;
}

size_t WavReader::decodeToI16(int16_t *buffer, size_t frames)
{
    if (!opened_) {
        return 0;
    }

    if (channel_size_ == 1) {
        return decodeUxToI16(buffer, frames);
    } else {
        return decodeIxToI16(buffer, frames);
    }
}

size_t WavReader::decodeUxToI16(int16_t *buffer, size_t frames)
{
    int16_t sample = 0;

    for (size_t frame_index = 0; frame_index < frames; frame_index++) {
        if (!decodeNextFrame()) {
            return frame_index;
        }

        for (unsigned int channel = 0; channel < channels_; channel++) {
            sample = static_cast<int16_t>(frame_[channel]) - 128;
            sample = static_cast<int16_t>(sample << 8);

            buffer[frame_index * channels_ + channel] = sample;
        }
    }

    return frames;
}

size_t WavReader::decodeIxToI16(int16_t *buffer, size_t frames)
{
    int16_t sample;

    for (size_t frame_index = 0; frame_index < frames; frame_index++) {
        if (!decodeNextFrame()) {
            return frame_index;
        }

        uint8_t *sample_pointer = frame_ + channel_size_ - sizeof(sample);

        for (unsigned int channel = 0; channel < channels_; channel++) {
            memcpy(&sample,
                   sample_pointer,
                   sizeof(sample));

            sample_pointer += channel_size_;

            buffer[frame_index * channels_ + channel] = le16toh(sample);
        }
    }

    return frames;
}

size_t WavReader::tell()
{
    return tell_callback_(file_context_);
}

bool WavReader::seek(size_t offset)
{
    return seek_callback_(file_context_, offset);
}

size_t WavReader::read(uint8_t *buffer, size_t length)
{
    return read_callback_(file_context_, buffer, length);
}

bool WavReader::readU16(uint16_t *value)
{
    if (read(reinterpret_cast<uint8_t *>(value),
             sizeof(uint16_t)) < sizeof(uint16_t)) {
        return false;
    }

    value = le16toh(value);

    return true;
}

bool WavReader::readU32(uint32_t *value)
{
    if (read(reinterpret_cast<uint8_t *>(value),
             sizeof(uint32_t)) < sizeof(uint32_t)) {
        return false;
    }

    value = le32toh(value);

    return true;
}

bool WavReader::readCharBuffer(char *buffer, size_t length)
{
    if (read(reinterpret_cast<uint8_t *>(buffer),
             length) < length) {
        return false;
    }

    return true;
}

bool WavReader::decodeNextFrame()
{
    switch (format_) {
    case Format::PCM:
        return decodeNextPcmFrame();
    }

    return false;
}

bool WavReader::decodeNextPcmFrame()
{
    char chunk_id[4];
    uint32_t chunk_size;

    if (current_data_chunk_frames_ == 0) {
        if (!seek(next_data_chunk_offset_)) {
            return false;
        }

        if (!readCharBuffer(chunk_id, sizeof(chunk_id))) {
            return false;
        }

        if (memcmp(chunk_id, "data", sizeof(chunk_id)) == 0) {
            silence_ = false;
        } else if (memcmp(chunk_id, "slnt", sizeof(chunk_id)) == 0) {
            silence_ = true;
        } else {
            return false;
        }

        if (!readU32(&chunk_size)) {
            return false;
        }

        if (!silence_) {
            current_data_chunk_frames_ = chunk_size / frame_size_;
        } else {
            uint32_t silent_frames;

            if (!readU32(&silent_frames)) {
                return false;
            }

            current_data_chunk_frames_ = silent_frames;
        }

        next_data_chunk_offset_ = tell() + chunk_size;
        if ((next_data_chunk_offset_ & 1) != 0) {
            next_data_chunk_offset_++;
        }
    }

    if (!silence_) {
        if (read(frame_, frame_size_) != frame_size_) {
            return false;
        }
    }

    current_data_chunk_frames_--;

    return true;
}

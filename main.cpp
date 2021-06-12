#include <cstdio>
#include <vector>

#include <pulse/simple.h>
#include <pulse/error.h>

#include "wavreader.h"

size_t tell_callback(void *file_context)
{
    long result = ftell(reinterpret_cast<FILE *>(file_context));

    if (result >= 0) {
        return static_cast<size_t>(result);
    }

    return 0;
}

bool seek_callback(void *file_context, size_t offset)
{
    int result = fseek(reinterpret_cast<FILE *>(file_context),
                       static_cast<long>(offset),
                       SEEK_SET);

    if (result >= 0) {
        return true;
    }

    return false;
}

size_t read_callback(void *file_context, uint8_t *buffer, size_t length)
{
    return fread(buffer,
                 1,
                 length,
                 reinterpret_cast<FILE *>(file_context));
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file> [mode]\n", argv[0]);
        return 1;
    }

    WavReader::Mode mode = WavReader::Mode::Single;

    if (argc >= 3) {
        switch (argv[2][0]) {
        case 'c':
            mode = WavReader::Mode::Continuous;
            break;
        case 's':
        default:
            mode = WavReader::Mode::Single;
        }
    }

    switch (mode) {
    case WavReader::Mode::Single:
        printf("Single mode\n");
        break;
    case WavReader::Mode::Continuous:
        printf("Continuous mode\n");
        break;
    }

    FILE *wav_file = fopen(argv[1], "r");
    if (wav_file == nullptr) {
        fprintf(stderr, "Cannot open file \"%s\"\n", argv[1]);
        return 1;
    }

    WavReader reader(&tell_callback,
                     &seek_callback,
                     &read_callback);

    if (!reader.open(wav_file, mode, true)) {
        fprintf(stderr, "Cannot parse WAV file header\n");
        fclose(wav_file);
        return 1;
    }

    printf("Format: ");
    switch (reader.format()) {
    case WavReader::Format::Pcm:
        printf("PCM\n");
        break;
#ifdef HAS_IEEE_FLOAT
    case WavReader::Format::IeeeFloat:
        printf("IEEE float\n");
        break;
#endif
    }

    printf("Channels: %u\n", reader.channels());

    printf("Sampling rate: %lu\n", reader.samplingRate());

    printf("Bytes per second: %lu\n", reader.bytesPerSecond());

    printf("Block alignment: %zu\n", reader.blockAlignment());

    printf("Bits per sample: %u\n", reader.bitsPerSample());

    pa_sample_spec sample_format = {
        .format = PA_SAMPLE_S16NE,
        .rate = static_cast<uint32_t>(reader.samplingRate()),
        .channels = static_cast<uint8_t>(reader.channels())
    };

    pa_simple *stream = nullptr;
    int error;

    stream = pa_simple_new(nullptr,
                           argv[0],
                           PA_STREAM_PLAYBACK,
                           nullptr,
                           "playback",
                           &sample_format,
                           nullptr,
                           nullptr,
                           &error);
    if (stream == nullptr) {
        fprintf(stderr,
                "pa_simple_new() failed: %s\n",
                pa_strerror(error));
        reader.close();
        fclose(wav_file);
        return 1;
    }

    static const size_t MAX_BUFFER_FRAMES = 1024;
    std::vector<int16_t> buffer(MAX_BUFFER_FRAMES * reader.channels());

    for (;;) {
        size_t frames = reader.decodeToI16(buffer.data(), MAX_BUFFER_FRAMES);
        if (frames == 0) {
            break;
        }

        if (pa_simple_write(stream,
                            buffer.data(),
                            frames * reader.channels() * sizeof(int16_t),
                            &error) < 0) {
            fprintf(stderr,
                    "pa_simple_write() failed: %s\n",
                    pa_strerror(error));
            pa_simple_free(stream);
            reader.close();
            fclose(wav_file);
            return 1;
        }
    }

    if (pa_simple_drain(stream, &error) < 0) {
        fprintf(stderr,
                "pa_simple_drain() failed: %s\n",
                pa_strerror(error));
        pa_simple_free(stream);
        reader.close();
        fclose(wav_file);
        return 1;
    }

    pa_simple_free(stream);
    reader.close();
    fclose(wav_file);

    return 0;
}

#ifndef WAV_IO_H
#define WAV_IO_H

#include <fstream>
#include <cstdint>

struct WavHeader {
    char riff[4];           // "RIFF"
    uint32_t file_size;     // File size - 8
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmt_size;      // Format chunk size
    uint16_t audio_format;  // Audio format (1 for PCM)
    uint16_t num_channels;  // Number of channels
    uint32_t sample_rate;   // Sample rate
    uint32_t byte_rate;     // Byte rate
    uint16_t block_align;   // Block align
    uint16_t bits_per_sample; // Bits per sample
    char data[4];           // "data"
    uint32_t data_size;     // Data size
};

bool readWavHeader(std::ifstream& file, WavHeader& header, uint32_t& sample_rate, uint16_t& channels);
void writeWavHeader(std::ofstream& file, uint32_t sample_rate, uint16_t channels, uint32_t data_size);
bool skipToDataChunk(std::ifstream& file);

#endif // WAV_IO_H
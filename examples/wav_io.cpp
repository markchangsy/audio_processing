#include "wav_io.h"
#include <cstring>
#include <iostream>

bool readWavHeader(std::ifstream& file, WavHeader& header, uint32_t& sample_rate, uint16_t& channels) {
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (file.gcount() != sizeof(header)) {
        std::cerr << "Error: Could not read WAV header" << std::endl;
        return false;
    }
    
    if (strncmp(header.riff, "RIFF", 4) != 0) {
        std::cerr << "Error: Not a valid RIFF file" << std::endl;
        return false;
    }
    
    if (strncmp(header.wave, "WAVE", 4) != 0) {
        std::cerr << "Error: Not a WAVE file" << std::endl;
        return false;
    }
    
    if (strncmp(header.fmt, "fmt ", 4) != 0) {
        std::cerr << "Error: fmt chunk not found" << std::endl;
        return false;
    }
    
    if (header.audio_format != 1) {
        std::cerr << "Error: Only PCM format supported" << std::endl;
        return false;
    }
    
    if (header.bits_per_sample != 16) {
        std::cerr << "Error: Only 16-bit samples supported" << std::endl;
        return false;
    }
    
    sample_rate = header.sample_rate;
    channels = header.num_channels;
    
    // Skip to data chunk if it's not immediately following fmt
    if (strncmp(header.data, "data", 4) != 0) {
        return skipToDataChunk(file);
    }
    
    return true;
}

bool skipToDataChunk(std::ifstream& file) {
    char chunk_id[4];
    uint32_t chunk_size;
    
    // Go back to after fmt chunk
    file.seekg(20 + sizeof(uint32_t) + 16, std::ios::beg);
    
    while (file.read(chunk_id, 4)) {
        file.read(reinterpret_cast<char*>(&chunk_size), sizeof(chunk_size));
        
        if (strncmp(chunk_id, "data", 4) == 0) {
            return true;
        }
        
        // Skip this chunk
        file.seekg(chunk_size, std::ios::cur);
    }
    
    std::cerr << "Error: data chunk not found" << std::endl;
    return false;
}

void writeWavHeader(std::ofstream& file, uint32_t sample_rate, uint16_t channels, uint32_t data_size) {
    WavHeader header;
    
    strncpy(header.riff, "RIFF", 4);
    header.file_size = 36 + data_size;
    strncpy(header.wave, "WAVE", 4);
    strncpy(header.fmt, "fmt ", 4);
    header.fmt_size = 16;
    header.audio_format = 1;  // PCM
    header.num_channels = channels;
    header.sample_rate = sample_rate;
    header.bits_per_sample = 16;
    header.byte_rate = sample_rate * channels * header.bits_per_sample / 8;
    header.block_align = channels * header.bits_per_sample / 8;
    strncpy(header.data, "data", 4);
    header.data_size = data_size;
    
    file.write(reinterpret_cast<char*>(&header), sizeof(header));
}
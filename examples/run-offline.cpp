#include "api/scoped_refptr.h"
#include <cstdlib>
#include <iostream>
#include <fstream>

#include <webrtc/modules/audio_processing/include/audio_processing.h>
#include "wav_io.h"

#define DEFAULT_BLOCK_MS 10
#define DEFAULT_RATE 16000
#define DEFAULT_CHANNELS 1

int main(int argc, char **argv) {
    if (argc != 4) {
	std::cerr << "Usage: " << argv[0] << " <farend_file.wav> <nearend_file.wav> <out_file.wav>" << std::endl;
	return EXIT_FAILURE;
    }

    std::ifstream play_file(argv[1], std::ios::binary);
    std::ifstream rec_file(argv[2], std::ios::binary);
    std::ofstream aec_file(argv[3], std::ios::binary);

    if (!play_file.is_open()) {
        std::cerr << "Error: Cannot open play file " << argv[1] << std::endl;
        return EXIT_FAILURE;
    }
    
    if (!rec_file.is_open()) {
        std::cerr << "Error: Cannot open rec file " << argv[2] << std::endl;
        return EXIT_FAILURE;
    }
    
    if (!aec_file.is_open()) {
        std::cerr << "Error: Cannot open output file " << argv[3] << std::endl;
        return EXIT_FAILURE;
    }

    // Read WAV headers
    WavHeader play_header, rec_header;
    uint32_t play_sample_rate, rec_sample_rate;
    uint16_t play_channels, rec_channels;
    
    if (!readWavHeader(play_file, play_header, play_sample_rate, play_channels)) {
        std::cerr << "Error: Cannot read play file WAV header" << std::endl;
        return EXIT_FAILURE;
    }
    
    if (!readWavHeader(rec_file, rec_header, rec_sample_rate, rec_channels)) {
        std::cerr << "Error: Cannot read rec file WAV header" << std::endl;
        return EXIT_FAILURE;
    }
    
    // Use the parameters from the input files (prefer rec file parameters for output)
    uint32_t sample_rate = rec_sample_rate;
    uint16_t channels = rec_channels;
    
    std::cout << "Play file: " << play_sample_rate << "Hz, " << play_channels << " channels" << std::endl;
    std::cout << "Rec file: " << rec_sample_rate << "Hz, " << rec_channels << " channels" << std::endl;

    rtc::scoped_refptr<webrtc::AudioProcessing> apm = webrtc::AudioProcessingBuilder().Create();

    webrtc::AudioProcessing::Config config;
    config.echo_canceller.enabled = true;
    
    // AGC1
    config.gain_controller1.enabled = false;
    config.gain_controller1.mode = webrtc::AudioProcessing::Config::GainController1::kAdaptiveDigital;

    // AGC2
    config.gain_controller2.enabled = false;
    config.gain_controller2.adaptive_digital.enabled = false;
    
    // high pass
    config.high_pass_filter.enabled = false;

    // noise suppression
    config.noise_suppression.enabled = false;
    config.noise_suppression.level = webrtc::AudioProcessing::Config::NoiseSuppression::kHigh;

    apm->ApplyConfig(config);

    webrtc::StreamConfig play_stream_config(play_sample_rate, play_channels);
    webrtc::StreamConfig rec_stream_config(rec_sample_rate, rec_channels);
    
    // Write WAV header for output file (we'll update data size later)
    std::streampos header_pos = aec_file.tellp();
    writeWavHeader(aec_file, sample_rate, channels, 0);
    uint32_t total_data_size = 0;

    while (!play_file.eof() && !rec_file.eof()) {
        // Calculate frame size based on actual file parameters
        size_t play_frame_size = play_sample_rate * DEFAULT_BLOCK_MS / 1000 * play_channels;
        size_t rec_frame_size = rec_sample_rate * DEFAULT_BLOCK_MS / 1000 * rec_channels;
        
        int16_t* play_frame = new int16_t[play_frame_size];
        int16_t* rec_frame = new int16_t[rec_frame_size];

        size_t play_bytes_read = play_file.read(reinterpret_cast<char *>(play_frame), play_frame_size * sizeof(int16_t)).gcount();
        size_t rec_bytes_read = rec_file.read(reinterpret_cast<char *>(rec_frame), rec_frame_size * sizeof(int16_t)).gcount();

        // Check if we have enough data for processing
        if (play_bytes_read < play_frame_size * sizeof(int16_t) || rec_bytes_read < rec_frame_size * sizeof(int16_t)) {
            delete[] play_frame;
            delete[] rec_frame;
            break;
        }

        apm->ProcessReverseStream(play_frame, play_stream_config, play_stream_config, play_frame);
        apm->ProcessStream(rec_frame, rec_stream_config, rec_stream_config, rec_frame);

        aec_file.write(reinterpret_cast<char *>(rec_frame), rec_frame_size * sizeof(int16_t));
        total_data_size += rec_frame_size * sizeof(int16_t);
        
        delete[] play_frame;
        delete[] rec_frame;
    }

    // Update WAV header with correct data size
    std::streampos current_pos = aec_file.tellp();
    aec_file.seekp(header_pos);
    writeWavHeader(aec_file, sample_rate, channels, total_data_size);
    aec_file.seekp(current_pos);

    play_file.close();
    rec_file.close();
    aec_file.close();
    
    std::cout << "Processing complete. Output written to " << argv[3] << std::endl;
    std::cout << "Processed " << total_data_size / (channels * sizeof(int16_t)) << " samples" << std::endl;

    return EXIT_SUCCESS;
}

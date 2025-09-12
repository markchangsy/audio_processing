#include "api/scoped_refptr.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <sstream>
#include <filesystem>

#include <webrtc/modules/audio_processing/include/audio_processing.h>
#include <webrtc/modules/audio_processing/include/audio_processing_statistics.h>
#include "wav_io.h"

#define DEFAULT_BLOCK_MS 10
#define DEFAULT_RATE 16000
#define DEFAULT_CHANNELS 1

class DataDumper {
private:
    bool enabled;
    std::string dump_dir;
    int frame_counter;
    std::ofstream play_raw_file;
    std::ofstream rec_raw_file;
    std::ofstream processed_raw_file;
    std::ofstream debug_log_file;
    std::ofstream echo_metrics_file;

public:
    DataDumper(bool enable_dump, const std::string& output_prefix) 
        : enabled(enable_dump), frame_counter(0) {
        if (!enabled) return;
        
        dump_dir = output_prefix + "_debug_dump";
        std::filesystem::create_directories(dump_dir);
        
        play_raw_file.open(dump_dir + "/play_raw.pcm", std::ios::binary);
        rec_raw_file.open(dump_dir + "/rec_raw.pcm", std::ios::binary);
        processed_raw_file.open(dump_dir + "/processed_raw.pcm", std::ios::binary);
        debug_log_file.open(dump_dir + "/debug_log.txt");
        echo_metrics_file.open(dump_dir + "/echo_metrics.txt");
        
        if (debug_log_file.is_open()) {
            debug_log_file << "Frame,Play_RMS,Rec_RMS,Processed_RMS,Play_Peak,Rec_Peak,Processed_Peak\n";
        }
        
        if (echo_metrics_file.is_open()) {
            echo_metrics_file << "Frame,ERL_dB,ERLE_dB,Filter_Delay_ms,Residual_Echo_Likelihood,Echo_Detected,AEC_Quality\n";
        }
        
        std::cout << "[DEBUG] Data dump directory created: " << dump_dir << std::endl;
    }
    
    ~DataDumper() {
        if (!enabled) return;
        
        play_raw_file.close();
        rec_raw_file.close();
        processed_raw_file.close();
        debug_log_file.close();
        echo_metrics_file.close();
        
        std::cout << "[DEBUG] Data dump completed. Total frames: " << frame_counter << std::endl;
    }
    
    void dumpFrame(const int16_t* play_data, const int16_t* rec_data, const int16_t* processed_data,
                   size_t play_frame_size, size_t rec_frame_size) {
        if (!enabled) return;
        
        frame_counter++;
        
        // Dump raw PCM data
        play_raw_file.write(reinterpret_cast<const char*>(play_data), play_frame_size * sizeof(int16_t));
        rec_raw_file.write(reinterpret_cast<const char*>(rec_data), rec_frame_size * sizeof(int16_t));
        processed_raw_file.write(reinterpret_cast<const char*>(processed_data), rec_frame_size * sizeof(int16_t));
        
        // Calculate and log statistics
        if (debug_log_file.is_open()) {
            double play_rms = calculateRMS(play_data, play_frame_size);
            double rec_rms = calculateRMS(rec_data, rec_frame_size);
            double processed_rms = calculateRMS(processed_data, rec_frame_size);
            
            int16_t play_peak = findPeak(play_data, play_frame_size);
            int16_t rec_peak = findPeak(rec_data, rec_frame_size);
            int16_t processed_peak = findPeak(processed_data, rec_frame_size);
            
            debug_log_file << frame_counter << ","
                          << std::fixed << std::setprecision(3)
                          << play_rms << "," << rec_rms << "," << processed_rms << ","
                          << play_peak << "," << rec_peak << "," << processed_peak << "\n";
        }
        
        if (frame_counter % 100 == 0) {
            std::cout << "[DEBUG] Processed " << frame_counter << " frames" << std::endl;
        }
    }
    
    void dumpEchoMetrics(webrtc::AudioProcessing* apm) {
        if (!enabled || !echo_metrics_file.is_open()) return;
        
        // Only dump metrics every 10 frames to allow statistics to accumulate
        if (frame_counter % 10 != 0) return;
        
        webrtc::AudioProcessingStats stats = apm->GetStatistics();
        
        // Record echo metrics if available
        float erl_db = stats.echo_return_loss.has_value() ? stats.echo_return_loss.value() : -1.0f;
        float erle_db = stats.echo_return_loss_enhancement.has_value() ? stats.echo_return_loss_enhancement.value() : -1.0f;
        float residual_echo_likelihood = stats.residual_echo_likelihood.has_value() ? stats.residual_echo_likelihood.value() : -1.0f;
        float divergent_filter_fraction = stats.divergent_filter_fraction.has_value() ? stats.divergent_filter_fraction.value() : -1.0f;
        int delay_ms = stats.delay_ms.has_value() ? stats.delay_ms.value() : -1;
        
        // Debug output to see what values are actually available
        if (frame_counter % 100 == 0) {
            std::cout << "[DEBUG] Frame " << frame_counter << " stats availability: "
                      << "ERL=" << (stats.echo_return_loss.has_value() ? "Y" : "N") << " "
                      << "ERLE=" << (stats.echo_return_loss_enhancement.has_value() ? "Y" : "N") << " "
                      << "REL=" << (stats.residual_echo_likelihood.has_value() ? "Y" : "N") << " "
                      << "DFF=" << (stats.divergent_filter_fraction.has_value() ? "Y" : "N") << " "
                      << "Delay=" << (stats.delay_ms.has_value() ? "Y" : "N") << std::endl;
        }
        
        echo_metrics_file << frame_counter << ","
                         << std::fixed << std::setprecision(3)
                         << erl_db << ","
                         << erle_db << ","
                         << delay_ms << ","
                         << residual_echo_likelihood << ","
                         << (residual_echo_likelihood > 0.0f && residual_echo_likelihood > 0.5 ? "1" : "0") << ","
                         << divergent_filter_fraction << "\n";
        
        echo_metrics_file.flush();
    }
    
    void logProcessingParams(const webrtc::AudioProcessing::Config& config) {
        if (!enabled) return;
        
        std::ofstream config_file(dump_dir + "/processing_config.txt");
        if (config_file.is_open()) {
            config_file << "Audio Processing Configuration:\n";
            config_file << "Echo Canceller: " << (config.echo_canceller.enabled ? "enabled" : "disabled") << "\n";
            config_file << "Gain Controller 1: " << (config.gain_controller1.enabled ? "enabled" : "disabled") << "\n";
            config_file << "Gain Controller 2: " << (config.gain_controller2.enabled ? "enabled" : "disabled") << "\n";
            config_file << "High Pass Filter: " << (config.high_pass_filter.enabled ? "enabled" : "disabled") << "\n";
            config_file << "Noise Suppression: " << (config.noise_suppression.enabled ? "enabled" : "disabled") << "\n";
            if (config.noise_suppression.enabled) {
                config_file << "Noise Suppression Level: " << static_cast<int>(config.noise_suppression.level) << "\n";
            }
            config_file.close();
        }
    }

private:
    double calculateRMS(const int16_t* data, size_t length) {
        double sum = 0.0;
        for (size_t i = 0; i < length; ++i) {
            sum += static_cast<double>(data[i]) * data[i];
        }
        return sqrt(sum / length);
    }
    
    int16_t findPeak(const int16_t* data, size_t length) {
        int16_t peak = 0;
        for (size_t i = 0; i < length; ++i) {
            int16_t abs_val = abs(data[i]);
            if (abs_val > peak) {
                peak = abs_val;
            }
        }
        return peak;
    }
};

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <farend_file.wav> <nearend_file.wav> <out_file.wav> [--debug]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --debug    Enable debug mode with data dumping" << std::endl;
}

int main(int argc, char **argv) {
    if (argc < 4 || argc > 5) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }
    
    bool debug_mode = false;
    if (argc == 5 && std::string(argv[4]) == "--debug") {
        debug_mode = true;
        std::cout << "[DEBUG] Debug mode enabled" << std::endl;
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

    // Initialize data dumper
    std::string output_prefix = std::string(argv[3]);
    if (output_prefix.find(".wav") != std::string::npos) {
        output_prefix = output_prefix.substr(0, output_prefix.find(".wav"));
    }
    DataDumper dumper(debug_mode, output_prefix);

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
    
    // Log processing configuration in debug mode
    dumper.logProcessingParams(config);

    webrtc::StreamConfig play_stream_config(play_sample_rate, play_channels);
    webrtc::StreamConfig rec_stream_config(rec_sample_rate, rec_channels);
    
    // Write WAV header for output file (we'll update data size later)
    std::streampos header_pos = aec_file.tellp();
    writeWavHeader(aec_file, sample_rate, channels, 0);
    uint32_t total_data_size = 0;

    std::cout << "[INFO] Starting audio processing..." << std::endl;
    
    while (!play_file.eof() && !rec_file.eof()) {
        // Calculate frame size based on actual file parameters
        size_t play_frame_size = play_sample_rate * DEFAULT_BLOCK_MS / 1000 * play_channels;
        size_t rec_frame_size = rec_sample_rate * DEFAULT_BLOCK_MS / 1000 * rec_channels;
        
        int16_t* play_frame = new int16_t[play_frame_size];
        int16_t* rec_frame = new int16_t[rec_frame_size];
        int16_t* processed_frame = new int16_t[rec_frame_size];

        size_t play_bytes_read = play_file.read(reinterpret_cast<char *>(play_frame), play_frame_size * sizeof(int16_t)).gcount();
        size_t rec_bytes_read = rec_file.read(reinterpret_cast<char *>(rec_frame), rec_frame_size * sizeof(int16_t)).gcount();

        // Check if we have enough data for processing
        if (play_bytes_read < play_frame_size * sizeof(int16_t) || rec_bytes_read < rec_frame_size * sizeof(int16_t)) {
            delete[] play_frame;
            delete[] rec_frame;
            delete[] processed_frame;
            break;
        }

        // Copy rec_frame to processed_frame for comparison
        memcpy(processed_frame, rec_frame, rec_frame_size * sizeof(int16_t));

        apm->ProcessReverseStream(play_frame, play_stream_config, play_stream_config, play_frame);
        apm->ProcessStream(processed_frame, rec_stream_config, rec_stream_config, processed_frame);

        // Dump debug data if enabled
        dumper.dumpFrame(play_frame, rec_frame, processed_frame, play_frame_size, rec_frame_size);
        dumper.dumpEchoMetrics(apm.get());

        aec_file.write(reinterpret_cast<char *>(processed_frame), rec_frame_size * sizeof(int16_t));
        total_data_size += rec_frame_size * sizeof(int16_t);
        
        delete[] play_frame;
        delete[] rec_frame;
        delete[] processed_frame;
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
    
    if (debug_mode) {
        std::cout << "[DEBUG] Debug data saved to " << output_prefix << "_debug_dump/" << std::endl;
        std::cout << "[DEBUG] Available debug files:" << std::endl;
        std::cout << "  - play_raw.pcm (far-end audio raw data)" << std::endl;
        std::cout << "  - rec_raw.pcm (near-end audio raw data)" << std::endl;
        std::cout << "  - processed_raw.pcm (processed audio raw data)" << std::endl;
        std::cout << "  - debug_log.txt (frame-by-frame statistics)" << std::endl;
        std::cout << "  - echo_metrics.txt (echo cancellation metrics)" << std::endl;
        std::cout << "  - processing_config.txt (processing configuration)" << std::endl;
    }

    return EXIT_SUCCESS;
}
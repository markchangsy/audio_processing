#!/bin/bash

echo "Testing debug version of run-offline with echo metrics..."

# Create test directory
mkdir -p test_output

# Build the debug version
echo "Building debug version..."
meson compile -C ../build run-offline-debug

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

# Check if we have test audio files, if not create simple sine wave files
if [ ! -f "test_farend.wav" ] || [ ! -f "test_nearend.wav" ]; then
    echo "Creating test audio files..."
    
    # Create a simple sine wave for testing (requires ffmpeg or similar)
    # This is a placeholder - in real usage, you would have actual WAV files
    echo "Please provide test WAV files: test_farend.wav and test_nearend.wav"
    echo "You can create test files using tools like Audacity, ffmpeg, or similar."
    echo ""
    echo "Example using ffmpeg:"
    echo "  ffmpeg -f lavfi -i 'sine=frequency=1000:duration=5:sample_rate=16000' -ac 1 -sample_fmt s16 test_farend.wav"
    echo "  ffmpeg -f lavfi -i 'sine=frequency=500:duration=5:sample_rate=16000' -ac 1 -sample_fmt s16 test_nearend.wav"
    exit 1
fi

# Run the debug version
echo "Running debug version..."
../build/examples/run-offline-debug test_farend.wav test_nearend.wav test_output/output.wav --debug

# Check if debug files were created
if [ -d "test_output_debug_dump" ]; then
    echo ""
    echo "Debug dump created successfully!"
    echo "Debug files:"
    ls -la test_output_debug_dump/
    
    echo ""
    echo "Echo metrics preview (first 10 lines):"
    head -n 10 test_output_debug_dump/echo_metrics.txt
    
    echo ""
    echo "Processing config:"
    cat test_output_debug_dump/processing_config.txt
else
    echo "Debug dump was not created. Check for errors above."
    exit 1
fi

echo ""
echo "Test completed successfully!"
# Ogg Opus to WAV Converter

Converts Ogg Opus files to WAV format using microOpus.

## Building

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

With sanitizers (recommended for development):

```bash
cmake -DENABLE_SANITIZERS=ON ..
cmake --build .
```

## Usage

```bash
./opus_to_wav <input.opus> <output.wav>
```

Output shows stream info and conversion progress:

```text
Opus stream info:
  Sample rate: 48000 Hz
  Channels: 2 (stereo)
  Pre-skip: 312 samples

Conversion complete!
Total samples written: 1234567
Output file: music.wav
```

## Supported Formats

- **Input**: Ogg Opus (.opus), mono or stereo
- **Output**: WAV with 16-bit PCM, sample rate and channels preserved from input

## Technical Details

- Reads input in 4KB chunks
- PCM buffer sized for maximum 60ms Opus frame (5760 samples stereo)
- Automatically handles pre-skip samples
- WAV header updated with final size on completion

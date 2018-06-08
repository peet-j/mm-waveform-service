# waveform

![](http://i.imgur.com/oNy41Cr.png)

Input: any format audio or video file

Output: Waveform Json compatible with: [BBC Peaks.js](https://github.com/bbc/peaks.js)

## Usage

    waveform [options] <input_file> --output <destination>
    (where `input_file` is a file path and `destination` is a file path or `-` for STDOUT)

    Options:
    --scan                       duration scan (default off)

    WaveformJs Options:
    --width 2000              width in samples
    --frames-per-pixel 256   number of frames per pixel/sample
    --plain                  exclude metadata in output JSON (default off)

e.g. ./waveform ./audio/d3f14888-5070-4176-8b4e-e4fcaba5c135.mp4 --output -

## Installation

1. Install [libgroove](https://github.com/andrewrk/libgroove) dev package.
   Only the main library is needed.

2. Install libpng and zlib dev packages.

3. `make`

## Related Projects

 * [Forked from andrewrk/waveform](https://github.com/andrewrk/waveform)
 * [BBC Peaks.js](https://github.com/bbc/peaks.js)
 * [Node.js module](https://github.com/andrewrk/node-waveform)
 * [PHP Wrapper Script](https://github.com/polem/WaveformGenerator)
 * [Native Interface for Go](https://github.com/dz0ny/podcaster/blob/master/utils/waveform.go)

## Docker

Build image: 
`docker build -t <tag> .`

Running waveform directly:
`docker run -v `pwd`/audio:/waveform/audio -w /waveform -it thisisglobal/waveform:latest ./waveform ./audio/d3f14888-5070-4176-8b4e-e4fcaba5c135.mp4 --output -`

See https://hub.docker.com/r/jpeet/waveform/

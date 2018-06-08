# waveform

![](http://i.imgur.com/oNy41Cr.png)

Input: any format audio or video file

Output: Waveform Json compatible with: https://github.com/bbc/peaks.js

## Usage

    waveform [options] <input_file> --output <destination>
    (where `input_file` is a file path and `destination` is a file path or `-` for STDOUT)

    Options:
    --scan                       duration scan (default off)

    WaveformJs Options:
    --width 2000              width in samples
    --frames-per-pixel 256   number of frames per pixel/sample
    --plain                  exclude metadata in output JSON (default off)

## Installation

1. Install [libgroove](https://github.com/andrewrk/libgroove) dev package.
   Only the main library is needed.

2. Install libpng and zlib dev packages.

3. `make`

## Related Projects

 * [Node.js module](https://github.com/andrewrk/node-waveform)
 * [PHP Wrapper Script](https://github.com/polem/WaveformGenerator)
 * [Native Interface for Go](https://github.com/dz0ny/podcaster/blob/master/utils/waveform.go)

## Docker

See https://hub.docker.com/r/jpeet/waveform/

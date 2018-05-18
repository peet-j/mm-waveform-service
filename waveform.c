#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <groove/groove.h>
#include <groove/encoder.h>
#include <pthread.h>
#include <limits.h>

// transcoding stuff
static FILE *transcode_out_f = NULL;
static struct GrooveEncoder *encoder = NULL;

static int version() {
    printf("2.0.0\n");
    return 0;
}

static int usage(const char *exe) {
    fprintf(stderr, "\
\n\
Usage:\n\
\n\
waveform [options] in [--transcode out] [--waveformjs out]\n\
(where `in` is a file path and `out` is a file path or `-` for STDOUT)\n\
\n\
Options:\n\
--scan                       duration scan (default off)\n\
\n\
Transcoding Options:\n\
--bitrate 320                audio bitrate in kbps\n\
--format name                e.g. mp3, ogg, mp4\n\
--codec name                 e.g. mp3, vorbis, flac, aac\n\
--mime mimetype              e.g. audio/vorbis\n\
--tag-artist artistname      artist tag\n\
--tag-title title            title tag\n\
--tag-year 2000              year tag\n\
--tag-comment comment        comment tag\n\
\n\
WaveformJs Options:\n\
--wjs-width 800              width in samples\n\
--wjs-frames-per-pixel 256   number of frames per pixel/sample\n\
--wjs-precision 4            how many digits of precision\n\
--wjs-plain                  exclude metadata in output JSON (default off)\n\
\n");

    return 1;
}

static void *encode_write_thread(void *arg) {
    struct GrooveBuffer *buffer;

    while (groove_encoder_buffer_get(encoder, &buffer, 1) == GROOVE_BUFFER_YES) {
        fwrite(buffer->data[0], 1, buffer->size, transcode_out_f);
        groove_buffer_unref(buffer);
    }
    return NULL;
}

static int16_t int16_abs(int16_t x) {
    return x < 0 ? -x : x;
}

static int double_ceil(double x) {
    int n = x;
    return (x == (double)n) ? n : n + 1;
}

int main(int argc, char * argv[]) {
    // arg parsing
    char *exe = argv[0];

    char *input_filename = NULL;
    char *transcode_output = NULL;
    char *waveformjs_output = NULL;

    int bit_rate_k = 320;
    char *format = NULL;
    char *codec = NULL;
    char *mime = NULL;
    char *tag_artist = NULL;
    char *tag_title = NULL;
    char *tag_year = NULL;
    char *tag_comment = NULL;

    int wjs_frames_per_pixel = 256;
    int wjs_width = 800;
    int wjs_calculate_width = 0;

    int scan = 0;

    int i;
    for (i = 1; i < argc; ++i) {
        char *arg = argv[i];
        if (arg[0] == '-' && arg[1] == '-') {
            arg += 2;
            if (strcmp(arg, "scan") == 0) {
                scan = 1;
            } else if (strcmp(arg, "version") == 0) {
                return version();
            } else if (i + 1 >= argc) {
                // args that take 1 parameter
                return usage(exe);
            } else if (strcmp(arg, "bitrate") == 0) {
                bit_rate_k = atoi(argv[++i]);
            } else if (strcmp(arg, "format") == 0) {
                format = argv[++i];
            } else if (strcmp(arg, "codec") == 0) {
                codec = argv[++i];
            } else if (strcmp(arg, "mime") == 0) {
                mime = argv[++i];
            } else if (strcmp(arg, "transcode") == 0) {
                transcode_output = argv[++i];
            } else if (strcmp(arg, "waveformjs") == 0) {
                waveformjs_output = argv[++i];
            } else if (strcmp(arg, "tag-artist") == 0) {
                tag_artist = argv[++i];
            } else if (strcmp(arg, "tag-title") == 0) {
                tag_title = argv[++i];
            } else if (strcmp(arg, "tag-year") == 0) {
                tag_year = argv[++i];
            } else if (strcmp(arg, "tag-comment") == 0) {
                tag_comment = argv[++i];
            } else if (strcmp(arg, "wjs-frames-per-pixel") == 0) {
                wjs_frames_per_pixel = atoi(argv[++i]);
                wjs_calculate_width = 1;
            } else if (strcmp(arg, "wjs-width") == 0) {
                wjs_width = atoi(argv[++i]);
            } else {
                fprintf(stderr, "Unrecognized argument: %s\n", arg);
                return usage(exe);
            }
        } else if (!input_filename) {
            input_filename = arg;
        } else {
            fprintf(stderr, "Unexpected parameter: %s\n", arg);
            return usage(exe);
        }
    }

    if (!input_filename) {
        fprintf(stderr, "input file parameter required\n");
        return usage(exe);
    }

    if (!transcode_output && !waveformjs_output) {
        fprintf(stderr, "at least one output required\n");
        return usage(exe);
    }

    // arg parsing done. let's begin.

    groove_init();
    atexit(groove_finish);

    struct GrooveFile *file = groove_file_open(input_filename);
    if (!file) {
        fprintf(stderr, "Error opening input file: %s\n", input_filename);
        return 1;
    }
    struct GroovePlaylist *playlist = groove_playlist_create();
    groove_playlist_set_fill_mode(playlist, GROOVE_ANY_SINK_FULL);

    struct GrooveSink *sink = groove_sink_create();
    sink->audio_format.sample_rate = 44100;
    sink->audio_format.channel_layout = GROOVE_CH_LAYOUT_STEREO;
    sink->audio_format.sample_fmt = GROOVE_SAMPLE_FMT_S16;

    if (groove_sink_attach(sink, playlist) < 0) {
        fprintf(stderr, "error attaching sink\n");
        return 1;
    }

    struct GrooveBuffer *buffer;
    int frame_count;

    // scan the song for the exact correct frame count and duration
    float duration;
    if (scan) {
        struct GroovePlaylistItem *item = groove_playlist_insert(playlist, file, 1.0, 1.0, NULL);
        frame_count = 0;
        while (groove_sink_buffer_get(sink, &buffer, 1) == GROOVE_BUFFER_YES) {
            frame_count += buffer->frame_count;
            groove_buffer_unref(buffer);
        }
        groove_playlist_remove(playlist, item);
        duration = frame_count / 44100.0f;
    } else {
        duration = groove_file_duration(file);
        frame_count = double_ceil(duration * 44100.0);
    }

    pthread_t thread_id;

    // insert the item *after* creating the sinks to avoid race conditions
    groove_playlist_insert(playlist, file, 1.0, 1.0, NULL);

    FILE *waveformjs_f = NULL;
    int16_t wjs_left_sample;
    int16_t wjs_right_sample;

    int wjs_frames_until_emit = 0;
    int wjs_emit_count;

    if (waveformjs_output) {
        if (strcmp(waveformjs_output, "-") == 0) {
            waveformjs_f = stdout;
        } else {
            waveformjs_f = fopen(waveformjs_output, "wb");
            if (!waveformjs_f) {
                fprintf(stderr, "Error opening output file: %s\n", waveformjs_output);
                return 1;
            }
        }

        if (wjs_calculate_width) {
            wjs_width = (frame_count / wjs_frames_per_pixel)-1;
        } else {
            wjs_frames_per_pixel = frame_count / wjs_width;
            if (wjs_frames_per_pixel < 1)
                wjs_frames_per_pixel = 1;
        }

        fprintf(waveformjs_f, "{\"total_frames\":%d,\"sample_rate\":%d, \"samples_per_pixel\":%d, \"length\":%d, \"data\":[",
                    frame_count, 44100, wjs_frames_per_pixel, wjs_width);

        wjs_left_sample = INT16_MIN;
        wjs_right_sample = INT16_MIN;

        wjs_frames_until_emit = wjs_frames_per_pixel;
        wjs_emit_count = 0;
    }

    while (groove_sink_buffer_get(sink, &buffer, 1) == GROOVE_BUFFER_YES) {

        int n = sizeof(buffer->data) / sizeof(int16_t);
        printf("Number of data points: %d\n", n);
        printf("frame_count: %d\n", buffer->frame_count);

        int idx;
        for (idx = 0; i < buffer->frame_count && wjs_emit_count < wjs_width; idx += 1, wjs_frames_until_emit -= 1)
        {
            // write next sample
            if (wjs_frames_until_emit == 0) {
                wjs_emit_count += 1;
                char *comma = (wjs_emit_count == wjs_width) ? "" : ",";
                fprintf(waveformjs_f, "%d,", (wjs_left_sample/wjs_frames_per_pixel)*-1);
                fprintf(waveformjs_f, "%d%s", wjs_right_sample/wjs_frames_per_pixel, comma);
                wjs_left_sample = INT16_MIN;
                wjs_right_sample = INT16_MIN;
                wjs_frames_until_emit = wjs_frames_per_pixel;
            }
            // get next samples
            int16_t *data = (int16_t *) buffer->data[0];
            int16_t *left = &data[idx];
            int16_t *right = &data[idx + 1];
            wjs_left_sample = int16_abs(*left);
            wjs_right_sample = int16_abs(*right);
        }
        groove_buffer_unref(buffer);
    }

    if (wjs_emit_count < wjs_width) {
        // write the last sample
        fprintf(waveformjs_f, "%d,", (wjs_left_sample/wjs_frames_per_pixel)*-1);
        fprintf(waveformjs_f, "%d", wjs_right_sample/wjs_frames_per_pixel);
    }
    fprintf(waveformjs_f, "]}");
    fclose(waveformjs_f);

    groove_sink_detach(sink);
    groove_sink_destroy(sink);

    groove_playlist_clear(playlist);
    groove_file_close(file);
    groove_playlist_destroy(playlist);

    return 0;
}

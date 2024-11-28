#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <glib.h>
#include "types.h"
#include "visualizer_types.h"

#ifdef __APPLE__
#include <AudioToolbox/AudioToolbox.h>
#else
#include <pulse/simple.h>
#include <pulse/error.h>
#endif

// For export file naming
#define MAX_FILENAME 256
#define EXPORT_PREFIX "tastewarp_export_"

typedef struct AudioData {
    char* filename;
    int16_t* buffer;
    size_t buffer_size;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    float mix_volume;
} AudioData;

// Function declarations...
AudioData* load_wav_file(const char* filename);
int save_wav_file(const char* filename, AudioData* audio);
void mix_audio_files(AudioPlayer* player);
void add_audio_file(AudioPlayer* player, const char* filename);
void remove_audio_file(AudioPlayer* player, AudioData* audio);
void reset_to_original(AudioPlayer* player);
void init_audio_player(AudioPlayer* player, AudioData* audio);
void cleanup_audio_player(AudioPlayer* player);
void play_audio(AudioPlayer* player);
void stop_audio(AudioPlayer* player);
char* generate_export_filename(void);
AudioData* create_audio_from_image(const char* filename, Visualizer* vis);

#endif 
#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <glib.h>
#include <time.h>

// Only keep AudioPlayer definition here
typedef struct AudioData AudioData;  // Forward declare AudioData

typedef struct AudioPlayer {
    GList* audio_files;
    AudioData* active_mix;
    AudioData* original_mix;
    size_t ring_buffer_pos;
    size_t last_60_seconds_samples;
    uint32_t target_sample_rate;
    time_t last_effect_time;
    gboolean effect_active;
    void* ui_ptr;
} AudioPlayer;

#endif 
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <glib/gstdio.h>
#include <fftw3.h>
#include "effects.h"
#include "ui.h"

// Add FFTW constants if not defined
#ifndef FFTW_FORWARD
#define FFTW_FORWARD (-1)
#define FFTW_BACKWARD 1
#define FFTW_ESTIMATE 64
#endif

// Add backup function
static void backup_original(AudioPlayer* player, AudioData* audio) {
    if (!player->effect_active) {
        if (!player->original_mix) {
            player->original_mix = malloc(sizeof(AudioData));
            player->original_mix->buffer = malloc(audio->buffer_size);
        }
        memcpy(player->original_mix->buffer, audio->buffer, audio->buffer_size);
        player->original_mix->buffer_size = audio->buffer_size;
        player->original_mix->sample_rate = audio->sample_rate;
        player->original_mix->channels = audio->channels;
        player->original_mix->bits_per_sample = audio->bits_per_sample;
        player->effect_active = TRUE;
    }
}

// Utility function for random float between 0 and 1
static float rand_float() {
    return (float)rand() / (float)RAND_MAX;
}

void bit_mash(AudioPlayer* player, AudioData* audio G_GNUC_UNUSED, float intensity) {
    if (!player || !player->active_mix) return;
    
    // Store original if not already stored
    if (!player->effect_active) {
        if (!player->original_mix) {
            player->original_mix = malloc(sizeof(AudioData));
            player->original_mix->buffer = malloc(player->active_mix->buffer_size);
            memcpy(player->original_mix->buffer, player->active_mix->buffer, player->active_mix->buffer_size);
            player->original_mix->buffer_size = player->active_mix->buffer_size;
            player->original_mix->sample_rate = player->active_mix->sample_rate;
            player->original_mix->channels = player->active_mix->channels;
            player->original_mix->bits_per_sample = player->active_mix->bits_per_sample;
        }
        player->effect_active = TRUE;
    }
    
    // Apply effect directly to active mix
    int mask = 0xFFFF >> (int)(intensity * 8);
    for (size_t i = 0; i < player->active_mix->buffer_size / sizeof(int16_t); i++) {
        player->active_mix->buffer[i] &= mask;
        if (rand_float() < intensity * 0.1) {
            player->active_mix->buffer[i] ^= (1 << (int)(rand_float() * 16));
        }
    }
}

void bit_drop(AudioPlayer* player, AudioData* audio G_GNUC_UNUSED, float probability) {
    if (!player || !player->active_mix) return;
    
    // Store original if not already stored
    if (!player->effect_active) {
        if (!player->original_mix) {
            player->original_mix = malloc(sizeof(AudioData));
            player->original_mix->buffer = malloc(player->active_mix->buffer_size);
            memcpy(player->original_mix->buffer, player->active_mix->buffer, player->active_mix->buffer_size);
            player->original_mix->buffer_size = player->active_mix->buffer_size;
            player->original_mix->sample_rate = player->active_mix->sample_rate;
            player->original_mix->channels = player->active_mix->channels;
            player->original_mix->bits_per_sample = player->active_mix->bits_per_sample;
        }
        player->effect_active = TRUE;
    }
    
    // Apply effect directly to active mix
    for (size_t i = 0; i < player->active_mix->buffer_size / sizeof(int16_t); i++) {
        if (rand_float() < probability) {
            player->active_mix->buffer[i] = 0;
        }
    }
}

void tempo_shift(AudioPlayer* player, AudioData* audio G_GNUC_UNUSED, float factor) {
    if (!player || !player->active_mix) return;
    
    // Store original if not already stored
    if (!player->effect_active) {
        if (!player->original_mix) {
            player->original_mix = malloc(sizeof(AudioData));
            player->original_mix->buffer = malloc(player->active_mix->buffer_size);
            memcpy(player->original_mix->buffer, player->active_mix->buffer, player->active_mix->buffer_size);
            player->original_mix->buffer_size = player->active_mix->buffer_size;
            player->original_mix->sample_rate = player->active_mix->sample_rate;
            player->original_mix->channels = player->active_mix->channels;
            player->original_mix->bits_per_sample = player->active_mix->bits_per_sample;
        }
        player->effect_active = TRUE;
    }
    
    // Apply effect directly to active mix
    size_t num_samples = player->active_mix->buffer_size / sizeof(int16_t);
    int16_t* new_buffer = malloc(player->active_mix->buffer_size);
    
    for (size_t i = 0; i < num_samples; i++) {
        size_t src_idx = (size_t)(i * factor) % num_samples;
        new_buffer[i] = player->active_mix->buffer[src_idx];
    }
    
    memcpy(player->active_mix->buffer, new_buffer, player->active_mix->buffer_size);
    free(new_buffer);
}

void pitch_shift(AudioPlayer* player, AudioData* audio, float semitones) {
    if (!audio || !audio->buffer) return;
    
    // Save original if needed
    backup_original(player, audio);
    
    // Calculate pitch shift factor
    float factor = pow(2.0f, semitones / 12.0f);
    
    // Create FFT context for phase vocoder
    size_t window_size = 2048;
    size_t hop_size = window_size / 4;
    
    fftw_complex* in = fftw_alloc_complex(window_size);
    fftw_complex* out = fftw_alloc_complex(window_size);
    fftw_plan forward = fftw_plan_dft_1d(window_size, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_plan backward = fftw_plan_dft_1d(window_size, out, in, FFTW_BACKWARD, FFTW_ESTIMATE);
    
    // Hann window for smooth overlapping
    double* window = malloc(window_size * sizeof(double));
    for (size_t i = 0; i < window_size; i++) {
        window[i] = 0.5 * (1.0 - cos(2.0 * M_PI * i / (window_size - 1)));
    }
    
    // Phase accumulation arrays
    double* phase = calloc(window_size/2 + 1, sizeof(double));
    double* phase_advance = calloc(window_size/2 + 1, sizeof(double));
    
    // Calculate frequency for each bin
    for (size_t i = 0; i <= window_size/2; i++) {
        phase_advance[i] = 2.0 * M_PI * i * hop_size / window_size;
    }
    
    // Process audio in overlapping windows
    size_t num_samples = audio->buffer_size / sizeof(int16_t);
    int16_t* output = malloc(audio->buffer_size);
    double* accumulator = calloc(num_samples + window_size, sizeof(double));
    
    for (size_t pos = 0; pos < num_samples; pos += hop_size) {
        // Fill input buffer
        for (size_t i = 0; i < window_size; i++) {
            size_t idx = pos + i;
            double sample = idx < num_samples ? audio->buffer[idx] / 32768.0 : 0.0;
            in[i][0] = sample * window[i];
            in[i][1] = 0.0;
        }
        
        // Forward FFT
        fftw_execute(forward);
        
        // Modify phases for pitch shift
        for (size_t i = 0; i <= window_size/2; i++) {
            double magnitude = sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]);
            double phase_now = atan2(out[i][1], out[i][0]);
            
            double phase_diff = phase_now - phase[i] - phase_advance[i];
            phase_diff = fmod(phase_diff + M_PI, 2*M_PI) - M_PI;
            
            double true_freq = phase_advance[i] + phase_diff;
            phase[i] = phase_now;
            
            // Apply pitch shift
            double shifted_phase = fmod(phase[i] + true_freq * factor, 2*M_PI);
            
            out[i][0] = magnitude * cos(shifted_phase);
            out[i][1] = magnitude * sin(shifted_phase);
        }
        
        // Inverse FFT
        fftw_execute(backward);
        
        // Overlap-add to output
        for (size_t i = 0; i < window_size; i++) {
            accumulator[pos + i] += in[i][0] * window[i] / window_size;
        }
    }
    
    // Convert back to int16
    for (size_t i = 0; i < num_samples; i++) {
        output[i] = (int16_t)CLAMP(accumulator[i] * 32768.0, -32768.0, 32767.0);
    }
    
    // Copy to audio buffer
    memcpy(audio->buffer, output, audio->buffer_size);
    
    // Cleanup
    fftw_destroy_plan(forward);
    fftw_destroy_plan(backward);
    fftw_free(in);
    fftw_free(out);
    free(window);
    free(phase);
    free(phase_advance);
    free(output);
    free(accumulator);
}

void add_echo(AudioPlayer* player, AudioData* audio, float delay_ms, float decay) {
    if (!audio || !player) return;
    
    // Store original if not already stored
    if (!player->effect_active) {
        if (!player->original_mix) {
            player->original_mix = malloc(sizeof(AudioData));
            player->original_mix->buffer = malloc(audio->buffer_size);
        }
        memcpy(player->original_mix->buffer, audio->buffer, audio->buffer_size);
        player->original_mix->buffer_size = audio->buffer_size;
        player->effect_active = TRUE;
    }
    
    // Apply effect directly
    size_t delay_samples = (size_t)(delay_ms * audio->sample_rate / 1000.0f);
    size_t num_samples = audio->buffer_size / sizeof(int16_t);
    int16_t* new_buffer = malloc(audio->buffer_size);
    memcpy(new_buffer, audio->buffer, audio->buffer_size);
    
    for (size_t i = delay_samples; i < num_samples; i++) {
        float echo = audio->buffer[i - delay_samples] * decay;
        new_buffer[i] = (int16_t)fmin(32767, fmax(-32768, new_buffer[i] + echo));
    }
    
    memcpy(audio->buffer, new_buffer, audio->buffer_size);
    free(new_buffer);
    
    // Update the active mix
    if (player->active_mix) {
        memcpy(player->active_mix->buffer, audio->buffer, audio->buffer_size);
    }
}

void add_robot(AudioPlayer* player, AudioData* audio, float modulation_freq) {
    if (!audio || !player) return;
    
    // Store original if not already stored
    if (!player->effect_active) {
        if (!player->original_mix) {
            player->original_mix = malloc(sizeof(AudioData));
            player->original_mix->buffer = malloc(audio->buffer_size);
        }
        memcpy(player->original_mix->buffer, audio->buffer, audio->buffer_size);
        player->original_mix->buffer_size = audio->buffer_size;
        player->effect_active = TRUE;
    }
    
    // Apply effect directly
    float phase = 0.0f;
    float phase_inc = 2.0f * M_PI * modulation_freq / audio->sample_rate;
    
    for (size_t i = 0; i < audio->buffer_size / sizeof(int16_t); i++) {
        float modulator = (sin(phase) + 1.0f) * 0.5f;
        audio->buffer[i] = (int16_t)(audio->buffer[i] * modulator);
        phase += phase_inc;
    }
    
    // Update the active mix
    if (player->active_mix) {
        memcpy(player->active_mix->buffer, audio->buffer, audio->buffer_size);
    }
}

void random_effect(AudioPlayer* player, AudioData* audio) {
    switch (rand() % 6) {
        case 0:
            bit_mash(player, audio, rand_float() * 0.8f);
            break;
        case 1:
            bit_drop(player, audio, rand_float() * 0.3f);
            break;
        case 2:
            tempo_shift(player, audio, 0.5f + rand_float());
            break;
        case 3:
            pitch_shift(player, audio, (rand_float() * 24.0f) - 12.0f);
            break;
        case 4:
            add_echo(player, audio, 100.0f + rand_float() * 400.0f, 0.3f + rand_float() * 0.4f);
            break;
        case 5:
            add_robot(player, audio, 1.0f + rand_float() * 10.0f);
            break;
    }
}

void export_last_60_seconds(AudioPlayer* player) {
    if (!player || !player->active_mix) {
        printf("Export failed: no active mix\n");
        return;
    }
    
    // Get full path in user's data directory
    char* export_path = get_export_path();
    
    size_t bytes_per_sample = sizeof(int16_t) * player->active_mix->channels;
    size_t total_samples = player->last_60_seconds_samples * player->active_mix->channels;
    size_t buffer_samples = player->active_mix->buffer_size / bytes_per_sample;
    
    AudioData export_audio = {
        .buffer = malloc(total_samples * bytes_per_sample),
        .buffer_size = total_samples * bytes_per_sample,
        .sample_rate = player->active_mix->sample_rate,
        .channels = player->active_mix->channels,
        .bits_per_sample = player->active_mix->bits_per_sample
    };
    
    size_t start_pos = (player->ring_buffer_pos + buffer_samples - total_samples) % buffer_samples;
    
    // Copy the last 60 seconds
    for (size_t i = 0; i < total_samples; i++) {
        size_t src_idx = (start_pos + i) % buffer_samples;
        export_audio.buffer[i] = player->active_mix->buffer[src_idx];
    }
    
    if (save_wav_file(export_path, &export_audio) != 0) {
        printf("Error saving WAV file to: %s\n", export_path);
    } else {
        printf("Successfully exported to: %s\n", export_path);
        UI* ui = (UI*)player->ui_ptr;
        if (ui) {
            update_recent_menu(ui, export_path);
        }
    }
    
    g_free(export_path);
    free(export_audio.buffer);
}

char* get_export_path(void) {
    const char* xdg_data_home = g_get_user_data_dir();
    char* app_data_dir = g_build_filename(xdg_data_home, "com.un1crom.tastewarp", "exports", NULL);
    
    // Debug print
    printf("Creating export directory: %s\n", app_data_dir);
    
    // Ensure the directory exists
    if (g_mkdir_with_parents(app_data_dir, 0755) == -1) {
        printf("Error creating directory: %s\n", g_strerror(errno));
    }
    
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    
    char* filename = g_strdup_printf("%s/tastewarp_export_%04d%02d%02d_%02d%02d%02d.wav",
                                   app_data_dir,
                                   t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                                   t->tm_hour, t->tm_min, t->tm_sec);
    
    // Debug print
    printf("Export path: %s\n", filename);
    
    g_free(app_data_dir);
    return filename;
}
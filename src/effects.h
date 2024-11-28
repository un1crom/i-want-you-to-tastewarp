#ifndef EFFECTS_H
#define EFFECTS_H

#include "audio.h"

// Effect functions
void bit_mash(AudioPlayer* player, AudioData* audio, float intensity);
void bit_drop(AudioPlayer* player, AudioData* audio, float probability);
void tempo_shift(AudioPlayer* player, AudioData* audio, float factor);
void pitch_shift(AudioPlayer* player, AudioData* audio, float semitones);
void add_echo(AudioPlayer* player, AudioData* audio, float delay_ms, float decay);
void add_robot(AudioPlayer* player, AudioData* audio, float modulation_freq);
void random_effect(AudioPlayer* player, AudioData* audio);
void export_last_60_seconds(AudioPlayer* player);

// Export path helper
char* get_export_path(void);

#endif 
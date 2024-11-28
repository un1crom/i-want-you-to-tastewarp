#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#ifdef __APPLE__
#include <AudioToolbox/AudioToolbox.h>
#else
#include <pulse/simple.h>
#include <pulse/error.h>
#endif

#include "audio.h"
#include "effects.h"
#include "visualizer_types.h"

// Platform-specific audio callback/stream handling
#ifdef __APPLE__
static OSStatus playbackCallback(void *inRefCon, 
                               AudioUnitRenderActionFlags *ioActionFlags,
                               const AudioTimeStamp *inTimeStamp,
                               UInt32 inBusNumber,
                               UInt32 inNumberFrames,
                               AudioBufferList *ioData) {
    // Mark unused parameters to silence warnings
    (void)ioActionFlags;
    (void)inTimeStamp;
    (void)inBusNumber;
    
    AudioPlayer *player = (AudioPlayer *)inRefCon;
    if (!player || !player->active_mix) {
        // Fill with silence if no audio
        for (UInt32 i = 0; i < ioData->mNumberBuffers; i++) {
            memset(ioData->mBuffers[i].mData, 0, ioData->mBuffers[i].mDataByteSize);
        }
        return noErr;
    }

    float *buffer = (float *)ioData->mBuffers[0].mData;
    size_t channels = player->active_mix->channels;
    
    // Fill the output buffer
    for (UInt32 frame = 0; frame < inNumberFrames; frame++) {
        for (size_t channel = 0; channel < channels; channel++) {
            int16_t sample = player->active_mix->buffer[player->ring_buffer_pos * channels + channel];
            buffer[frame * channels + channel] = sample / 32768.0f;
        }
        
        player->ring_buffer_pos++;
        if (player->ring_buffer_pos >= player->active_mix->buffer_size / (channels * sizeof(int16_t))) {
            player->ring_buffer_pos = 0;
        }
    }
    
    return noErr;
}

static void setup_audio_unit(AudioPlayer* player) {
    AudioComponentDescription desc = {
        .componentType = kAudioUnitType_Output,
        .componentSubType = kAudioUnitSubType_DefaultOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
        .componentFlags = 0,
        .componentFlagsMask = 0
    };

    AudioComponent comp = AudioComponentFindNext(NULL, &desc);
    if (!comp) return;

    AudioUnit audioUnit;
    OSStatus status = AudioComponentInstanceNew(comp, &audioUnit);
    if (status != noErr) return;

    AURenderCallbackStruct callback = {
        .inputProc = playbackCallback,
        .inputProcRefCon = player
    };

    status = AudioUnitSetProperty(audioUnit,
                                kAudioUnitProperty_SetRenderCallback,
                                kAudioUnitScope_Input,
                                0,
                                &callback,
                                sizeof(callback));
    if (status != noErr) {
        AudioComponentInstanceDispose(audioUnit);
        return;
    }

    AudioStreamBasicDescription format = {
        .mSampleRate = player->target_sample_rate,
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked,
        .mFramesPerPacket = 1,
        .mChannelsPerFrame = player->active_mix->channels,
        .mBitsPerChannel = 32,
        .mBytesPerPacket = 4 * player->active_mix->channels,
        .mBytesPerFrame = 4 * player->active_mix->channels
    };

    status = AudioUnitSetProperty(audioUnit,
                                kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Input,
                                0,
                                &format,
                                sizeof(format));
    if (status != noErr) {
        AudioComponentInstanceDispose(audioUnit);
        return;
    }

    status = AudioUnitInitialize(audioUnit);
    if (status != noErr) {
        AudioComponentInstanceDispose(audioUnit);
        return;
    }

    status = AudioOutputUnitStart(audioUnit);
    if (status != noErr) {
        AudioUnitUninitialize(audioUnit);
        AudioComponentInstanceDispose(audioUnit);
        return;
    }
}
#else
// Linux PulseAudio stream
static pa_simple *pa_stream = NULL;

static void init_pulseaudio(AudioPlayer* player) {
    pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = player->target_sample_rate,
        .channels = player->active_mix->channels
    };
    
    int error;
    pa_stream = pa_simple_new(NULL,               // Use default server
                             "TasteWarp",         // Application name
                             PA_STREAM_PLAYBACK,   // Stream direction
                             NULL,                // Use default device
                             "Music",             // Stream description
                             &ss,                 // Sample format
                             NULL,                // Use default channel map
                             NULL,                // Use default buffering attributes
                             &error);             // Error code
    
    if (!pa_stream) {
        fprintf(stderr, "pa_simple_new() failed: %s\n", pa_strerror(error));
        exit(1);
    }
}

static void cleanup_pulseaudio(void) {
    if (pa_stream) {
        pa_simple_free(pa_stream);
        pa_stream = NULL;
    }
}
#endif

// Platform-independent functions
AudioData* load_wav_file(const char* filename) {
#ifdef __APPLE__
    AudioData* audio = malloc(sizeof(AudioData));
    if (!audio) return NULL;
    
    // Store filename
    audio->filename = strdup(filename);
    audio->mix_volume = 1.0f;
    
    // Open the audio file
    AudioFileID audioFile;
    CFURLRef fileURL = CFURLCreateFromFileSystemRepresentation(NULL, 
                                                           (const UInt8*)filename,
                                                           strlen(filename), 
                                                           false);
    
    OSStatus status = AudioFileOpenURL(fileURL, kAudioFileReadPermission, 0, &audioFile);
    CFRelease(fileURL);
    
    if (status != noErr) {
        free(audio->filename);
        free(audio);
        return NULL;
    }
    
    // Get audio file properties
    AudioStreamBasicDescription asbd;
    UInt32 propSize = sizeof(asbd);
    status = AudioFileGetProperty(audioFile, 
                             kAudioFilePropertyDataFormat,
                             &propSize, 
                             &asbd);
    
    if (status != noErr) {
        AudioFileClose(audioFile);
        free(audio->filename);
        free(audio);
        return NULL;
    }
    
    // Get file size
    UInt64 fileSize = 0;
    propSize = sizeof(fileSize);
    status = AudioFileGetProperty(audioFile,
                             kAudioFilePropertyAudioDataByteCount,
                             &propSize,
                             &fileSize);
    
    if (status != noErr) {
        AudioFileClose(audioFile);
        free(audio->filename);
        free(audio);
        return NULL;
    }
    
    // Allocate buffer and read audio data
    audio->buffer = malloc(fileSize);
    audio->buffer_size = fileSize;
    audio->sample_rate = asbd.mSampleRate;
    audio->channels = asbd.mChannelsPerFrame;
    audio->bits_per_sample = asbd.mBitsPerChannel;
    
    UInt32 bytesToRead = fileSize;
    status = AudioFileReadBytes(audioFile,
                           false,
                           0,
                           &bytesToRead,
                           audio->buffer);
    
    AudioFileClose(audioFile);
    
    if (status != noErr) {
        free(audio->buffer);
        free(audio->filename);
        free(audio);
        return NULL;
    }
    
    return audio;
#else
    // Linux WAV loading implementation
    AudioData* audio = malloc(sizeof(AudioData));
    if (!audio) return NULL;
    
    // Store filename
    audio->filename = strdup(filename);
    audio->mix_volume = 1.0f;
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        free(audio->filename);
        free(audio);
        return NULL;
    }
    
    // Read WAV header
    typedef struct {
        char riff_header[4];    // "RIFF"
        int32_t wav_size;       // Total file size - 8
        char wave_header[4];    // "WAVE"
        char fmt_header[4];     // "fmt "
        int32_t fmt_chunk_size; // Size of format chunk
        int16_t audio_format;   // Audio format (1 for PCM)
        int16_t num_channels;   // Number of channels
        int32_t sample_rate;    // Sample rate
        int32_t byte_rate;      // Bytes per second
        int16_t block_align;    // Bytes per sample * channels
        int16_t bits_per_sample;// Bits per sample
        char data_header[4];    // "data"
        int32_t data_bytes;     // Size of audio data
    } WavHeader;
    
    WavHeader header;
    if (fread(&header, sizeof(WavHeader), 1, file) != 1) {
        fclose(file);
        free(audio->filename);
        free(audio);
        return NULL;
    }
    
    // Verify WAV format
    if (memcmp(header.riff_header, "RIFF", 4) != 0 ||
        memcmp(header.wave_header, "WAVE", 4) != 0 ||
        memcmp(header.fmt_header, "fmt ", 4) != 0 ||
        memcmp(header.data_header, "data", 4) != 0) {
        fclose(file);
        free(audio->filename);
        free(audio);
        return NULL;
    }
    
    // Fill audio data structure
    audio->channels = header.num_channels;
    audio->sample_rate = header.sample_rate;
    audio->bits_per_sample = header.bits_per_sample;
    audio->buffer_size = header.data_bytes;
    
    // Allocate and read audio data
    audio->buffer = malloc(audio->buffer_size);
    if (!audio->buffer) {
        fclose(file);
        free(audio->filename);
        free(audio);
        return NULL;
    }
    
    if (fread(audio->buffer, 1, audio->buffer_size, file) != audio->buffer_size) {
        fclose(file);
        free(audio->buffer);
        free(audio->filename);
        free(audio);
        return NULL;
    }
    
    fclose(file);
    return audio;
#endif
}

void mix_audio_files(AudioPlayer* player) {
    if (!player || !player->audio_files) return;
    
    // Get first audio file
    AudioData* first = (AudioData*)player->audio_files->data;
    
    // Create or resize active mix buffer if needed
    if (!player->active_mix) {
        player->active_mix = malloc(sizeof(AudioData));
        player->active_mix->buffer = malloc(first->buffer_size);
        player->active_mix->buffer_size = first->buffer_size;
        player->active_mix->sample_rate = first->sample_rate;
        player->active_mix->channels = first->channels;
        player->active_mix->bits_per_sample = first->bits_per_sample;
    }
    
    // If we have active effects, don't overwrite the active mix
    if (player->effect_active) return;
    
    // Otherwise, copy the first file's data to active mix
    memcpy(player->active_mix->buffer, first->buffer, first->buffer_size);
    
    // Mix in any additional files
    for (GList* l = player->audio_files->next; l != NULL; l = l->next) {
        AudioData* audio = (AudioData*)l->data;
        for (size_t i = 0; i < audio->buffer_size / sizeof(int16_t); i++) {
            float sample = audio->buffer[i] * audio->mix_volume;
            player->active_mix->buffer[i] = (int16_t)fmin(32767, fmax(-32768, 
                player->active_mix->buffer[i] + sample));
        }
    }
}

void add_audio_file(AudioPlayer* player, const char* filename) {
    AudioData* audio = load_wav_file(filename);
    if (!audio) return;
    
    // Check if this is the first file
    gboolean is_first = (player->audio_files == NULL);
    
    // Add to list
    player->audio_files = g_list_append(player->audio_files, audio);
    
    // If this is the first file, set up the player
    if (is_first) {
        player->target_sample_rate = audio->sample_rate;
        player->last_60_seconds_samples = audio->sample_rate * 60;
        
        // Create initial backup for effects
        if (!player->original_mix) {
            player->original_mix = malloc(sizeof(AudioData));
            player->original_mix->buffer = malloc(audio->buffer_size);
            memcpy(player->original_mix->buffer, audio->buffer, audio->buffer_size);
            player->original_mix->buffer_size = audio->buffer_size;
            player->original_mix->sample_rate = audio->sample_rate;
            player->original_mix->channels = audio->channels;
            player->original_mix->bits_per_sample = audio->bits_per_sample;
        }
    }
    
    mix_audio_files(player);
}

void remove_audio_file(AudioPlayer* player, AudioData* audio) {
    player->audio_files = g_list_remove(player->audio_files, audio);
    free(audio->buffer);
    free(audio->filename);
    free(audio);
    mix_audio_files(player);
}

void reset_to_original(AudioPlayer* player) {
    if (player->original_mix) {
        memcpy(player->active_mix->buffer, player->original_mix->buffer,
               player->original_mix->buffer_size);
        free(player->original_mix->buffer);
        free(player->original_mix);
        player->original_mix = NULL;
    }
    player->effect_active = FALSE;
    
    // Keep only the first file
    while (g_list_length(player->audio_files) > 1) {
        GList* last = g_list_last(player->audio_files);
        AudioData* audio = (AudioData*)last->data;
        remove_audio_file(player, audio);
    }
    
    // Reset mix volume
    if (player->audio_files) {
        AudioData* first = (AudioData*)player->audio_files->data;
        first->mix_volume = 1.0f;
    }
    
    mix_audio_files(player);
}

char* generate_export_filename(void) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    
    char* filename = malloc(MAX_FILENAME);
    snprintf(filename, MAX_FILENAME, "exports/%s%04d%02d%02d_%02d%02d%02d.wav",
             EXPORT_PREFIX,
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
    
    return filename;
}

void init_audio_player(AudioPlayer* player, AudioData* audio) {
    // Clear all fields first
    memset(player, 0, sizeof(AudioPlayer));
    
    player->audio_files = NULL;
    player->active_mix = NULL;
    player->original_mix = NULL;
    player->ring_buffer_pos = 0;
    player->effect_active = FALSE;
    player->last_effect_time = 0;
    
    if (audio) {
        player->audio_files = g_list_append(player->audio_files, audio);
        player->target_sample_rate = audio->sample_rate;
        player->last_60_seconds_samples = audio->sample_rate * 60;
        mix_audio_files(player);
        
        // Create initial backup for effects
        player->original_mix = malloc(sizeof(AudioData));
        player->original_mix->buffer = malloc(audio->buffer_size);
        memcpy(player->original_mix->buffer, audio->buffer, audio->buffer_size);
        player->original_mix->buffer_size = audio->buffer_size;
        player->original_mix->sample_rate = audio->sample_rate;
        player->original_mix->channels = audio->channels;
        player->original_mix->bits_per_sample = audio->bits_per_sample;
        
#ifdef __APPLE__
        // Initialize AudioUnit for macOS
        setup_audio_unit(player);
#else
        // Initialize PulseAudio for Linux
        init_pulseaudio(player);
#endif
    }
}

void cleanup_audio_player(AudioPlayer* player) {
#ifdef __APPLE__
    // ... existing macOS cleanup code ...
#else
    cleanup_pulseaudio();
#endif

    while (player->audio_files) {
        AudioData* audio = (AudioData*)player->audio_files->data;
        remove_audio_file(player, audio);
    }
    
    if (player->active_mix) {
        free(player->active_mix->buffer);
        free(player->active_mix->filename);
        free(player->active_mix);
    }
    
    if (player->original_mix) {
        free(player->original_mix->buffer);
        free(player->original_mix);
    }
}

void play_audio(AudioPlayer* player) {
    // Audio playback is handled by the callback
    (void)player;
}

void stop_audio(AudioPlayer* player) {
    // Implementation for stopping audio would go here
    (void)player;
}

int save_wav_file(const char* filename, AudioData* audio) {
    // WAV file header
    typedef struct {
        char riff_header[4];    // Contains "RIFF"
        int32_t wav_size;       // Size of WAV file in bytes - 8
        char wave_header[4];    // Contains "WAVE"
        char fmt_header[4];     // Contains "fmt "
        int32_t fmt_chunk_size; // Size of format chunk
        int16_t audio_format;   // Audio format (1 for PCM)
        int16_t num_channels;   // Number of channels
        int32_t sample_rate;    // Sample rate
        int32_t byte_rate;      // Byte rate
        int16_t block_align;    // Block align
        int16_t bits_per_sample;// Bits per sample
        char data_header[4];    // Contains "data"
        int32_t data_bytes;     // Number of bytes in data
    } WavHeader;
    
    FILE* file = fopen(filename, "wb");
    if (!file) {
        printf("Error opening file for writing: %s (%s)\n", filename, strerror(errno));
        return -1;
    }
    
    // Prepare WAV header
    WavHeader header;
    memcpy(header.riff_header, "RIFF", 4);
    memcpy(header.wave_header, "WAVE", 4);
    memcpy(header.fmt_header, "fmt ", 4);
    memcpy(header.data_header, "data", 4);
    
    header.fmt_chunk_size = 16;
    header.audio_format = 1;
    header.num_channels = audio->channels;
    header.sample_rate = audio->sample_rate;
    header.bits_per_sample = audio->bits_per_sample;
    header.block_align = audio->channels * (audio->bits_per_sample / 8);
    header.byte_rate = header.sample_rate * header.block_align;
    header.data_bytes = audio->buffer_size;
    header.wav_size = header.data_bytes + sizeof(WavHeader) - 8;
    
    // Write header and data
    size_t written = fwrite(&header, sizeof(WavHeader), 1, file);
    if (written != 1) {
        printf("Error writing WAV header\n");
        fclose(file);
        return -1;
    }
    
    written = fwrite(audio->buffer, audio->buffer_size, 1, file);
    if (written != 1) {
        printf("Error writing WAV data\n");
        fclose(file);
        return -1;
    }
    
    fclose(file);
    return 0;
}

// Add new function to convert image to audio
AudioData* create_audio_from_image(const char* filename, Visualizer* vis) {
    if (!filename) return NULL;
    
    GError* error = NULL;
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file(filename, &error);
    
    if (!pixbuf) {
        if (error) {
            printf("Error loading image: %s\n", error->message);
            g_error_free(error);
        }
        return NULL;
    }
    
    // Validate image dimensions
    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    
    if (width <= 0 || height <= 0) {
        g_object_unref(pixbuf);
        return NULL;
    }
    
    // Create audio data first
    AudioData* audio = malloc(sizeof(AudioData));
    if (!audio) {
        g_object_unref(pixbuf);
        return NULL;
    }
    
    audio->filename = strdup(filename);
    audio->sample_rate = 44100;
    audio->channels = 2;
    audio->bits_per_sample = 16;
    audio->mix_volume = 1.0f;
    
    // Convert to grayscale for edge detection
    GdkPixbuf* gray = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                                    gdk_pixbuf_get_width(pixbuf),
                                    gdk_pixbuf_get_height(pixbuf));
    
    int channels = gdk_pixbuf_get_n_channels(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
    guchar* gray_pixels = gdk_pixbuf_get_pixels(gray);
    
    // Simple edge detection
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            float intensity = 0;
            
            // Sobel operator for edge detection
            float gx = 0, gy = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int pixel_idx = (y + dy) * rowstride + (x + dx) * channels;
                    float pixel_intensity = 0;
                    for (int c = 0; c < MIN(channels, 3); c++) {
                        pixel_intensity += pixels[pixel_idx + c];
                    }
                    pixel_intensity /= MIN(channels, 3);
                    
                    // Sobel weights
                    gx += pixel_intensity * dx;
                    gy += pixel_intensity * dy;
                }
            }
            
            intensity = sqrt(gx*gx + gy*gy) / 4.0f;  // Scale down edge intensity
            gray_pixels[y * width + x] = (guchar)CLAMP(intensity, 0, 255);
        }
    }
    
    // Convert edges to frequencies
    size_t num_samples = width * 100;  // 100 samples per pixel column
    audio->buffer_size = num_samples * sizeof(int16_t) * 2;
    audio->buffer = malloc(audio->buffer_size);
    
    printf("Converting image to audio:\n");
    printf("- Found %d edge points\n", width * height);
    printf("- Creating %zu audio samples\n", num_samples);
    
    // Create surface for edge visualization
    cairo_surface_t* edge_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t* cr = cairo_create(edge_surface);
    
    // Clear background
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);  // Transparent background
    cairo_paint(cr);
    
    // Draw detected edges
    cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 0.5);  // Semi-transparent green
    cairo_set_line_width(cr, 2.0);
    
    gboolean started = FALSE;
    int last_y = 0;
    
    for (int x = 0; x < width; x++) {
        // Find strongest edge in this column
        float max_edge = 0;
        int max_y = 0;
        for (int y = 0; y < height; y++) {
            float edge = gray_pixels[y * width + x] / 255.0f;
            if (edge > max_edge) {
                max_edge = edge;
                max_y = y;
            }
        }
        
        // Draw edge point if strong enough
        if (max_edge > 0.1f) {
            if (!started) {
                cairo_move_to(cr, x, max_y);
                started = TRUE;
            } else {
                cairo_curve_to(cr, 
                    x-0.5, last_y,
                    x-0.5, max_y,
                    x, max_y);
            }
            last_y = max_y;
            
            cairo_arc(cr, x, max_y, 1.5, 0, 2 * M_PI);
            cairo_fill(cr);
        } else if (started) {
            cairo_stroke(cr);
            started = FALSE;
        }
        
        // Convert edge position to frequency with more dramatic range
        float base_freq = 55.0f * pow(2.0f, (float)max_y / height * 8.0f);  // 8 octave range
        printf("Column %d: Edge at y=%d -> Base Frequency %.1f Hz\n", x, max_y, base_freq);
        
        // Generate audio samples with harmonics
        for (int t = 0; t < 100; t++) {
            float time = (float)t / 100.0f;
            float sample = 0;
            
            // Add fundamental frequency
            sample += 0.5f * max_edge * sin(2.0f * M_PI * base_freq * time);
            // Add octave
            sample += 0.25f * max_edge * sin(4.0f * M_PI * base_freq * time);
            // Add fifth
            sample += 0.15f * max_edge * sin(6.0f * M_PI * base_freq * time);
            
            // Add some noise for texture based on edge intensity
            sample += 0.1f * max_edge * ((float)rand() / RAND_MAX - 0.5f);
            
            int idx = (x * 100 + t) * 2;
            audio->buffer[idx] = (int16_t)(sample * 32767.0f);
            audio->buffer[idx + 1] = (int16_t)(sample * 32767.0f);
        }
    }
    
    if (started) {
        cairo_stroke(cr);
    }
    
    // Handle edge visualization only if visualizer is provided
    if (vis) {
        // Create surface for edge visualization
        cairo_surface_t* edge_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
        if (edge_surface) {
            cairo_t* cr = cairo_create(edge_surface);
            if (cr) {
                // Draw detected edges
                cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 0.5);  // Semi-transparent green
                cairo_set_line_width(cr, 2.0);
                
                gboolean started = FALSE;
                int last_y = 0;
                
                for (int x = 0; x < width; x++) {
                    // Find strongest edge in this column
                    float max_edge = 0;
                    int max_y = 0;
                    for (int y = 0; y < height; y++) {
                        float edge = gray_pixels[y * width + x] / 255.0f;
                        if (edge > max_edge) {
                            max_edge = edge;
                            max_y = y;
                        }
                    }
                    
                    // Draw edge point if strong enough
                    if (max_edge > 0.1f) {
                        if (!started) {
                            cairo_move_to(cr, x, max_y);
                            started = TRUE;
                        } else {
                            cairo_curve_to(cr, 
                                x-0.5, last_y,
                                x-0.5, max_y,
                                x, max_y);
                        }
                        last_y = max_y;
                        
                        cairo_arc(cr, x, max_y, 1.5, 0, 2 * M_PI);
                        cairo_fill(cr);
                    } else if (started) {
                        cairo_stroke(cr);
                        started = FALSE;
                    }
                    
                    // Convert edge position to frequency with more dramatic range
                    float base_freq = 55.0f * pow(2.0f, (float)max_y / height * 8.0f);  // 8 octave range
                    printf("Column %d: Edge at y=%d -> Base Frequency %.1f Hz\n", x, max_y, base_freq);
                    
                    // Generate audio samples with harmonics
                    for (int t = 0; t < 100; t++) {
                        float time = (float)t / 100.0f;
                        float sample = 0;
                        
                        // Add fundamental frequency
                        sample += 0.5f * max_edge * sin(2.0f * M_PI * base_freq * time);
                        // Add octave
                        sample += 0.25f * max_edge * sin(4.0f * M_PI * base_freq * time);
                        // Add fifth
                        sample += 0.15f * max_edge * sin(6.0f * M_PI * base_freq * time);
                        
                        // Add some noise for texture based on edge intensity
                        sample += 0.1f * max_edge * ((float)rand() / RAND_MAX - 0.5f);
                        
                        int idx = (x * 100 + t) * 2;
                        audio->buffer[idx] = (int16_t)(sample * 32767.0f);
                        audio->buffer[idx + 1] = (int16_t)(sample * 32767.0f);
                    }
                }
                
                if (started) {
                    cairo_stroke(cr);
                }
                
                // Store in visualizer
                if (vis->edge_surface) {
                    cairo_surface_destroy(vis->edge_surface);
                }
                vis->edge_surface = edge_surface;
                cairo_destroy(cr);
            } else {
                cairo_surface_destroy(edge_surface);
            }
        }
    }
    
    g_object_unref(gray);
    g_object_unref(pixbuf);
    return audio;
} 
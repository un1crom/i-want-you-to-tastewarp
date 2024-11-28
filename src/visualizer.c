#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fftw3.h>
#include "effects.h"
#include "visualizer.h"
#include "ui.h"

#define FFT_SIZE 2048
#define SPECTROGRAM_HEIGHT 256
#define EFFECT_DELAY_MS 2000  // Wait 2 seconds after last input

// Add these function declarations at the top with other forward declarations
static void init_fft();
static void cleanup_fft();
double calculate_line_width(double dx, double dy, double dt, double base_thickness);
void detect_and_apply_shape(Visualizer* vis);
static gboolean process_deferred_effects(gpointer data);

typedef struct {
    double* input;
    fftw_complex* output;
    fftw_plan plan;
} FFTContext;

static FFTContext* fft_context = NULL;

static void init_fft() {
    if (!fft_context) {
        fft_context = malloc(sizeof(FFTContext));
        fft_context->input = fftw_alloc_real(FFT_SIZE);
        fft_context->output = fftw_alloc_complex(FFT_SIZE / 2 + 1);
        fft_context->plan = fftw_plan_dft_r2c_1d(FFT_SIZE, 
                                                fft_context->input,
                                                fft_context->output,
                                                FFTW_MEASURE);
    }
}

static void cleanup_fft() {
    if (fft_context) {
        fftw_destroy_plan(fft_context->plan);
        fftw_free(fft_context->input);
        fftw_free(fft_context->output);
        free(fft_context);
        fft_context = NULL;
    }
}

void init_visualizer(Visualizer* vis, AudioPlayer* player) {
    vis->player = player;
    vis->color_scheme = 0;
    vis->drawing = FALSE;
    vis->last_x = 0;
    vis->last_y = 0;
    vis->draw_surface = NULL;
    vis->erase_mode = FALSE;
    vis->effect_timer_id = 0;
    vis->needs_processing = FALSE;
    vis->last_input_time = 0;
    
    // Create waveform drawing area
    vis->waveform_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(vis->waveform_drawing_area, -1, 150);
    gtk_widget_add_events(vis->waveform_drawing_area, GDK_BUTTON_PRESS_MASK);
    
    // Create spectrogram drawing area
    vis->spectrogram_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(vis->spectrogram_drawing_area, -1, SPECTROGRAM_HEIGHT);
    gtk_widget_set_vexpand(vis->spectrogram_drawing_area, TRUE);
    gtk_widget_set_valign(vis->spectrogram_drawing_area, GTK_ALIGN_FILL);
    gtk_widget_add_events(vis->spectrogram_drawing_area,
                         GDK_BUTTON_PRESS_MASK |
                         GDK_BUTTON_RELEASE_MASK |
                         GDK_POINTER_MOTION_MASK |
                         GDK_KEY_PRESS_MASK |
                         GDK_KEY_RELEASE_MASK);
    
    // Connect signals
    g_signal_connect(vis->waveform_drawing_area, "draw",
                    G_CALLBACK(draw_waveform), vis);
    g_signal_connect(vis->waveform_drawing_area, "button-press-event",
                    G_CALLBACK(on_waveform_click), vis);
    g_signal_connect(vis->spectrogram_drawing_area, "draw",
                    G_CALLBACK(draw_spectrogram), vis);
    
    // Drawing handlers only
    g_signal_connect(vis->spectrogram_drawing_area, "button-press-event",
                    G_CALLBACK(on_spectrogram_draw_start), vis);
    g_signal_connect(vis->spectrogram_drawing_area, "button-release-event",
                    G_CALLBACK(on_spectrogram_draw_end), vis);
    g_signal_connect(vis->spectrogram_drawing_area, "motion-notify-event",
                    G_CALLBACK(on_spectrogram_draw_motion), vis);
    g_signal_connect(vis->spectrogram_drawing_area, "key-press-event",
                    G_CALLBACK(on_key_press), vis);
    g_signal_connect(vis->spectrogram_drawing_area, "key-release-event",
                    G_CALLBACK(on_key_release), vis);
    
    init_fft();
}

void cleanup_visualizer(Visualizer* vis) {
    if (vis->draw_surface) {
        cairo_surface_destroy(vis->draw_surface);
        vis->draw_surface = NULL;
    }
    cleanup_fft();
    if (vis->effect_timer_id > 0) {
        g_source_remove(vis->effect_timer_id);
        vis->effect_timer_id = 0;
    }
}

gboolean update_visualizer(Visualizer* vis) {
    if (!vis || !vis->player || !vis->player->active_mix) return G_SOURCE_CONTINUE;
    
    gtk_widget_queue_draw(vis->waveform_drawing_area);
    gtk_widget_queue_draw(vis->spectrogram_drawing_area);
    
    return G_SOURCE_CONTINUE;
}

// Color schemes
static void set_color_by_intensity(cairo_t* cr, double intensity, int scheme) {
    switch (scheme) {
        case COLOR_CLASSIC:  // Original green
            cairo_set_source_rgb(cr, 0.0, intensity, 0.0);
            break;
            
        case COLOR_WARM:  // Warm colors
            cairo_set_source_rgb(cr, 
                intensity,                    // Red
                intensity * 0.6,              // Orange component
                intensity * 0.2);             // Slight yellow
            break;
            
        case COLOR_COOL:  // Cool colors
            cairo_set_source_rgb(cr,
                intensity * 0.2,              // Slight red
                intensity * 0.8,              // Blue-green
                intensity);                   // Full blue
            break;
            
        case COLOR_DARK:  // Dark theme
            cairo_set_source_rgb(cr,
                intensity * 0.3,              // Dark red
                0,                           // No green
                intensity * 0.4);             // Deep blue
            break;
            
        case COLOR_LIGHT:  // Light theme
            cairo_set_source_rgb(cr,
                0.7 + (intensity * 0.3),      // High base red
                0.7 + (intensity * 0.3),      // High base green
                0.8 + (intensity * 0.2));     // High base blue
            break;
            
        case COLOR_GOTH:  // Gothic theme
            cairo_set_source_rgb(cr,
                intensity * 0.8,              // Deep red
                intensity * 0.1,              // Almost no green
                intensity * 0.1);             // Almost no blue
            break;
            
        case COLOR_BAROQUE:  // Rich baroque
            cairo_set_source_rgb(cr,
                0.6 + (intensity * 0.4),      // Gold base
                0.4 * intensity,              // Rich middle
                0.1 + (intensity * 0.3));     // Deep undertones
            break;
            
        case COLOR_ROMANTIC:  // Romantic pastels
            cairo_set_source_rgb(cr,
                0.7 + (intensity * 0.3),      // Pink base
                0.6 + (intensity * 0.3),      // Soft middle
                0.8 + (intensity * 0.2));     // Lavender tint
            break;
    }
}

// Update pen color based on scheme
static void set_pen_color(cairo_t* cr, int scheme) {
    switch (scheme) {
        case COLOR_CLASSIC:
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.8);
            break;
        case COLOR_WARM:
            cairo_set_source_rgba(cr, 1.0, 0.6, 0.2, 0.8);
            break;
        case COLOR_COOL:
            cairo_set_source_rgba(cr, 0.2, 0.8, 1.0, 0.8);
            break;
        case COLOR_DARK:
            cairo_set_source_rgba(cr, 0.3, 0.0, 0.4, 0.8);
            break;
        case COLOR_LIGHT:
            cairo_set_source_rgba(cr, 1.0, 0.9, 0.7, 0.8);
            break;
        case COLOR_GOTH:
            cairo_set_source_rgba(cr, 0.8, 0.0, 0.0, 0.8);
            break;
        case COLOR_BAROQUE:
            cairo_set_source_rgba(cr, 0.8, 0.6, 0.2, 0.8);
            break;
        case COLOR_ROMANTIC:
            cairo_set_source_rgba(cr, 1.0, 0.7, 0.9, 0.8);
            break;
    }
}

// Add musical effects based on color scheme
void apply_color_scheme_effects(AudioPlayer* player, int scheme) {
    if (!player || !player->active_mix) return;
    
    switch (scheme) {
        case COLOR_WARM:
            // Warm: Add harmonics and slight distortion
            bit_mash(player, player->active_mix, 0.2);
            pitch_shift(player, player->active_mix, 0.5);
            break;
            
        case COLOR_COOL:
            // Cool: Add reverb and slight pitch down
            add_echo(player, player->active_mix, 300, 0.3);
            pitch_shift(player, player->active_mix, -0.5);
            break;
            
        case COLOR_DARK:
            // Dark: Heavy reverb and pitch down
            add_echo(player, player->active_mix, 500, 0.5);
            pitch_shift(player, player->active_mix, -2.0);
            break;
            
        case COLOR_LIGHT:
            // Light: Bright harmonics
            pitch_shift(player, player->active_mix, 2.0);
            break;
            
        case COLOR_GOTH:
            // Gothic: Distortion and deep pitch
            bit_mash(player, player->active_mix, 0.4);
            pitch_shift(player, player->active_mix, -4.0);
            break;
            
        case COLOR_BAROQUE:
            // Baroque: Rich harmonics
            add_echo(player, player->active_mix, 200, 0.3);
            pitch_shift(player, player->active_mix, 1.0);
            break;
            
        case COLOR_ROMANTIC:
            // Romantic: Soft reverb and slight pitch up
            add_echo(player, player->active_mix, 400, 0.2);
            pitch_shift(player, player->active_mix, 0.5);
            break;
    }
}

gboolean draw_waveform(GtkWidget* widget, cairo_t* cr, gpointer data) {
    Visualizer* vis = (Visualizer*)data;
    if (!vis || !vis->player || !vis->player->active_mix) {
        // Draw empty background
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
        cairo_paint(cr);
        return FALSE;
    }
    
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);
    
    // Clear background
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_paint(cr);
    
    // Draw waveform
    set_color_by_intensity(cr, 1.0, vis->color_scheme);
    cairo_set_line_width(cr, 1.0);
    
    size_t num_samples = vis->player->active_mix->buffer_size / sizeof(int16_t);
    size_t step = num_samples / width;
    if (step < 1) step = 1;
    
    cairo_move_to(cr, 0, height / 2);
    
    // Start drawing from current playback position
    size_t start_pos = vis->player->ring_buffer_pos;
    for (int x = 0; x < width; x++) {
        size_t idx = (start_pos + x * step) % num_samples;
        float sample = vis->player->active_mix->buffer[idx] / 32768.0f;
        int y = (int)(height / 2 * (1.0 - sample));
        cairo_line_to(cr, x, y);
    }
    
    cairo_stroke(cr);
    
    // Add click handler if not already connected
    static gboolean handler_connected = FALSE;
    if (!handler_connected) {
        g_signal_connect(G_OBJECT(widget), "button-press-event",
                        G_CALLBACK(on_waveform_click), data);
        handler_connected = TRUE;
    }
    
    return FALSE;
}

gboolean draw_spectrogram(GtkWidget* widget, cairo_t* cr, gpointer data) {
    Visualizer* vis = (Visualizer*)data;
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);
    
    // Create drawing surface if needed
    if (!vis->draw_surface) {
        vis->draw_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    }
    
    // Draw spectrogram
    // Clear background
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_paint(cr);
    
    // Prepare FFT data
    size_t num_samples = vis->player->active_mix->buffer_size / sizeof(int16_t);
    size_t start_idx = (vis->player->ring_buffer_pos + num_samples - FFT_SIZE) % num_samples;
    
    for (int i = 0; i < FFT_SIZE; i++) {
        size_t idx = (start_idx + i) % num_samples;
        fft_context->input[i] = vis->player->active_mix->buffer[idx] / 32768.0;
    }
    
    // Use a sliding window for the spectrogram
    static float* history = NULL;
    static int history_pos = 0;
    static const int HISTORY_WIDTH = 512;
    
    if (!history) {
        history = calloc(HISTORY_WIDTH * FFT_SIZE, sizeof(float));
    }
    
    // Perform FFT
    fftw_execute(fft_context->plan);
    
    // Store current FFT results in history
    int fft_height = FFT_SIZE / 4;  // Only show quarter of the spectrum
    if (fft_height > height) fft_height = height;
    
    for (int y = 0; y < fft_height; y++) {
        double magnitude = sqrt(
            fft_context->output[y][0] * fft_context->output[y][0] +
            fft_context->output[y][1] * fft_context->output[y][1]
        ) / FFT_SIZE;
        history[history_pos * fft_height + y] = magnitude;
    }
    
    // Draw spectrogram from history
    for (int y = 0; y < fft_height; y++) {
        for (int x = 0; x < width; x++) {
            int hist_idx = ((history_pos - x + HISTORY_WIDTH) % HISTORY_WIDTH);
            float magnitude = history[hist_idx * fft_height + y];
            
            // Convert magnitude to color (green intensity)
            double intensity = fmin(1.0, magnitude * 5.0);
            set_color_by_intensity(cr, intensity, vis->color_scheme);
            
            // Draw point
            cairo_rectangle(cr, x, height - y, 1, 1);
            cairo_fill(cr);
        }
    }
    
    history_pos = (history_pos + 1) % HISTORY_WIDTH;
    
    // Draw edge visualization if it exists
    if (vis->edge_surface) {
        cairo_set_source_surface(cr, vis->edge_surface, 0, 0);
        cairo_paint_with_alpha(cr, 0.7);  // Slightly transparent
    }
    
    // Overlay drawing surface
    if (vis->draw_surface) {
        cairo_set_source_surface(cr, vis->draw_surface, 0, 0);
        cairo_paint(cr);
    }
    
    return FALSE;
}

// Click handlers for interactive effects
gboolean on_waveform_click(GtkWidget* widget, GdkEventButton* event, gpointer data) {
    Visualizer* vis = (Visualizer*)data;
    if (!vis || !vis->player || !vis->player->active_mix) return TRUE;
    int width = gtk_widget_get_allocated_width(widget);
    
    // Apply random effect based on click position
    switch (rand() % 4) {
        case 0:
            bit_mash(vis->player, vis->player->active_mix, event->x / width);
            break;
        case 1:
            bit_drop(vis->player, vis->player->active_mix, event->x / width * 0.5f);
            break;
        case 2: {
            float tempo = 0.5f + (event->x / width) * 1.5f;
            tempo_shift(vis->player, vis->player->active_mix, tempo);
            break;
        }
        case 3:
            add_robot(vis->player, vis->player->active_mix, 1.0f + (event->x / width) * 10.0f);
            break;
    }
    mix_audio_files(vis->player);
    
    return TRUE;
}

void cycle_color_scheme(Visualizer* vis) {
    vis->color_scheme = (vis->color_scheme + 1) % NUM_COLOR_SCHEMES;
    
    // Apply corresponding audio effects
    apply_color_scheme_effects(vis->player, vis->color_scheme);
    
    // Redraw
    gtk_widget_queue_draw(vis->waveform_drawing_area);
    gtk_widget_queue_draw(vis->spectrogram_drawing_area);
}

gboolean on_spectrogram_draw_end(GtkWidget* widget G_GNUC_UNUSED, 
                                GdkEventButton* event G_GNUC_UNUSED, 
                                gpointer data) {
    Visualizer* vis = (Visualizer*)data;
    vis->drawing = FALSE;
    
    // Detect and apply shape effect
    detect_and_apply_shape(vis);
    
    return TRUE;
}

gboolean on_spectrogram_draw_start(GtkWidget* widget, GdkEventButton* event, gpointer data) {
    Visualizer* vis = (Visualizer*)data;
    vis->drawing = TRUE;
    vis->last_x = event->x;
    vis->last_y = event->y;
    vis->last_time = g_get_monotonic_time() / 1000.0;
    
    // Initialize stroke points
    if (!vis->stroke_points) {
        vis->stroke_points = g_array_new(FALSE, FALSE, sizeof(DrawPoint));
    } else {
        g_array_remove_range(vis->stroke_points, 0, vis->stroke_points->len);
    }
    
    DrawPoint p = {event->x, event->y};
    g_array_append_val(vis->stroke_points, p);
    
    // Create new drawing surface if needed
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);
    if (!vis->draw_surface) {
        vis->draw_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    }
    return TRUE;
}

gboolean on_spectrogram_draw_motion(GtkWidget* widget, GdkEventMotion* event, gpointer data) {
    Visualizer* vis = (Visualizer*)data;
    if (!vis->drawing) return FALSE;
    
    // Update last input time
    vis->last_input_time = g_get_monotonic_time() / 1000.0;
    vis->needs_processing = TRUE;
    
    // Start/restart effect timer if needed
    if (vis->effect_timer_id == 0) {
        vis->effect_timer_id = g_timeout_add(100, process_deferred_effects, vis);
    }
    
    // Get base thickness from UI slider (with safety checks)
    double base_thickness = 3.0;  // Default thickness
    if (vis->player && vis->player->ui_ptr) {
        UI* ui = vis->player->ui_ptr;
        if (ui->pen_thickness_scale && GTK_IS_RANGE(ui->pen_thickness_scale)) {
            base_thickness = gtk_range_get_value(GTK_RANGE(ui->pen_thickness_scale));
        }
    }
    
    // Calculate time delta
    double current_time = g_get_monotonic_time() / 1000.0;
    double dt = current_time - vis->last_time;
    
    // Calculate movement delta
    double dx = event->x - vis->last_x;
    double dy = event->y - vis->last_y;
    
    // Draw the visual line with dynamic width
    cairo_t* cr = cairo_create(vis->draw_surface);
    cairo_set_line_width(cr, calculate_line_width(dx, dy, dt, base_thickness));
    
    if (vis->erase_mode) {
        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        cairo_set_line_width(cr, 10.0);
    } else {
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
        set_pen_color(cr, vis->color_scheme);
    }
    
    cairo_move_to(cr, vis->last_x, vis->last_y);
    cairo_line_to(cr, event->x, event->y);
    cairo_stroke(cr);
    cairo_destroy(cr);
    
    // Store point for shape detection
    DrawPoint p = {event->x, event->y};
    g_array_append_val(vis->stroke_points, p);
    
    // Update position and time
    vis->last_x = event->x;
    vis->last_y = event->y;
    vis->last_time = current_time;
    
    // Queue redraw
    gtk_widget_queue_draw(widget);
    return TRUE;
}

gboolean on_key_press(GtkWidget* widget G_GNUC_UNUSED, 
                     GdkEventKey* event, 
                     gpointer data) {
    Visualizer* vis = (Visualizer*)data;
    if (event->keyval == GDK_KEY_Shift_L || event->keyval == GDK_KEY_Shift_R) {
        vis->erase_mode = TRUE;
    }
    return TRUE;
}

gboolean on_key_release(GtkWidget* widget G_GNUC_UNUSED, 
                       GdkEventKey* event, 
                       gpointer data) {
    Visualizer* vis = (Visualizer*)data;
    if (event->keyval == GDK_KEY_Shift_L || event->keyval == GDK_KEY_Shift_R) {
        vis->erase_mode = FALSE;
    }
    return TRUE;
}

void on_clear_clicked(GtkButton* button G_GNUC_UNUSED, gpointer data) {
    Visualizer* vis = (Visualizer*)data;
    if (vis->draw_surface) {
        cairo_t* cr = cairo_create(vis->draw_surface);
        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(cr);
        cairo_destroy(cr);
        gtk_widget_queue_draw(vis->spectrogram_drawing_area);
    }
}

void save_visualizations(Visualizer* vis) {
    if (!vis || !vis->player || !vis->player->active_mix) return;
    
    // Get base export path (without the WAV filename)
    char* export_path = get_export_path();
    char* base_path = g_path_get_dirname(export_path);  // Get just the directory
    g_free(export_path);  // Free the full WAV path
    
    // Generate filename with timestamp
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char filename[256];
    snprintf(filename, sizeof(filename), 
             "%s/tastewarp_visual_%04d%02d%02d_%02d%02d%02d.png",
             base_path,
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
    
    // Get dimensions
    int wave_width = gtk_widget_get_allocated_width(vis->waveform_drawing_area);
    int wave_height = gtk_widget_get_allocated_height(vis->waveform_drawing_area);
    int spec_height = gtk_widget_get_allocated_height(vis->spectrogram_drawing_area);
    
    // Create combined surface
    int total_height = wave_height + spec_height;
    cairo_surface_t* combined = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
                                                         wave_width, 
                                                         total_height);
    cairo_t* cr = cairo_create(combined);
    
    // Draw waveform
    cairo_save(cr);
    draw_waveform(vis->waveform_drawing_area, cr, vis);
    cairo_restore(cr);
    
    // Draw spectrogram below waveform
    cairo_save(cr);
    cairo_translate(cr, 0, wave_height);
    draw_spectrogram(vis->spectrogram_drawing_area, cr, vis);
    
    // Overlay drawing surface if it exists
    if (vis->draw_surface) {
        cairo_set_source_surface(cr, vis->draw_surface, 0, 0);
        cairo_paint(cr);
    }
    cairo_restore(cr);
    
    // Save to PNG
    cairo_surface_write_to_png(combined, filename);
    
    // Cleanup
    cairo_destroy(cr);
    cairo_surface_destroy(combined);
    
    printf("Saved visualizations to: %s\n", filename);
    
    g_free(base_path);
}

void on_save_visuals_clicked(GtkButton* button G_GNUC_UNUSED, gpointer data) {
    Visualizer* vis = (Visualizer*)data;
    save_visualizations(vis);
}

// Move the implementations up before they're used
double calculate_line_width(double dx, double dy, double dt, double base_thickness) {
    if (dt <= 0) return base_thickness;  // When not moving
    
    // Calculate speed in pixels per millisecond
    double speed = sqrt(dx*dx + dy*dy) / dt;
    
    // More dramatic inverse relationship with customizable base thickness
    double width = base_thickness * exp(-speed * 0.2);  // More sensitive to speed
    
    // Clamp between 1 and base_thickness
    return CLAMP(width, 1.0, base_thickness);
}

void detect_and_apply_shape(Visualizer* vis) {
    if (!vis->stroke_points || vis->stroke_points->len < 3) return;
    
    // Calculate bounding box
    double min_x = G_MAXDOUBLE, min_y = G_MAXDOUBLE;
    double max_x = -G_MAXDOUBLE, max_y = -G_MAXDOUBLE;
    
    for (guint i = 0; i < vis->stroke_points->len; i++) {
        DrawPoint* p = &g_array_index(vis->stroke_points, DrawPoint, i);
        min_x = MIN(min_x, p->x);
        min_y = MIN(min_y, p->y);
        max_x = MAX(max_x, p->x);
        max_y = MAX(max_y, p->y);
    }
    
    double width = max_x - min_x;
    double height = max_y - min_y;
    double aspect_ratio = width / height;
    
    // Check if it's a line across the spectrogram
    if (width > gtk_widget_get_allocated_width(vis->spectrogram_drawing_area) * 0.9) {
        printf("Detected: Horizontal line (reset gesture)\n");
        printf("Effect: Resetting audio to original\n");
        reset_to_original(vis->player);
        return;
    }
    
    // Analyze shape based on aspect ratio and size
    printf("Shape analysis:\n");
    printf("- Width: %.1f pixels\n", width);
    printf("- Height: %.1f pixels\n", height);
    printf("- Aspect ratio: %.2f\n", aspect_ratio);
    printf("- Number of points: %u\n", vis->stroke_points->len);
    
    // Calculate and apply effects
    float intensity = width / gtk_widget_get_allocated_width(vis->spectrogram_drawing_area);
    float pitch = (height / gtk_widget_get_allocated_height(vis->spectrogram_drawing_area)) * 12.0;
    
    printf("Applying effects:\n");
    printf("- Bit mash intensity: %.2f\n", intensity);
    printf("- Pitch shift amount: %.1f semitones\n", pitch);
    
    bit_mash(vis->player, vis->player->active_mix, intensity);
    pitch_shift(vis->player, vis->player->active_mix, pitch);
}

// Add this function to process effects after delay
static gboolean process_deferred_effects(gpointer data) {
    Visualizer* vis = (Visualizer*)data;
    
    // Check if enough time has passed since last input
    double current_time = g_get_monotonic_time() / 1000.0;
    if (current_time - vis->last_input_time < EFFECT_DELAY_MS) {
        return G_SOURCE_CONTINUE;  // Keep waiting
    }
    
    if (vis->needs_processing && !vis->drawing) {
        // Process the shape and apply effects
        detect_and_apply_shape(vis);
        vis->needs_processing = FALSE;
        vis->effect_timer_id = 0;
        return G_SOURCE_REMOVE;  // Stop timer
    }
    
    return G_SOURCE_CONTINUE;
} 
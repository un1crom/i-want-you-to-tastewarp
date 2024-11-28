#ifndef VISUALIZER_H
#define VISUALIZER_H

#include "visualizer_types.h"
#include "ui_types.h"

// Function declarations
void init_visualizer(Visualizer* vis, AudioPlayer* player);
void cleanup_visualizer(Visualizer* vis);
gboolean update_visualizer(Visualizer* vis);
gboolean draw_waveform(GtkWidget* widget, cairo_t* cr, gpointer data);
gboolean draw_spectrogram(GtkWidget* widget, cairo_t* cr, gpointer data);
void cycle_color_scheme(Visualizer* vis);
gboolean on_spectrogram_draw_start(GtkWidget* widget, GdkEventButton* event, gpointer data);
gboolean on_spectrogram_draw_end(GtkWidget* widget, GdkEventButton* event, gpointer data);
gboolean on_spectrogram_draw_motion(GtkWidget* widget, GdkEventMotion* event, gpointer data);
gboolean on_waveform_click(GtkWidget* widget, GdkEventButton* event, gpointer data);
gboolean on_spectrogram_click(GtkWidget* widget, GdkEventButton* event, gpointer data);
gboolean on_key_press(GtkWidget* widget, GdkEventKey* event, gpointer data);
gboolean on_key_release(GtkWidget* widget, GdkEventKey* event, gpointer data);
void on_clear_clicked(GtkButton* button, gpointer data);
void save_visualizations(Visualizer* vis);
void on_save_visuals_clicked(GtkButton* button, gpointer data);

// Add to color scheme definitions
typedef enum {
    COLOR_CLASSIC = 0,    // Original green
    COLOR_WARM,           // Reds, oranges, golds
    COLOR_COOL,           // Blues, cyans, silvers
    COLOR_DARK,           // Deep purples, blacks, midnight blues
    COLOR_LIGHT,          // Pastels, whites, soft yellows
    COLOR_GOTH,           // Black, blood red, dark greys
    COLOR_BAROQUE,        // Rich golds, deep reds, royal purples
    COLOR_ROMANTIC,       // Rose pinks, lavenders, soft blues
    NUM_COLOR_SCHEMES
} ColorScheme;

#endif 
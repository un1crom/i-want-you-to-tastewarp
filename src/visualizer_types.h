#ifndef VISUALIZER_TYPES_H
#define VISUALIZER_TYPES_H

#include <gtk/gtk.h>
#include "types.h"

typedef struct {
    GtkWidget* waveform_drawing_area;
    GtkWidget* spectrogram_drawing_area;
    AudioPlayer* player;
    int color_scheme;
    gboolean drawing;
    gdouble last_x;
    gdouble last_y;
    gdouble last_time;
    cairo_surface_t* draw_surface;
    cairo_surface_t* edge_surface;
    gboolean erase_mode;
    GArray* stroke_points;
    
    // Add new fields for deferred processing
    guint effect_timer_id;     // Timer ID for deferred effects
    gboolean needs_processing; // Flag for pending effects
    gdouble last_input_time;  // Time of last user input
} Visualizer;

typedef struct {
    double x;
    double y;
} DrawPoint;

#endif 
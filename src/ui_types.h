#ifndef UI_TYPES_H
#define UI_TYPES_H

#include <gtk/gtk.h>
#include "audio.h"
#include "visualizer_types.h"

typedef struct UI {
    GtkWidget* window;
    GtkWidget* menubar;
    GtkWidget* file_menu;
    GtkWidget* recent_menu;
    GtkWidget* bit_mash_button;
    GtkWidget* bit_drop_button;
    GtkWidget* tempo_up_button;
    GtkWidget* tempo_down_button;
    GtkWidget* pitch_up_button;
    GtkWidget* pitch_down_button;
    GtkWidget* random_effect_button;
    GtkWidget* echo_button;
    GtkWidget* robot_button;
    GtkWidget* export_button;
    GtkWidget* color_button;
    GtkWidget* reset_button;
    GtkWidget* mix_controls;
    Visualizer visualizer;
    AudioPlayer* player;
    GList* loaded_files;    // List of loaded WAV files
    GList* export_files;    // List of exported WAV files
    int export_counter;     // For auto-incrementing export names
    GtkWidget* clear_button;
    GtkWidget* save_visuals_button;
    GtkWidget* pen_thickness_scale;  // Pen thickness slider
} UI;

#endif 
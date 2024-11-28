#ifndef UI_H
#define UI_H

#include <gtk/gtk.h>
#include "audio.h"
#include "visualizer.h"
#include "ui_types.h"

// Forward declaration
typedef struct UI UI;

// Function prototypes
void update_recent_menu(UI* ui, const char* filename);

// Button callbacks
void on_bit_mash_clicked(GtkButton* button, gpointer data);
void on_bit_drop_clicked(GtkButton* button, gpointer data);
void on_tempo_up_clicked(GtkButton* button, gpointer data);
void on_tempo_down_clicked(GtkButton* button, gpointer data);
void on_pitch_up_clicked(GtkButton* button, gpointer data);
void on_pitch_down_clicked(GtkButton* button, gpointer data);
void on_random_effect_clicked(GtkButton* button, gpointer data);
void on_echo_clicked(GtkButton* button, gpointer data);
void on_robot_clicked(GtkButton* button, gpointer data);
void on_export_clicked(GtkButton* button, gpointer data);
void on_color_clicked(GtkButton* button, gpointer data);
void on_open_file(GtkMenuItem* item, gpointer data);
void on_reset(GtkMenuItem* item, gpointer data);
void on_volume_changed(GtkRange* range, gpointer data);
void on_remove_file(GtkButton* button, gpointer data);
void on_import_image(GtkMenuItem* item, gpointer data);

void create_ui(UI* ui, AudioPlayer* player);
void cleanup_ui(UI* ui);

#endif 
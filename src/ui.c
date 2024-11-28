#include "ui.h"
#include "effects.h"

// Forward declarations of static functions
static void create_menu(UI* ui);
static void create_mix_controls(UI* ui);
static void update_mix_controls(UI* ui);
static void on_recent_export_clicked(GtkMenuItem* item, gpointer data);
static gboolean on_window_delete_event(GtkWidget* widget G_GNUC_UNUSED, 
                                     GdkEvent* event G_GNUC_UNUSED, 
                                     gpointer data);

void create_ui(UI* ui, AudioPlayer* player) {
    ui->player = player;
    player->ui_ptr = ui;
    ui->export_counter = 0;
    ui->loaded_files = NULL;
    ui->export_files = NULL;
    
    // Create main window
    ui->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ui->window), "TasteWarp");
    gtk_window_set_default_size(GTK_WINDOW(ui->window), 800, 600);
    g_signal_connect(G_OBJECT(ui->window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(ui->window), vbox);
    
    // Create menu
    create_menu(ui);
    gtk_box_pack_start(GTK_BOX(vbox), ui->menubar, FALSE, FALSE, 0);
    
    // Create mix controls
    create_mix_controls(ui);
    gtk_box_pack_start(GTK_BOX(vbox), ui->mix_controls, FALSE, FALSE, 5);
    update_mix_controls(ui);
    
    // Create button toolbar
    GtkWidget* toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 5);
    
    // Create all buttons
    ui->bit_mash_button = gtk_button_new_with_label("Bit Mash");
    ui->bit_drop_button = gtk_button_new_with_label("Bit Drop");
    ui->tempo_up_button = gtk_button_new_with_label("Tempo +");
    ui->tempo_down_button = gtk_button_new_with_label("Tempo -");
    ui->pitch_up_button = gtk_button_new_with_label("Pitch +");
    ui->pitch_down_button = gtk_button_new_with_label("Pitch -");
    ui->random_effect_button = gtk_button_new_with_label("Random");
    ui->echo_button = gtk_button_new_with_label("Echo");
    ui->robot_button = gtk_button_new_with_label("Robot");
    ui->export_button = gtk_button_new_with_label("Export");
    ui->color_button = gtk_button_new_with_label("Color");
    ui->clear_button = gtk_button_new_with_label("Clear Drawing");
    ui->save_visuals_button = gtk_button_new_with_label("Save Visuals");
    
    // Add buttons to toolbar
    gtk_box_pack_start(GTK_BOX(toolbar), ui->bit_mash_button, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(toolbar), ui->bit_drop_button, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(toolbar), ui->tempo_up_button, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(toolbar), ui->tempo_down_button, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(toolbar), ui->pitch_up_button, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(toolbar), ui->pitch_down_button, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(toolbar), ui->random_effect_button, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(toolbar), ui->echo_button, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(toolbar), ui->robot_button, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(toolbar), ui->export_button, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(toolbar), ui->color_button, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(toolbar), ui->clear_button, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(toolbar), ui->save_visuals_button, TRUE, TRUE, 2);
    
    // Add pen thickness control
    GtkWidget* thickness_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget* thickness_label = gtk_label_new("Pen Size:");
    ui->pen_thickness_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1.0, 10.0, 0.5);
    gtk_range_set_value(GTK_RANGE(ui->pen_thickness_scale), 3.0);  // Default thickness
    
    gtk_box_pack_start(GTK_BOX(thickness_box), thickness_label, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(thickness_box), ui->pen_thickness_scale, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(toolbar), thickness_box, TRUE, TRUE, 2);
    
    // Initialize visualizer
    init_visualizer(&ui->visualizer, player);
    gtk_box_pack_start(GTK_BOX(vbox), ui->visualizer.waveform_drawing_area, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), ui->visualizer.spectrogram_drawing_area, TRUE, TRUE, 5);
    
    // Connect button signals
    g_signal_connect(ui->bit_mash_button, "clicked", G_CALLBACK(on_bit_mash_clicked), player);
    g_signal_connect(ui->bit_drop_button, "clicked", G_CALLBACK(on_bit_drop_clicked), player);
    g_signal_connect(ui->tempo_up_button, "clicked", G_CALLBACK(on_tempo_up_clicked), player);
    g_signal_connect(ui->tempo_down_button, "clicked", G_CALLBACK(on_tempo_down_clicked), player);
    g_signal_connect(ui->pitch_up_button, "clicked", G_CALLBACK(on_pitch_up_clicked), player);
    g_signal_connect(ui->pitch_down_button, "clicked", G_CALLBACK(on_pitch_down_clicked), player);
    g_signal_connect(ui->random_effect_button, "clicked", G_CALLBACK(on_random_effect_clicked), player);
    g_signal_connect(ui->echo_button, "clicked", G_CALLBACK(on_echo_clicked), player);
    g_signal_connect(ui->robot_button, "clicked", G_CALLBACK(on_robot_clicked), player);
    g_signal_connect(ui->export_button, "clicked", G_CALLBACK(on_export_clicked), player);
    g_signal_connect(ui->color_button, "clicked", G_CALLBACK(on_color_clicked), &ui->visualizer);
    g_signal_connect(ui->clear_button, "clicked", G_CALLBACK(on_clear_clicked), &ui->visualizer);
    g_signal_connect(ui->save_visuals_button, "clicked", G_CALLBACK(on_save_visuals_clicked), &ui->visualizer);
    
    // Show all widgets
    gtk_widget_show_all(ui->window);
    
    // Start visualization update timer
    g_timeout_add_full(G_PRIORITY_DEFAULT, 33, 
                      (GSourceFunc)update_visualizer,
                      &ui->visualizer,
                      NULL);
    
    // Store UI pointer in all buttons that need it
    g_object_set_data(G_OBJECT(ui->export_button), "ui", ui);
    g_object_set_data(G_OBJECT(ui->bit_mash_button), "ui", ui);
    g_object_set_data(G_OBJECT(ui->bit_drop_button), "ui", ui);
    // ... set for all other buttons ...
    
    // Create mix controls
    create_mix_controls(ui);
    g_object_set_data(G_OBJECT(ui->mix_controls), "ui", ui);
    
    // Connect window close handler
    g_signal_connect(G_OBJECT(ui->window), "delete-event",
                    G_CALLBACK(on_window_delete_event), ui);
}

void cleanup_ui(UI* ui) {
    // Clean up loaded files list
    g_list_free_full(ui->loaded_files, g_free);
    
    // Clean up export files list
    g_list_free_full(ui->export_files, g_free);
    
    // Clean up visualizer
    cleanup_visualizer(&ui->visualizer);
}

// Button callback implementations
void on_bit_mash_clicked(GtkButton* button, gpointer data) {
    (void)button;
    AudioPlayer* player = (AudioPlayer*)data;
    if (!player || !player->active_mix) return;
    
    // Apply effect directly to active mix
    bit_mash(player, player->active_mix, 0.5f);
}

void on_bit_drop_clicked(GtkButton* button, gpointer data) {
    (void)button;
    AudioPlayer* player = (AudioPlayer*)data;
    if (!player || !player->active_mix) return;
    bit_drop(player, player->active_mix, 0.2f);
    mix_audio_files(player);
}

void on_tempo_up_clicked(GtkButton* button, gpointer data) {
    (void)button;
    AudioPlayer* player = (AudioPlayer*)data;
    tempo_shift(player, player->active_mix, 1.2f);
    mix_audio_files(player);
}

void on_tempo_down_clicked(GtkButton* button, gpointer data) {
    (void)button;
    AudioPlayer* player = (AudioPlayer*)data;
    tempo_shift(player, player->active_mix, 0.8f);
    mix_audio_files(player);
}

void on_pitch_up_clicked(GtkButton* button, gpointer data) {
    (void)button;
    AudioPlayer* player = (AudioPlayer*)data;
    pitch_shift(player, player->active_mix, 2.0f);
    mix_audio_files(player);
}

void on_pitch_down_clicked(GtkButton* button, gpointer data) {
    (void)button;
    AudioPlayer* player = (AudioPlayer*)data;
    if (!player || !player->active_mix) return;
    
    // Apply effect directly to active mix
    pitch_shift(player, player->active_mix, -2.0f);
}

void on_random_effect_clicked(GtkButton* button, gpointer data) {
    (void)button;
    AudioPlayer* player = (AudioPlayer*)data;
    random_effect(player, player->active_mix);
    mix_audio_files(player);
}

void on_echo_clicked(GtkButton* button, gpointer data) {
    (void)button;
    AudioPlayer* player = (AudioPlayer*)data;
    add_echo(player, player->active_mix, 200.0f, 0.5f);
    mix_audio_files(player);
}

void on_robot_clicked(GtkButton* button, gpointer data) {
    (void)button;
    AudioPlayer* player = (AudioPlayer*)data;
    add_robot(player, player->active_mix, 5.0f);
    mix_audio_files(player);
}

void on_export_clicked(GtkButton* button, gpointer data) {
    AudioPlayer* player = (AudioPlayer*)data;
    UI* ui = g_object_get_data(G_OBJECT(button), "ui");
    if (!ui || !player || !player->active_mix) return;
    
    export_last_60_seconds(player);
}

static void create_menu(UI* ui) {
    // Create menu bar
    ui->menubar = gtk_menu_bar_new();
    
    // File menu
    GtkWidget* file_menu_item = gtk_menu_item_new_with_label("File");
    ui->file_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_menu_item), ui->file_menu);
    
    // Add file menu items
    GtkWidget* open_item = gtk_menu_item_new_with_label("Open WAV...");
    GtkWidget* reset_item = gtk_menu_item_new_with_label("Reset to Original");
    ui->recent_menu = gtk_menu_new();
    GtkWidget* recent_item = gtk_menu_item_new_with_label("Recent Exports");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(recent_item), ui->recent_menu);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(ui->file_menu), open_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(ui->file_menu), reset_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(ui->file_menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(ui->file_menu), recent_item);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(ui->menubar), file_menu_item);
    
    // Connect signals
    g_signal_connect(G_OBJECT(open_item), "activate", G_CALLBACK(on_open_file), ui);
    g_signal_connect(G_OBJECT(reset_item), "activate", G_CALLBACK(on_reset), ui);
    
    // Add Import Image item after Open WAV
    GtkWidget* import_image_item = gtk_menu_item_new_with_label("Import Image...");
    gtk_menu_shell_append(GTK_MENU_SHELL(ui->file_menu), import_image_item);
    g_signal_connect(G_OBJECT(import_image_item), "activate", G_CALLBACK(on_import_image), ui);
}

static void create_mix_controls(UI* ui) {
    ui->mix_controls = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget* label = gtk_label_new("Active Files:");
    gtk_box_pack_start(GTK_BOX(ui->mix_controls), label, FALSE, FALSE, 2);
}

static void update_mix_controls(UI* ui) {
    // Remove old controls
    GList* children = gtk_container_get_children(GTK_CONTAINER(ui->mix_controls));
    for (GList* l = children->next; l != NULL; l = l->next) { // Skip label
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);
    
    // Add slider for each audio file
    for (GList* l = ui->player->audio_files; l != NULL; l = l->next) {
        AudioData* audio = (AudioData*)l->data;
        GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        
        // File name label
        char* basename = g_path_get_basename(audio->filename);
        GtkWidget* name = gtk_label_new(basename);
        g_free(basename);
        
        // Volume slider
        GtkWidget* scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 2.0, 0.1);
        gtk_range_set_value(GTK_RANGE(scale), audio->mix_volume);
        g_object_set_data(G_OBJECT(scale), "player", ui->player);
        g_signal_connect(G_OBJECT(scale), "value-changed",
                        G_CALLBACK(on_volume_changed), audio);
        
        // Remove button
        GtkWidget* remove_btn = gtk_button_new_with_label("×");
        g_signal_connect(G_OBJECT(remove_btn), "clicked",
                        G_CALLBACK(on_remove_file), audio);
        
        gtk_box_pack_start(GTK_BOX(hbox), name, FALSE, FALSE, 2);
        gtk_box_pack_start(GTK_BOX(hbox), scale, TRUE, TRUE, 2);
        gtk_box_pack_start(GTK_BOX(hbox), remove_btn, FALSE, FALSE, 2);
        
        gtk_box_pack_start(GTK_BOX(ui->mix_controls), hbox, FALSE, FALSE, 2);
    }
    
    gtk_widget_show_all(ui->mix_controls);
} 

void on_open_file(GtkMenuItem* item, gpointer data) {
    (void)item;
    UI* ui = (UI*)data;
    
    GtkWidget* dialog = gtk_file_chooser_dialog_new("Open WAV File",
                                                   GTK_WINDOW(ui->window),
                                                   GTK_FILE_CHOOSER_ACTION_OPEN,
                                                   "_Cancel", GTK_RESPONSE_CANCEL,
                                                   "_Open", GTK_RESPONSE_ACCEPT,
                                                   NULL);
    
    // Add file filter for WAV files
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.wav");
    gtk_file_filter_set_name(filter, "WAV files");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        add_audio_file(ui->player, filename);
        update_mix_controls(ui);
        g_free(filename);
    }
    
    gtk_widget_destroy(dialog);
}

void on_reset(GtkMenuItem* item, gpointer data) {
    (void)item;
    UI* ui = (UI*)data;
    reset_to_original(ui->player);
    update_mix_controls(ui);
}

void on_volume_changed(GtkRange* range, gpointer data) {
    AudioData* audio = (AudioData*)data;
    audio->mix_volume = gtk_range_get_value(range);
    mix_audio_files((AudioPlayer*)g_object_get_data(G_OBJECT(range), "player"));
}

void on_remove_file(GtkButton* button, gpointer data) {
    (void)button;
    AudioData* audio = (AudioData*)data;
    UI* ui = g_object_get_data(G_OBJECT(button), "ui");
    remove_audio_file(ui->player, audio);
    update_mix_controls(ui);
}

void update_recent_menu(UI* ui, const char* filename) {
    // Remove old menu items
    GList* children = gtk_container_get_children(GTK_CONTAINER(ui->recent_menu));
    for (GList* l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);
    
    // Add the new export to the list
    ui->export_files = g_list_append(ui->export_files, g_strdup(filename));
    
    // Keep only the last 10 exports
    while (g_list_length(ui->export_files) > 10) {
        GList* first = g_list_first(ui->export_files);
        g_free(first->data);
        ui->export_files = g_list_delete_link(ui->export_files, first);
    }
    
    // Add menu items for recent exports (most recent first)
    for (GList* l = g_list_last(ui->export_files); l != NULL; l = l->prev) {
        char* basename = g_path_get_basename((char*)l->data);
        GtkWidget* item = gtk_menu_item_new_with_label(basename);
        g_free(basename);
        
        // Store full path in item data
        g_object_set_data_full(G_OBJECT(item), "filename", 
                              g_strdup((char*)l->data), g_free);
        g_signal_connect(G_OBJECT(item), "activate", 
                        G_CALLBACK(on_recent_export_clicked), ui);
        
        gtk_menu_shell_prepend(GTK_MENU_SHELL(ui->recent_menu), item);
    }
    
    gtk_widget_show_all(ui->recent_menu);
}

// Add this function to handle clicking on recent exports
static void on_recent_export_clicked(GtkMenuItem* item, gpointer data) {
    UI* ui = (UI*)data;
    const char* filename = g_object_get_data(G_OBJECT(item), "filename");
    
    if (!filename) {
        printf("Error: No filename associated with menu item\n");
        return;
    }
    
    // Check if file exists
    if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
        GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(ui->window),
                                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_MESSAGE_ERROR,
                                                 GTK_BUTTONS_CLOSE,
                                                 "File not found: %s", filename);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }
    
    // Load the exported file
    AudioData* audio = load_wav_file(filename);
    if (!audio) {
        GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(ui->window),
                                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_MESSAGE_ERROR,
                                                 GTK_BUTTONS_CLOSE,
                                                 "Error loading file: %s", filename);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }
    
    // Reset the player before adding new file
    if (ui->player) {
        reset_to_original(ui->player);
        
        // Add the new file
        add_audio_file(ui->player, filename);
        update_mix_controls(ui);
    } else {
        printf("Error: Player not initialized\n");
        free(audio->buffer);
        free(audio->filename);
        free(audio);
    }
}

void on_color_clicked(GtkButton* button, gpointer data) {
    (void)button;
    Visualizer* vis = (Visualizer*)data;
    cycle_color_scheme(vis);
}

static gboolean on_window_delete_event(GtkWidget* widget G_GNUC_UNUSED, 
                                     GdkEvent* event G_GNUC_UNUSED, 
                                     gpointer data) {
    UI* ui = (UI*)data;
    
    // Stop audio playback
    stop_audio(ui->player);
    
    // Clean up audio player
    cleanup_audio_player(ui->player);
    
    // Clean up visualizer
    cleanup_visualizer(&ui->visualizer);
    
    // Allow window to close
    gtk_main_quit();
    return FALSE;
}

void on_import_image(GtkMenuItem* item G_GNUC_UNUSED, gpointer data) {
    UI* ui = (UI*)data;
    
    GtkWidget* dialog = gtk_file_chooser_dialog_new("Import Image",
                                                   GTK_WINDOW(ui->window),
                                                   GTK_FILE_CHOOSER_ACTION_OPEN,
                                                   "_Cancel", GTK_RESPONSE_CANCEL,
                                                   "_Open", GTK_RESPONSE_ACCEPT,
                                                   NULL);
    
    // Add file filters
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Image files");
    gtk_file_filter_add_pattern(filter, "*.png");
    gtk_file_filter_add_pattern(filter, "*.jpg");
    gtk_file_filter_add_pattern(filter, "*.jpeg");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        
        // Pass visualizer to create_audio_from_image
        AudioData* audio = create_audio_from_image(filename, &ui->visualizer);
        if (audio) {
            reset_to_original(ui->player);
            add_audio_file(ui->player, filename);
            update_mix_controls(ui);
        } else {
            // Show error dialog
            GtkWidget* error_dialog = gtk_message_dialog_new(GTK_WINDOW(ui->window),
                                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                                           GTK_MESSAGE_ERROR,
                                                           GTK_BUTTONS_CLOSE,
                                                           "Failed to convert image to audio");
            gtk_dialog_run(GTK_DIALOG(error_dialog));
            gtk_widget_destroy(error_dialog);
        }
        
        g_free(filename);
    }
    
    gtk_widget_destroy(dialog);
}
 
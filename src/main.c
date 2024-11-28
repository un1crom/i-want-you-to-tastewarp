#include <gtk/gtk.h>
#include <time.h>
#include <glib/gstdio.h>
#include "audio.h"
#include "ui.h"

#define APP_NAME "TasteWarp"
#define APP_VERSION "1.0"

// Get the resource path for macOS app bundle
static char* get_resource_path(const char* filename) {
#ifdef __APPLE__
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
    char path[PATH_MAX];
    
    if (CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX)) {
        CFRelease(resourcesURL);
        return g_build_filename(path, filename, NULL);
    }
    
    CFRelease(resourcesURL);
    return g_strdup(filename);
#else
    return g_strdup(filename);
#endif
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    // Get path to wav file resource
    char* wav_path = get_resource_path("wav.wav");
    AudioData* audio = load_wav_file(wav_path);
    g_free(wav_path);
    
    if (!audio) {
        fprintf(stderr, "Error: Could not load audio file\n");
        return 1;
    }
    
    // Initialize audio player
    AudioPlayer player;
    memset(&player, 0, sizeof(AudioPlayer));
    init_audio_player(&player, audio);
    
    // Create UI
    UI ui;
    memset(&ui, 0, sizeof(UI));
    create_ui(&ui, &player);
    
    // Start GTK main loop
    gtk_main();
    
    // Cleanup
    cleanup_ui(&ui);
    cleanup_audio_player(&player);
    
    return 0;
} 
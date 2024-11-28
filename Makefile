CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
UNAME := $(shell uname)

# Platform-specific settings
ifeq ($(UNAME), Darwin)
    # macOS (Homebrew)
    INCLUDES = $(shell pkg-config --cflags gtk+-3.0) -I/opt/homebrew/include
    LIBS = $(shell pkg-config --libs gtk+-3.0) -L/opt/homebrew/lib -lm -lfftw3 -framework AudioToolbox -framework CoreAudio
else
    # Linux - use standard paths
    INCLUDES = -I/usr/include/gtk-3.0 \
              -I/usr/include/glib-2.0 \
              -I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
              -I/usr/include/pango-1.0 \
              -I/usr/include/cairo \
              -I/usr/include/gdk-pixbuf-2.0 \
              -I/usr/include/atk-1.0 \
              -I/usr/include/harfbuzz
    
    LIBS = -lgtk-3 -lgdk-3 -lpangocairo-1.0 -lpango-1.0 -lgobject-2.0 \
           -lglib-2.0 -lcairo -lgdk_pixbuf-2.0 -lfftw3 -lm -lpulse -lpulse-simple
endif

# Source files
SRCS = src/audio.c src/effects.c src/main.c src/ui.c src/visualizer.c
OBJS = $(SRCS:src/%.c=obj/%.o)

# Target executable
TARGET = tastewarp

# Default target
ifeq ($(UNAME), Linux)
    all: rebuild
else
    all: $(TARGET)
endif

# Create object directory
$(shell mkdir -p obj)

# Compile source files
obj/%.o: src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Link object files
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LIBS)

# Clean build files
clean:
	rm -rf obj
	rm -f $(TARGET)

# Full rebuild target
rebuild: clean
	mkdir -p obj
	$(MAKE) $(TARGET)

.PHONY: all clean rebuild
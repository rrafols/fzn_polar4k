# Polarfield - macOS x86_64 port
#
#   make            build/polarfield
#   make run        build and run windowed
#   make ARCH=arm64 native Apple Silicon build instead of x86_64

ARCH   ?= x86_64
CC      = clang
COMMON  = ../../common
BUILD   = build
TARGET  = $(BUILD)/polarfield

CFLAGS  = -O2 -arch $(ARCH) -std=c99 -Wall -Wextra -I$(COMMON) -mmacosx-version-min=10.13
OBJCFLAGS = -O2 -arch $(ARCH) -fobjc-arc -Wall -Wextra -Wno-unused-parameter -I$(COMMON) -mmacosx-version-min=10.13
LDFLAGS = -arch $(ARCH) -framework Cocoa -framework OpenGL -framework AudioToolbox -framework CoreAudio

SRC_C   = $(COMMON)/polar_scene.c $(COMMON)/polar_render.c $(COMMON)/polar_synth.c
SRC_M   = main_mac.m
OBJ     = $(patsubst $(COMMON)/%.c,$(BUILD)/%.o,$(SRC_C)) $(BUILD)/main_mac.o

all: $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: $(COMMON)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/main_mac.o: main_mac.m | $(BUILD)
	$(CC) $(OBJCFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) $^ -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD)

.PHONY: all run clean

# headless capture tool (frames as PNG + song as WAV), see tools/capture_mac.c
CAPTURE = $(BUILD)/capture
$(BUILD)/capture_mac.o: ../../tools/capture_mac.c | $(BUILD)
	$(CC) $(CFLAGS) -Wno-misleading-indentation -c $< -o $@
$(CAPTURE): $(BUILD)/polar_scene.o $(BUILD)/polar_render.o $(BUILD)/polar_synth.o $(BUILD)/capture_mac.o
	$(CC) -arch $(ARCH) -framework OpenGL -framework ImageIO -framework CoreServices -framework CoreGraphics $^ -o $@
capture: $(CAPTURE)
.PHONY: capture

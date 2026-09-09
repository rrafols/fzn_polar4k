# Polarfield - web port (WebAssembly + WebGL2 via Emscripten)
#
#   make            build/index.html + index.js + index.wasm
#   make serve      build and serve on http://localhost:8080
#   make docs       single-file build into ../../docs/index.html (GitHub Pages)
#
# Needs emcc on the PATH (https://emscripten.org, or `brew install emscripten`).

EMCC   ?= emcc
COMMON  = ../../common
BUILD   = build
DOCS    = ../../docs

SRC = $(COMMON)/polar_scene.c $(COMMON)/polar_render.c $(COMMON)/polar_synth.c main_web.c

CFLAGS  = -O2 -std=c99 -Wall -Wextra -I$(COMMON)
EMFLAGS = -sUSE_WEBGL2=1 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 \
          -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB \
          -sEXPORTED_FUNCTIONS=_main,_web_begin \
          -sEXPORTED_RUNTIME_METHODS=HEAPF32 \
          -sENVIRONMENT=web -sNO_EXIT_RUNTIME=1

all: $(BUILD)/index.html

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/index.html: $(SRC) shell.html $(COMMON)/*.h | $(BUILD)
	$(EMCC) $(CFLAGS) $(EMFLAGS) $(SRC) --shell-file shell.html -o $@

serve: all
	@echo "open http://localhost:8080/"
	cd $(BUILD) && python3 -m http.server 8080

# Everything (JS + wasm, base64) embedded in one index.html so the docs/
# folder can be served as-is by GitHub Pages.
docs: $(DOCS)/index.html

$(DOCS)/index.html: $(SRC) shell.html $(COMMON)/*.h
	mkdir -p $(DOCS)
	$(EMCC) $(CFLAGS) $(EMFLAGS) -sSINGLE_FILE=1 $(SRC) --shell-file shell.html -o $@

clean:
	rm -rf $(BUILD)

.PHONY: all serve docs clean

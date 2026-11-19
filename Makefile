CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -g -Iinclude
EXEC    = donjon
EXEC_GUI = duckgeon
EXEC_SDL = donjon_sdl

# Interface graphique : SDL2 + SDL2_ttf (via pkg-config)
SDL_CFLAGS = $(shell pkg-config --cflags sdl2 SDL2_ttf)
SDL_LIBS   = $(shell pkg-config --libs sdl2 SDL2_ttf) -lm

# Windows (MSYS2) : le duel en ligne utilise Winsock
ifeq ($(OS),Windows_NT)
SDL_LIBS  += -lws2_32 -liphlpapi
endif

# [COMMUN] Logique métier, partagée par les deux versions
LOGIQUE = personnage objet inventaire saisie donjon sauvegarde boutique combat
# [DONJON - terminal] Version console
CONSOLE = main console_jeu
# [DUCKGEON - SDL] Version graphique (+ moteur de combat en équipe, duel en ligne)
GUI     = gui gui_formes gui_sprite gui_main gui_heros gui_editeur gui_combat gui_boutique gui_bestiaire \
          gui_en_ligne reseau

# [DONJON SDL - partie 3] Version graphique simple du terminal (aucun fichier gui_*)
SDLJEU  = sdl_main sdl_ui sdl_donjon sdl_combat

OBJ_LOGIQUE = $(LOGIQUE:%=build/%.o)
OBJ_CONSOLE = $(CONSOLE:%=build/%.o)
OBJ_GUI     = $(GUI:%=build/%.o)
OBJ_SDLJEU  = $(SDLJEU:%=build/%.o)

all: $(EXEC) $(EXEC_GUI) $(EXEC_SDL)

console: $(EXEC)

gui: $(EXEC_GUI)

sdl: $(EXEC_SDL)

$(EXEC): $(OBJ_LOGIQUE) $(OBJ_CONSOLE)
	$(CC) $(CFLAGS) -o $@ $^

$(EXEC_GUI): $(OBJ_LOGIQUE) $(OBJ_GUI)
	$(CC) $(CFLAGS) -o $@ $^ $(SDL_LIBS)

$(EXEC_SDL): $(OBJ_LOGIQUE) $(OBJ_SDLJEU)
	$(CC) $(CFLAGS) -o $@ $^ $(SDL_LIBS)

# Les fichiers de l'interface graphique ont besoin des en-têtes SDL
$(OBJ_GUI) $(OBJ_SDLJEU): CFLAGS += $(SDL_CFLAGS)

build/%.o: src/%.c include/*.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

run: $(EXEC)
	./$(EXEC)

run-gui: $(EXEC_GUI)
	./$(EXEC_GUI)

run-sdl: $(EXEC_SDL)
	./$(EXEC_SDL)

# Vérifie l'absence de fuites mémoire
valgrind: $(EXEC)
	valgrind --leak-check=full --show-leak-kinds=all ./$(EXEC)

clean:
	rm -rf build $(EXEC) $(EXEC_GUI) $(EXEC_SDL) $(EXEC).exe $(EXEC_GUI).exe $(EXEC_SDL).exe

# Supprime aussi la sauvegarde locale
reset: clean
	rm -f donjon.sav donjon.sav.tmp progression.sav

.PHONY: all console gui sdl run run-gui run-sdl valgrind clean reset

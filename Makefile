CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -g -Iinclude
EXEC    = donjon
EXEC_GUI = duckgeon

# Interface graphique : SDL2 + SDL2_ttf (via pkg-config)
SDL_CFLAGS = $(shell pkg-config --cflags sdl2 SDL2_ttf)
SDL_LIBS   = $(shell pkg-config --libs sdl2 SDL2_ttf) -lm

# Logique métier, partagée par les deux versions
LOGIQUE = personnage objet inventaire saisie donjon sauvegarde boutique combat
# Version console
CONSOLE = main console_jeu
# Version graphique (+ moteur de combat en équipe)
GUI     = gui gui_formes gui_sprite gui_main gui_heros gui_editeur gui_combat gui_boutique gui_bestiaire

OBJ_LOGIQUE = $(LOGIQUE:%=build/%.o)
OBJ_CONSOLE = $(CONSOLE:%=build/%.o)
OBJ_GUI     = $(GUI:%=build/%.o)

all: $(EXEC) $(EXEC_GUI)

console: $(EXEC)

gui: $(EXEC_GUI)

$(EXEC): $(OBJ_LOGIQUE) $(OBJ_CONSOLE)
	$(CC) $(CFLAGS) -o $@ $^

$(EXEC_GUI): $(OBJ_LOGIQUE) $(OBJ_GUI)
	$(CC) $(CFLAGS) -o $@ $^ $(SDL_LIBS)

# Les fichiers de l'interface graphique ont besoin des en-têtes SDL
$(OBJ_GUI): CFLAGS += $(SDL_CFLAGS)

build/%.o: src/%.c include/*.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

run: $(EXEC)
	./$(EXEC)

run-gui: $(EXEC_GUI)
	./$(EXEC_GUI)

# Vérifie l'absence de fuites mémoire
valgrind: $(EXEC)
	valgrind --leak-check=full --show-leak-kinds=all ./$(EXEC)

clean:
	rm -rf build $(EXEC) $(EXEC_GUI)

# Supprime aussi la sauvegarde locale
reset: clean
	rm -f donjon.sav donjon.sav.tmp progression.sav

.PHONY: all console gui run run-gui valgrind clean reset

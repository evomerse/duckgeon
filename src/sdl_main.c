/**
 * @file sdl_main.c
 * @brief Point d'entrée de ./donjon_sdl (partie 3).
 */

/* [DONJON SDL - partie 3] */
#include <stdlib.h>
#include <time.h>

#include "sdl_donjon.h"

int main(void)
{
    srand((unsigned)time(NULL)); /* critiques, butin, apparence aléatoire */
    return donjonSdl();
}

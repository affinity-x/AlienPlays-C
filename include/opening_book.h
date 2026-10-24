#ifndef ALIENPLAYS_OPENING_BOOK_H
#define ALIENPLAYS_OPENING_BOOK_H

#include "chess.h"
#include <stdbool.h>

bool opening_book_init(const char *path);
void opening_book_close(void);
bool opening_book_get_move(Position *position, Move *move);

#endif
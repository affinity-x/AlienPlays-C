#ifndef ALIENPLAYS_ENGINE_H
#define ALIENPLAYS_ENGINE_H

#include "chess.h"

#define ENGINE_MAX_DEPTH 8
#define ENGINE_MATE_SCORE 30000

typedef struct {
    Move best_move;
    int score;
    uint64_t nodes;
    int depth;
    bool has_move;
} SearchResult;

int evaluate_position(const Position *position);
SearchResult engine_search(Position *position, int depth);

#endif

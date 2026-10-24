
#include "chess.h"

#include <stddef.h>

void generate_legal_moves(Position *position, MoveList *list) {
    if (position == NULL || list == NULL) {
        return;
    }

    Color us = position->side_to_move;
    MoveList candidates;

    generate_pseudo_legal_moves(position, &candidates);
    list->count = 0;

    for (int i = 0; i < candidates.count; ++i) {
        Move move = candidates.moves[i];
        UndoState undo;

        if (!make_move(position, move, &undo)) {
            continue;
        }

        Square king = position->king_square[us];

        bool legal = !square_is_attacked(
            position, king, position->side_to_move
        );

        unmake_move(position, &undo);

        if (legal && list->count < MAX_MOVES) {
            list->moves[list->count++] = move;
        }
    }
}

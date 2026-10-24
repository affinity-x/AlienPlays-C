
#include "chess.h"

#include <stdio.h>

int main(void)
{
    Position position;

    if (!position_set_start(&position)) {
        printf("FAIL: could not initialize position\n");
        return 1;
    }

    uint64_t original_hash = position.hash_key;

    if (original_hash != position_compute_hash(&position)) {
        printf("FAIL: start position hash mismatch\n");
        return 1;
    }

    MoveList moves;
    generate_legal_moves(&position, &moves);

    if (moves.count == 0) {
        printf("FAIL: no legal moves in start position\n");
        return 1;
    }

    int tested = 0;

    for (int i = 0; i < moves.count; ++i) {
        UndoState undo;

        if (!make_move(&position, moves.moves[i], &undo)) {
            printf("FAIL: make_move rejected a generated legal move\n");
            return 1;
        }

        if (position.hash_key != position_compute_hash(&position)) {
            printf("FAIL: hash mismatch after move %d\n", i);
            return 1;
        }

        unmake_move(&position, &undo);

        if (position.hash_key != original_hash ||
            position.hash_key != position_compute_hash(&position)) {
            printf("FAIL: hash mismatch after unmake_move %d\n", i);
            return 1;
        }

        ++tested;
    }

    printf("PASS: hash matches after %d legal moves\n", tested);
    printf("PASS: unmake_move restores the original hash\n");
    return 0;
}

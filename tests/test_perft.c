
#include "chess.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

static uint64_t perft(Position *position, int depth) {
    if (depth == 0) {
        return 1;
    }

    MoveList moves;
    generate_legal_moves(position, &moves);

    uint64_t nodes = 0;

    for (int i = 0; i < moves.count; ++i) {
        UndoState undo;

        if (!make_move(position, moves.moves[i], &undo)) {
            continue;
        }

        nodes += perft(position, depth - 1);
        unmake_move(position, &undo);
    }

    return nodes;
}

int main(void) {
    Position position;

    assert(position_set_start(&position));

    const uint64_t expected[] = {
    0, 20, 400, 8902, 197281
    };

    for (int depth = 1; depth <= 4; ++depth) {
        uint64_t actual = perft(&position, depth);

        printf(
            "Depth %d: %" PRIu64 " (expected %" PRIu64 ")\n",
            depth, actual, expected[depth]
        );

        assert(actual == expected[depth]);
    }

    puts("PASS: all starting-position perft tests.");
    return 0;
}


#include "chess.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    Position position;
    Position original;
    UndoState undo;

    assert(position_set_start(&position));
    original = position;

    /* White: e2-e4 */
    Move move = MAKE_MOVE(E2, E4, 0, MOVE_NORMAL);

    assert(make_move(&position, move, &undo));
    assert(position.side_to_move == BLACK);
    assert(position.pieces[WHITE][PAWN] & BIT(E4));
    assert(!(position.pieces[WHITE][PAWN] & BIT(E2)));
    assert(position.en_passant == E3);
    assert(position_is_valid(&position));

    unmake_move(&position, &undo);

    assert(memcmp(&position, &original, sizeof(Position)) == 0);

    puts("PASS: e2-e4");
    puts("PASS: unmake restores the exact position");
    puts("All make/unmake tests passed.");

    return 0;
}

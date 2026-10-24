#include "chess.h"

#include <assert.h>
#include <stdio.h>

static int count_bits(Bitboard board) {
    int count = 0;

    while (board != 0) {
        board &= board - 1;
        count++;
    }

    return count;
}

int main(void) {
    Position position;

    assert(position_set_start(&position));
    assert(position_is_valid(&position));

    assert(position.side_to_move == WHITE);
    assert(position.king_square[WHITE] == E1);
    assert(position.king_square[BLACK] == E8);

    assert(count_bits(position.occupied[WHITE]) == 16);
    assert(count_bits(position.occupied[BLACK]) == 16);
    assert(count_bits(position.all) == 32);

    assert(count_bits(position.pieces[WHITE][PAWN]) == 8);
    assert(count_bits(position.pieces[BLACK][PAWN]) == 8);

    puts("PASS: start position");
    puts("PASS: kings");
    puts("PASS: piece counts");
    puts("All C position tests passed.");

    return 0;
}
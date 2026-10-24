#include "chess.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void put(Position *p, Color c, PieceType t, Square s) {
    p->pieces[c][t] |= BIT(s);
    p->occupied[c] |= BIT(s);
    p->all |= BIT(s);
    if (t == KING) p->king_square[c] = s;
}

static void empty_position(Position *p, Color side) {
    memset(p, 0, sizeof(*p));
    p->side_to_move = side;
    p->en_passant = NO_SQUARE;
    p->fullmove_number = 1;
    p->king_square[WHITE] = NO_SQUARE;
    p->king_square[BLACK] = NO_SQUARE;
}

static bool has_move(const MoveList *l, Square from, Square to,
                     MoveFlag flag, PieceType promo) {
    for (int i = 0; i < l->count; ++i) {
        Move m = l->moves[i];
        if (MOVE_FROM(m) == from && MOVE_TO(m) == to &&
            MOVE_FLAG(m) == flag && MOVE_PROMO(m) == promo)
            return true;
    }
    return false;
}

static void test_castling(void) {
    Position p;
    empty_position(&p, WHITE);
    put(&p, WHITE, KING, E1);
    put(&p, WHITE, ROOK, H1);
    put(&p, BLACK, KING, E8);
    p.castling_rights = CASTLE_WHITE_KING;

    MoveList l;
    generate_legal_moves(&p, &l);
    assert(has_move(&l, E1, G1, MOVE_CASTLING, 0));
    puts("PASS: castling");
}

static void test_en_passant(void) {
    Position p;
    empty_position(&p, WHITE);
    put(&p, WHITE, KING, A1);
    put(&p, BLACK, KING, H8);
    put(&p, WHITE, PAWN, E5);
    put(&p, BLACK, PAWN, D5);
    p.en_passant = D6;

    MoveList l;
    generate_legal_moves(&p, &l);
    assert(has_move(&l, E5, D6, MOVE_EN_PASSANT, 0));
    puts("PASS: en passant");
}

static void test_promotion(void) {
    Position p;
    empty_position(&p, WHITE);
    put(&p, WHITE, KING, A1);
    put(&p, BLACK, KING, H8);
    put(&p, WHITE, PAWN, A7);

    MoveList l;
    generate_legal_moves(&p, &l);
    assert(has_move(&l, A7, A8, MOVE_PROMOTION, QUEEN));
    assert(has_move(&l, A7, A8, MOVE_PROMOTION, ROOK));
    assert(has_move(&l, A7, A8, MOVE_PROMOTION, BISHOP));
    assert(has_move(&l, A7, A8, MOVE_PROMOTION, KNIGHT));
    puts("PASS: all four promotions");
}

int main(void) {
    test_castling();
    test_en_passant();
    test_promotion();
    puts("All special-move tests passed.");
    return 0;
}

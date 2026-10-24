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

static Move find_move(Position *p, Square from, Square to,
                      MoveFlag flag, PieceType promo) {
    MoveList list;
    generate_legal_moves(p, &list);

    for (int i = 0; i < list.count; ++i) {
        Move m = list.moves[i];
        if (MOVE_FROM(m) == from &&
            MOVE_TO(m) == to &&
            MOVE_FLAG(m) == flag &&
            MOVE_PROMO(m) == promo)
            return m;
    }

    assert(!"Expected legal move was not generated");
    return 0;
}

static void test_castling_make_unmake(void) {
    Position p;
    empty_position(&p, WHITE);
    put(&p, WHITE, KING, E1);
    put(&p, WHITE, ROOK, H1);
    put(&p, BLACK, KING, E8);
    p.castling_rights = CASTLE_WHITE_KING;

    Position original = p;
    Move m = find_move(&p, E1, G1, MOVE_CASTLING, 0);
    UndoState undo;

    assert(make_move(&p, m, &undo));
    assert(p.king_square[WHITE] == G1);
    assert(p.pieces[WHITE][KING] & BIT(G1));
    assert(p.pieces[WHITE][ROOK] & BIT(F1));
    assert(!(p.pieces[WHITE][ROOK] & BIT(H1)));

    unmake_move(&p, &undo);
    assert(memcmp(&p, &original, sizeof(p)) == 0);
    puts("PASS: castling make/unmake");
}

static void test_en_passant_make_unmake(void) {
    Position p;
    empty_position(&p, WHITE);
    put(&p, WHITE, KING, A1);
    put(&p, BLACK, KING, H8);
    put(&p, WHITE, PAWN, E5);
    put(&p, BLACK, PAWN, D5);
    p.en_passant = D6;

    Position original = p;
    Move m = find_move(&p, E5, D6, MOVE_EN_PASSANT, 0);
    UndoState undo;

    assert(make_move(&p, m, &undo));
    assert(p.pieces[WHITE][PAWN] & BIT(D6));
    assert(!(p.pieces[WHITE][PAWN] & BIT(E5)));
    assert(!(p.pieces[BLACK][PAWN] & BIT(D5)));

    unmake_move(&p, &undo);
    assert(memcmp(&p, &original, sizeof(p)) == 0);
    puts("PASS: en passant make/unmake");
}

static void test_promotion_make_unmake(void) {
    Position p;
    empty_position(&p, WHITE);
    put(&p, WHITE, KING, A1);
    put(&p, BLACK, KING, H8);
    put(&p, WHITE, PAWN, A7);

    Position original = p;
    Move m = find_move(&p, A7, A8, MOVE_PROMOTION, QUEEN);
    UndoState undo;

    assert(make_move(&p, m, &undo));
    assert(p.pieces[WHITE][QUEEN] & BIT(A8));
    assert(!(p.pieces[WHITE][PAWN] & BIT(A7)));

    unmake_move(&p, &undo);
    assert(memcmp(&p, &original, sizeof(p)) == 0);
    puts("PASS: promotion make/unmake");
}

int main(void) {
    test_castling_make_unmake();
    test_en_passant_make_unmake();
    test_promotion_make_unmake();
    puts("All special-move make/unmake tests passed.");
    return 0;
}

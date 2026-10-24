
#include "chess.h"

#include <stddef.h>
#include <stdlib.h>

static Piece find_piece(const Position *position, Square square) {
    Bitboard mask = square_bb(square);

    for (int color = 0; color < COLOR_NB; ++color) {
        for (int type = 0; type < PIECE_TYPE_NB; ++type) {
            if (position->pieces[color][type] & mask) {
                return make_piece((Color)color, (PieceType)type);
            }
        }
    }

    return NO_PIECE;
}

static void remove_piece(Position *position, Piece piece, Square square) {
    Bitboard mask = square_bb(square);
    Color color = piece_color(piece);
    PieceType type = piece_type(piece);

    position->pieces[color][type] &= ~mask;
    position->occupied[color] &= ~mask;
    position->all &= ~mask;

    if (type == KING) {
        position->king_square[color] = NO_SQUARE;
    }
}

static void put_piece(Position *position, Piece piece, Square square) {
    Bitboard mask = square_bb(square);
    Color color = piece_color(piece);
    PieceType type = piece_type(piece);

    position->pieces[color][type] |= mask;
    position->occupied[color] |= mask;
    position->all |= mask;

    if (type == KING) {
        position->king_square[color] = square;
    }
}

static bool valid_promotion(unsigned promo) {
    return promo == KNIGHT || promo == BISHOP ||
           promo == ROOK || promo == QUEEN;
}

static void update_castling_rights(
    Position *position,
    Piece moving,
    Square from,
    Piece captured,
    Square capture_square
) {
    if (moving == W_KING) {
        position->castling_rights &=
            (uint8_t)~(CASTLE_WHITE_KING | CASTLE_WHITE_QUEEN);
    } else if (moving == B_KING) {
        position->castling_rights &=
            (uint8_t)~(CASTLE_BLACK_KING | CASTLE_BLACK_QUEEN);
    }

    if (moving == W_ROOK) {
        if (from == H1)
            position->castling_rights &= (uint8_t)~CASTLE_WHITE_KING;
        else if (from == A1)
            position->castling_rights &= (uint8_t)~CASTLE_WHITE_QUEEN;
    } else if (moving == B_ROOK) {
        if (from == H8)
            position->castling_rights &= (uint8_t)~CASTLE_BLACK_KING;
        else if (from == A8)
            position->castling_rights &= (uint8_t)~CASTLE_BLACK_QUEEN;
    }

    if (captured == W_ROOK) {
        if (capture_square == H1)
            position->castling_rights &= (uint8_t)~CASTLE_WHITE_KING;
        else if (capture_square == A1)
            position->castling_rights &= (uint8_t)~CASTLE_WHITE_QUEEN;
    } else if (captured == B_ROOK) {
        if (capture_square == H8)
            position->castling_rights &= (uint8_t)~CASTLE_BLACK_KING;
        else if (capture_square == A8)
            position->castling_rights &= (uint8_t)~CASTLE_BLACK_QUEEN;
    }
}

bool make_move(Position *position, Move move, UndoState *undo) {
    if (position == NULL || undo == NULL) {
        return false;
    }

    Square from = MOVE_FROM(move);
    Square to = MOVE_TO(move);
    unsigned promo = MOVE_PROMO(move);
    unsigned flag = MOVE_FLAG(move);

    if (!square_is_valid(from) || !square_is_valid(to) || from == to) {
        return false;
    }

    if (flag > MOVE_PROMOTION) {
        return false;
    }

    Piece moving = find_piece(position, from);

    if (moving == NO_PIECE ||
        piece_color(moving) != position->side_to_move) {
        return false;
    }

    Color us = position->side_to_move;
    Color them = opposite_color(us);
    PieceType moving_type = piece_type(moving);
    Piece captured = find_piece(position, to);
    Square capture_square = to;

    if (captured != NO_PIECE &&
        (piece_color(captured) == us || piece_type(captured) == KING)) {
        return false;
    }

    if (flag == MOVE_PROMOTION) {
        int last_rank = us == WHITE ? 7 : 0;

        if (moving_type != PAWN ||
            square_rank(to) != last_rank ||
            !valid_promotion(promo)) {
            return false;
        }
    } else {
        if (promo != 0) {
            return false;
        }

        if (moving_type == PAWN &&
            square_rank(to) == (us == WHITE ? 7 : 0)) {
            return false;
        }
    }

    if (flag == MOVE_EN_PASSANT) {
        if (moving_type != PAWN ||
            to != position->en_passant ||
            captured != NO_PIECE) {
            return false;
        }

        capture_square = (Square)(to + (us == WHITE ? -8 : 8));
        captured = find_piece(position, capture_square);

        if (captured != make_piece(them, PAWN)) {
            return false;
        }
    } else if (flag == MOVE_CASTLING) {
        if (moving_type != KING || captured != NO_PIECE ||
            (from != E1 && from != E8)) {
            return false;
        }

        bool king_side = square_file(to) == 6;
        bool queen_side = square_file(to) == 2;

        if (square_rank(from) != square_rank(to) ||
            (!king_side && !queen_side) ||
            square_file(from) != 4) {
            return false;
        }

        uint8_t right;

        if (us == WHITE) {
            right = king_side ? CASTLE_WHITE_KING : CASTLE_WHITE_QUEEN;
        } else {
            right = king_side ? CASTLE_BLACK_KING : CASTLE_BLACK_QUEEN;
        }

        Square rook_from = (Square)(
            square_rank(from) * 8 + (king_side ? 7 : 0)
        );

        if (!(position->castling_rights & right) ||
            find_piece(position, rook_from) != make_piece(us, ROOK)) {
            return false;
        }

        int step = king_side ? 1 : -1;

        for (int file = square_file(from) + step;
             file != square_file(rook_from);
             file += step) {
            Square sq = (Square)(square_rank(from) * 8 + file);
            if (find_piece(position, sq) != NO_PIECE) {
                return false;
            }
        }
    } else if (flag != MOVE_PROMOTION && moving_type == PAWN &&
               to == position->en_passant &&
               square_file(from) != square_file(to)) {
        return false;
    }

    *undo = (UndoState){0};
    undo->position = *position;
    undo->move = move;
    undo->hash_key = position->hash_key;
    undo->halfmove_clock = position->halfmove_clock;
    undo->en_passant = position->en_passant;
    undo->castling_rights = position->castling_rights;

    update_castling_rights(
        position, moving, from, captured, capture_square
    );

    remove_piece(position, moving, from);

    if (captured != NO_PIECE) {
        remove_piece(position, captured, capture_square);
    }

    if (flag == MOVE_CASTLING) {
        bool king_side = square_file(to) == 6;
        Square rook_from = (Square)(
            square_rank(from) * 8 + (king_side ? 7 : 0)
        );
        Square rook_to = (Square)(
            square_rank(from) * 8 + (king_side ? 5 : 3)
        );

        Piece rook = find_piece(position, rook_from);

        remove_piece(position, rook, rook_from);
        put_piece(position, rook, rook_to);
    }

    Piece placed = flag == MOVE_PROMOTION
        ? make_piece(us, (PieceType)promo)
        : moving;

    put_piece(position, placed, to);

    position->en_passant = NO_SQUARE;

    if (moving_type == PAWN && abs((int)to - (int)from) == 16) {
        position->en_passant = (Square)(((int)from + (int)to) / 2);
    }

    if (moving_type == PAWN || captured != NO_PIECE) {
        position->halfmove_clock = 0;
    } else if (position->halfmove_clock < UINT16_MAX) {
        position->halfmove_clock++;
    }

    if (us == BLACK && position->fullmove_number < UINT16_MAX) {
        position->fullmove_number++;
    }

    position->side_to_move = them;

    /* Zobrist hashing is not implemented yet. */
    position->hash_key = position_compute_hash(position);

    if (!position_is_valid(position)) {
        *position = undo->position;
        return false;
    }

    return true;
}

void unmake_move(Position *position, const UndoState *undo) {
    if (position == NULL || undo == NULL) {
        return;
    }

    *position = undo->position;
}

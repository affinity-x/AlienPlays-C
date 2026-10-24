
#include "chess.h"

#include <stddef.h>

static bool inside_board(int file, int rank) {
    return file >= 0 && file < 8 && rank >= 0 && rank < 8;
}

static Piece piece_at(const Position *position, int square) {
    Bitboard bit = BIT(square);

    for (int color = 0; color < COLOR_NB; ++color) {
        for (int type = 0; type < PIECE_TYPE_NB; ++type) {
            if (position->pieces[color][type] & bit) {
                return make_piece((Color)color, (PieceType)type);
            }
        }
    }

    return NO_PIECE;
}

static void add_move(
    MoveList *list, int from, int to,
    PieceType promo, MoveFlag flag
) {
    if (list->count >= MAX_MOVES) return;

    list->moves[list->count++] =
        MAKE_MOVE(from, to, promo, flag);
}

static void add_pawn_move(MoveList *list, int from, int to) {
    int destination_rank = to / 8;

    if (destination_rank == 0 || destination_rank == 7) {
        add_move(list, from, to, QUEEN, MOVE_PROMOTION);
        add_move(list, from, to, ROOK, MOVE_PROMOTION);
        add_move(list, from, to, BISHOP, MOVE_PROMOTION);
        add_move(list, from, to, KNIGHT, MOVE_PROMOTION);
    } else {
        add_move(list, from, to, 0, MOVE_NORMAL);
    }
}

static void try_move(
    const Position *position, MoveList *list,
    int from, int file, int rank
) {
    if (!inside_board(file, rank)) return;

    int to = rank * 8 + file;
    Piece target = piece_at(position, to);

    if (target != NO_PIECE &&
        (piece_color(target) == position->side_to_move ||
         piece_type(target) == KING)) {
        return;
    }

    add_move(list, from, to, 0, MOVE_NORMAL);
}

static void generate_pawn_moves(
    const Position *position, MoveList *list, int from
) {
    Color us = position->side_to_move;
    int file = from % 8;
    int rank = from / 8;
    int direction = us == WHITE ? 1 : -1;
    int next_rank = rank + direction;

    if (!inside_board(file, next_rank)) return;

    int forward = next_rank * 8 + file;

    if (piece_at(position, forward) == NO_PIECE) {
        add_pawn_move(list, from, forward);

        int start_rank = us == WHITE ? 1 : 6;
        int double_rank = rank + 2 * direction;

        if (rank == start_rank) {
            int double_to = double_rank * 8 + file;

            if (piece_at(position, double_to) == NO_PIECE) {
                add_move(list, from, double_to, 0, MOVE_NORMAL);
            }
        }
    }

    for (int df = -1; df <= 1; df += 2) {
        int capture_file = file + df;

        if (!inside_board(capture_file, next_rank)) continue;

        int to = next_rank * 8 + capture_file;
        Piece target = piece_at(position, to);

        if (target != NO_PIECE &&
            piece_color(target) != us &&
            piece_type(target) != KING) {
            add_pawn_move(list, from, to);
        }

        /* En passant */
        if (position->en_passant == (Square)to &&
            target == NO_PIECE) {
            int captured_square = to + (us == WHITE ? -8 : 8);

            if (piece_at(position, captured_square) ==
                make_piece(opposite_color(us), PAWN)) {
                add_move(
                    list, from, to, 0, MOVE_EN_PASSANT
                );
            }
        }
    }
}

static void generate_castling(
    const Position *position, MoveList *list
) {
    Color us = position->side_to_move;
    Color them = opposite_color(us);
    int rank = us == WHITE ? 0 : 7;
    Square king_from = (Square)(rank * 8 + 4);

    if (position->king_square[us] != king_from) return;

    if (square_is_attacked(position, king_from, them)) return;

    uint8_t king_right = us == WHITE
        ? CASTLE_WHITE_KING : CASTLE_BLACK_KING;
    uint8_t queen_right = us == WHITE
        ? CASTLE_WHITE_QUEEN : CASTLE_BLACK_QUEEN;

    /* King-side castling */
    if (position->castling_rights & king_right) {
        Square rook = (Square)(rank * 8 + 7);
        Square f = (Square)(rank * 8 + 5);
        Square g = (Square)(rank * 8 + 6);

        if (piece_at(position, rook) == make_piece(us, ROOK) &&
            piece_at(position, f) == NO_PIECE &&
            piece_at(position, g) == NO_PIECE &&
            !square_is_attacked(position, f, them) &&
            !square_is_attacked(position, g, them)) {
            add_move(
                list, king_from, g, 0, MOVE_CASTLING
            );
        }
    }

    /* Queen-side castling */
    if (position->castling_rights & queen_right) {
        Square rook = (Square)(rank * 8);
        Square b = (Square)(rank * 8 + 1);
        Square c = (Square)(rank * 8 + 2);
        Square d = (Square)(rank * 8 + 3);

        if (piece_at(position, rook) == make_piece(us, ROOK) &&
            piece_at(position, b) == NO_PIECE &&
            piece_at(position, c) == NO_PIECE &&
            piece_at(position, d) == NO_PIECE &&
            !square_is_attacked(position, d, them) &&
            !square_is_attacked(position, c, them)) {
            add_move(
                list, king_from, c, 0, MOVE_CASTLING
            );
        }
    }
}

void generate_pseudo_legal_moves(
    const Position *position, MoveList *list
) {
    if (position == NULL || list == NULL) return;

    list->count = 0;
    Color us = position->side_to_move;

    static const int knight_moves[8][2] = {
        {1, 2}, {2, 1}, {2, -1}, {1, -2},
        {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}
    };

    static const int king_moves[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    static const int bishop_dirs[4][2] = {
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    static const int rook_dirs[4][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    };

    for (int from = 0; from < BOARD_SQUARES; ++from) {
        Piece piece = piece_at(position, from);

        if (piece == NO_PIECE || piece_color(piece) != us) continue;

        PieceType type = piece_type(piece);
        int file = from % 8;
        int rank = from / 8;

        if (type == PAWN) {
            generate_pawn_moves(position, list, from);
        } else if (type == KNIGHT || type == KING) {
            const int (*moves)[2] =
                type == KNIGHT ? knight_moves : king_moves;

            for (int i = 0; i < 8; ++i) {
                try_move(
                    position, list, from,
                    file + moves[i][0],
                    rank + moves[i][1]
                );
            }

            if (type == KING) {
                generate_castling(position, list);
            }
        } else {
            const int (*dirs)[2] =
                type == BISHOP ? bishop_dirs : rook_dirs;
            int dir_count = type == QUEEN ? 8 : 4;

            for (int i = 0; i < dir_count; ++i) {
                int df, dr;

                if (type == QUEEN) {
                    if (i < 4) {
                        df = bishop_dirs[i][0];
                        dr = bishop_dirs[i][1];
                    } else {
                        df = rook_dirs[i - 4][0];
                        dr = rook_dirs[i - 4][1];
                    }
                } else {
                    df = dirs[i][0];
                    dr = dirs[i][1];
                }

                int f = file + df;
                int r = rank + dr;

                while (inside_board(f, r)) {
                    int to = r * 8 + f;
                    Piece target = piece_at(position, to);

                    if (target != NO_PIECE &&
                        piece_color(target) == us) {
                        break;
                    }

                    if (target == NO_PIECE ||
                        piece_type(target) != KING) {
                        add_move(
                            list, from, to, 0, MOVE_NORMAL
                        );
                    }

                    if (target != NO_PIECE) break;

                    f += df;
                    r += dr;
                }
            }
        }
    }
}

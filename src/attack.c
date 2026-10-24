
#include "chess.h"

#include <stddef.h>

static bool inside(int file, int rank) {
    return file >= 0 && file < 8 && rank >= 0 && rank < 8;
}

static bool has_piece(
    const Position *position,
    Color color,
    PieceType type,
    int square
) {
    return (position->pieces[color][type] & BIT(square)) != 0;
}

bool square_is_attacked(
    const Position *position,
    Square square,
    Color by_color
) {
    if (position == NULL ||
        !square_is_valid(square) ||
        by_color >= COLOR_NB) {
        return false;
    }

    int file = square_file(square);
    int rank = square_rank(square);

    /* Pawn attacks. */
    int pawn_source_rank = rank + (by_color == WHITE ? -1 : 1);

    for (int df = -1; df <= 1; df += 2) {
        int f = file + df;

        if (inside(f, pawn_source_rank) &&
            has_piece(position, by_color, PAWN,
                      pawn_source_rank * 8 + f)) {
            return true;
        }
    }

    /* Knight attacks. */
    static const int knight_steps[8][2] = {
        {1, 2}, {2, 1}, {2, -1}, {1, -2},
        {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}
    };

    for (int i = 0; i < 8; ++i) {
        int f = file + knight_steps[i][0];
        int r = rank + knight_steps[i][1];

        if (inside(f, r) &&
            has_piece(position, by_color, KNIGHT, r * 8 + f)) {
            return true;
        }
    }

    /* King attacks. */
    for (int df = -1; df <= 1; ++df) {
        for (int dr = -1; dr <= 1; ++dr) {
            if (df == 0 && dr == 0) {
                continue;
            }

            int f = file + df;
            int r = rank + dr;

            if (inside(f, r) &&
                has_piece(position, by_color, KING, r * 8 + f)) {
                return true;
            }
        }
    }

    /* Bishop, rook and queen attacks. */
    static const int directions[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    for (int i = 0; i < 8; ++i) {
        int f = file + directions[i][0];
        int r = rank + directions[i][1];
        bool diagonal = i >= 4;

        while (inside(f, r)) {
            int target = r * 8 + f;
            Bitboard mask = BIT(target);

            if (position->all & mask) {
                if (has_piece(position, by_color, QUEEN, target) ||
                    (diagonal &&
                     has_piece(position, by_color, BISHOP, target)) ||
                    (!diagonal &&
                     has_piece(position, by_color, ROOK, target))) {
                    return true;
                }

                /* Any piece blocks the ray. */
                break;
            }

            f += directions[i][0];
            r += directions[i][1];
        }
    }

    return false;
}

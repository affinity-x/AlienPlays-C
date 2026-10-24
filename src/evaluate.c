#include "engine.h"
#include "cerebrum.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static const int piece_value[PIECE_TYPE_NB] = {
    100, 320, 335, 500, 900, 0
};

static int popcount(Bitboard b)
{
    return __builtin_popcountll(b);
}

static int file_of(int sq) { return sq & 7; }
static int rank_of(int sq) { return sq >> 3; }

static int clamp(int x, int lo, int hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

static int center_bonus(int sq)
{
    int f = file_of(sq), r = rank_of(sq);
    int df = 3 - (f < 4 ? f : 7 - f);
    int dr = 3 - (r < 4 ? r : 7 - r);
    return 6 - 2 * (df + dr);
}

static int relative_rank(int sq, int color)
{
    int r = rank_of(sq);
    return color == WHITE ? r : 7 - r;
}

static int pawn_structure(const Position *p, int color, int sq)
{
    int score = 0;
    int f = file_of(sq);
    int rr = relative_rank(sq, color);
    int enemy = color == WHITE ? BLACK : WHITE;

    Bitboard own_pawns = p->pieces[color][PAWN];
    Bitboard enemy_pawns = p->pieces[enemy][PAWN];

    int same_file = 0;
    int adjacent_file = 0;

    for (int s = 0; s < 64; ++s) {
        if (!(own_pawns & (UINT64_C(1) << s))) continue;

        if (file_of(s) == f) ++same_file;
        if (file_of(s) == f - 1 || file_of(s) == f + 1)
            adjacent_file = 1;
    }

    if (same_file > 1)
        score -= 13 * (same_file - 1);

    if (!adjacent_file)
        score -= 11;

    int passed = 1;

    for (int s = 0; s < 64; ++s) {
        if (!(enemy_pawns & (UINT64_C(1) << s))) continue;

        int ef = file_of(s);
        int er = relative_rank(s, color);

        if (ef >= f - 1 && ef <= f + 1 && er < rr) {
            passed = 0;
                break;
        }
    }

    if (passed)
        score += 8 + rr * rr * 3;

    if (rr == 6)
        score += 18;

    return score;
}


static int cerebrum_loaded = 0;
static int cerebrum_attempted = 0;

static int evaluate_cerebrum(const Position *position)
{
    if (!cerebrum_attempted) {
        cerebrum_attempted = 1;
        cerebrum_loaded = nn_load("cerebrum-v2.0.nn") == 0;
        if (cerebrum_loaded)
            fprintf(stderr, "Cerebrum NN loaded successfully\n");
        else
            fprintf(stderr, "Cerebrum NN failed to load\n");
    }

    if (!cerebrum_loaded)
        return 0;

    NN_Accumulator accumulator;
    nn_init_accumulator(accumulator);
    nn_update_all_pieces(accumulator,
                         position->pieces[WHITE],
                         position->pieces[BLACK]);

    return nn_evaluate(accumulator, position->side_to_move);
}

int evaluate_position(const Position *position)
{
    if (position == NULL) return 0;

    int score = 0;
    int phase = 0;
    const int phase_value[PIECE_TYPE_NB] = {0, 1, 1, 2, 4, 0};

    for (int c = 0; c < COLOR_NB; ++c) {
        for (int t = KNIGHT; t <= QUEEN; ++t) {
            phase += popcount(position->pieces[c][t])
                   * phase_value[t];
        }
    }

    phase = clamp(phase, 0, 24);

    for (int color = WHITE; color < COLOR_NB; ++color) {
        int sign = color == WHITE ? 1 : -1;
        int enemy = color == WHITE ? BLACK : WHITE;

        Bitboard pawns = position->pieces[color][PAWN];
        Bitboard enemy_pawns = position->pieces[enemy][PAWN];
        Bitboard bishops = position->pieces[color][BISHOP];

        int bishop_count = popcount(bishops);

        for (int t = PAWN; t < PIECE_TYPE_NB; ++t) {
            Bitboard pieces = position->pieces[color][t];

            score += sign * popcount(pieces) * piece_value[t];

            while (pieces) {
                int sq = __builtin_ctzll(pieces);
                pieces &= pieces - 1;

                int f = file_of(sq);
                int rr = relative_rank(sq, color);
                int cb = center_bonus(sq);

                switch (t) {
                case PAWN:
                    score += sign * (
                        rr * 5 + pawn_structure(position, color, sq)
                    );

                    if (f >= 2 && f <= 5 && rr >= 2 && rr <= 5)
                        score += sign * 5;
                    break;

                case KNIGHT:
                    score += sign * (
                        cb * 5 + (rr > 0 && rr < 7 ? 5 : -8)
                    );
                    break;

                case BISHOP:
                    score += sign * (
                        cb * 3 + (rr > 0 && rr < 7 ? 3 : 0)
                    );
                    break;

                case ROOK: {
                    int own_pawn_on_file = 0;
                    int enemy_pawn_on_file = 0;

                    for (int s = 0; s < 64; ++s) {
                        if (file_of(s) != f) continue;

                        Bitboard bit = UINT64_C(1) << s;

                        if (pawns & bit) own_pawn_on_file = 1;
                        if (enemy_pawns & bit) enemy_pawn_on_file = 1;
                    }

                    if (!own_pawn_on_file && !enemy_pawn_on_file)
                        score += sign * 20;
                    else if (!own_pawn_on_file)
                        score += sign * 10;

                    if (rr == 6)
                        score += sign * 16;

                    break;
                }

                case QUEEN:
                    score += sign * cb;
                    break;

                case KING: {
                    int king_sq = sq;
                    int kf = file_of(king_sq);
                    int kr = rank_of(king_sq);

                    /* King activity grows as the game enters an endgame. */
                    score += sign * (
                        cb * (24 - phase) * 2 / 24
                    );

                    /* Small pawn-shield bonus in the middlegame. */
                    int shelter = 0;
                    int front_rank = color == WHITE ? kr + 1 : kr - 1;

                    if (front_rank >= 0 && front_rank < 8) {
                        for (int df = -1; df <= 1; ++df) {
                            int nf = kf + df;

                            if (nf < 0 || nf > 7) continue;

                            Bitboard bit =
                                UINT64_C(1) << (front_rank * 8 + nf);

                            if (pawns & bit) ++shelter;
                        }
                    }

                    score += sign * shelter * phase / 3;
                    break;
                }

                default:
                    break;
                }
            }
        }

        /* The bishop pair is useful in many positions. */
        if (bishop_count >= 2)
            score += sign * 28;

        /* Penalize enemy heavy pieces approaching the king. */
        if (phase > 8) {
            int king_sq = (int)position->king_square[color];

            if (king_sq >= 0 && king_sq < 64) {
                int kf = file_of(king_sq);
                int kr = rank_of(king_sq);
                int danger = 0;

                Bitboard enemy_heavy =
                    position->pieces[enemy][QUEEN] |
                    position->pieces[enemy][ROOK];

                for (int s = 0; s < 64; ++s) {
                    if (!(enemy_heavy & (UINT64_C(1) << s)))
                        continue;

                    int df = file_of(s) - kf;
                    int dr = rank_of(s) - kr;

                    if (df < 0) df = -df;
                    if (dr < 0) dr = -dr;

                    if (df + dr <= 2)
                        ++danger;
                }

                score -= sign * danger * phase;
            }
        }
    }

    score = position->side_to_move == WHITE ? score : -score;

    int nn_score = evaluate_cerebrum(position);
    if (cerebrum_loaded)
        score = (score + nn_score) / 2;

    return score;
}

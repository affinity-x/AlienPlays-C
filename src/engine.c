
#include "engine.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#define INF_SCORE 32000
#define QUIESCENCE_MAX_PLY 16

/* 2^18 entries; each entry stores one searched position. */
#define TT_BITS 18
#define TT_SIZE (1U << TT_BITS)
#define TT_MASK (TT_SIZE - 1U)

#define TT_NONE 0
#define TT_EXACT 1
#define TT_LOWERBOUND 2
#define TT_UPPERBOUND 3

typedef struct {
    uint64_t key;
    Move best_move;
    int score;
    int depth;
    unsigned char bound;
    bool valid;
} TTEntry;

static TTEntry transposition_table[TT_SIZE];

static uint64_t search_nodes;

/* Two killer quiet moves per ply. */
static Move killer_moves[MAX_PLY][2];

/* History scores indexed by side, origin square, destination square. */
static int history_scores[COLOR_NB][64][64];



static Piece piece_at(const Position *p, Square sq)
{
    Bitboard bit = BIT(sq);

    for (int c = 0; c < COLOR_NB; ++c) {
        for (int t = 0; t < PIECE_TYPE_NB; ++t) {
            if (p->pieces[c][t] & bit) {
                return make_piece((Color)c, (PieceType)t);
            }
        }
    }

    return NO_PIECE;
}

static int piece_value(Piece piece)
{
    if (piece == NO_PIECE) {
        return 0;
    }

    switch (piece_type(piece)) {
        case PAWN:   return 100;
        case KNIGHT: return 320;
        case BISHOP: return 330;
        case ROOK:   return 500;
        case QUEEN:  return 900;
        case KING:   return 20000;
        default:     return 0;
    }
}

static bool move_is_capture(const Position *p, Move move)
{
    if (MOVE_FLAG(move) == MOVE_EN_PASSANT) {
        return true;
    }

    return piece_at(p, MOVE_TO(move)) != NO_PIECE;
}

static bool move_is_quiet(const Position *p, Move move)
{
    return MOVE_FLAG(move) == MOVE_NORMAL
        && !move_is_capture(p, move);
}

static bool move_gives_check(const Position *p, Move move)
{
    Position next = *p;
    UndoState undo;

    if (!make_move(&next, move, &undo)) {
        return false;
    }

    Square enemy_king = NO_SQUARE;

    Bitboard kings = next.pieces[opposite_color(next.side_to_move)][KING];

    if (kings == 0) {
        return false;
    }

    enemy_king = (Square)__builtin_ctzll(kings);

    return square_is_attacked(
        &next,
        enemy_king,
        next.side_to_move
    );
}

static int move_score(
    const Position *p,
    Move move,
    Move preferred,
    int ply
)
{
    if (preferred != 0 && move == preferred) {
        return 2000000;
    }

    Piece victim = piece_at(p, MOVE_TO(move));
    Piece attacker = piece_at(p, MOVE_FROM(move));

    if (MOVE_FLAG(move) == MOVE_EN_PASSANT) {
        victim = make_piece(
           opposite_color(p->side_to_move),
           PAWN
    );
}
    int score = 0;

    if (move_is_capture(p, move)) {
        /*
        * MVV-LVA: pirmiausia vertingesnės aukos,
        * o tarp jų — mažesnės puolančios figūros.
        */
        score += 10000
            + 16 * piece_value(victim)
            - piece_value(attacker);

        if (MOVE_FLAG(move) == MOVE_EN_PASSANT) {
            score += 100;
        }
    }    

    if (MOVE_FLAG(move) == MOVE_PROMOTION) {
        score += 8000 + piece_value(
            make_piece(
                p->side_to_move,
                (PieceType)MOVE_PROMO(move)
            )
        );
    }

    if (MOVE_FLAG(move) == MOVE_CASTLING) {
        score += 50;
    }

    if (move_is_quiet(p, move) && ply >= 0 && ply < MAX_PLY) {
        if (killer_moves[ply][0] == move) {
            score += 7000;
        } else if (killer_moves[ply][1] == move) {
            score += 6000;
        }

        score += history_scores[p->side_to_move]
                                [MOVE_FROM(move)]
                                [MOVE_TO(move)];
    if (move_gives_check(p, move)) {
        score += 200;
    }
    }

    return score;
}

static void sort_moves(
    const Position *p,
    MoveList *list,
    Move preferred,
    int ply
)
{
    /*
     * Selection sort: the move-ordering score includes TT,
     * captures/promotions, killer moves, and history scores.
     */
    for (int i = 0; i < list->count - 1; ++i) {
        int best_index = i;
        int best_score = move_score(
            p, list->moves[i], preferred, ply
        );

        for (int j = i + 1; j < list->count; ++j) {
            int score = move_score(
                p, list->moves[j], preferred, ply
            );

            if (score > best_score) {
                best_score = score;
                best_index = j;
            }
        }

        if (best_index != i) {
            Move temp = list->moves[i];
            list->moves[i] = list->moves[best_index];
            list->moves[best_index] = temp;
        }
    }
}

static int score_to_tt(int score, int ply)
{
    if (score > ENGINE_MATE_SCORE - MAX_PLY) {
        return score + ply;
    }

    if (score < -ENGINE_MATE_SCORE + MAX_PLY) {
        return score - ply;
    }

    return score;
}

static int score_from_tt(int score, int ply)
{
    if (score > ENGINE_MATE_SCORE - MAX_PLY) {
        return score - ply;
    }

    if (score < -ENGINE_MATE_SCORE + MAX_PLY) {
        return score + ply;
    }

    return score;
}

static TTEntry *tt_entry(uint64_t key)
{
    return &transposition_table[(unsigned)(key & TT_MASK)];
}

static void tt_store(
    uint64_t key,
    Move best_move,
    int score,
    int depth,
    unsigned char bound,
    int ply
)
{
    TTEntry *entry = tt_entry(key);

    bool same_key = entry->valid && entry->key == key;

    /*
     * Preserve useful information from a deeper search.
     * Always replace matching keys with exact results.
     */
    if (same_key) {
        if (depth < entry->depth && bound != TT_EXACT) {
            return;
        }
    } else if (entry->valid) {
        /*
         * On a collision, prefer deeper entries unless
         * the new result is exact and the old one is not.
         */
        if (depth < entry->depth &&
            !(bound == TT_EXACT && entry->bound != TT_EXACT)) {
            return;
        }
    }

    entry->key = key;
    entry->best_move = best_move;
    entry->score = score_to_tt(score, ply);
    entry->depth = depth;
    entry->bound = bound;
    entry->valid = true;
}

static void tt_clear(void)
{
    for (unsigned i = 0; i < TT_SIZE; ++i) {
        transposition_table[i].valid = false;
    }
}

static int quiescence(
    Position *p,
    int alpha,
    int beta,
    int ply
)
{
    ++search_nodes;

    if (p->halfmove_clock >= 100) {
        return 0;
    }

    bool in_check = square_is_attacked(
        p,
        p->king_square[p->side_to_move],
        opposite_color(p->side_to_move)
    );

    /* Emergency limit: handle check before static evaluation. */
    if (ply >= MAX_PLY - 1) {
        if (!in_check) {
            return evaluate_position(p);
        }

        MoveList evasions;
        generate_legal_moves(p, &evasions);

        if (evasions.count == 0) {
            return -ENGINE_MATE_SCORE + ply;
        }

        int best = -INF_SCORE;

        for (int i = 0; i < evasions.count; ++i) {
            UndoState undo;
            Move move = evasions.moves[i];

            if (!make_move(p, move, &undo)) {
                continue;
            }

            int score = -evaluate_position(p);
            unmake_move(p, &undo);

            if (score > best) {
                best = score;
            }
        }

        return best == -INF_SCORE
            ? -ENGINE_MATE_SCORE + ply
            : best;
    }

    int stand_pat = -INF_SCORE;

    if (!in_check) {
        stand_pat = evaluate_position(p);

        if (stand_pat >= beta) {
            return stand_pat;
        }

        if (stand_pat > alpha) {
            alpha = stand_pat;
        }

        if (ply >= QUIESCENCE_MAX_PLY) {
            return stand_pat;
        }
    }

    MoveList list;
    generate_legal_moves(p, &list);

    if (list.count == 0) {
        return in_check
            ? -ENGINE_MATE_SCORE + ply
            : stand_pat;
    }

    if (!in_check) {
        int write = 0;

        for (int i = 0; i < list.count; ++i) {
            Move move = list.moves[i];

            if (move_is_capture(p, move) ||
                MOVE_FLAG(move) == MOVE_PROMOTION) {
                list.moves[write++] = move;
            }
        }

        list.count = write;

        if (list.count == 0) {
            return stand_pat;
        }
    }

    sort_moves(p, &list, 0, ply);

    int best_score = in_check ? -INF_SCORE : stand_pat;

    for (int i = 0; i < list.count; ++i) {
        Move move = list.moves[i];
        UndoState undo;

        if (!make_move(p, move, &undo)) {
            continue;
        }

        int score = -quiescence(
            p, -beta, -alpha, ply + 1
        );

        unmake_move(p, &undo);

        if (score > best_score) {
            best_score = score;
        }

        if (score > alpha) {
            alpha = score;
        }

        if (alpha >= beta) {
            break;
        }
    }

    return best_score;
}

static int negamax(
    Position *p,
    int depth,
    int alpha,
    int beta,
    int ply,
    Move *best_move
)
{
    ++search_nodes;

    if (ply >= MAX_PLY - 1) {
       return quiescence(p, alpha, beta, ply);
    }

    if (p->halfmove_clock >= 100) {
        return 0;
    }

    if (depth <= 0) {
        return quiescence(p, alpha, beta, ply);
    }

    const int original_alpha = alpha;
    const int original_beta = beta;
    const uint64_t key = p->hash_key;

    TTEntry *entry = tt_entry(key);
    Move tt_move = 0;

    if (entry->valid && entry->key == key) {
        tt_move = entry->best_move;

        if (entry->depth >= depth) {
            int tt_score = score_from_tt(entry->score, ply);

            if (entry->bound == TT_EXACT) {
                if (best_move != NULL) {
                    *best_move = tt_move;
                }
                return tt_score;
            }

            if (entry->bound == TT_LOWERBOUND &&
                tt_score >= beta) {
                if (best_move != NULL) {
                    *best_move = tt_move;
                }
                return tt_score;
            }

            if (entry->bound == TT_UPPERBOUND &&
                tt_score <= alpha) {
                if (best_move != NULL) {
                    *best_move = tt_move;
                }
                return tt_score;
            }
        }
    }

    MoveList list;
    generate_legal_moves(p, &list);

    if (list.count == 0) {
        if (square_is_attacked(
                p,
                p->king_square[p->side_to_move],
                opposite_color(p->side_to_move))) {
            return -ENGINE_MATE_SCORE + ply;
        }

        return 0;
    }

    sort_moves(p, &list, tt_move, ply);

    Move local_best = 0;
    int best_score = -INF_SCORE;
    int legal_moves_searched = 0;

    for (int i = 0; i < list.count; ++i) {
        Move move = list.moves[i];
        UndoState undo;

        if (!make_move(p, move, &undo)) {
            continue;
        }

        int score;

        if (legal_moves_searched == 0) {
            /* Pirmasis ėjimas ieškomas pilnu langu. */
            score = -negamax(
                p,
                depth - 1,
                -beta,
                -alpha,
                ply + 1,
                NULL
            );
        } else {
            /*
             * PVS: kitus ėjimus pirmiausia tikriname
             * siauru nuliniu langu.
             */
            score = -negamax(
                p,
                depth - 1,
                -alpha - 1,
                -alpha,
                ply + 1,
                NULL
            );

            /*
             * Jei ėjimas pagerina alpha, bet nepasiekia
             * beta ribos, būtina atlikti pilną pakartotinę
             * paiešką.
             */
            if (score > alpha && score < beta) {
                score = -negamax(
                    p,
                    depth - 1,
                    -beta,
                    -alpha,
                    ply + 1,
                    NULL
                );
            }
        }

        unmake_move(p, &undo);
        ++legal_moves_searched;

        if (score > best_score) {
            best_score = score;
            local_best = move;
        }

        if (score > alpha) {
            alpha = score;
        }

        if (alpha >= beta) {
            /*
             * Killer/history updates apply only to quiet moves.
             * Captures and promotions already have tactical ordering.
             */
            if (move_is_quiet(p, move) &&
                ply >= 0 && ply < MAX_PLY) {
                if (killer_moves[ply][0] != move) {
                    killer_moves[ply][1] = killer_moves[ply][0];
                    killer_moves[ply][0] = move;
                }

                int from = MOVE_FROM(move);
                int to = MOVE_TO(move);
                int side = p->side_to_move;
                int bonus = depth * depth;

                history_scores[side][from][to] += bonus;

                /* Keep history values bounded to avoid overflow. */
                if (history_scores[side][from][to] > 100000) {
                    history_scores[side][from][to] = 100000;
                }
            }

            break;
        }
    }

    if (legal_moves_searched == 0) {
        return evaluate_position(p);
    }

    unsigned char bound = TT_EXACT;

    if (best_score <= original_alpha) {
        bound = TT_UPPERBOUND;
    } else if (best_score >= original_beta) {
        bound = TT_LOWERBOUND;
    }

    tt_store(
        key,
        local_best,
        best_score,
        depth,
        bound,
        ply
    );

    if (best_move != NULL) {
        *best_move = local_best;
    }

    return best_score;
}

SearchResult engine_search(Position *position, int depth)
{
    SearchResult result = {0};

    if (position == NULL || !position_is_valid(position)) {
        return result;
    }

    if (depth < 1) {
        depth = 1;
    }

    if (depth > ENGINE_MAX_DEPTH) {
        depth = ENGINE_MAX_DEPTH;
    }

    /*
     * Išvalome TT tik pirmojo paieškos iškvietimo metu.
     * Vėlesni gyliai gali naudoti anksčiau rastus ėjimus.
     */
    static bool tt_initialized = false;

    if (!tt_initialized) {
        tt_clear();

        for (int ply = 0; ply < MAX_PLY; ++ply) {
            killer_moves[ply][0] = 0;
            killer_moves[ply][1] = 0;
        }

        for (int side = 0; side < COLOR_NB; ++side) {
            for (int from = 0; from < 64; ++from) {
                for (int to = 0; to < 64; ++to) {
                    history_scores[side][from][to] = 0;
                }
            }
        }

        tt_initialized = true;
    }

    search_nodes = 0;

    Move best = 0;
    int previous_score = 0;

    for (int current_depth = 1;
         current_depth <= depth;
         ++current_depth) {

        int alpha = -INF_SCORE;
        int beta = INF_SCORE;
        int window = 50;
        int score = 0;
        Move iteration_best = 0;

        /*
         * Pirmajame gylyje naudojame pilną langą.
         * Vėliau bandome siauresnį aspiration langą.
         */
        if (current_depth > 1) {
            alpha = previous_score - window;
            beta = previous_score + window;
        }

        for (;;) {
            iteration_best = 0;

            score = negamax(
                position,
                current_depth,
                alpha,
                beta,
                0,
                &iteration_best
            );

            /*
             * Nepavykus aspiration langui, jį praplečiame.
             * Galų gale visada pasiekiame pilną langą.
             */
            if (score <= alpha && alpha > -INF_SCORE) {
                window *= 2;
                alpha = previous_score - window;

                if (alpha < -INF_SCORE) {
                    alpha = -INF_SCORE;
                }

                continue;
            }

            if (score >= beta && beta < INF_SCORE) {
                window *= 2;
                beta = previous_score + window;

                if (beta > INF_SCORE) {
                    beta = INF_SCORE;
                }

                continue;
            }

            break;
        }

        if (iteration_best != 0) {
            best = iteration_best;
        }

        previous_score = score;

        result.score = score;
        result.nodes = search_nodes;
        result.depth = current_depth;
        result.has_move = best != 0;
        result.best_move = best;

        /*
         * Jei pasiekta matinė pozicija, gilesnių iteracijų
         * nebereikia, kai matas jau rastas.
         */
        if (score >= ENGINE_MATE_SCORE - MAX_PLY ||
            score <= -ENGINE_MATE_SCORE + MAX_PLY) {
            break;
        }
    }

    return result;
}

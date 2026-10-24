
#include "chess.h"

#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>


/* Zobrist hashing */

static uint64_t zobrist_piece[COLOR_NB][PIECE_TYPE_NB][64];
static uint64_t zobrist_side;
static uint64_t zobrist_castling[16];
static uint64_t zobrist_ep[8];
static bool zobrist_initialized = false;

static uint64_t splitmix64(uint64_t *state)
{
    uint64_t z = (*state += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

static void init_zobrist(void)
{
    if (zobrist_initialized) {
        return;
    }

    uint64_t seed = UINT64_C(0xA11E1A9B20261023);

    for (int c = 0; c < COLOR_NB; ++c) {
        for (int t = 0; t < PIECE_TYPE_NB; ++t) {
            for (int sq = 0; sq < 64; ++sq) {
                zobrist_piece[c][t][sq] = splitmix64(&seed);
            }
        }
    }

    zobrist_side = splitmix64(&seed);

    for (int i = 0; i < 16; ++i) {
        zobrist_castling[i] = splitmix64(&seed);
    }

    for (int i = 0; i < 8; ++i) {
        zobrist_ep[i] = splitmix64(&seed);
    }

    zobrist_initialized = true;
}

uint64_t position_compute_hash(const Position *position)
{
    if (position == NULL) {
        return 0;
    }

    init_zobrist();

    uint64_t hash = 0;

    for (int c = 0; c < COLOR_NB; ++c) {
        for (int t = 0; t < PIECE_TYPE_NB; ++t) {
            Bitboard pieces = position->pieces[c][t];

            while (pieces != 0) {
                int sq = __builtin_ctzll(pieces);
                pieces &= pieces - 1;

                hash ^= zobrist_piece[c][t][sq];
            }
        }
    }

    if (position->side_to_move == BLACK) {
        hash ^= zobrist_side;
    }

    hash ^= zobrist_castling[position->castling_rights & 15];

    if (square_is_valid(position->en_passant)) {
        hash ^= zobrist_ep[square_file(position->en_passant)];
    }

    return hash;
}


bool position_set_start(Position *position) {
    if (position == NULL) {
        return false;
    }

    memset(position, 0, sizeof(*position));

    position->side_to_move = WHITE;
    position->castling_rights =
        CASTLE_WHITE_KING |
        CASTLE_WHITE_QUEEN |
        CASTLE_BLACK_KING |
        CASTLE_BLACK_QUEEN;
    position->en_passant = NO_SQUARE;
    position->halfmove_clock = 0;
    position->fullmove_number = 1;

    const PieceType back_rank[8] = {
        ROOK, KNIGHT, BISHOP, QUEEN,
        KING, BISHOP, KNIGHT, ROOK
    };

    for (int file = 0; file < 8; ++file) {
        Square white_back = (Square)file;
        Square white_pawn = (Square)(8 + file);
        Square black_pawn = (Square)(48 + file);
        Square black_back = (Square)(56 + file);

        position->pieces[WHITE][back_rank[file]] |= BIT(white_back);
        position->pieces[WHITE][PAWN] |= BIT(white_pawn);

        position->pieces[BLACK][PAWN] |= BIT(black_pawn);
        position->pieces[BLACK][back_rank[file]] |= BIT(black_back);
    }

    position->king_square[WHITE] = E1;
    position->king_square[BLACK] = E8;

    position->occupied[WHITE] = 0;
    position->occupied[BLACK] = 0;

    for (int type = 0; type < PIECE_TYPE_NB; ++type) {
        position->occupied[WHITE] |= position->pieces[WHITE][type];
        position->occupied[BLACK] |= position->pieces[BLACK][type];
    }

    position->all =
        position->occupied[WHITE] |
        position->occupied[BLACK];

    position->hash_key = position_compute_hash(position);

    return position_is_valid(position);
}


bool position_set_from_fen(Position *position, const char *fen)
{
    if (position == NULL || fen == NULL)
        return false;

    char board[8][8];
    memset(board, '.', sizeof(board));

    const char *s = fen;
    int rank = 7;
    int file = 0;

    while (*s && *s != ' ') {
        char c = *s++;

        if (c == '/') {
            if (file != 8 || rank == 0)
                return false;
            --rank;
            file = 0;
        } else if (c >= '1' && c <= '8') {
            file += c - '0';
            if (file > 8)
                return false;
        } else {
            if (file >= 8 || strchr("pnbrqkPNBRQK", c) == NULL)
                return false;
            board[rank][file++] = c;
        }
    }

    if (*s != ' ' || rank != 0 || file != 8)
        return false;

    ++s;
    Color side;

    if (*s == 'w')
        side = WHITE;
    else if (*s == 'b')
        side = BLACK;
    else
        return false;

    ++s;
    if (*s++ != ' ')
        return false;

    uint8_t castling = 0;

    if (*s == '-') {
        ++s;
    } else {
        while (*s && *s != ' ') {
            switch (*s++) {
                case 'K': castling |= CASTLE_WHITE_KING; break;
                case 'Q': castling |= CASTLE_WHITE_QUEEN; break;
                case 'k': castling |= CASTLE_BLACK_KING; break;
                case 'q': castling |= CASTLE_BLACK_QUEEN; break;
                default: return false;
            }
        }
    }

    if (*s++ != ' ')
        return false;

    Square ep = NO_SQUARE;

    if (*s == '-') {
        ++s;
    } else {
        if (s[0] < 'a' || s[0] > 'h' ||
            (s[1] != '3' && s[1] != '6'))
            return false;

        ep = (Square)((s[1] - '1') * 8 + (s[0] - 'a'));
        s += 2;
    }

    if (*s++ != ' ')
        return false;

    char *end;
    errno = 0;
    unsigned long halfmove = strtoul(s, &end, 10);

    if (errno || end == s || halfmove > UINT16_MAX || *end != ' ')
        return false;

    s = end + 1;
    errno = 0;
    unsigned long fullmove = strtoul(s, &end, 10);

    if (errno || end == s || fullmove == 0 ||
        fullmove > UINT16_MAX || *end != '\0')
        return false;

    Position next;
    memset(&next, 0, sizeof(next));

    next.side_to_move = side;
    next.castling_rights = castling;
    next.en_passant = ep;
    next.halfmove_clock = (uint16_t)halfmove;
    next.fullmove_number = (uint16_t)fullmove;
    next.king_square[WHITE] = NO_SQUARE;
    next.king_square[BLACK] = NO_SQUARE;

    for (int r = 0; r < 8; ++r) {
        for (int f = 0; f < 8; ++f) {
            char c = board[r][f];

            if (c == '.')
                continue;

            Color color = isupper((unsigned char)c) ? WHITE : BLACK;
            PieceType type;

            switch (tolower((unsigned char)c)) {
                case 'p': type = PAWN; break;
                case 'n': type = KNIGHT; break;
                case 'b': type = BISHOP; break;
                case 'r': type = ROOK; break;
                case 'q': type = QUEEN; break;
                case 'k': type = KING; break;
                default: return false;
            }

            Square sq = (Square)(r * 8 + f);
            next.pieces[color][type] |= BIT(sq);

            if (type == KING)
                next.king_square[color] = sq;
        }
    }

    for (int color = 0; color < COLOR_NB; ++color) {
        for (int type = 0; type < PIECE_TYPE_NB; ++type)
            next.occupied[color] |= next.pieces[color][type];
        next.all |= next.occupied[color];
    }

    if (!position_is_valid(&next))
        return false;

    next.hash_key = position_compute_hash(&next);

    *position = next;
    return true;
}


bool position_is_valid(const Position *position) {
    if (position == NULL) {
        return false;
    }

    if (position->side_to_move != WHITE &&
        position->side_to_move != BLACK) {
        return false;
    }

    if (position->fullmove_number == 0) {
        return false;
    }

    Bitboard white = 0;
    Bitboard black = 0;

    for (int type = 0; type < PIECE_TYPE_NB; ++type) {
        Bitboard w = position->pieces[WHITE][type];
        Bitboard b = position->pieces[BLACK][type];

        if ((white & w) != 0 || (black & b) != 0) {
            return false;
        }

        white |= w;
        black |= b;
    }

    if ((white & black) != 0) {
        return false;
    }

    if (position->occupied[WHITE] != white ||
        position->occupied[BLACK] != black) {
        return false;
    }

    if (position->all != (white | black)) {
        return false;
    }

    if (position->pieces[WHITE][KING] == 0 ||
        position->pieces[BLACK][KING] == 0) {
        return false;
    }

    if ((position->pieces[WHITE][KING] &
         (position->pieces[WHITE][KING] - 1)) != 0 ||
        (position->pieces[BLACK][KING] &
         (position->pieces[BLACK][KING] - 1)) != 0) {
        return false;
    }

    if (position->king_square[WHITE] < A1 ||
        position->king_square[WHITE] > H8 ||
        position->king_square[BLACK] < A1 ||
        position->king_square[BLACK] > H8) {
        return false;
    }

    if (!(position->pieces[WHITE][KING] &
          BIT(position->king_square[WHITE])) ||
        !(position->pieces[BLACK][KING] &
          BIT(position->king_square[BLACK]))) {
        return false;
    }

    if (position->en_passant != NO_SQUARE &&
        !square_is_valid(position->en_passant)) {
        return false;
    }

    return true;
}


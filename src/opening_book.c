#include "opening_book.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "polyglot_random.h"

#define POLYGLOT_RECORD_SIZE 16

typedef struct {
    uint64_t key;
    uint16_t move;
    uint16_t weight;
} BookEntry;

static BookEntry *entries = NULL;
static size_t entry_count = 0;
static bool random_seeded = false;

static uint16_t read_be16(const unsigned char *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint64_t read_be64(const unsigned char *p)
{
    uint64_t value = 0;

    for (int i = 0; i < 8; ++i)
        value = (value << 8) | p[i];

    return value;
}

static int piece_at_square(const Position *p, int square)
{
    Bitboard bit = BIT(square);

    for (int color = 0; color < COLOR_NB; ++color) {
        for (int type = 0; type < PIECE_TYPE_NB; ++type) {
            if (p->pieces[color][type] & bit)
                return color * 6 + type;
        }
    }

    return -1;
}

/*
 * Polyglot piece order:
 * black pawn, white pawn, black knight, white knight,
 * black bishop, white bishop, black rook, white rook,
 * black queen, white queen, black king, white king.
 */
static uint64_t polyglot_hash(const Position *p)
{
    uint64_t key = 0;

    for (int square = 0; square < BOARD_SQUARES; ++square) {
        int piece = piece_at_square(p, square);

        if (piece < 0)
            continue;

        int color = piece / 6;
        int type = piece % 6;
        int polyglot_piece = type * 2 + (color == WHITE ? 1 : 0);

        key ^= POLYGLOT_RANDOM[polyglot_piece * 64 + square];
    }

    if (p->castling_rights & CASTLE_WHITE_KING)
        key ^= POLYGLOT_RANDOM[768];

    if (p->castling_rights & CASTLE_WHITE_QUEEN)
        key ^= POLYGLOT_RANDOM[769];

    if (p->castling_rights & CASTLE_BLACK_KING)
        key ^= POLYGLOT_RANDOM[770];

    if (p->castling_rights & CASTLE_BLACK_QUEEN)
        key ^= POLYGLOT_RANDOM[771];

    /*
     * Polyglot includes the en-passant file only if the side
     * to move has a pawn that can capture en passant.
     */
    if (square_is_valid(p->en_passant)) {
        int ep = p->en_passant;
        int file = square_file((Square)ep);
        int rank = square_rank((Square)ep);
        int source_rank = rank + (p->side_to_move == WHITE ? -1 : 1);

        if (source_rank >= 0 && source_rank < 8) {
            for (int df = -1; df <= 1; df += 2) {
                int source_file = file + df;

                if (source_file < 0 || source_file >= 8)
                    continue;

                int source = source_rank * 8 + source_file;
                int piece = piece_at_square(p, source);

                if (piece == (int)p->side_to_move * 6 + PAWN) {
                    key ^= POLYGLOT_RANDOM[772 + file];
                    break;
                }
            }
        }
    }

    /* Polyglot XORs this key when White is to move. */
    if (p->side_to_move == WHITE)
        key ^= POLYGLOT_RANDOM[780];

    return key;
}

bool opening_book_init(const char *path)
{
    opening_book_close();

    if (path == NULL)
        return false;

    FILE *file = fopen(path, "rb");

    if (file == NULL)
        return false;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }

    long size = ftell(file);

    if (size < 0 || size % POLYGLOT_RECORD_SIZE != 0) {
        fclose(file);
        return false;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }

    size_t count = (size_t)size / POLYGLOT_RECORD_SIZE;

    if (count == 0 || count > SIZE_MAX / sizeof(BookEntry)) {
        fclose(file);
        return false;
    }

    BookEntry *loaded = malloc(count * sizeof(*loaded));

    if (loaded == NULL) {
        fclose(file);
        return false;
    }

    unsigned char raw[POLYGLOT_RECORD_SIZE];

    for (size_t i = 0; i < count; ++i) {
        if (fread(raw, 1, sizeof(raw), file) != sizeof(raw)) {
            free(loaded);
            fclose(file);
            return false;
        }

        loaded[i].key = read_be64(raw);
        loaded[i].move = read_be16(raw + 8);
        loaded[i].weight = read_be16(raw + 10);
    }

    fclose(file);

    /* Polyglot records must be sorted by their 64-bit key. */
    for (size_t i = 1; i < count; ++i) {
        if (loaded[i - 1].key > loaded[i].key) {
            free(loaded);
            return false;
        }
    }

    entries = loaded;
    entry_count = count;

    if (!random_seeded) {
        srand((unsigned int)time(NULL));
        random_seeded = true;
    }

    return true;
}

void opening_book_close(void)
{
    free(entries);
    entries = NULL;
    entry_count = 0;
}

static bool decode_book_move(
    Position *position,
    uint16_t encoded,
    Move *result
)
{
    int to = encoded & 0x3F;
    int from = (encoded >> 6) & 0x3F;
    int promotion = (encoded >> 12) & 0x7;

    /*
     * Polyglot encodes castling as king-to-rook-square.
     * This engine encodes it as king-to-final-square.
     */
    if ((from == E1 || from == E8) &&
        (to == H1 || to == A1 || to == H8 || to == A8)) {
        if (to == H1) to = G1;
        else if (to == A1) to = C1;
        else if (to == H8) to = G8;
        else if (to == A8) to = C8;
    }

    unsigned internal_promo = 0;

    switch (promotion) {
        case 0: internal_promo = 0; break;
        case 1: internal_promo = KNIGHT; break;
        case 2: internal_promo = BISHOP; break;
        case 3: internal_promo = ROOK; break;
        case 4: internal_promo = QUEEN; break;
        default: return false;
    }

    MoveList legal;
    generate_legal_moves(position, &legal);

    for (int i = 0; i < legal.count; ++i) {
        Move candidate = legal.moves[i];

        if ((int)MOVE_FROM(candidate) != from ||
            (int)MOVE_TO(candidate) != to ||
            MOVE_PROMO(candidate) != internal_promo) {
            continue;
        }

        if ((promotion != 0) !=
            (MOVE_FLAG(candidate) == MOVE_PROMOTION)) {
            continue;
        }

        *result = candidate;
        return true;
    }

    return false;
}

bool opening_book_get_move(Position *position, Move *move)
{
    if (position == NULL || move == NULL ||
        entries == NULL || entry_count == 0) {
        return false;
    }

    uint64_t key = polyglot_hash(position);
    fprintf(stderr,
            "[BOOK DEBUG] hash=%016llx entries=%zu\\n",
            (unsigned long long)key, entry_count);

    /* Binary search for the first record with this key. */
    size_t low = 0;
    size_t high = entry_count;

    while (low < high) {
        size_t mid = low + (high - low) / 2;

        if (entries[mid].key < key)
            low = mid + 1;
        else
            high = mid;
    }

    size_t first = low;

    if (first >= entry_count || entries[first].key != key)
        return false;

    Move candidates[MAX_MOVES];
    unsigned int weights[MAX_MOVES];
    size_t count = 0;
    uint64_t total_weight = 0;

    for (size_t i = first;
         i < entry_count && entries[i].key == key;
         ++i) {
        Move decoded;

        if (!decode_book_move(position, entries[i].move, &decoded))
            continue;

        if (count >= MAX_MOVES)
            break;

        candidates[count] = decoded;
        weights[count] = entries[i].weight;
        total_weight += entries[i].weight;
        ++count;
    }

    if (count == 0)
        return false;

    /* If all weights are zero, choose uniformly among legal moves. */
    if (total_weight == 0) {
        *move = candidates[(size_t)rand() % count];
        return true;
    }

    uint64_t choice =
        ((uint64_t)(unsigned int)rand() * total_weight) /
        ((uint64_t)RAND_MAX + 1);

    for (size_t i = 0; i < count; ++i) {
        if (choice < weights[i]) {
            *move = candidates[i];
            return true;
        }

        choice -= weights[i];
    }

    *move = candidates[count - 1];
    return true;
}

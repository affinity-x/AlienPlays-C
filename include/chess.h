
#ifndef ALIENPLAYS_CHESS_H
#define ALIENPLAYS_CHESS_H

#include <stdbool.h>
#include <stdint.h>

#define BOARD_SQUARES 64
#define MAX_MOVES 256
#define MAX_PLY 128

typedef uint64_t Bitboard;

typedef enum {
    WHITE = 0,
    BLACK = 1,
    COLOR_NB = 2
} Color;

typedef enum {
    PAWN = 0,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
    PIECE_TYPE_NB
} PieceType;

typedef enum {
    NO_PIECE = 0,
    W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
    PIECE_NB
} Piece;

typedef enum {
    NO_SQUARE = -1,
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    SQUARE_NB
} Square;

typedef uint32_t Move;

typedef enum {
    MOVE_NORMAL = 0,
    MOVE_EN_PASSANT = 1,
    MOVE_CASTLING = 2,
    MOVE_PROMOTION = 3
} MoveFlag;

#define MOVE_FROM(m)       ((Square)((m) & 0x3FU))
#define MOVE_TO(m)         ((Square)(((m) >> 6) & 0x3FU))
#define MOVE_PROMO(m)      (((m) >> 12) & 0x7U)
#define MOVE_FLAG(m)       (((m) >> 15) & 0x7U)

#define MAKE_MOVE(from, to, promo, flag) \
    ((Move)((from) | ((to) << 6) | ((promo) << 12) | ((flag) << 15)))


typedef struct {
    Bitboard pieces[COLOR_NB][PIECE_TYPE_NB];
    Bitboard occupied[COLOR_NB];
    Bitboard all;
    Square king_square[COLOR_NB];

    Color side_to_move;
    uint8_t castling_rights;
    Square en_passant;
    uint16_t halfmove_clock;
    uint16_t fullmove_number;

    uint64_t hash_key;
} Position;

typedef struct {
    Move moves[MAX_MOVES];
    int count;
} MoveList;

typedef struct {
    Position position;
    Move move;
    uint64_t hash_key;
    uint16_t halfmove_clock;
    Square en_passant;
    uint8_t castling_rights;
} UndoState;

enum {
    CASTLE_WHITE_KING  = 1,
    CASTLE_WHITE_QUEEN = 2,
    CASTLE_BLACK_KING  = 4,
    CASTLE_BLACK_QUEEN = 8
};

#define BIT(square) (UINT64_C(1) << (square))

static inline Color opposite_color(Color color) {
    return (Color)(color ^ 1);
}

static inline Piece make_piece(Color color, PieceType type) {
    return (Piece)(1 + (int)color * 6 + (int)type);
}

static inline Color piece_color(Piece piece) {
    return piece >= B_PAWN ? BLACK : WHITE;
}

static inline PieceType piece_type(Piece piece) {
    return (PieceType)(((piece - 1) % 6));
}

static inline bool square_is_valid(int square) {
    return square >= 0 && square < SQUARE_NB;
}

static inline int square_file(Square square) {
    return (int)square & 7;
}

static inline int square_rank(Square square) {
    return (int)square >> 3;
}

static inline Bitboard square_bb(Square square) {
    return square_is_valid(square) ? BIT(square) : UINT64_C(0);
}

bool position_set_start(Position *position);
bool position_is_valid(const Position *position);
uint64_t position_compute_hash(const Position *position);
bool position_set_from_fen(Position *position, const char *fen);

void generate_pseudo_legal_moves(const Position *position, MoveList *list);
bool make_move(Position *position, Move move, UndoState *undo);
void unmake_move(Position *position, const UndoState *undo);
bool square_is_attacked(
    const Position *position,
    Square square,
    Color by_color
);

void generate_legal_moves(
    Position *position,
    MoveList *list
);

bool opening_book_init(const char *path);
void opening_book_close(void);
bool opening_book_get_move(
    Position *position,
    Move *move
);

#endif


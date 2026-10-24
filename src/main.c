#include "chess.h"
#include "engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
#include "opening_book.h"

#define ENGINE_NAME "AlienPlays-C"
#define ENGINE_VERSION "0.2"
#define INPUT_SIZE 8192
#define DEBUG_LOG_PATH "/home/nick/Dokumentai/AlienPlays-C/alienplays-uci.log"

static FILE *debug_log = NULL;

static void log_debug(const char *format, ...)
{
    if (debug_log == NULL) {
        return;
    }

    va_list args;
    va_start(args, format);
    vfprintf(debug_log, format, args);
    va_end(args);

    fflush(debug_log);
}

static int square_from_text(const char *s)
{
    if (s[0] < 'a' || s[0] > 'h' ||
        s[1] < '1' || s[1] > '8') {
        return -1;
    }

    return (s[1] - '1') * 8 + (s[0] - 'a');
}

static void move_to_uci(Move move, char out[6])
{
    Square from = MOVE_FROM(move);
    Square to = MOVE_TO(move);

    out[0] = (char)('a' + square_file(from));
    out[1] = (char)('1' + square_rank(from));
    out[2] = (char)('a' + square_file(to));
    out[3] = (char)('1' + square_rank(to));
    out[4] = '\0';
    out[5] = '\0';

    if (MOVE_FLAG(move) == MOVE_PROMOTION) {
        switch (MOVE_PROMO(move)) {
            case KNIGHT: out[4] = 'n'; break;
            case BISHOP: out[4] = 'b'; break;
            case ROOK:   out[4] = 'r'; break;
            case QUEEN:  out[4] = 'q'; break;
            default:     out[4] = '\0'; break;
        }
    }
}

static int parse_uci_move(Position *p, const char *text, Move *result)
{
    if (strlen(text) < 4) {
        return 0;
    }

    int from = square_from_text(text);
    int to = square_from_text(text + 2);

    if (from < 0 || to < 0) {
        return 0;
    }

    char promotion = '\0';

    if (text[4] != '\0') {
        promotion = (char)tolower((unsigned char)text[4]);
    }

    MoveList list;
    generate_legal_moves(p, &list);

    for (int i = 0; i < list.count; ++i) {
        Move m = list.moves[i];

        if ((int)MOVE_FROM(m) != from ||
            (int)MOVE_TO(m) != to) {
            continue;
        }

        if (MOVE_FLAG(m) == MOVE_PROMOTION) {
            unsigned wanted;

            switch (promotion) {
                case 'n': wanted = KNIGHT; break;
                case 'b': wanted = BISHOP; break;
                case 'r': wanted = ROOK; break;
                case 'q': wanted = QUEEN; break;
                default: continue;
            }

            if (MOVE_PROMO(m) != wanted) {
                continue;
            }
        } else if (promotion != '\0') {
            continue;
        }

        *result = m;
        return 1;
    }

    return 0;
}

static void handle_position(Position *p, char *args)
{
    log_debug("POSITION args: %s\n", args);

    char *token = strtok(args, " \t\r\n");

    if (token == NULL)
        return;

    if (strcmp(token, "startpos") == 0) {
        if (!position_set_start(p)) {
            log_debug("ERROR: startpos initialization failed\n");
            return;
        }
        token = strtok(NULL, " \t\r\n");
    } else if (strcmp(token, "fen") == 0) {
        char fen[256] = {0};

        for (int i = 0; i < 6; ++i) {
            token = strtok(NULL, " \t\r\n");

            if (token == NULL) {
                log_debug("ERROR: incomplete FEN\n");
                return;
            }

            if (i > 0)
                strcat(fen, " ");
            strcat(fen, token);
        }

        if (!position_set_from_fen(p, fen)) {
            log_debug("ERROR: invalid FEN: %s\n", fen);
            printf("info string Invalid FEN\n");
            return;
        }

        token = strtok(NULL, " \t\r\n");
    } else {
        log_debug("ERROR: unsupported position format: %s\n", token);
        return;
    }

    if (token == NULL)
        return;

    if (strcmp(token, "moves") != 0) {
        log_debug("ERROR: unexpected position token: %s\n", token);
        return;
    }

    while ((token = strtok(NULL, " \t\r\n")) != NULL) {
        Move move;

        if (!parse_uci_move(p, token, &move)) {
            log_debug("ERROR: illegal position move: %s\n", token);
            printf("info string Invalid position move: %s\n", token);
            return;
        }

        UndoState undo;

        if (!make_move(p, move, &undo)) {
            log_debug("ERROR: failed to apply move: %s\n", token);
            return;
        }

        log_debug("Applied position move: %s\n", token);
    }

    log_debug("Position command processed successfully\n");
}

static void handle_go(Position *p, const char *args)
{
    int depth = 4;
    const char *cursor = args;

    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t') {
            ++cursor;
        }

        if (*cursor == '\0') {
            break;
        }

        if (strncmp(cursor, "depth", 5) == 0 &&
            (cursor[5] == ' ' || cursor[5] == '\t')) {
            cursor += 5;

            while (*cursor == ' ' || *cursor == '\t') {
                ++cursor;
            }

            char *end = NULL;
            long parsed = strtol(cursor, &end, 10);

            if (end != cursor && parsed >= 1 &&
                parsed <= ENGINE_MAX_DEPTH) {
                depth = (int)parsed;
            }

            if (end != cursor) {
                cursor = end;
            }
        } else {
            while (*cursor != '\0' &&
                   *cursor != ' ' && *cursor != '\t') {
                ++cursor;
            }
        }
    }

    log_debug("\n--- SEARCH START ---\n");
    log_debug("Go arguments: %s\n", args);
    log_debug("Search depth: %d\n", depth);

    Move book_move;

if (opening_book_get_move(p, &book_move)) {
    char text[6];
    move_to_uci(book_move, text);

    log_debug("Opening book hit: %s\n", text);

    printf("info depth 0 score cp 0 nodes 0\n");
    printf("info string opening book move\n");
    printf("bestmove %s\n", text);
    fflush(stdout);
    return;
}

log_debug("Opening book miss; starting search\n");

SearchResult result = engine_search(p, depth);

    MoveList legal;
    generate_legal_moves(p, &legal);

    log_debug("Legal moves (%d):\n", legal.count);

    for (int i = 0; i < legal.count; ++i) {
        char move_text[6];
        move_to_uci(legal.moves[i], move_text);

        log_debug("  %s (raw=%u)\n",
                  move_text, (unsigned)legal.moves[i]);
    }

    log_debug("Search result: has_move=%d, raw_move=%u\n",
              result.has_move, (unsigned)result.best_move);
    log_debug("Depth reached: %d, score: %d, nodes: %llu\n",
              result.depth,
              result.score,
              (unsigned long long)result.nodes);

    int best_is_legal = 0;

    for (int i = 0; i < legal.count; ++i) {
        if (legal.moves[i] == result.best_move) {
            best_is_legal = 1;
            break;
        }
    }

    log_debug("Best move is legal: %s\n",
              best_is_legal ? "YES" : "NO");

    printf("info depth %d score cp %d nodes %llu\n",
           result.depth,
           result.score,
           (unsigned long long)result.nodes);

    if (!result.has_move) {
        log_debug("ENGINE -> GUI: bestmove 0000\n");
        puts("bestmove 0000");
        fflush(stdout);
        return;
    }

    char text[6];
    move_to_uci(result.best_move, text);

    log_debug("ENGINE -> GUI: bestmove %s\n", text);

    if (!best_is_legal) {
        log_debug("ERROR: engine returned an illegal root move!\n");
    }

    printf("bestmove %s\n", text);
    fflush(stdout);

    log_debug("--- SEARCH END ---\n\n");
}

int main(void)
{
    char input[INPUT_SIZE];

    debug_log = fopen(DEBUG_LOG_PATH, "w");

    if (debug_log == NULL) {
        fprintf(stderr, "Warning: cannot open UCI debug log: %s\n",
                DEBUG_LOG_PATH);
    } else {
        log_debug("=== AlienPlays-C UCI debug started ===\n");
    }

    Position position;

    if (!position_set_start(&position)) {
        fprintf(stderr, "Failed to initialize AlienPlays-C\n");

        if (debug_log != NULL) {
            fclose(debug_log);
        }

        return 1;
    }

    if (opening_book_init("codekiddy.bin")) {
        log_debug("Opening book loaded: codekiddy.bin\n");
    } else {
        log_debug("Opening book unavailable; using search only\n");
    }

    while (fgets(input, sizeof(input), stdin) != NULL) {
        input[strcspn(input, "\r\n")] = '\0';

        log_debug("GUI -> ENGINE: %s\n", input);

        if (strcmp(input, "uci") == 0) {
            printf("id name %s %s\n", ENGINE_NAME, ENGINE_VERSION);
            puts("id author AlienPlays");
            puts("uciok");
            fflush(stdout);

        } else if (strcmp(input, "isready") == 0) {
            puts("readyok");
            fflush(stdout);

        } else if (strcmp(input, "ucinewgame") == 0) {
            if (!position_set_start(&position)) {
                log_debug("ERROR: ucinewgame initialization failed\n");
            } else {
                log_debug("New game initialized\n");
            }

        } else if (strncmp(input, "setoption ", 10) == 0) {
           log_debug("Accepted option command: %s\n", input);

        } else if (strncmp(input, "position ", 9) == 0) {
            handle_position(&position, input + 9);

        } else if (strncmp(input, "go", 2) == 0 &&
                   (input[2] == '\0' ||
                    isspace((unsigned char)input[2]))) {
            handle_go(&position, input + 2);

        } else if (strcmp(input, "quit") == 0) {
            log_debug("GUI -> ENGINE: quit\n");
            break;

        } else if (input[0] != '\0') {
            log_debug("Unhandled UCI command: %s\n", input);
        }
    }

    log_debug("=== AlienPlays-C UCI debug stopped ===\n");

    if (debug_log != NULL) {
        fclose(debug_log);
    }
opening_book_close();
    return 0;
}
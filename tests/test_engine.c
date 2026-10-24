#include "engine.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    Position p;

    assert(position_set_start(&p));

    Position original = p;

    int score = evaluate_position(&p);
    assert(score == 0);

    SearchResult result = engine_search(&p, 3);

    assert(result.has_move);
    assert(result.nodes > 0);
    assert(result.depth == 3);
    assert(memcmp(&p, &original, sizeof(p)) == 0);

    MoveList legal;
    generate_legal_moves(&p, &legal);

    int found = 0;
    for (int i = 0; i < legal.count; ++i) {
        if (legal.moves[i] == result.best_move) {
            found = 1;
            break;
        }
    }

    assert(found);

    printf("PASS: evaluation = %d\n", score);
    printf("PASS: engine found a legal move at depth %d\n",
           result.depth);
    printf("PASS: searched nodes = %llu\n",
           (unsigned long long)result.nodes);
    puts("All engine tests passed.");

    return 0;
}

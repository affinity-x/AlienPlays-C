
#include <stdio.h>
#include <stdbool.h>

#include "chess.h"
#include "engine.h"

int main(void)
{
    Position position;

    if (!position_set_start(&position)) {
        fprintf(stderr, "Nepavyko inicializuoti pozicijos.\n");
        return 1;
    }

    MoveList moves;
    generate_legal_moves(&position, &moves);

    printf("Pradine pozicija: %d legalus(-iu) ejimu.\n",
           moves.count);

    SearchResult result = engine_search(&position, 4);

    printf("Gylis: %d\n", result.depth);
    printf("Mazgai: %llu\n",
           (unsigned long long)result.nodes);
    printf("Rastas ejimas: %s\n",
           result.has_move ? "taip" : "ne");

    return 0;
}

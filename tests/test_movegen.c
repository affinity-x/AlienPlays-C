
#include "chess.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    Position position;
    MoveList pseudo;
    MoveList legal;

    assert(position_set_start(&position));

    generate_pseudo_legal_moves(&position, &pseudo);
    generate_legal_moves(&position, &legal);

    printf("Pseudo-legalus kandidatai: %d\n", pseudo.count);
    printf("Legalus ejimai: %d\n", legal.count);

    assert(pseudo.count == 20);
    assert(legal.count == 20);

    puts("PASS: pradine pozicija turi 20 legaliu ejimu.");
    return 0;
}

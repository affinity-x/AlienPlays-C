
#include "chess.h"
#include "pgn.h"

#include <stdio.h>

int main(void) {
    Position position;
    TrainingDataset original;
    TrainingDataset loaded;

    if (!position_set_start(&position)) {
        puts("FAIL: start position");
        return 1;
    }

    dataset_init(&original);
    dataset_init(&loaded);

    if (!dataset_add(&original, &position, 1.0f)) {
        puts("FAIL: dataset_add");
        return 1;
    }

    if (!dataset_save(&original, "tests/test_data.bin")) {
        puts("FAIL: dataset_save");
        dataset_free(&original);
        dataset_free(&loaded);
        return 1;
    }

    if (!dataset_load(&loaded, "tests/test_data.bin")) {
        puts("FAIL: dataset_load");
        dataset_free(&original);
        dataset_free(&loaded);
        return 1;
    }

    if (loaded.count != 1 ||
        loaded.samples[0].result != 1.0f ||
        loaded.samples[0].position.hash_key != position.hash_key) {
        puts("FAIL: data mismatch");
        dataset_free(&original);
        dataset_free(&loaded);
        return 1;
    }

    puts("PASS: dataset save/load works!");

    dataset_free(&original);
    dataset_free(&loaded);
    return 0;
}


#ifndef ALIENPLAYS_PGN_H
#define ALIENPLAYS_PGN_H

#include "chess.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    Position position;
    float result; /* 1.0 = White win, 0.5 = draw, 0.0 = Black win */
} TrainingSample;

typedef struct {
    TrainingSample *samples;
    size_t count;
    size_t capacity;
} TrainingDataset;

void dataset_init(TrainingDataset *dataset);
void dataset_free(TrainingDataset *dataset);

bool dataset_add(
    TrainingDataset *dataset,
    const Position *position,
    float result
);

bool dataset_save(
    const TrainingDataset *dataset,
    const char *path
);

bool dataset_load(
    TrainingDataset *dataset,
    const char *path
);

#endif

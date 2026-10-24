
#ifndef ALIENPLAYS_LEARN_H
#define ALIENPLAYS_LEARN_H

#include "chess.h"

#define LEARN_FEATURES 6

typedef struct {
    double weights[LEARN_FEATURES];
    double bias;
    uint64_t positions_trained;
} LearnModel;

/* Initialize model with sensible starting values. */
void learn_init(LearnModel *model);

/* Evaluate a position from White's perspective. */
double learn_evaluate(
    const LearnModel *model,
    const Position *position
);

/* Train on one position.
   target: 1.0 = White win, 0.5 = draw, 0.0 = Black win. */
bool learn_train(
    LearnModel *model,
    const Position *position,
    double target,
    double learning_rate
);

/* Save/load model weights. */
bool learn_save(const LearnModel *model, const char *path);
bool learn_load(LearnModel *model, const char *path);

#endif

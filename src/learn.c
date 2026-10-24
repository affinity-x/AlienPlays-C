
#include "learn.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const double initial_weights[LEARN_FEATURES] = {
    1.0, 3.2, 3.3, 5.0, 9.0, 0.0
};

static double sigmoid(double x) {
    if (x > 30.0) return 1.0;
    if (x < -30.0) return 0.0;
    return 1.0 / (1.0 + exp(-x));
}

static void extract_features(
    const Position *position,
    double features[LEARN_FEATURES]
) {
    for (int type = PAWN; type <= KING; ++type) {
        int white_count = __builtin_popcountll(
            position->pieces[WHITE][type]
        );
        int black_count = __builtin_popcountll(
            position->pieces[BLACK][type]
        );

        features[type] = (double)(white_count - black_count);
    }
}

void learn_init(LearnModel *model) {
    if (!model) return;

    memcpy(model->weights, initial_weights, sizeof(initial_weights));
    model->bias = 0.0;
    model->positions_trained = 0;
}

double learn_evaluate(
    const LearnModel *model,
    const Position *position
) {
    if (!model || !position) return 0.0;

    double features[LEARN_FEATURES];
    extract_features(position, features);

    double score = model->bias;

    for (int i = 0; i < LEARN_FEATURES; ++i) {
        score += model->weights[i] * features[i];
    }

    return score;
}

bool learn_train(
    LearnModel *model,
    const Position *position,
    double target,
    double learning_rate
) {
    if (!model || !position ||
        target < 0.0 || target > 1.0 ||
        learning_rate <= 0.0) {
        return false;
    }

    double features[LEARN_FEATURES];
    extract_features(position, features);

    /* Convert the material score into a win-probability estimate. */
    double score = learn_evaluate(model, position);
    double prediction = sigmoid(score / 4.0);
    double error = target - prediction;

    for (int i = 0; i < LEARN_FEATURES; ++i) {
        model->weights[i] +=
            learning_rate * error * features[i] / 4.0;
    }

    model->bias += learning_rate * error / 4.0;
    model->positions_trained++;

    return true;
}

bool learn_save(const LearnModel *model, const char *path) {
    if (!model || !path) return false;

    FILE *file = fopen(path, "wb");
    if (!file) return false;

    bool ok =
        fwrite(model, sizeof(*model), 1, file) == 1;

    if (fclose(file) != 0) ok = false;
    return ok;
}

bool learn_load(LearnModel *model, const char *path) {
    if (!model || !path) return false;

    FILE *file = fopen(path, "rb");
    if (!file) return false;

    LearnModel loaded;
    bool ok = fread(&loaded, sizeof(loaded), 1, file) == 1;

    if (fclose(file) != 0) ok = false;
    if (!ok) return false;

    for (int i = 0; i < LEARN_FEATURES; ++i) {
        if (!isfinite(loaded.weights[i])) return false;
    }

    if (!isfinite(loaded.bias)) return false;

    *model = loaded;
    return true;
}

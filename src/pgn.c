
#include "pgn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dataset_init(TrainingDataset *dataset) {
    if (!dataset) return;

    dataset->samples = NULL;
    dataset->count = 0;
    dataset->capacity = 0;
}

void dataset_free(TrainingDataset *dataset) {
    if (!dataset) return;

    free(dataset->samples);
    dataset->samples = NULL;
    dataset->count = 0;
    dataset->capacity = 0;
}

bool dataset_add(
    TrainingDataset *dataset,
    const Position *position,
    float result
) {
    if (!dataset || !position ||
        result < 0.0f || result > 1.0f) {
        return false;
    }

    if (dataset->count == dataset->capacity) {
        size_t new_capacity =
            dataset->capacity == 0 ? 256 : dataset->capacity * 2;

        if (new_capacity < dataset->capacity ||
            new_capacity > SIZE_MAX / sizeof(TrainingSample)) {
            return false;
        }

        TrainingSample *new_samples = realloc(
            dataset->samples,
            new_capacity * sizeof(TrainingSample)
        );

        if (!new_samples) return false;

        dataset->samples = new_samples;
        dataset->capacity = new_capacity;
    }

    TrainingSample *sample = &dataset->samples[dataset->count++];
    sample->position = *position;
    sample->result = result;

    return true;
}

bool dataset_save(
    const TrainingDataset *dataset,
    const char *path
) {
    if (!dataset || !path ||
        dataset->count > dataset->capacity ||
        (dataset->count > 0 && !dataset->samples)) {
        return false;
    }

    FILE *file = fopen(path, "wb");
    if (!file) return false;

    const char magic[8] = {'A','P','D','A','T','A','1','\0'};
    uint64_t count = (uint64_t)dataset->count;

    bool ok =
        fwrite(magic, sizeof(magic), 1, file) == 1 &&
        fwrite(&count, sizeof(count), 1, file) == 1;

    if (ok && count > 0) {
        ok = fwrite(
            dataset->samples,
            sizeof(TrainingSample),
            dataset->count,
            file
        ) == dataset->count;
    }

    if (fclose(file) != 0) ok = false;
    return ok;
}

bool dataset_load(
    TrainingDataset *dataset,
    const char *path
) {
    if (!dataset || !path) return false;

    FILE *file = fopen(path, "rb");
    if (!file) return false;

    char magic[8];
    uint64_t count = 0;

    bool ok =
        fread(magic, sizeof(magic), 1, file) == 1 &&
        memcmp(magic, "APDATA1", 7) == 0 &&
        fread(&count, sizeof(count), 1, file) == 1;

    if (!ok ||
        count > SIZE_MAX / sizeof(TrainingSample)) {
        fclose(file);
        return false;
    }

    TrainingDataset loaded;
    dataset_init(&loaded);

    if (count > 0) {
        loaded.samples = malloc(
            (size_t)count * sizeof(TrainingSample)
        );

        if (!loaded.samples) {
            fclose(file);
            return false;
        }

        if (fread(
                loaded.samples,
                sizeof(TrainingSample),
                (size_t)count,
                file
            ) != (size_t)count) {
            dataset_free(&loaded);
            fclose(file);
            return false;
        }

        loaded.count = (size_t)count;
        loaded.capacity = (size_t)count;
    }

    /* Reject truncated or unexpectedly extended files. */
    int extra = fgetc(file);
    if (extra != EOF || ferror(file)) ok = false;

    if (fclose(file) != 0) ok = false;

    if (!ok) {
        dataset_free(&loaded);
        return false;
    }

    dataset_free(dataset);
    *dataset = loaded;
    return true;
}

#pragma once

#include <cstdint>
#include <sstream>
#include <stdint.h>
#include <string>
#include <stdio.h>
#include <iostream>
#include "hnswindex.h"

using namespace std;

class HNSWBinding {
public:
    HNSWBinding(int dimension) {
        index = new Index<float>("L2", dimension);
    }

    int initNewIndex(int maxNumberOfElements, int M=16, int efConstruction=200, int randomSeed=100) const {
        index->init_new_index(maxNumberOfElements, M, efConstruction, randomSeed);
    }

    int addItem(float* item, bool item_normalized, int id) const {
        index->add_item(item, item_normalized, id);
    }

    int knnQuery(float* input, bool input_normalized, int k, int* indices /* output */, float* coefficients /* output */) {
        index->knn_query(input, input_normalized, k, indices, coefficients);
    }

    int getCurrentCount() const {
        return index->get_current_count();
    }

private:
    Index<float>* index;
};

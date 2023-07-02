#include <iostream>
#include <atomic>
#include <cmath>
#include "hnswlib/hnswlib.h"

#define RESULT_SUCCESSFUL 0
#define RESULT_EXCEPTION_THROWN 1
#define RESULT_INDEX_ALREADY_INITIALIZED 2
#define RESULT_QUERY_CANNOT_RETURN 3
#define RESULT_ITEM_CANNOT_BE_INSERTED_INTO_THE_VECTOR_SPACE 4
#define RESULT_ONCE_INDEX_IS_CLEARED_IT_CANNOT_BE_REUSED 5
#define RESULT_GET_DATA_FAILED 6
#define RESULT_ID_NOT_IN_INDEX 7
#define RESULT_INDEX_NOT_INITIALIZED 8

#define TRY_CATCH_NO_INITIALIZE_CHECK_AND_RETURN_INT_BLOCK(block)    if (index_cleared) return RESULT_ONCE_INDEX_IS_CLEARED_IT_CANNOT_BE_REUSED;  int result_code = RESULT_SUCCESSFUL; try { block } catch (...) { result_code = RESULT_EXCEPTION_THROWN; }; return result_code;
#define TRY_CATCH_RETURN_INT_BLOCK(block)    if (!index_initialized) return RESULT_INDEX_NOT_INITIALIZED; TRY_CATCH_NO_INITIALIZE_CHECK_AND_RETURN_INT_BLOCK(block)

template<typename dist_t, typename data_t=float>
class Index {
public:
    Index(const std::string &space_name, const int dim) :
            space_name(space_name), dim(dim) {
        data_must_be_normalized = false;
        if(space_name=="L2") {
            l2space = new hnswlib::L2Space(dim);
        } else if(space_name=="IP") {
            l2space = new hnswlib::InnerProductSpace(dim);
        } else if(space_name=="COSINE") {
            l2space = new hnswlib::InnerProductSpace(dim);
            data_must_be_normalized = true;
        }
        appr_alg = NULL;
        index_initialized = false;
        index_cleared = false;
    }

    int init_new_index(const size_t maxElements, const size_t M, const size_t efConstruction, const size_t random_seed) {
        TRY_CATCH_NO_INITIALIZE_CHECK_AND_RETURN_INT_BLOCK({
            if (appr_alg) {
                return RESULT_INDEX_ALREADY_INITIALIZED;
            }
            appr_alg = new hnswlib::HierarchicalNSW<dist_t>(l2space, maxElements, M, efConstruction, random_seed);
            index_initialized = true;
        });
    }

    int set_ef(size_t ef) {
     	TRY_CATCH_RETURN_INT_BLOCK({
        	appr_alg->ef_ = ef;
    	});
    }

   	int get_ef() {
   		return appr_alg->ef_;
   	}

    int get_ef_construction() {
        return appr_alg->ef_construction_;
    }

    int get_M() {
        return appr_alg->M_;
    }

    int save_index(const std::string &path_to_index) {
        TRY_CATCH_RETURN_INT_BLOCK({
            appr_alg->saveIndex(path_to_index);
        });
    }

    int load_index(const std::string &path_to_index, size_t max_elements) {
        TRY_CATCH_NO_INITIALIZE_CHECK_AND_RETURN_INT_BLOCK({
            if (appr_alg) {
                std::cerr << "Warning: Calling load_index for an already initialized index. Old index is being deallocated.";
                delete appr_alg;
            }
            appr_alg = new hnswlib::HierarchicalNSW<dist_t>(l2space, path_to_index, false, max_elements);
        });
    }

	void normalize_array(float* array){
        float norm = 0.0f;
        for (int i=0; i<dim; i++) {
            norm += (array[i] * array[i]);
        }
        norm = 1.0f / (sqrtf(norm) + 1e-30f);
        for (int i=0; i<dim; i++) {
            array[i] = array[i] * norm;
        }
    }

    int add_item(float* item, bool item_normalized, int id) {
        TRY_CATCH_RETURN_INT_BLOCK({
            if (get_current_count() >= get_max_elements()) {
                return RESULT_ITEM_CANNOT_BE_INSERTED_INTO_THE_VECTOR_SPACE;
            }            
            if ((data_must_be_normalized == true) && (item_normalized == false)) {
                normalize_array(item);                
            }
            int current_id = id != -1 ? id : incremental_id++;             
            appr_alg->addPoint(item, current_id);                
        });
    }

    int hasId(int id) {
    	TRY_CATCH_RETURN_INT_BLOCK({
    		int label_c;
			auto search = (appr_alg->label_lookup_.find(id));
			if (search == (appr_alg->label_lookup_.end()) || (appr_alg->isMarkedDeleted(search->second))) {
				return RESULT_ID_NOT_IN_INDEX;
			}
		});
    }

    int getDataById(int id, float* data, int dim) {
    	TRY_CATCH_RETURN_INT_BLOCK({
			int label_c;
			auto search = (appr_alg->label_lookup_.find(id));
			if (search == (appr_alg->label_lookup_.end()) || (appr_alg->isMarkedDeleted(search->second))) {
				return RESULT_ID_NOT_IN_INDEX;
			}
			label_c = search->second;
			char* data_ptrv = (appr_alg->getDataByInternalId(label_c));
			float* data_ptr = (float*) data_ptrv;
			for (int i = 0; i < dim; i++) {
				data[i] = *data_ptr;
				data_ptr += 1;
			}
		});
    }

    float compute_similarity(float* vector1, float* vector2) {
    	float similarity;
        try {
        	similarity = (appr_alg->fstdistfunc_(vector1, vector2, (appr_alg -> dist_func_param_)));
        } catch (...) {
        	similarity = NAN;
        }
    	return similarity;
    }

    int knn_query(float* input, bool input_normalized, int k, int* indices /* output */, float* coefficients /* output */) {
        std::priority_queue<std::pair<dist_t, hnswlib::labeltype >> result;
        TRY_CATCH_RETURN_INT_BLOCK({
            if ((data_must_be_normalized == true) && (input_normalized == false)) {
                normalize_array(input);
            }
            result = appr_alg->searchKnn((void*) input, k);
            if (result.size() != k)
                return RESULT_QUERY_CANNOT_RETURN;
            for (int i = k - 1; i >= 0; i--) {
                auto &result_tuple = result.top();
                coefficients[i] = result_tuple.first;
                indices[i] = result_tuple.second;
                result.pop();
            }       
        });
    }

    int mark_deleted(int label) {
        TRY_CATCH_RETURN_INT_BLOCK({
        	appr_alg->markDelete(label);
        });
    }

    void resize_index(size_t new_size) {
        appr_alg->resizeIndex(new_size);
    }

    int get_max_elements() const {
        return appr_alg->max_elements_;
    }

    int get_current_count() const {
        return appr_alg->cur_element_count;
    }

    int clear_index() {
    	TRY_CATCH_NO_INITIALIZE_CHECK_AND_RETURN_INT_BLOCK({
			delete l2space;
			if (appr_alg)
				delete appr_alg;
			index_cleared = true;
        });
    }

    std::string space_name;
    int dim;
    bool index_cleared;
    bool index_initialized;
    bool data_must_be_normalized;
    std::atomic<unsigned long> incremental_id{0};
    hnswlib::HierarchicalNSW<dist_t> *appr_alg;
    hnswlib::SpaceInterface<float> *l2space;

    ~Index() {
        clear_index();
    }
};

//An Array of pointer to structure
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>

#define MAX_PREVS 3

struct Tensor *transpose(struct Tensor *self);
struct Tensor *reshape(struct Tensor *self, int *shape, int ndim); 
struct Tensor *flatten(struct Tensor *self);

typedef union
{
    float* float32;
    double* float64;
    int32_t* int32;
    int64_t* int64;
    void* raw_data;
} Data;

typedef union
{
    float *float32;
    double *float64;
}Grad;

typedef enum{
    FLOAT32,
    FLOAT64,
    INT32,
    INT64
}DType;

typedef enum{
    ADD,
    SUM,
    SUB,
    MUL,
    MATMUL,
    RELU,
    LEAKY_RELU,
    SIGMOID,
    TANH,
    MEAN,
    SOFTMAX,
    DIV,
    POW,
    EXP,
    MSE
}Op;

typedef enum{
    true,
    false
}bool;

typedef struct Tensor{
    Data data;
    DType dtype;
    double extra;
    int *dims;
    int ndim;
    int size;
    Op op;
    Grad grad;
    struct Tensor * prevs[MAX_PREVS];
    bool requires_grad;
    int num_prevs;
    struct Tensor *(*T)(struct Tensor *self);
    struct Tensor *(*reshape)(struct Tensor *self, int *shape, int ndim);
    struct Tensor *(*flatten)(struct Tensor *self);
}Tensor;

static size_t dtype_size(DType dtype){
    switch(dtype){
        case FLOAT32: return sizeof(float);
        case FLOAT64: return sizeof(double);
        case INT32: return sizeof(int32_t);
        case INT64: return sizeof(int64_t);
        default: return 0;
    }
}

static int total_size(int * dims, int ndim){
    int size=1;
    for(int i=0; i<ndim; i++){
        size *= dims[i];
    }
    return size;
}
static int *copy_dims(const int *dims, int ndim){
    int *new_dims = (int *)malloc(sizeof(int)*ndim);
    if(!new_dims) return NULL;
    memcpy(new_dims, dims, sizeof(int)*ndim);
    return new_dims;
}

//Memory Management
void t_free(Tensor* t){
    if(t == NULL)return;

    if(t->dims) free(t->dims);

    switch (t->dtype)
    {
    case FLOAT32:
        if (t->data.float32) free(t->data.float32);
        if(t->grad.float32) free(t->grad.float32);
        break;

    case FLOAT64:
        if (t->data.float64) free(t->data.float64);
        if(t->grad.float64) free(t->grad.float64);
        break;

    case INT32:
        if (t->data.int32) free(t->data.int32);
        break;

    case INT64:
        if (t->data.int64) free(t->data.int64);
        break;
    }
    free(t);
}

static int T_index(int index, int rows, int cols){
    int row = index / cols;
    int col = index % cols;
    return col * rows + row;
}

static int Reshape_index(int index, int rows, int cols){
    int row = index / cols;
    int col = index % cols;
    return row * cols + col;
}

static int shape_index(int index, int rows, int cols){
    int row =index/cols;
    int col = index % cols;
    return row * cols + col;
}

static void grad_mem_init(Tensor * self){
    if (self->dtype == FLOAT32){
        if(!self->requires_grad){
            self->grad.float32 = (float *)calloc(self->size, sizeof(float));
            if(!self->grad.float32){
                fprintf(stderr, "Memory allocation for grad failed (FLOAT32)\n");
                t_free(self);
                return;
            }
        }else{
            self->grad.float32 = NULL;
        }
    }else if(self->dtype == FLOAT64){
        if(!self->requires_grad){
            self->grad.float64 = (double *)calloc(self->size, sizeof(double));
            if(!self->grad.float64){
                fprintf(stderr, "Memory allocation for grad failed (FLOAT64)\n");
                t_free(self);
                return;
            }
        }else{
            self->grad.float64 = NULL;
        }
    }else{
        fprintf(stderr, "Unsupported dtype for grad memory allocation\n");
        t_free(self);
        return;
    }
}

Tensor * tensor(void * data, DType dtype, int * dims, int ndim, bool requires_grad){
    if(!dims || ndim <= 0) return NULL;
    //allocate memory for the tensor structure
    Tensor *t = (Tensor *)malloc(sizeof(Tensor));
    if(!t){
        fprintf(stderr, "Memory allocation for tensor failed\n");
        return NULL;
    }
    t->dtype = dtype;
    t->size = total_size(dims, ndim);
    t->ndim = ndim;
    t->extra = 0;
    t->requires_grad = requires_grad;
    t->op = -1;
    t->num_prevs = 0;
    t->T=transpose;
    t->reshape = reshape;
    t->flatten = flatten;

    t->dims = copy_dims(dims, ndim);
    if(!t->dims){
        fprintf(stderr, "Memory allocation for dims failed\n");
        t_free(t);
        return NULL;
    }

    switch(dtype){
        case FLOAT32:
            t->data.float32 = (float*) calloc(t->size , sizeof(float));
            if(!t->data.float32){
                fprintf(stderr, "Memory allocation for data failed\n");
                t_free(t);
                return NULL;
            }
            if(data){
                memcpy(t->data.float32, data, t->size*sizeof(float));
            }
            grad_mem_init(t);
            break;
        case FLOAT64:
            t->data.float64 = (double*) calloc(t->size, sizeof(double));
            if(!t->data.float64){
                fprintf(stderr, "Memory allocation for data failed\n");
                t_free(t);
                return NULL;
            }
            if(data){
                memcpy(t->data.float64, data, t->size*sizeof(double));
            }
            grad_mem_init(t);
            break;
        case INT32:
            t->data.int32 = (int32_t*) calloc(t->size, sizeof(int32_t));
            if(!t->data.int32){
                fprintf(stderr, "Memory allocation for data failed\n");
                t_free(t);
                return NULL;
            }
            if(data){
                memcpy(t->data.int32, data, t->size*sizeof(int32_t));
            }
            break;
        case INT64:
            t->data.int64 = (int64_t*) calloc(t->size, sizeof(int64_t));
            if(!t->data.int64){
                fprintf(stderr, "Memory allocation for data failed\n");
                t_free(t);
                return NULL;
            }
            if(data){
                memcpy(t->data.int64, data, t->size*sizeof(int64_t));
            }
            break;
        default:
            fprintf(stderr, "Unsupported data type\n");
            t_free(t);
            return NULL;
    }
    return t;
}

Tensor * transpose(Tensor *self){
    if(!self || self->ndim != 2){
        fprintf(stderr, "Cannot transpose a tensor with %d dimensions\n", self->ndim);
        return NULL;
    }

    int rows = self->dims[0];
    int cols = self->dims[1];
    
    int new_dims[2] = {cols, rows};

    Tensor * t = tensor(NULL, self->dtype, new_dims, 2, self->requires_grad);
    if(!t) return NULL;

    switch (self->dtype){
        case FLOAT32:
            for (int i = 0; i < self->size; i++){
                int idx = shape_index(i,rows, cols);
                int t_idx = T_index(i, rows, cols);
                t->data.float32[t_idx] = self->data.float32[idx];
                if(!self->requires_grad){
                    t->grad.float32[t_idx]=self->grad.float32[idx];
                }
            }
            break;
        case FLOAT64:
            for (int i = 0; i < self->size; i++){
                int idx = shape_index(i, rows, cols);
                int t_idx = T_index(i, rows, cols);
                t->data.float64[t_idx] = self->data.float64[idx];
                if(!self->requires_grad){
                    t->grad.float64[t_idx] = self->grad.float64[idx];
                }
            }
            break;
        case INT32:
            for (int i = 0; i < self->size; i++){
                int idx = shape_index(i, rows, cols);
                int t_idx = T_index(i, rows, cols);
                t->data.int32[t_idx] = self->data.int32[idx];
            }
            break;
        case INT64:
            for (int i = 0; i < self->size; i++){
                int idx = shape_index(i, rows, cols);
                int t_idx = T_index(i, rows, cols);
                t->data.int64[t_idx] = self->data.int64[idx];
            }
            break;
        default:
            t_free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
    }
    return t;
}

Tensor * reshape(Tensor *self, int *dims, int ndim){
    if(!self || self->ndim!= 2){
        fprintf(stderr, "Cannot reshape a tensor with %d dimensions\n", self->ndim);
        return NULL;
    }

    int size = total_size(dims, ndim);
    if(size!= self->size){
        fprintf(stderr, "Reshaping tensor of size %d to %d is not possible\n", self->size, size);
        return NULL;
    }

    Tensor * t = tensor(NULL, self->dtype, dims, ndim, self->requires_grad);
    if(!t) return NULL;

    int old_rows = self->dims[0];
    int old_cols = self->dims[1];


    int new_rows = t->dims[0];
    int new_cols = t->dims[1];

    switch (self->dtype){
        case FLOAT32:
            for (int i = 0; i < size; i++){
                int s_idx = Reshape_index(i, new_rows, new_cols);
                int idx = shape_index(i, old_rows, old_cols);
                t->data.float32[s_idx] = self->data.float32[idx];
                if(!self->requires_grad){
                    t->grad.float32[s_idx] = self->grad.float32[idx];
                }
            }
            break;
        case FLOAT64:
            for (int i = 0; i < size; i++){
                int s_idx = Reshape_index(i, new_rows, new_cols);
                int idx = shape_index(i, old_rows, old_cols);
                t->data.float64[s_idx] = self->data.float64[idx];
                if(!self->requires_grad){
                    t->grad.float64[s_idx] = self->grad.float64[idx];
                }
            }
            break;
        case INT32:
            for (int i = 0; i < size; i++){
                int s_idx = Reshape_index(i, new_rows, new_cols);
                int idx = shape_index(i, old_rows, old_cols);
                t->data.int32[s_idx] = self->data.int32[idx];
            }
            break;
        case INT64:
            for (int i = 0; i < size; i++){
                int s_idx = Reshape_index(i, new_rows, new_cols);
                int idx = shape_index(i, old_rows, old_cols);
                t->data.int64[s_idx] = self->data.int64[idx];
            }
            break;
        default:
            t_free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
    }
    return t;
}

Tensor * flatten(Tensor * self){
    if(!self || self->ndim!= 2){
        fprintf(stderr, "Cannot flatten a tensor with %d dimensions\n", self->ndim);
        return NULL;
    }

    int size = total_size(self->dims, self->ndim);
    if(size!= self->size){
        fprintf(stderr, "Flattening tensor of size %d to %d is not possible\n", self->size, size);
        return NULL;
    }

    Tensor * t = tensor(NULL, self->dtype, &size, 1, self->requires_grad);
    if(!t) return NULL;

    int rows = self->dims[0];
    int cols = self->dims[1];

    switch (self->dtype){
        case FLOAT32:
            for (int i = 0; i < size; i++){
                int idx = shape_index(i, rows, cols);
                t->data.float32[i] = self->data.float32[idx];
                if(!self->requires_grad){
                    t->grad.float32[i] = self->grad.float32[idx];
                }
            }
            break;
        case FLOAT64:
            for (int i = 0; i < size; i++){
                int idx = shape_index(i, rows, cols);
                t->data.float64[i] = self->data.float64[idx];
                if(!self->requires_grad){
                    t->grad.float64[i] = self->grad.float64[idx];
                }
            }
            break;
        case INT32:
            for (int i = 0; i < size; i++){
                int idx = shape_index(i, rows, cols);
                t->data.int32[i] = self->data.int32[idx];
            }
            break;
        case INT64:
            for (int i = 0; i < size; i++){
                int idx = shape_index(i, rows, cols);
                t->data.int64[i] = self->data.int64[idx];
            }
            break;
        default:
            t_free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;  
    }
    return t;
}

Tensor * eye(DType dtype,int dim, bool requires_grad){
    Tensor *t = tensor(NULL, dtype, (int[]){dim, dim}, (int)2, requires_grad);
    if(!t) return NULL;

    t->size = total_size(t->dims, t->ndim);
    switch (dtype){
        case FLOAT32:
            for (int i = 0; i < t->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                t->data.float32[idx] = (i / t->dims[1]) == (i % t->dims[1])? 1.0f : 0.0f;
            }
            break;
        case FLOAT64:
            for (int i = 0; i < t->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                t->data.float64[idx] = (i / t->dims[1]) == (i % t->dims[1])? 1.0 : 0.0;
            }
            break;
        case INT32:
            for (int i = 0; i < t->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                t->data.int32[idx] = (i / t->dims[1]) == (i % t->dims[1])? 1 : 0;
            }
            break;
        case INT64:
            for(int i=0; i<t->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                    t->data.int64[idx] = (i / t->dims[1]) == (i % t->dims[1])? 1 : 0;
            }
            break;
        default:
            t_free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
    }
    return t;
}

Tensor * zeros(DType dtype, int * dims, int ndim, bool requires_grad){
    Tensor *t = tensor(NULL, dtype, dims, ndim, requires_grad);
    if(!t)return NULL;
    switch(dtype){
        case FLOAT32:
            for (int i = 0; i < t->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
               t->data.float32[idx] = 0.0f;
            }
            break;
        case FLOAT64:
            for(int i=0; i<t->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                t->data.float64[idx] = 0.0;
            }
            break;
        case INT32:
            for(int i=0; i<t->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                t->data.int32[idx] = 0;
            }
            break;
        case INT64:
            for(int i=0; i<t->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                t->data.int64[idx] = 0;
            }
            break;
        default:
            t_free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
    }
    return t;
}
    
Tensor * ones(DType dtype, int * dims, int ndim, bool requires_grad) {
    Tensor *t = tensor(NULL, dtype, dims, ndim, requires_grad);
    if(!t) return NULL;
    //Fill with ones based on dtype
    switch (dtype){
        case FLOAT32:
            for (int i = 0; i < t->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                t->data.float32[idx] = 1.0f;
            } 
            break;
        case FLOAT64:
            for (int i = 0; i < t->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                t->data.float64[idx] = 1.0;
            } 
            break;
        case INT32:
            for (int i = 0; i < t->size; i++) {
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                t->data.int32[idx] = 1;
            }
            break;
        case INT64:
            for (int i = 0; i < t->size; i++) {
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                t->data.int64[idx] = 1;
            }
            break;
        default:
            free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
    }
    return t;
}

/*
In PyTorch, the function torch.randn() generates random numbers from a normal (Gaussian) distribution with a mean of 0 and a standard deviation of 1. The values typically range from around -3 to 3, but there’s no strict bound because normal distribution tails extend infinitely.

Here’s how it works:

	•	The values are centered around 0, with most numbers lying within approximately ±3 due to the distribution’s properties.
	•	About 68% of values will be within ±1 (one standard deviation from the mean).
	•	About 95% will be within ±2.
	•	Around 99.7% will fall within ±3.
*/

Tensor * randn(DType dtype, int *dims, int ndim, bool requires_grad){
    Tensor *t = tensor(NULL, dtype, dims, ndim, requires_grad);
    if (!t) return NULL;
    srand(time(NULL));
    switch (dtype){
        case FLOAT32:
            for (int i = 0; i < t->size; i++){
                float u1 = (float)rand() / (float)RAND_MAX;
                float u2 = (float)rand() / (float)RAND_MAX;
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                t->data.float32[idx] =(float) sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
            }
            break;
        case FLOAT64:
            for (int i = 0; i < t->size; i++){
                double u1 = (double)rand() / (double)RAND_MAX;
                double u2 = (double)rand() / (double)RAND_MAX;
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                t->data.float64[idx] =(double) sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
            }
            break;
        case INT32:
        case INT64:
            free(t);
            fprintf(stderr, " \"randn\" not implemented for \'int\' dtype \n"); 
            return NULL;
            break;
        default:
            free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
    }
    return t;
}

Tensor * randd(DType dtype, int * dims, int ndim, bool requires_grad){
    Tensor *t = tensor(NULL, dtype, dims, ndim, requires_grad);
    if (!t) return NULL;  
    srand(time(NULL));
    switch (dtype){
        case FLOAT32:
            for (int i = 0; i < t->size; i++) {
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                t->data.float32[idx] = (float)rand() / (float)RAND_MAX;
                }
            break;
        case FLOAT64:
            for (int i = 0; i < t->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                t->data.float64[idx] = (double)rand() / (double)RAND_MAX;
                }
            break;
        case INT32:
        case INT64:
            t_free(t);
            fprintf(stderr," \"randd\" not implemented for \'int\' dtype \n"); 
            return NULL;
            // break;
        default:
            free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
    }
    return t;    
}

void grad_init(Tensor * self){
    if(!self) return;

    if(self->dtype == FLOAT64){
        if(!self->requires_grad){
            for (int i = 0; i < self->size; i++){
                int idx = shape_index(i, self->dims[0], self->dims[1]);
                self->grad.float64[i] = 1.0;
            }
        }
    }
    if(self->dtype == FLOAT32){
        if(!self->requires_grad){
            for (int i = 0; i < self->size; i++){
                int idx = shape_index(i, self->dims[0], self->dims[1]);
                self->grad.float32[i] = 1.0f;
            }
            // memset(self->grad.float32, 1.0f, self->size*sizeof(float));
        }
    }
}

// element-wise addition
Tensor * add(Tensor * t1, Tensor * t2){
    if (!t1 || !t2) return NULL;
    if(t1->ndim != t2->ndim || t1->dtype != t2->dtype) return NULL;
    for(int i=0; i<t1->ndim; i++){
        if(t1->dims[i] != t2->dims[i]) return NULL;
    }

    int require_grad = (!t1->requires_grad || !t2->requires_grad) ? true : false;
    Tensor * t = tensor(NULL, t1->dtype, t1->dims, t1->ndim, require_grad);

    switch(t1->dtype){
        case FLOAT32:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                int idx1 = shape_index(i, t1->dims[0], t1->dims[1]);
                int idx2 = shape_index(i, t2->dims[0], t2->dims[1]);
                t->data.float32[idx] = t1->data.float32[idx1] + t2->data.float32[idx2];
            }
            
            break;
        case FLOAT64:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                int idx1 = shape_index(i, t1->dims[0], t1->dims[1]);
                int idx2 = shape_index(i, t2->dims[0], t2->dims[1]);
                t->data.float64[idx] = t1->data.float64[idx1] + t2->data.float64[idx2];
            }
            break;
        case INT32:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                int idx1 = shape_index(i, t1->dims[0], t1->dims[1]);
                int idx2 = shape_index(i, t2->dims[0], t2->dims[1]);
                t->data.int32[idx] = t1->data.int32[idx1] + t2->data.int32[idx2];
            }
            break;
        case INT64:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                int idx1 = shape_index(i, t1->dims[0], t1->dims[1]);
                int idx2 = shape_index(i, t2->dims[0], t2->dims[1]);
                t->data.int64[idx] = t1->data.int64[idx1] + t2->data.int64[idx2];
            }
            break;
        default:
            t_free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
    }
    
    t->op = ADD;
    t->prevs[0] = t1;
    t->prevs[1] = t2;
    t->num_prevs = 2;
    return t;
}

void add_backward(Tensor * out){
    if(!out) return;
    if(!out->prevs[0]->requires_grad){
        switch (out->dtype){
            case FLOAT32:
                for(int i=0; i<out->size; i++){
                    int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                    int out_idx = shape_index(i, out->dims[0], out->dims[1]);
                    out->prevs[0]->grad.float32[idx] += (float)1.0 * out->grad.float32[out_idx];
                }
                break;
            case FLOAT64:
                for(int i=0; i<out->size; i++){
                    int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                    int out_idx = shape_index(i, out->dims[0], out->dims[1]);
                    out->prevs[0]->grad.float64[idx] += (double)1.0 * out->grad.float64[out_idx];
                }
                break;
            default:
                t_free(out);
                fprintf(stderr, "Unsupported data type \n");
                return;
        }
    }
    if(out->prevs[1]->requires_grad==true){
        switch (out->dtype){
            case FLOAT32:
                for(int i=0; i<out->size; i++){
                    int idx = shape_index(i, out->prevs[1]->dims[0], out->prevs[1]->dims[1]);
                    int out_idx = shape_index(i, out->dims[0], out->dims[1]);
                    out->prevs[1]->grad.float32[idx] += (float)1.0 * out->grad.float32[out_idx];
                }
                break;
            case FLOAT64:
                for(int i=0; i<out->size; i++){
                    int idx = shape_index(i, out->prevs[1]->dims[0], out->prevs[1]->dims[1]);
                    int out_idx = shape_index(i, out->dims[0], out->dims[1]);
                    out->prevs[1]->grad.float64[idx] += (double)1.0 * out->grad.float64[out_idx];
                }
                break;
            default:
                t_free(out);
                fprintf(stderr, "Unsupported data type \n");
                return;
        }
    }
}

//element-wise subtraction
Tensor * sub(Tensor * t1, Tensor * t2){
    if(!t1 || !t2) return NULL;
    if(t1->ndim != t2->ndim || t1->dtype != t2->dtype){
        return NULL;
    }
    for(int i =0; i < t1->ndim; i++){
        if(t1->dims[i]!= t2->dims[i]) return NULL;
    }

    int require_grad = (!t1->requires_grad || !t2->requires_grad) ? true : false;
    Tensor * t = tensor(NULL, t1->dtype, t1->dims, t1->ndim, require_grad);
    switch(t1->dtype){
        case FLOAT32:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                int idx1 = shape_index(i, t1->dims[0], t1->dims[1]);
                int idx2 = shape_index(i, t2->dims[0], t2->dims[1]);
                t->data.float32[i] = t1->data.float32[i] - t2->data.float32[i];
            }
            break;
        case FLOAT64:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                int idx1 = shape_index(i, t1->dims[0], t1->dims[1]);
                int idx2 = shape_index(i, t2->dims[0], t2->dims[1]);
                t->data.float64[idx] = t1->data.float64[idx1] - t2->data.float64[idx2];
            }
            break;
        case INT32:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                int idx1 = shape_index(i, t1->dims[0], t1->dims[1]);
                int idx2 = shape_index(i, t2->dims[0], t2->dims[1]);
                t->data.int32[idx] = t1->data.int32[idx1] - t2->data.int32[idx2];
            }
            break;
        case INT64:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                int idx1 = shape_index(i, t1->dims[0], t1->dims[1]);
                int idx2 = shape_index(i, t2->dims[0], t2->dims[1]);
                t->data.int64[idx] = t1->data.int64[idx1] - t2->data.int64[idx2];
            }
        default:
            t_free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
    }

    t->op = SUB;
    t->prevs[0] = t1;
    t->prevs[1] = t2;
    t->num_prevs = 2;
    return t;
}

void sub_backward(Tensor * out){
    if(!out) return;
    if(out->prevs[0]->requires_grad==true){
        switch (out->dtype){
            case FLOAT32:
                for(int i=0; i<out->size; i++){
                    int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                    int out_idx = shape_index(i, out->dims[0], out->dims[1]);
                    out->prevs[0]->grad.float32[idx] += (float)1.0 * out->grad.float32[out_idx];
                }
                break;
            case FLOAT64:
                for(int i=0; i<out->size; i++){
                    int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                    int out_idx = shape_index(i, out->dims[0], out->dims[1]);
                    out->prevs[0]->grad.float64[idx] += (double)1.0 * out->grad.float64[out_idx];
                }
                break;           
            default:
                t_free(out);
                fprintf(stderr, "Unsupported data type \n");
                return;
        }
    }
    if(out->prevs[1]->requires_grad==true){
        switch (out->dtype){
            case FLOAT32:
                for(int i=0; i<out->size; i++){
                    int idx = shape_index(i, out->prevs[1]->dims[0], out->prevs[1]->dims[1]);
                    int out_idx = shape_index(i, out->dims[0], out->dims[1]);
                    out->prevs[1]->grad.float32[idx] += (float)-1.0 * out->grad.float32[out_idx];
                }
                break;
            case FLOAT64:
                for(int i=0; i<out->size; i++){
                    int idx = shape_index(i, out->prevs[1]->dims[0], out->prevs[1]->dims[1]);
                    int out_idx = shape_index(i, out->dims[0], out->dims[1]);
                    out->prevs[1]->grad.float64[idx] += (double)-1.0 * out->grad.float64[out_idx];
                }
                break;
            default:
                t_free(out);
                fprintf(stderr, "Unsupported data type \n");
                return;
        }
    }
}

//element-wise multiplication
Tensor * mul(Tensor *t1, Tensor *t2){
    if(!t1 || !t2) return NULL;
    if(t1->ndim != t2->ndim || t1->dtype != t2->dtype){
        return NULL;
    }
    for(int i =0; i<t1->ndim; i++){
        if(t1->dims[i] != t2->dims[i]){
            return NULL;
        }
    }

    int require_grad = (!t1->requires_grad || !t2->requires_grad) ? true : false;
    Tensor * t = tensor(NULL, t1->dtype, t1->dims, t1->ndim, require_grad);
    switch(t1->dtype){
        case FLOAT32:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                int idx1 = shape_index(i, t1->dims[0], t1->dims[1]);
                int idx2 = shape_index(i, t2->dims[0], t2->dims[1]);
                t->data.float32[idx] = t1->data.float32[idx1] * t2->data.float32[idx2];
            }
            break;
        case FLOAT64:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                int idx1 = shape_index(i, t1->dims[0], t1->dims[1]);
                int idx2 = shape_index(i, t2->dims[0], t2->dims[1]);
                t->data.float64[idx] = t1->data.float64[idx1] * t2->data.float64[idx2];
            }
            break;
        case INT32:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                int idx1 = shape_index(i, t1->dims[0], t1->dims[1]);
                int idx2 = shape_index(i, t2->dims[0], t2->dims[1]);
                t->data.int32[idx] = t1->data.int32[idx1] * t2->data.int32[idx2];
            }
            break;
        case INT64:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
                int idx1 = shape_index(i, t1->dims[0], t1->dims[1]);
                int idx2 = shape_index(i, t2->dims[0], t2->dims[1]);
                t->data.int64[idx] = t1->data.int64[idx1] * t2->data.int64[idx2];
            }
        default:
            free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
    }

    t->op = MUL;
    t->prevs[0] = t1;
    t->prevs[1] = t2;
    t->num_prevs = 2;
    return t;
}

void mul_backward(Tensor * out){
    if(!out) return;
    if(out->prevs[0]->requires_grad==true){
        switch (out->dtype){
            case FLOAT32:
                for(int i=0; i<out->size; i++){
                    int idx1 = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                    int idx2 = shape_index(i, out->prevs[1]->dims[0], out->prevs[1]->dims[1]);
                    int idx = shape_index(i, out->dims[0], out->dims[1]);
                    out->prevs[0]->grad.float32[idx1] += out->prevs[1]->data.float32[idx2] * out->grad.float32[idx];
                }
                break;
            case FLOAT64:
                for(int i=0; i<out->size; i++){
                    int idx1 = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                    int idx2 = shape_index(i, out->prevs[1]->dims[0], out->prevs[1]->dims[1]);
                    int idx = shape_index(i, out->dims[0], out->dims[1]);
                    out->prevs[0]->grad.float64[idx1] += out->prevs[1]->data.float64[idx2] * out->grad.float64[idx];
                }
                break;
            default:
                t_free(out);
                fprintf(stderr, "Unsupported data type \n");
                return;
        }
    }
    if(out->prevs[1]->requires_grad == true){
        switch (out->dtype){
            case FLOAT32:
                for(int i=0; i<out->size; i++){
                    int idx1 = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                    int idx2 = shape_index(i, out->prevs[1]->dims[0], out->prevs[1]->dims[1]);
                    int idx = shape_index(i, out->dims[0], out->dims[1]);
                    out->prevs[1]->grad.float32[idx2] += out->prevs[0]->data.float32[idx1] * out->grad.float32[idx];
                }
                break;
            case FLOAT64:
                for(int i=0; i<out->size; i++){
                    int idx1 = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                    int idx2 = shape_index(i, out->prevs[1]->dims[0], out->prevs[1]->dims[1]);
                    int idx = shape_index(i, out->dims[0], out->dims[1]);
                    out->prevs[1]->grad.float64[idx2] += out->prevs[0]->data.float64[idx1] * out->grad.float64[idx];
                }
                break;
            default:
                t_free(out);
                fprintf(stderr, "Unsupported data type \n");
                return;
        }
    }
}

//dot preoduct
Tensor * matmul(Tensor *t1, Tensor *t2){
    if(!t1 || !t2) return NULL;
    int m = t1->dims[0];
    int n = t2->dims[1];
    int l = t1->dims[1];
    int dims[]=(int[]){m, n};
    if(t1->dims[1]!= t2->dims[0] || t1->dtype != t2->dtype){
        return NULL;
    }
    int require_grad = (!t1->requires_grad || !t2->requires_grad) ? true : false;
    Tensor * t = tensor(NULL, t1->dtype, dims, t1->ndim, require_grad);
    if(!t) return NULL;
    switch(t1->dtype){
        case FLOAT32:
            for(int i = 0; i < m; i++){
                for (int j = 0; j < n; j++){
                    for (int k = 0; k < l; k++)
                    {
                        t->data.float32[i*n + j] += t1->data.float32[i*l + k] * t2->data.float32[k*n + j];
                    }
                }
            }
            break;
        case FLOAT64:
            for(int i=0; i<m; i++){
                for(int j=0; j<n; j++){
                    for(int k=0; k<l; k++){
                        t->data.float64[i*n + j] += t1->data.float64[i*l + k] * t2->data.float64[k*n + j];
                    }
                }
            }
            break;
        case INT32:
            for(int i=0; i<m; i++){
                for(int j=0; j<n; j++){
                    for(int k=0; k<l; k++){
                        t->data.int32[i*n + j] += t1->data.int32[i*l + k] * t2->data.int32[k*n + j];
                    }
                }
            }
            break;
        case INT64:
            for(int i=0; i<m; i++){
                for(int j=0; j<n; j++){
                    for(int k=0; k<l; k++){
                        t->data.int64[i*n + j] += t1->data.int64[i*l + k] * t2->data.int64[k*n + j];
                    }
                }
            }
            break;
        default:
            t_free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
    }

    t->op = MATMUL;
    t->prevs[0] = t1;
    t->prevs[1] = t2;
    t->num_prevs = 2;
    return t;
}

void matmul_backward(Tensor * out){
    if(!out) return;
    int m = out->prevs[0]->dims[0];
    int n = out->prevs[1]->dims[1];
    int l = out->prevs[1]->dims[0];
    switch(out->dtype){
        case FLOAT32:
            if(out->prevs[0]->requires_grad == true){
                for(int i=0; i<m; i++){
                    for(int k=0; k<l; k++){
                        for(int j=0; j<n; j++){
                            out->prevs[0]->grad.float32[i*l + k] += out->grad.float32[i*n + j] * out->prevs[1]->data.float32[j*l + k];
                        }
                    }
                }
            }
            if(out->prevs[1]->requires_grad == true){
                for(int k=0; k<l; k++){
                    for(int j=0; j<n; j++){
                        for(int i=0; i<m; i++){
                            out->prevs[1]->grad.float32[k*n + j] += out->prevs[0]->data.float32[k*m + i] * out->grad.float32[i*n + j];
                        }
                    }
                }
            }
            break;
        case FLOAT64:
            if(out->prevs[0]->requires_grad == true){
                for(int i=0; i<m; i++){
                    for(int k=0; k<l; k++){
                        for(int j=0; j<n; j++){
                            out->prevs[0]->grad.float64[i*l + k] += out->grad.float64[i*n + j] * out->prevs[1]->data.float64[j*l + k];
                        }
                    }
                }
            }
            if(out->prevs[1]->requires_grad == true){
                for(int k=0; k<l; k++){
                    for(int j=0; j<n; j++){
                        for(int i=0; i<m; i++){
                            out->prevs[1]->grad.float64[k*n + j] += out->prevs[0]->data.float64[k*m + i] * out->grad.float64[i*n + j];
                        }
                    }
                }
            }
            break;
        default:
            t_free(out);
            fprintf(stderr, "Unsupported data type \n");
            return;
    }
}

Tensor * Div( Tensor * t1, Tensor *t2){
    if(!t1 || !t2) return NULL;
    if(t1->dtype != t2->dtype || t1->ndim != t2->ndim) return NULL;
    for (int i = 0; i < t1->ndim; i++)
    {
        if(t1->dims[i]!= t2->dims[i]) return NULL;
    }

    int require_grad = (!t1->requires_grad || !t2->requires_grad) ? true : false;
    Tensor * t = tensor(NULL, t1->dtype, t1->dims, t1->ndim, require_grad);
    if(!t) return NULL;

    switch (t1->dtype){
    case FLOAT32:
        for (int i = 0; i < t1->size; i++){
            int idx = shape_index(i, t->dims[0], t->dims[1]);
            t->data.float32[idx] = t1->data.float32[idx] / t2->data.float32[idx];
        }
        break;
    case FLOAT64:
        for (int i = 0; i < t1->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
            t->data.float64[idx] = t1->data.float64[idx] / t2->data.float64[idx];
        }
        break;
    case INT32:
        for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
            t->data.float32[idx] = t1->data.int32[idx] / t2->data.int32[idx];
        }
        break;
    case INT64:
        for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t->dims[0], t->dims[1]);
            t->data.float64[idx] = t1->data.int64[idx] / t2->data.int64[idx];
        }
        break;
    default:
        t_free(t);
        fprintf(stderr, "Unsupported data type \n");
        return NULL;
        break;
    }
    t->op=DIV;
    t->num_prevs=2;
    t->prevs[0]= t1;
    t->prevs[1]= t2;
    
    return t;
}

void Div_backward(Tensor *out){
    if(!out) return;

    switch (out->dtype){
    case FLOAT32:
        if(out->prevs[0]->requires_grad==true){
            for (int i = 0; i < out->size; i++){
                int idx = shape_index(i, out->dims[0], out->dims[1]);
                out->prevs[0]->grad.float32[idx] += out->grad.float32[idx]/out->prevs[1]->data.float32[idx]; 
            }
        }
        if(out->prevs[1]->requires_grad==true){
            for (int i = 0; i < out->size; i++){
                int idx = shape_index(i, out->dims[0], out->dims[1]);
                out->prevs[1]->grad.float32[idx] += (-out->grad.float32[idx] * out->prevs[0]->data.float32[idx]) / pow(out->prevs[1]->data.float32[idx] , 2);
            }
        }
        break;

    case FLOAT64:
        if(out->prevs[0]->requires_grad==true){
            for (int i = 0; i < out->size; i++){
                int idx = shape_index(i, out->dims[0], out->dims[1]);
                out->prevs[0]->grad.float64[idx] += out->grad.float64[idx]/out->prevs[1]->data.float64[idx]; 
            }
        }
        if(out->prevs[1]->requires_grad==true){
            for (int i = 0; i < out->size; i++){
                int idx = shape_index(i, out->dims[0], out->dims[1]);
                out->prevs[1]->grad.float64[idx] += (-out->grad.float64[idx] * out->prevs[0]->data.float64[idx]) / pow(out->prevs[1]->data.float64[idx] , 2);
            }  
        }
        break;    
    default:
        t_free(out);
        fprintf(stderr, "Unsupported data type \n");
        return;
        break;
    }
}

Tensor* Pow(Tensor *t1, double exponent){
    if(!t1)return NULL;
    int require_grad = (!t1->requires_grad)? true: false;

    Tensor * t = tensor(NULL, t1->dtype, t1->dims, t1->ndim, require_grad);
    if(!t) return NULL;
    switch(t1->dtype){
        case FLOAT32:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float32[idx] = powf(t1->data.float32[idx], (float )exponent);
            }
            break;

        case FLOAT64:
            for (int i = 0; i < t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float64[idx] = pow(t1->data.float64[idx], exponent);
            }
            break;

        case INT32:
            for (int i = 0; i < t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.int32[idx] =(int32_t)pow((double)t1->data.int32[idx], exponent);
            }
            // if (!t1->requires_grad)
            // {
            //     fprintf(stderr, "Gradient calculation not supported for integer types.\n");
            //     t_free(t);
            //     return NULL;
            // }else{
            //     t->grad.float32 = NULL;
            // }
            break;
        
        case INT64:
            for (int i = 0; i < t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.int64[idx] = (int64_t)pow((double)t1->data.int64[idx], exponent);
            }
            break;

        default:
            t_free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
            break;           
    }
    t->op=POW;
    t->num_prevs=1;
    t->prevs[0]= t1;
    t->extra = exponent;

    return t;
}

void Pow_backward(Tensor * out){
    if(!out)return;
    switch (out->dtype){
        case FLOAT32:
            if(out->prevs[0]->requires_grad==true){
                for (int i = 0; i < out->prevs[0]->size; i++){
                    int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                    out->prevs[0]->grad.float32[idx] += out->grad.float32[idx] * (float)out->extra * powf(out->prevs[0]->data.float32[idx], ((float)out->extra-1));
                }
            }
            break;

        case FLOAT64:
            if(out->prevs[0]->requires_grad==true){
                for (int i = 0; i < out->prevs[0]->size; i++){
                    int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                    out->prevs[0]->grad.float64[idx] += out->grad.float64[idx] * out->extra * pow(out->prevs[0]->data.float64[idx], (out->extra-1));
                }
            }
            break;
        default:
        t_free(out);
        fprintf(stderr, "Unsupported data type \n");
        return ;
        break;
    }
}

Tensor * Exp(Tensor *t1){
    if(!t1) return NULL;
    int require_grad = (!t1->requires_grad)? true: false;
    Tensor *t = tensor(NULL, t1->dtype, t1->dims, t1->ndim, require_grad);
    if(!t) return NULL;

    switch (t1->dtype)
    {
        case FLOAT32:
            for (int i = 0; i < t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float32[idx] = expf(t->data.float32[idx]);
            }
            break;
        case FLOAT64:
            for (int i = 0; i < t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float64[idx] = exp(t->data.float64[idx]);
            }
            break;
        case INT32:
            for (int i = 0; i < t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float32[idx] = exp((float)t->data.int32[idx]);
            }
            break;
        case INT64:
            for (int i = 0; i < t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float64[idx] = exp((double)t->data.int64[idx]);
            }
            break;
        default:
            t_free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
            break;           
    }
    t->op=EXP;
    t->num_prevs=1;
    t->prevs[0]= t1;

    return t;
}

void Exp_backward(Tensor * out){
    if(!out)return;
    switch (out->dtype){
        case FLOAT32:
            if(out->prevs[0]->requires_grad==true){
                for (int i = 0; i < out->prevs[0]->size; i++){
                    int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                    out->prevs[0]->grad.float32[idx] += out->grad.float32[idx] * expf(out->prevs[0]->data.float32[idx]);
                }
            }
            break;

        case FLOAT64:
            if(out->prevs[0]->requires_grad==true){
                for (int i = 0; i < out->prevs[0]->size; i++){
                    int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                    out->prevs[0]->grad.float64[idx] += out->grad.float64[idx] * exp(out->prevs[0]->data.float64[idx]);
                }
            }
            break;
        default:
        t_free(out);
        fprintf(stderr, "Unsupported data type \n");
        return ;
        break;
    }
}

Tensor * relu(Tensor *t1){
    if(!t1) return NULL;
    int require_grad = (!t1->requires_grad)? true: false;
    Tensor * t = tensor(NULL, t1->dtype, t1->dims, t1->ndim, require_grad);
    if(!t) return NULL;
    switch(t1->dtype){
        case FLOAT32:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float32[idx] = (t1->data.float32[idx]<0) ? 0 : (t1->data.float32[idx]);
            }
            break;
        case FLOAT64:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float64[idx] = (t1->data.float64[idx]<0) ? 0 : (t1->data.float64[idx]);
            }
            break;
        case INT32:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.int32[idx] = (t1->data.int32[idx]<0) ? 0 : (t1->data.int32[idx]);
            }
            break;
        case INT64:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.int64[idx] = (t1->data.int64[idx]<0) ? 0 : (t1->data.int64[idx]);
            }
            break;
        default:
            t_free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
    }
    t->op=RELU;
    t->prevs[0]= t1;
    t->num_prevs = 1;
    return t;
}

void relu_backward(Tensor * out){
    if(!out) return;
    if(out->prevs[0]->requires_grad == true){
        switch(out->dtype){
        case FLOAT32:
            for(int i=0; i<out->prevs[0]->size; i++){
                int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                out->prevs[0]->grad.float32[idx] += (out->data.float32[idx] < 0) ? 0 : (out->grad.float32[idx]);
            }
            break;
        case FLOAT64:
            for(int i=0; i<out->prevs[0]->size; i++){
                int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                out->prevs[0]->grad.float64[idx] += (out->data.float64[idx] < 0) ? 0 : (out->grad.float64[idx]);
            }
            break;
        default:
            t_free(out);
            fprintf(stderr, "Unsupported data type \n");
            return;
        }
    }
}

Tensor * leaky_relu(double negative_slope, Tensor *t1){
    if(!t1) return NULL;
    int require_grad = (!t1->requires_grad)? true: false;
    Tensor * t = tensor(NULL, t1->dtype, t1->dims, t1->ndim, require_grad);
    if(!t) return NULL;
    switch(t1->dtype){
        case FLOAT32:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float32[idx] = (t1->data.float32[idx]<0) ? ((float)negative_slope * t1->data.float32[idx]) : (t1->data.float32[idx]);
            }
            break;
        case FLOAT64:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float64[idx] = (t1->data.float64[idx]<0) ? (negative_slope * t1->data.float64[idx]) : (t1->data.float64[idx]);
            }
            break;
        case INT32:
            t_free(t);
            fprintf(stderr, " \"leaky_relu\" not implemented for 'int32' \n");
            return NULL;
            break;
        case INT64:
            t_free(t);
            fprintf(stderr, " \"leaky_relu\" not implemented for 'int64' \n");
            return NULL;
            break;
        default:
            t_free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
    }
    t->op=LEAKY_RELU;
    t->prevs[0] = t1;
    t->extra = negative_slope;
    t->num_prevs = 1;
    return t;
}

void leaky_relu_backward(Tensor * out){
    if(!out) return;

    if(out->prevs[0]->requires_grad == true){
        switch(out->dtype){
        case FLOAT32:
            for(int i=0; i<out->prevs[0]->size; i++){
                int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                out->prevs[0]->grad.float32[idx] += (out->data.float32[idx]<0) ? ((float)out->extra * out->grad.float32[idx]) : (out->grad.float32[idx]);
            }
            break;

        case FLOAT64:
            for(int i=0; i<out->prevs[0]->size; i++){
                int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                out->prevs[0]->grad.float64[idx] += (out->data.float64[idx]<0) ? (out->extra * out->grad.float64[idx]) : (out->grad.float64[idx]);
            }
            break;

        default:
            t_free(out);
            fprintf(stderr, "Unsupported data type \n");
            return;
        }
    }

}

Tensor * Tanh(Tensor * t1){
    if(!t1) return NULL;
    int require_grad = (!t1->requires_grad)? true : false;
    Tensor * t = tensor(NULL, t1->dtype, t1->dims, t1->ndim, require_grad);
    if(!t) return NULL;

    switch(t1->dtype){
        case FLOAT32:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float32[idx] = (exp(2*t1->data.float32[idx]) - 1) / (exp(2*t1->data.float32[idx]) + 1);
            }
            break;
        case FLOAT64:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float64[idx] = (exp(2*t1->data.float64[idx]) - 1) / (exp(2*t1->data.float64[idx]) + 1);
            }
            break;
        case INT32:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float32[idx] = (exp(2*t1->data.int32[idx]) - 1) / (exp(2*t1->data.int32[idx]) + 1);
            }
            break;
        case INT64:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float64[idx] = (exp(2*t1->data.int64[idx]) - 1) / (exp(2*t1->data.int64[idx]) + 1);
            }
            break;

        default:
            t_free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
    }

    t->prevs[0]=t1;
    t->op=TANH;
    t->num_prevs = 1;

    return t;
}

void Tanh_backward(Tensor * out){
    if(!out) return;

    if(out->prevs[0]->requires_grad==true){
        switch(out->dtype){
        case FLOAT32:
            for(int i=0; i<out->prevs[0]->size; i++){
                int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                out->prevs[0]->grad.float32[idx] += (1 - pow(out->data.float32[idx], 2)) * out->grad.float32[idx];
            }
            break;
        case FLOAT64:
            for(int i=0; i<out->prevs[0]->size; i++){
                int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                out->prevs[0]->grad.float64[idx] += (1 - pow(out->data.float64[idx], 2)) * out->grad.float64[idx];
            }
            break;

        default:
            t_free(out);
            fprintf(stderr, "Unsupported data type \n");
            return;
        }
    }
}

Tensor * Sigmoid(Tensor * t1){
    if(!t1) return NULL;
    int require_grad = (!t1->requires_grad)? true:false;
    Tensor * t = tensor(NULL, t1->dtype, t1->dims, t1->ndim, require_grad);
    if(!t) return NULL;

    switch(t1->dtype){
        case FLOAT32:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float32[idx] = 1 / (1 + exp(-t1->data.float32[idx]));
            }
            break;
        case FLOAT64:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float64[idx] = 1 / (1 + exp(-t1->data.float64[idx]));
            }
            break;
        case INT32:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float32[idx] = 1 / (1 + exp(-t1->data.int32[idx]));
            }
            break;
        case INT64:
            for(int i=0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float64[idx] = 1 / (1 + exp(-t1->data.int64[idx]));
            }
            break;
        default:
            t_free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
    }

    t->op = SIGMOID;
    t->prevs[0] = t1;
    t->num_prevs = 1;

    return t;
}

void Sigmoid_backward(Tensor * out){
    if(!out) return;

    if(out->prevs[0]->requires_grad == true){
        switch(out->prevs[0]->dtype){
            case FLOAT32:
                for(int i=0; i<out->prevs[0]->size; i++){
                    int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                    out->prevs[0]->grad.float32[idx] += out->data.float32[idx] * (1 - out->data.float32[idx]) * out->grad.float32[idx];
                }
                break;
            case FLOAT64:
                for(int i=0; i<out->prevs[0]->size; i++){
                    int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                    out->prevs[0]->grad.float64[idx] += out->data.float64[idx] * (1 - out->data.float64[idx]) * out->grad.float64[idx];
                }
                break;
            default:
                t_free(out);
                fprintf(stderr, "Unsupported data type \n");
                return;
            }
    }
}

Tensor * softmax(Tensor *t1){
    if(!t1) return NULL;
    int require_grad = (!t1->requires_grad)? true : false;
    Tensor * t = tensor(NULL, t1->dtype, t1->dims, t1->ndim, require_grad);
    if(!t) return NULL;

    float s_float = 0.0f;
    float max_val_float;
    double s_double = 0.0;
    double max_val_double;
    float *ex_float = (float *)malloc(t1->size * sizeof(float));
    double *ex_double = (double *)malloc(t1->size * sizeof(double));

    switch(t1->dtype){
        case FLOAT32:
            if(!ex_float){
                t_free(t);
                fprintf(stderr, "Memory allocation failed \n");
                return NULL;
            }

            max_val_float = t1->data.float32[0];
            for(int i = 1; i < t1->size; i++) {
                if(t1->data.float32[i] > max_val_float) {
                    max_val_float = t1->data.float32[i];
                }
            }

            for( int i = 0; i<t1->size; i++){
                ex_float[i] = expf(t1->data.float32[i] - max_val_float);
                s_float += ex_float[i];
            }

            for(int i = 0; i<t1->size; i++){
                t->data.float32[i] = ex_float[i] / s_float;
            }
            free(ex_float);
            break;

        case FLOAT64:
            if(!ex_double){
                t_free(t);
                fprintf(stderr, "Memory allocation failed \n");
                return NULL;
            }

            max_val_double = t1->data.float64[0];
            for(int i = 1; i < t1->size; i++) {
                if(t1->data.float64[i] > max_val_double) {
                    max_val_double = t1->data.float64[i];
                }
            }

            for( int i = 0; i<t1->size;i++){
                ex_double[i] = exp(t1->data.float64[i] - max_val_double);
                s_double += ex_double[i];
            }

            for(int i = 0; i<t1->size; i++){
                t->data.float64[i] = ex_double[i] / s_double;
            }
            free(ex_double);
            break;

        case INT32:
            t_free(t);
            fprintf(stderr, " \"softmax\" not implemented for 'int32' \n");
            return NULL;
            break;

        case INT64:
            t_free(t);
            fprintf(stderr, " \"softmax\" not implemented for 'int64' \n");
            return NULL;
            break;

        default:
            t_free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
    }

    t->op=SOFTMAX;
    t->prevs[0] = t1;
    t->num_prevs = 1;

    return t;
}

void softmax_backward(Tensor *out){
    if(!out) return;

    if(!out->requires_grad){
        switch(out->prevs[0]->dtype){
            case FLOAT32:
                for( int i = 0; i<out->prevs[0]->dims[0]; i++){
                    for (int j = 0; j < out->prevs[0]->dims[1]; j++)
                    {
                        if (i != j)
                        {
                            out->prevs[0]->grad.float32[i*out->prevs[0]->dims[1] + j] += -(out->prevs[0]->data.float32[i] * out->prevs[0]->data.float32[j]) * out->grad.float32[i*out->prevs[0]->dims[1] + j];
                        }else{
                            out->prevs[0]->grad.float32[i*out->prevs[0]->dims[1] + j] += out->prevs[0]->data.float32[i] * (1 - out->prevs[0]->data.float32[i]) * out->grad.float32[i*out->prevs[0]->dims[1] + j];
                        } 
                    }
                }
                break;
            case FLOAT64:
                for( int i = 0; i<out->prevs[0]->dims[0]; i++){
                    for (int j = 0; j < out->prevs[0]->dims[1]; j++)
                    {
                        if (i != j)
                        {
                            out->prevs[0]->grad.float64[i*out->prevs[0]->dims[1] + j] += -(out->prevs[0]->data.float64[i] * out->prevs[0]->data.float64[j]) * out->grad.float64[i*out->prevs[0]->dims[1] + j];
                        }else{
                            out->prevs[0]->grad.float64[i*out->prevs[0]->dims[1] + j] += out->prevs[0]->data.float64[i] * (1 - out->prevs[0]->data.float64[i]) * out->grad.float64[i*out->prevs[0]->dims[1] + j];
                        } 
                    }
                }
                break;

            default:
                t_free(out);
                fprintf(stderr, "Unsupported data type \n");
                return;
        }
    }
}

Tensor * sum(Tensor * t1){
    if(!t1) return NULL;
    int require_grad = (!t1->requires_grad)? true : false;
    Tensor *t=tensor(NULL, t1->dtype, (int[]){1}, 1, require_grad);
    if (!t) return NULL;

    switch(t1->dtype){
        case FLOAT32:
            for(int i = 0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float32[0] += t1->data.float32[idx];
            }
            break;
        case FLOAT64:
            for(int i = 0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float64[0] += t1->data.float64[idx];
            }
            break;
        case INT32:
            for(int i = 0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.int32[0] += t1->data.int32[idx];
            }
            break;
        case INT64:
            for(int i = 0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.int64[0] += t1->data.int64[idx];
            }
        default:
            free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
    }

    t->op=SUM;
    t->prevs[0] = t1;

    return t;
}

void sum_backward(Tensor * out){
    if (!out) return;
    if (!out->requires_grad)
    {
        switch (out->dtype)
        {
        case FLOAT32:
            for(int i = 0; i<out->prevs[0]->size; i++){
                int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                out->prevs[0]->grad.float32[idx] += out->grad.float32[0] * 1.0f;
            }
            break;
        case FLOAT64:
            for(int i = 0; i<out->prevs[0]->size; i++){
                    int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                    out->prevs[0]->grad.float64[idx] += out->grad.float64[0] * 1.0;
                }
            break;
        default:
            t_free(out);
            fprintf(stderr, "Unsupported data type \n");
            return;
            break;
        }
    } 
}

Tensor * mean(Tensor * t1){
    if(!t1) return NULL;
    int require_grad = (!t1->requires_grad)? true : false;
    Tensor *t = tensor(NULL, t1->dtype, (int[]){1}, 1, require_grad);
    if(!t) return NULL;

    switch(t1->dtype){
        case FLOAT32:
            for(int i = 0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float32[0] += t1->data.float32[idx];
            }
            t->data.float32[0] = t->data.float32[0]/t1->size;
            break;
        case FLOAT64:
            for(int i = 0; i<t1->size; i++){
                int idx = shape_index(i, t1->dims[0], t1->dims[1]);
                t->data.float64[0] += t1->data.float64[idx];
            }
            t->data.float64[0] = t->data.float64[0]/t1->size;
            break;
        case INT32:
            t_free(t);
            fprintf(stderr, " mean(): could not infer output dtype. Input dtype must be either a floating point or complex dtype. Got: int32 \n");
            return NULL;
            break;

        case INT64:
            t_free(t);
            fprintf(stderr, " mean(): could not infer output dtype. Input dtype must be either a floating point or complex dtype. Got: int64 \n");
            return NULL;
            break;

        default:
            t_free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
    }

    t->op=MEAN;
    t->prevs[0] = t1;
    t->num_prevs =1;

    return t;
}

void mean_backward(Tensor * out){
    if(!out) return;
    
    if(out->requires_grad == true){
        switch(out->dtype){
            case FLOAT32:
                // Ensure `out->grad` and `out->prevs[0]->grad` are allocated
                if (!out->grad.float32 || !out->prevs[0]->grad.float32) {
                    fprintf(stderr, "Gradient memory not allocated\n");
                    return;
                }
                for(int i=0; i < out->prevs[0]->size; i++){
                    int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                    out->prevs[0]->grad.float32[idx] += out->grad.float32[0] / out->prevs[0]->size;
                }
                break;
            case FLOAT64:
                // Ensure `out->grad` and `out->prevs[0]->grad` are allocated
                if (!out->grad.float64 || !out->prevs[0]->grad.float64) {
                    fprintf(stderr, "Gradient memory not allocated\n");
                    return;
                }
                for(int i=0; i < out->prevs[0]->size; i++){
                    int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                    out->prevs[0]->grad.float64[idx] += out->grad.float64[0] / out->prevs[0]->size;
                }
                break;
            default:
                t_free(out);
                fprintf(stderr, "Unsupported data type \n");
                break;
        }
    }
}

Tensor *MSELoss(Tensor * yTrue, Tensor * yPred){
    if(!yTrue ||!yPred) return NULL;
    if(yTrue->ndim != yPred->ndim || yPred->dtype != yTrue->dtype) return NULL;
    for (int i = 0; i < yPred->ndim; i++)
    {
        if(yTrue->dims[i] != yPred->dims[i]) return NULL;
    }

    int require_grad = (!yPred->requires_grad)? true : false;

    Tensor *t=tensor(NULL, yPred->dtype, (int[]){1}, 1, require_grad);
    if(!t){
        fprintf(stderr, "Memory allocation for MSE tensor failed\n");
        t_free(t);
        return NULL;
    }
    
    switch (yPred->dtype){
        case FLOAT32:
            for (int i = 0; i < yPred->size; i++){
                int idx = shape_index(i, yPred->dims[0], yPred->dims[1]);
                t->data.float32[0] += powf((yPred->data.float32[idx] - yTrue->data.float32[idx]), 2.0f);
            }
            t->data.float32[0] /= (2*yPred->size);
            
            break;
        case FLOAT64:
            for (int i = 0; i < yPred->size; i++){
                int idx = shape_index(i, yPred->dims[0], yPred->dims[1]);
                t->data.float64[0] += pow((yPred->data.float64[idx] - yTrue->data.float64[idx]), (double)2.0);
            }
            t->data.float64[0] /= (2*yPred->size);
            
            break;

        case INT32:
            t_free(t);
            fprintf(stderr, " RuntimeError: \"mse_cpu\" not implemented for 'Int' \n");
            return NULL;
            break;

        case INT64:
            t_free(t);
            fprintf(stderr, " RuntimeError: \"mse_cpu\" not implemented for 'Int' \n");
            return NULL;
            break;

        default:
            t_free(t);
            fprintf(stderr, "Unsupported data type \n");
            return NULL;
    }
    t->op=MSE;
    t->prevs[0] = yPred;
    t->prevs[1] = yTrue;
    t->num_prevs =2;
    return t;
}

void MSELoss_backward(Tensor * out){
    if(!out) return;

    if(!out->requires_grad){
        switch (out->dtype){
            case FLOAT32:
                for (int i = 0; i < out->prevs[0]->size; i++){
                    int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                    out->prevs[0]->grad.float32[idx] += (float)(1/out->prevs[0]->size) * (out->prevs[0]->data.float32[idx] - out->prevs[1]->data.float32[idx]) * out->grad.float32[0];
                }
                break;
            case FLOAT64:
                for (int i = 0; i < out->prevs[0]->size; i++){
                    int idx = shape_index(i, out->prevs[0]->dims[0], out->prevs[0]->dims[1]);
                    out->prevs[0]->grad.float64[idx] += (double)(1/out->prevs[0]->size) * (out->prevs[0]->data.float64[idx] - out->prevs[1]->data.float64[idx]) * out->grad.float64[0];
                }
                break;
            
            default:
                fprintf(stderr, "Unsupported data type \n");
                t_free(out);
                break;
        }
    }
}

void backward(Tensor * t){
    //check if loss is NULL
    if(!t) return;

    if(t->op == MUL){
        mul_backward(t);
    }else if(t->op == ADD){
        add_backward(t);
    }else if(t->op == SUB){
        sub_backward(t);
    }else if(t->op == MATMUL){
        matmul_backward(t);
    }else if(t->op == MEAN){
        mean_backward(t);
    }else if(t->op == RELU){
        relu_backward(t);
    }else if(t->op == LEAKY_RELU){
        leaky_relu_backward(t);
    }else if(t->op == TANH){
        Tanh_backward(t);
    }else if(t->op == SIGMOID){
        Sigmoid_backward(t);
    }else if(t->op == SOFTMAX){
        softmax_backward(t);
    }else if(t->op ==POW){
        Pow_backward(t);
    }else if(t->op == EXP){
        Exp_backward(t);
    }else if(t->op == DIV){
        Div_backward(t);
    }else if(t->op == SUM){
        sum_backward(t);
    }

    for(int i=0; i<t->num_prevs; i++){
        backward(t->prevs[i]);
    }
}

// print data
void print(Tensor* t){
    if(!t) return;

    printf("Tensor {\n");
    printf("  dtype: ");
    switch(t->dtype){
        case FLOAT32: printf("float32\n"); break;
        case FLOAT64: printf("float64\n"); break;
        case INT32: printf("int32\n"); break;
        case INT64: printf("int64\n"); break;
        default: printf("unknown\n"); break;
    }

    printf("  dims:  [");
    for(int i=0; i<t->ndim; i++){
        printf("%d%s", t->dims[i], i<t->ndim-1 ? ", " : "");
    }
    printf("]\n");

    int rows = t->dims[0];
    int cols = t->dims[1];

    if(t->ndim > 1){
        printf("  data:  [");
        for(int i=0; i<t->size; i++){
            int row = i/cols;
            int col = i%cols;
            int idx = shape_index(i, rows, cols);
            if (col == 0 && row == 0){printf("[");}
            if(col == 0 && row > 0){printf("\n\t  [");}
            
            switch (t->dtype){
                case FLOAT32: printf("%.4f", t->data.float32[idx]); break;
                case FLOAT64: printf("%.4lf", t->data.float64[idx]); break;
                case INT32: printf("%d", t->data.int32[idx]); break;
                case INT64: printf("%lld", t->data.int64[idx]); break;
                default: printf("Unsupported type"); break;
            }
            if(col==cols-1){
                    printf("]");
                    if(row < rows-1){
                        printf(",");
                    }
            }else{
                printf(", ");
            }
        }
        printf("]\n\n");
        if(t->dtype==FLOAT32){
            if (!t->requires_grad){
                printf("  grads: [");
                for (int i = 0; i < t->size; i++){
                    int row = i/cols;
                    int col = i%cols;
                    int idx = shape_index(i, rows, cols);
                    if (col == 0 && row == 0){printf("[");}
                    if(col == 0 && row > 0){printf("\n\t  [");}

                    if (t->grad.float32){
                        printf("%.4e", t->grad.float32[idx]);

                    }
                    if(col==cols-1){
                        printf("]");
                        if(row < rows-1){
                            printf(",");
                        }
                    }else{
                        printf(", ");
                    }
                }
                printf("]\n");
            }else {
            printf("  grads:  ");
            printf("None\n");
            }
        } else if(t->dtype==FLOAT64){
            if (!t->requires_grad){
                printf("  grads: [");
                for (int i = 0; i < t->size; i++){
                    int row = i/cols;
                    int col = i%cols;
                    int idx = shape_index(i, rows, cols);
                    if (col == 0 && row == 0){printf("[");}
                    if(col == 0 && row > 0){printf("\n\t  [");}

                    if (t->grad.float64){
                        printf("%.4e", t->grad.float64[idx]);

                    }
                    if(col==cols-1){
                        printf("]");
                        if(row < rows-1){
                            printf(",");
                        }
                    }else{
                        printf(", ");
                    }
                }
                printf("]\n");
            }else {
            printf("  grads:  ");
            printf("None\n");
            }
        }

    }else{
        printf("  data:  [");
        for(int i=0; i<t->size; i++){
            
            switch (t->dtype){
                case FLOAT32: printf("%.4f", t->data.float32[i]); break;
                case FLOAT64: printf("%.4lf", t->data.float64[i]); break;
                case INT32: printf("%d", t->data.int32[i]); break;
                case INT64: printf("%lld", t->data.int64[i]); break;
                default: printf("Unsupported type"); break;
            }
            if(i < t->size-1){
                    printf(", ");
            }
        }
        printf("]\n\n");
        if(t->dtype == FLOAT32){
            if (!t->requires_grad){
                printf("  grads: [");
                for (int i = 0; i < t->size; i++){
                    printf("%.4e", t->grad.float32[i]);
                    if(i < t->size-1){
                        printf(", ");
                    }
                }
            printf("]\n");
            }else {
            printf("  grads:  ");
            printf("None\n");
            }
        }else if(t->dtype == FLOAT64){
            if (!t->requires_grad){
                printf("  grads: [");
                for (int i = 0; i < t->size; i++){
                    printf("%.4e", t->grad.float64[i]);
                    if(i < t->size-1){
                        printf(", ");
                    }
                }
                printf("]\n");
            }else {
            printf("  grads:  ");
            printf("None\n");
            }
        }
    }
    printf("}\n");
}
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <limits.h>
#include "trie.h"
#include "theft.h"

/* Oracle implementation */
/* This is a very simple implmentation that I know is correct */
static uint64_t* oracle_init(void) {
    return calloc(USHRT_MAX+1, sizeof(uint64_t));
}
static void oracle_free(uint64_t* array) { free(array); }
static void oracle_insert(uint64_t* array, uint16_t value) {
    ++array[value];
}
static void oracle_print(uint64_t* array, FILE* fp) {
    for(int i = 0; i<=USHRT_MAX; i++) {
        while(array[i] > 0) {
            fprintf(fp,"%d ", i);
            array[i]--;
        }
    }
}
#define SHORT_ARRAY_MAX 1028
struct test_env {
    int current_size;
};
struct short_array {
    int size;
    uint16_t* array;
};
static enum theft_alloc_res
short_array_alloc(struct theft *t, void *envp, void **instance) {
    (void) envp;
    struct short_array *shorts = calloc(1,sizeof(struct short_array));
    shorts->size = SHORT_ARRAY_MAX;
    shorts->array = calloc(SHORT_ARRAY_MAX,sizeof(uint16_t));
    if (shorts->array == NULL) {
        return THEFT_ALLOC_ERROR;
    }

    for (size_t i = 0; i < shorts->size; i++) {
        uint16_t v = theft_random_bits(t,16);
        shorts->array[i] = v;
    }
    *instance = shorts;
    return THEFT_ALLOC_OK;
}
static void short_array_free(void *instancep, void *env) {
    (void)env;
    struct short_array *shorts = (struct short_array*) instancep;
    free(shorts->array);
    free(shorts);
}

static void short_array_print(FILE *f, const void *instance, void *envp) {
    (void)envp;
    uint64_t* array = oracle_init();
    const struct short_array* shorts = (const struct short_array*)instance;
    for (size_t i = 0; i < shorts->size; i++) {
        fprintf(f,"INS(%u)\n",shorts->array[i]);
        oracle_insert(array, shorts->array[i]);
    }
    fprintf(f,"EXPECT_TRIE(\"");
    oracle_print(array, f);
    fprintf(f,"\");");
    oracle_free(array);
}

static enum theft_shrink_res
short_array_shrink(struct theft *t, const void *instancep,
                   uint32_t tactic, void *envp, void **output) {
    (void)envp;
    const struct short_array *shorts = (const struct short_array*) instancep;
    struct short_array *new_array = calloc(1,sizeof(struct short_array));
    if(tactic == 0) { /* halve array */
        new_array->size = shorts->size / 2;
        new_array->array = calloc(new_array->size,sizeof(uint16_t));
        memcpy(new_array->array, shorts->array, new_array->size);
    } else if (tactic == 1) { /* chomp last element */
        new_array->size = shorts->size-1;
        new_array->array = calloc(new_array->size,sizeof(uint16_t));
        memcpy(new_array->array, shorts->array, new_array->size);
    } else {
        return THEFT_SHRINK_NO_MORE_TACTICS;
    }
    *output = new_array;
    return THEFT_SHRINK_OK;
}
static enum theft_trial_res
prop_output_matches_oracle(struct theft *t, void *arg1) {
    (void) t;
    struct short_array *valp = (struct short_array*)arg1;

    struct sTrie* triep = trie_init(); 
    uint64_t* oraclep = oracle_init();
    for(size_t i=0; i<valp->size; i++) {
        trie_insert_value(triep,valp->array[i]);
        oracle_insert(oraclep,valp->array[i]);
    }
    /* Collect results */
    char *trie_bytep;
    size_t trie_size;
    {
        FILE* trie_fp = open_memstream (&trie_bytep, &trie_size);
        trie_print_values(triep, trie_fp);
        fclose(trie_fp);
    }

    char *oracle_bytep;
    size_t oracle_size;
    {
        FILE* oracle_fp = open_memstream (&oracle_bytep, &oracle_size);
        oracle_print(oraclep, oracle_fp);
        fclose(oracle_fp);
    }
    if (trie_size != oracle_size) {
        return THEFT_TRIAL_FAIL;
    }
    /* compare trie with oracle */
    int res = memcmp(trie_bytep, oracle_bytep, oracle_size);
    if (res != 0) {
        return THEFT_TRIAL_FAIL;
    }
    
    free(trie_bytep);
    free(oracle_bytep);
    oracle_free(oraclep);
    trie_free(&triep);
    return THEFT_TRIAL_PASS;
}

int main(void) {
    struct test_env env = {};
    struct theft_type_info trie_info = {
        .alloc = short_array_alloc,
        .free = short_array_free,
        .print = short_array_print,
        .shrink = short_array_shrink,
    };

    theft_seed seed = theft_seed_of_time();
    //theft_seed seed = 0xa86453500d0d6df9;
    struct theft_run_config cfg = {
        .prop1 = prop_output_matches_oracle,
        .type_info = { &trie_info },
        .trials = 10000,
        .seed = seed,
        .hooks = {
            .env = &env,
        }
    };

    enum theft_run_res res = theft_run(&cfg);
    assert(THEFT_RUN_PASS == res);

    return (res == THEFT_RUN_PASS ? 0 : EXIT_FAILURE);
}
    

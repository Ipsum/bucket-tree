#include <stdint.h>
#include <stdio.h>

#define CACHE_LINE_SIZE 64
#define CACHE_ALIGNED __attribute__((aligned(CACHE_LINE_SIZE)))

typedef struct DataNode {
    uint16_t data[32];
} CACHE_ALIGNED DataNode_t;

typedef struct {
    struct sNode* link[8];
} CACHE_ALIGNED TravelNode_t;

typedef struct {
    uint64_t count[8];
} CACHE_ALIGNED CountNode_t;

typedef struct sNode {
    union {
        DataNode_t data;
        TravelNode_t travel;
        CountNode_t count;
    };  
} CACHE_ALIGNED Node_t;

typedef struct sTrie {
    Node_t* base_node;
    uint64_t number_of_zeros;
} Trie_t;

struct sTrie* trie_init(void);
void trie_free(struct sTrie**);
void trie_insert_value(struct sTrie* trie_ctxp, uint16_t value);
void trie_print_values(struct sTrie* triep, FILE* fp);

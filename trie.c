/* trie.c
 *
 * This data structure is loosely based on the following two papers:
 *  - https://db.in.tum.de/~leis/papers/ART.pdf
 *  - http://www.lindstaedt.com.br/estruturas/bursttries.pdf
 *
 * The problem those papers attempt to solve is the storing of key/value pairs with
 * string keys, but I adapted some of the ideas to make a datastructure that attempts 
 * to balance memory use with insertion speed. Reducing the number of cache misses when
 * walking the datastructure was also a goal.
 *
 * Example Layout:
 *     _________________Travel_____________
 *    [0]     [1] [2] [3] [4] [5] [6]    [7]
 * (3,2,1,1)  ( ) ( ) ( ) ( ) ( ) ( )   Travel
 *                           [0] [1] [2] [3] [4] [5] [6]  [7]                        
 *                           ( ) ( ) ( ) ( ) ( ) ( ) ( ) Travel
 *                                            [0] [1] [2] [3] [4] [5] [6]  [7]                        
 *                                            ( ) ( ) ( ) ( ) ( ) ( ) ( ) Travel
 *                                                             [0] [1] [2] [3] [4] [5] [6]  [7]                        
 *                                                             ( ) ( ) ( ) ( ) ( ) ( ) ( ) Travel
 *                                                                               [0]   [1]   [2]   [3]   [4]   [5]   [6]    [7]                        
 *                                                                              (0,0) (0,0) (0,0) (0,0) (0,0) (0,0) (0,0) (0,200)
 *
 *  This example is storing [1,1,2,3] and 65535 200 times.
 *  Each Node starts as a DataNode. A DataNode is a sorted list of non-zero unsigned shorts. A value of zero indicates the slot is empty.
 *  Once a DataNode is full, it is transformed into a TravelNode and its contents are copied into the correct sub-DataNode under the new TravelNode.
 *  The index for a DataNode under a TravelNode is 3 bits of the value. The bits are chosen by the current depth of the tree.
 *  A node at the maximum trie depth is a CountNode. Count nodes are just counter buckets the accumulate the amount of a specific value. In the above
 *  example, the bottom right nodes are all count nodes. The value of 200 indicates that the value 65535 was stored 200 times.
 *
 * Possible future improvements:
 *  Allocate the memory of the trie vertically instead of horizontally. This would get us some prefetching as we walk down the trie.
 *  Write an iterative solution to walking to trie in order. This would reduce the overhead incurred by recursion.
 *  The count buckets can be 4 times larger (256 bits instead of 64 bits) or the count buckets can be tagged with the values stored in them.
*/
#include "trie.h"

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#define NELEMS( _x ) \
    ( sizeof( _x ) / sizeof( *_x ) )

/* 3 bits masked per level for uint16_t */
#define MASK_N_BITS 3

const uint8_t shift_amount [] = {13, 10, 7, 4, 1, 0};
#define GEN_SHIFT(__depth) shift_amount[__depth]

const uint16_t mask_array [] = {0b1110000000000000,
                                0b0001110000000000,
                                0b0000001110000000,
                                0b0000000001110000,
                                0b0000000000001110,
                                0b0000000000000001};
#define GEN_MASK(__depth) ( mask_array[__depth] )

#define IDX_FROM_VALUE(__val, __depth) \
    ( (__val & GEN_MASK(__depth)) >> GEN_SHIFT(__depth) )

#define TRIE_MAX_DEPTH ( 5 )

#define TAG_PTR(__ptr) \
    ( (typeof(__ptr))((uintptr_t)__ptr | 0x1 ) )
#define UNTAG_PTR(__ptr) \
    ( (typeof(__ptr))((uintptr_t)__ptr & ~((uintptr_t)0x1)) )
 /* TODO MUNGE TRAVEL POINTERS
 */

typedef enum {
    NODE_TYPE_NONE,
    NODE_TYPE_DATA,
    NODE_TYPE_TRAVEL,
    NODE_TYPE_COUNT /* Note: This is a counting bucket do not use as an enum end marker */
} NodeType_t;

/* Small helper functions */
static void trie__alloc_node(struct sNode** new_nodepp) {
    *new_nodepp = (struct sNode*)aligned_alloc(CACHE_LINE_SIZE, sizeof(**new_nodepp));
    if(*new_nodepp == NULL) {
        assert(!"Node allocation failed");
    }
    bzero(*new_nodepp, sizeof(struct sNode));
}

static NodeType_t trie__determine_node_type(struct sNode* nodep, uint8_t current_depth) {
    if(current_depth == TRIE_MAX_DEPTH) {
        return NODE_TYPE_COUNT;
    }
    return (((DataNode_t*)nodep)->data[0] == 0) ? NODE_TYPE_DATA : NODE_TYPE_TRAVEL;
}
static bool trie__data_node_is_full(DataNode_t* nodep) {
    return nodep->data[0] == 0 ? false : true;
}

static uint64_t* trie__get_count_node_bucket(CountNode_t* nodep, uint16_t value) {
    return &nodep->count[IDX_FROM_VALUE(value,TRIE_MAX_DEPTH)];
}

static Node_t* trie__follow_travel_node(TravelNode_t* nodep, uint8_t depth, uint16_t value) {
    return UNTAG_PTR(nodep->link[IDX_FROM_VALUE(value,depth)]);
}

/* This happens when a data node is full and we need to transform it into a travel node */
static void trie__burst_data_node(DataNode_t* nodep, uint8_t current_depth) {
    /* This is set if the subnode needs to be re-bursted */
    DataNode_t* node_to_burst = NULL;
    /* Allocate all the memory we know we are going to need as a slab */
    Node_t* all_new_nodesp = (Node_t*)aligned_alloc(CACHE_LINE_SIZE,sizeof(*all_new_nodesp)*NELEMS(((TravelNode_t*)nodep)->link));
    if(all_new_nodesp == NULL) {
        assert(!"Travel Node allocation failed");
    }
    bzero(all_new_nodesp, sizeof(*all_new_nodesp)*NELEMS(((TravelNode_t*)nodep)->link));
    /* Copy the data into the correct subnodes and transform current node into a travel node */
    if ( current_depth == TRIE_MAX_DEPTH - 1 ) {
        /* Subbuckets will be counting buckets */
        uint16_t* current_elemp = (uint16_t*)&nodep->data[31];
        while(current_elemp != ((&nodep->data[0])-1)) {
            ++(*trie__get_count_node_bucket(&all_new_nodesp[IDX_FROM_VALUE(*current_elemp,current_depth)].count, *current_elemp));
            --current_elemp;
        }
    } else { /* subbuckets are data nodes */
        uint16_t* current_elemp = (uint16_t*)&nodep->data[31];
        while(current_elemp != ((&nodep->data[0])-1)) {
            uint16_t* elem_to_insert_atp = &all_new_nodesp[IDX_FROM_VALUE(*current_elemp,current_depth)].data.data[31];
            while(*elem_to_insert_atp != 0 && *elem_to_insert_atp <= *current_elemp) {
                --elem_to_insert_atp;
            }
            *elem_to_insert_atp = *current_elemp;
            if(elem_to_insert_atp == &all_new_nodesp[IDX_FROM_VALUE(*current_elemp,current_depth)].data.data[0]) {
                /* We have to burst all the way down
                 * Since we only bursted a single node, this case only happens if all the elementes burst out
                 * into the same node. Therefore, we only have to worry about bursting that specific node 
                 * so we just record it here */
                node_to_burst = &all_new_nodesp[IDX_FROM_VALUE(*current_elemp,current_depth)].data;
            }
            --current_elemp;
        }
    }
    for(uint8_t i=0; i<NELEMS(((TravelNode_t*)nodep)->link); i++) {
        ((TravelNode_t*)nodep)->link[i] = TAG_PTR(&all_new_nodesp[i]);
    }
    if(node_to_burst != NULL) {
        /* This tail recursive call is not ideal, but it does simplify the logic. 
         * A loop would be better, but this is a rare/pathological case so i'm
         * not very worried */
        trie__burst_data_node(node_to_burst, current_depth+1);
    }
}
    
static void trie__free_subtrie(Node_t* nodep, uint8_t depth) {
    switch(trie__determine_node_type(nodep, depth)) {
    case NODE_TYPE_DATA:
    case NODE_TYPE_COUNT:
        break;
    case NODE_TYPE_TRAVEL:
        trie__free_subtrie(UNTAG_PTR(nodep->travel.link[7]), depth+1);
        trie__free_subtrie(UNTAG_PTR(nodep->travel.link[6]), depth+1);
        trie__free_subtrie(UNTAG_PTR(nodep->travel.link[5]), depth+1);
        trie__free_subtrie(UNTAG_PTR(nodep->travel.link[4]), depth+1);
        trie__free_subtrie(UNTAG_PTR(nodep->travel.link[3]), depth+1);
        trie__free_subtrie(UNTAG_PTR(nodep->travel.link[2]), depth+1);
        trie__free_subtrie(UNTAG_PTR(nodep->travel.link[1]), depth+1);
        trie__free_subtrie(UNTAG_PTR(nodep->travel.link[0]), depth+1);
        free(UNTAG_PTR(nodep->travel.link[0]));
        break;
    default:
        assert(!"Unexpected NODE_TYPE");
    }
}
    
static void trie__print_count_node(FILE* fp, CountNode_t* nodep, uint16_t value) {
    for(uint64_t i = *trie__get_count_node_bucket(nodep, value); i>0; i--) {
        fprintf(fp,"%u ", value);
    }
}
static void trie__print_data_node(FILE* fp, DataNode_t* nodep) {
    uint16_t* current_elemp = &nodep->data[31];
    while( current_elemp != (&nodep->data[0])-1 && *current_elemp != 0 ) {
        fprintf(fp,"%u ",*current_elemp);
        --current_elemp;
    }
}
    
static void trie__print_subtrie(FILE* fp, Node_t* nodep, uint16_t value, uint8_t depth) {
    /* We need to accumulate the value as we walk the tree because count buckets don't
     *  actually store the value */
    switch(trie__determine_node_type(nodep, depth)) {
    case NODE_TYPE_COUNT:
        trie__print_count_node(fp,(CountNode_t*)nodep,value);
        trie__print_count_node(fp,(CountNode_t*)nodep,value+1);
        break;
    case NODE_TYPE_DATA:
        trie__print_data_node(fp,(DataNode_t*)nodep);
        break;
    case NODE_TYPE_TRAVEL:
        trie__print_subtrie(fp, UNTAG_PTR(nodep->travel.link[0]), value + ((uint16_t)0x0000>>(depth*MASK_N_BITS)), depth+1);
        trie__print_subtrie(fp, UNTAG_PTR(nodep->travel.link[1]), value + ((uint16_t)0x2000>>(depth*MASK_N_BITS)), depth+1);
        trie__print_subtrie(fp, UNTAG_PTR(nodep->travel.link[2]), value + ((uint16_t)0x4000>>(depth*MASK_N_BITS)), depth+1);
        trie__print_subtrie(fp, UNTAG_PTR(nodep->travel.link[3]), value + ((uint16_t)0x6000>>(depth*MASK_N_BITS)), depth+1);
        trie__print_subtrie(fp, UNTAG_PTR(nodep->travel.link[4]), value + ((uint16_t)0x8000>>(depth*MASK_N_BITS)), depth+1);
        trie__print_subtrie(fp, UNTAG_PTR(nodep->travel.link[5]), value + ((uint16_t)0xA000>>(depth*MASK_N_BITS)), depth+1);
        trie__print_subtrie(fp, UNTAG_PTR(nodep->travel.link[6]), value + ((uint16_t)0xC000>>(depth*MASK_N_BITS)), depth+1);
        trie__print_subtrie(fp, UNTAG_PTR(nodep->travel.link[7]), value + ((uint16_t)0xE000>>(depth*MASK_N_BITS)), depth+1);
        break;
    default:
        assert(!"Unexpected NODE_TYPE");
    }
}

/* Public functions */
struct sTrie* trie_init(void) {
    Node_t* base_nodep;
    struct sTrie* triep = calloc(sizeof(struct sTrie), 1);
    trie__alloc_node(&base_nodep);
    triep->base_node = base_nodep;
    triep->number_of_zeros = 0;
    return triep;
}

void trie_free(struct sTrie** triepp) {
    trie__free_subtrie((*triepp)->base_node, 0);
    free((*triepp)->base_node);
    free(*triepp);
    *triepp = NULL;
}

void trie_print_values(struct sTrie* triep, FILE* fp) {
    for(uint64_t i = triep->number_of_zeros; i > 0; i--) {
        fprintf(fp,"0 ");
    }
    trie__print_subtrie(fp, triep->base_node, 0, 0);
}

void trie_insert_value(struct sTrie* trie_ctxp, uint16_t value) {
    if (value == 0) {
        ++(trie_ctxp->number_of_zeros);
        return;
    }

    uint8_t current_depth = 0;
    Node_t* current_node = trie_ctxp->base_node;
    while(trie__determine_node_type(current_node,current_depth) == NODE_TYPE_TRAVEL) {
        current_node = trie__follow_travel_node((TravelNode_t*)current_node, 
                                                current_depth++, 
                                                value);
    }
    if (trie__determine_node_type(current_node,current_depth) == NODE_TYPE_COUNT) {
        assert(*trie__get_count_node_bucket((CountNode_t*)current_node, value) != USHRT_MAX);
        /* Simply increment the bucket count and exit */
        ++(*trie__get_count_node_bucket((CountNode_t*)current_node, value));
        return;
    } else {
        /* We must be NODE_TYPE_DATA */
        assert(trie__determine_node_type(current_node,current_depth) == NODE_TYPE_DATA);
        uint16_t* current_elemp = &((DataNode_t*)current_node)->data[31];
        while(*current_elemp != 0 && *current_elemp < value) {
            /* This will not happen because the last element will 
             * always be 0 at this point */
            assert(current_elemp != &((DataNode_t*)current_node)->data[0]);
            --current_elemp;
        }
        if(*current_elemp == 0) {
            *current_elemp = value;
        } else {
            /* We need to insert into the middle of the array, so move everything after
             * us down by one element */
            memmove(&current_node->data.data[0], &current_node->data.data[1], (uint8_t*)current_elemp - (uint8_t*)current_node->data.data);
            *current_elemp = value;
        }
        if(trie__data_node_is_full((DataNode_t*)current_node)) {
            /* Burst the node here */
            trie__burst_data_node((DataNode_t*)current_node, current_depth);
        }
    }
}
        

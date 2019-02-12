#include "trie.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
/* MinUnit test framework - see http://www.jera.com/techinfo/jtns/jtn002.html */
 #define mu_assert(message, test) do { if (!(test)) return message; } while (0)
 #define mu_run_test(test) do { char *message = test(); tests_run++; \
                                if (message) return message; } while (0)
int tests_run = 0;
/* End of MinUnit */

#define SETUP \
    struct sTrie* triep = trie_init();
#define TEARDOWN \
    trie_free(&triep); \
    return 0;

#define EXPECT_TRIE(_string) \
    char * res_bytp; \
    size_t res_size; \
    FILE* res_fp = open_memstream(&res_bytp, &res_size); \
    trie_print_values(triep, res_fp); \
    fclose(res_fp); \
    printf("%s\n%s\n",_string, res_bytp); \
    mu_assert("error, size mismatch", (sizeof(_string) -1) == res_size);\
    mu_assert("error, string mismatch", memcmp(_string,res_bytp,res_size) == 0);\
    free(res_bytp);

#define INS(_val) \
    trie_insert_value(triep, _val);

static char * test_simple() {
    SETUP
    
    INS(0)
    INS(1)
    INS(2)
    INS(3)
    INS(5)
    INS(1)
    INS(8)
    INS(0)
    INS(8)
    INS(13)
    INS(65535)
    INS(90)

    EXPECT_TRIE("0 0 1 1 2 3 5 8 8 13 90 65535 ")

    TEARDOWN
}
static char * test_simple_burst() {
    SETUP

    INS(1)
    INS(2)
    INS(3)
    INS(4)
    INS(5)
    INS(6)
    INS(7)
    INS(8)
    INS(9)
    INS(10)
    INS(11)
    INS(12)
    INS(13)
    INS(14)
    INS(15)
    INS(16)
    INS(17)
    INS(18)
    INS(19)
    INS(20)
    INS(21)
    INS(22)
    INS(23)
    INS(24)
    INS(65535)
    INS(25)
    INS(26)
    INS(27)
    INS(28)
    INS(29)
    INS(30)
    INS(31)
    INS(32)

    EXPECT_TRIE("1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 65535 ");

    TEARDOWN
}

static char * test_simple_counting_bucket() {
    SETUP

    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)
    INS(1)

    EXPECT_TRIE("1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ");

    TEARDOWN
}

static char * test_low_number_to_same_bucket_after_burst() {
    SETUP

    INS(1)
    INS(2)
    INS(3)
    INS(4)
    INS(5)
    INS(1)
    INS(2)
    INS(3)
    INS(4)
    INS(5)
    INS(1)
    INS(2)
    INS(3)
    INS(4)
    INS(5)
    INS(1)
    INS(2)
    INS(3)
    INS(4)
    INS(5)
    INS(1)
    INS(2)
    INS(3)
    INS(4)
    INS(5)
    INS(65534)
    INS(65533)
    INS(65534)
    INS(65533)
    INS(65535)
    INS(65535)
    INS(65535)
    INS(65535)
    INS(1)
    INS(2)

    EXPECT_TRIE("1 1 1 1 1 1 2 2 2 2 2 2 3 3 3 3 3 4 4 4 4 4 5 5 5 5 5 65533 65533 65534 65534 65535 65535 65535 65535 ");

    TEARDOWN
}
    
static char * all_tests() {
     mu_run_test(test_simple);
     mu_run_test(test_simple_burst);
     mu_run_test(test_simple_counting_bucket);
     mu_run_test(test_low_number_to_same_bucket_after_burst);
     return 0;
 }
int main(void) {
    char *result = all_tests();
    if (result != 0) {
        printf("%s\n", result);
    }
    else {
        printf("ALL TESTS PASSED\n");
    }
    printf("Tests run: %d\n", tests_run);

    return result != 0;
}

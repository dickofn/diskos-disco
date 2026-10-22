#include <stdlib.h>
static void test_context_init(void){ds_context=calloc(1,sizeof *ds_context);if(!ds_context)abort();pthread_mutex_init(&ds_shared.mu,NULL);pthread_cond_init(&ds_shared.wake,NULL);}

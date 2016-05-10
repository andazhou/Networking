#include "csapp.h"
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400


typedef struct cache {
    llist *start;
    llist *end;
    int storage;
} cache;

typedef struct llist {
    char* URL;
    char* content;
    struct llist *prev;
    struct llist *next;
} llist;

cache init_cache()
{
    cache *new = Malloc(sizeof(*new));
    new->start = NULL;
    new->end = NULL;
    new->storage = MAX_CACHE_SIZE;

    return new;
}



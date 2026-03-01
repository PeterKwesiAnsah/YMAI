#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define WORD 4
#define DWORD 8
#define MAX_FREE_BLOCK_SIZE 4096

uintptr_t *yami_bp = NULL;
uintptr_t *yami_heap_list = NULL;

typedef struct yami_f_blck_hdr {
  unsigned int size;
  struct yami_f_blck_hdr *next;
  struct yami_f_blck_hdr *prev;
} yami_f_blck_hdr;

// we store the recently free block
struct yami_f_blck_hdr *yami_f_hdptr = NULL;

#define MIN_BLOCK_SIZE (sizeof(yami_f_blck_hdr))

void yami_init_heap() {
  uint8_t *brk_ptr = sbrk(0);
  if ((size_t)brk_ptr % DWORD == 0) {
    yami_heap_list = (uintptr_t *)brk_ptr;
    sbrk(2 * MIN_BLOCK_SIZE);
    yami_bp = (uintptr_t *)(brk_ptr + (2 * MIN_BLOCK_SIZE));
    // start and end heap boundary blocks
    memset(yami_heap_list, '\0', (2 * MIN_BLOCK_SIZE));
    return;
  }
  sbrk(2 * MIN_BLOCK_SIZE + (DWORD - 1));
  yami_bp =
      (uintptr_t *)(((uintptr_t)brk_ptr + (2 * MIN_BLOCK_SIZE + (DWORD - 1))) &
                    ~(DWORD - 1));
  yami_heap_list = yami_bp - (2 * MIN_BLOCK_SIZE);
  // start and end heap boundary blocks
  memset(yami_heap_list, '\0', (2 * MIN_BLOCK_SIZE));
}

// extend the heap
void *yami_extend_heap() {
  assert(yami_heap_list != NULL);
  uint8_t *brk_ptr = sbrk(MAX_FREE_BLOCK_SIZE);
  if (brk_ptr == (void *)-1) {
    perror("YAMI heap extension by MAX_FREE_BLOCK_SIZE (4096) failed");
    return NULL;
  }
  struct yami_f_blck_hdr *temp = ((struct yami_f_blck_hdr *)yami_bp) - 1;
  temp->size = MAX_FREE_BLOCK_SIZE;
  if (yami_f_hdptr == NULL) {
    // add to the free list
    temp->prev = NULL;
    temp->next = NULL;
    yami_f_hdptr = temp;
    uintptr_t *yami_bp_old = yami_bp;
    memset(
        ((struct yami_f_blck_hdr *)((uintptr_t)yami_bp + MAX_FREE_BLOCK_SIZE)) -
            1,
        '\0', (MIN_BLOCK_SIZE));
    yami_bp = (uintptr_t *)((uintptr_t)yami_bp + MAX_FREE_BLOCK_SIZE);

    assert((uintptr_t)yami_bp_old % DWORD == 0);
    assert((uintptr_t)yami_bp % DWORD == 0);
    return yami_bp_old;
  }

  uintptr_t *yami_bp_old = yami_bp;
  memset(
      ((struct yami_f_blck_hdr *)((uintptr_t)yami_bp + MAX_FREE_BLOCK_SIZE)) -
          1,
      '\0', (MIN_BLOCK_SIZE));
  yami_bp = (uintptr_t *)((uintptr_t)yami_bp + MAX_FREE_BLOCK_SIZE);
  // add to the free list
  temp->prev = NULL;
  temp->next = yami_f_hdptr;
  yami_f_hdptr->prev = temp;
  yami_f_hdptr = temp;
  assert((uintptr_t)yami_bp_old % DWORD == 0);
  assert((uintptr_t)yami_bp % DWORD == 0);
  return yami_bp_old;
}

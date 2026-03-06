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
#define HEADER_BLOCK_SIZE MIN_BLOCK_SIZE
#define MIN_FREE_BLOCK_SIZE (HEADER_BLOCK_SIZE + WORD)

#define SPLITTING_FACTOR 5

void *yami_init_heap() {
  uint8_t *brk_ptr = sbrk(0);
  if ((size_t)brk_ptr % DWORD == 0) {
    yami_heap_list = (uintptr_t *)brk_ptr;
    sbrk(2 * MIN_BLOCK_SIZE);
    yami_bp = (uintptr_t *)(brk_ptr + (2 * MIN_BLOCK_SIZE));
    // start and end heap boundary blocks
    memset(yami_heap_list, '\0', (2 * MIN_BLOCK_SIZE));
    return yami_bp;
  }
  sbrk(2 * MIN_BLOCK_SIZE + (DWORD - 1));
  yami_bp =
      (uintptr_t *)(((uintptr_t)brk_ptr + (2 * MIN_BLOCK_SIZE + (DWORD - 1))) &
                    ~(DWORD - 1));
  yami_heap_list = (uintptr_t *)((uintptr_t)yami_bp - (2 * MIN_BLOCK_SIZE));
  // start and end heap boundary blocks
  memset(yami_heap_list, '\0', (2 * MIN_BLOCK_SIZE));
  return yami_bp;
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

void *yami_alloc(size_t len) {
  if (yami_heap_list == NULL) {
    yami_init_heap();
  }
  assert(yami_heap_list != NULL);
  size_t alloc_size = (HEADER_BLOCK_SIZE + len + (DWORD - 1)) & ~(DWORD - 1);
  // When we do we contact the OS for memory??
  // 1.) yami_f_hdptr is NULL
  if (yami_f_hdptr == NULL) {
    void *bp = yami_extend_heap();
    if (bp == NULL) {
      return bp;
    }
  }

  assert(yami_bp != NULL && yami_f_hdptr != NULL);
  struct yami_f_blck_hdr *yami_f_start = yami_f_hdptr;
  struct yami_f_blck_hdr *yami_f_end = NULL;

  struct yami_f_blck_hdr *cur = yami_f_hdptr;
  while (cur) {
    if (cur->size < alloc_size) {
      // we gather data for colleascing
    } else {
      // we look forward to hitting this branch
      break;
    }
    cur = cur->next;
  }
// When we do we split memory??
SPLIT:
  if (cur) {
    uintptr_t block_size = cur->size - alloc_size;
    if (block_size > MIN_FREE_BLOCK_SIZE * SPLITTING_FACTOR) {
      // we split
      // former old free block we are using to satisfy an allocate request
      struct yami_f_blck_hdr *yami_old_f_blck_hr = cur;
      // new free block we just created from splitting
      struct yami_f_blck_hdr *yami_new_f_blck_hdr =
          (struct yami_f_blck_hdr *)((uintptr_t)cur + alloc_size);

      yami_new_f_blck_hdr->size = block_size;
      yami_old_f_blck_hr->size = alloc_size;
      // cases to join a free block created from splitting
      if (yami_old_f_blck_hr->prev == NULL &&
          yami_old_f_blck_hr->next == NULL) {
        // 1) cur->prev is null, cur->next is null (only node)
        yami_f_hdptr = yami_new_f_blck_hdr;
        yami_new_f_blck_hdr->next = NULL;
        yami_new_f_blck_hdr->prev = NULL;
      } else if (cur->prev == NULL && cur->next) {
        // 2) cur->prev is null cur->next is a valid node (head)
        struct yami_f_blck_hdr *next = cur->next;
        yami_f_hdptr = yami_new_f_blck_hdr;
        next->prev = yami_new_f_blck_hdr;

        yami_new_f_blck_hdr->prev = NULL;
        yami_new_f_blck_hdr->next = next;

      } else if (cur->prev && cur->next) {
        // 3) cur->prev is a valid node, cur->next is a valid node
        struct yami_f_blck_hdr *next = cur->next;
        struct yami_f_blck_hdr *prev = cur->prev;
        prev->next = yami_new_f_blck_hdr;
        next->prev = yami_new_f_blck_hdr;

        yami_new_f_blck_hdr->prev = prev;
        yami_new_f_blck_hdr->next = next;
      } else {
        // 4) cur->prev is a valid node, cur->next is null (tail)
        struct yami_f_blck_hdr *prev = cur->prev;
        prev->next = yami_new_f_blck_hdr;

        yami_new_f_blck_hdr->prev = prev;
        yami_new_f_blck_hdr->next = NULL;
      }
      uintptr_t *bp = (uintptr_t *)(yami_old_f_blck_hr + 1);
      assert((uintptr_t)bp % WORD == 0);
      return bp;
    }

    // cases to remove an allocated header
    if (cur->prev == NULL && cur->next == NULL) {
      // 1) cur->prev is null, cur->next is null (only node)
      yami_f_hdptr = NULL;
    } else if (cur->prev == NULL && cur->next) {
      // 2) cur->prev is null cur->next is a valid node (head)
      yami_f_hdptr = cur->next;
      yami_f_hdptr->prev = NULL;
    } else if (cur->prev && cur->next) {
      // 3) cur->prev is a valid node, cur->next is a valid node
      struct yami_f_blck_hdr *next = cur->next;
      struct yami_f_blck_hdr *prev = cur->prev;
      prev->next = next;
      next->prev = prev;
    } else {
      // 4) cur->prev is a valid node, cur->next is null (tail)
      struct yami_f_blck_hdr *prev = cur->prev;
      prev->next = NULL;
    }
    uintptr_t *bp = (uintptr_t *)(cur + 1);
    assert((uintptr_t)bp % WORD == 0);
    return bp;
  }
  // When we do we coleasce memory??

  // How do we satisfy allocations that are bigger than the default
  // MAX_FREE_BLOCK_SIZE

  return NULL;
}

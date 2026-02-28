#### Y.M.A.I (Yet Another Malloc Implementation)
YMAI is a general purpose allocator with similar apis like libc malloc() and free() functions.
- It uses an double-linked list to manage free heap blocks.
- On-demand coalescing of free blocks
- First fit placing strategy

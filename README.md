#### Y.M.A.I (Yet Another Malloc Implementation)
YMAI is a general-purpose allocator with APIs similar to libc's malloc() and free() functions.
- It uses a double-linked list to manage free heap blocks.
- On-demand coalescing of free blocks
- First fit placing strategy(Memory is cheap, so why not?)

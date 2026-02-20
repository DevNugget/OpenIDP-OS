/* date = February 17th 2026 10:00 pm */
#ifndef ALIGN_H
#define ALIGN_H

#define ALIGN_UP(x, a)   (((x) + ((a) - 1)) & ~((a) - 1))
#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))

#define IS_ALIGNED(x, a) (((x) & ((a) - 1)) == 0)

#endif
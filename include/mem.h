#ifndef YUEAH_MEM
#define YUEAH_MEM

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t mem_t;

#define KiB(num) ((mem_t)(num) << 10)
#define MiB(num) ((mem_t)(num) << 20)
#define GiB(num) ((mem_t)(num) << 30)
#define ALIGN_UP_TO_POW2(num, pow)                                             \
  (((uint64_t)(num) + ((mem_t)(pow) - 1)) & (~((mem_t)(pow) - 1)))

typedef struct {
  mem_t reserved;
  mem_t committed;
  mem_t position;
  mem_t commit_position;
} mem_arena;

typedef struct {
  mem_t start_pos;
  mem_arena *arena;
} temp_arena;

#define ARENA_BASE_POS (sizeof(mem_arena))
#define ARENA_ALIGN (sizeof(void *))

mem_arena *arena_init(mem_t reserve_size, mem_t commit_size);
void arena_destroy(mem_arena *arena);
void *__arena_push_impl(mem_arena *arena, mem_t size, bool non_zero);
void arena_pop(mem_arena *arena, mem_t size);
void arena_pop_to(mem_arena *arena, mem_t pos);
void arena_clear(mem_arena *arena);

temp_arena temp_arena_begin(mem_arena *arena);
void temp_arena_end(temp_arena temp);

temp_arena arena_scratch_get(mem_arena **conflicts, uint32_t num_conflicts);
void arena_scratch_release(temp_arena scratch);

void *arena_strdup(mem_arena *arena, char *str, mem_t size);

#define arena_push(arena, size, ...)                                           \
  __arena_push_impl(arena, size,                                               \
                    (struct { bool z; }){.z = false, __VA_ARGS__}.z)

#define arena_push_struct(arena, T, ...)                                       \
  (T *)arena_push(arena, sizeof(T), __VA_ARGS__)
#define arena_push_array(arena, T, num, ...)                                   \
  (T *)arena_push(arena, sizeof(T) * (num), __VA_ARGS__)

#endif

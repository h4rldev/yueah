#include <mem.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#if defined(__linux__)
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <sys/mman.h>
#include <unistd.h>

uint32_t mem_get_page_size(void) { return (uint32_t)sysconf(_SC_PAGESIZE); }

static void *mem_reserve(uint64_t size) {
  void *ret = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ret == MAP_FAILED) {
    fprintf(stderr, "Failed to mmap memory\n");
    return NULL;
  }

  return ret;
}

static bool mem_commit(void *ptr, uint64_t size) {
  int32_t ret = mprotect(ptr, size, PROT_READ | PROT_WRITE);
  return ret == 0;
}

static bool mem_decommit(void *ptr, uint64_t size) {
  int32_t ret = mprotect(ptr, size, PROT_NONE);
  if (ret != 0) {
    fprintf(stderr, "Failed to mprotect memory\n");
    return false;
  }
  ret = madvise(ptr, size, MADV_DONTNEED);
  return ret == 0;
}

static bool mem_release(void *ptr, uint64_t size) {
  int32_t ret = munmap(ptr, size);
  return ret == 0;
}
#else
#error "MacOS & Windows are unsupported"
#endif

mem_arena *arena_init(mem_t reserve_size, mem_t commit_size) {
  uint32_t page_size = mem_get_page_size();

  reserve_size = ALIGN_UP_TO_POW2(reserve_size, page_size);
  commit_size = ALIGN_UP_TO_POW2(commit_size, page_size);

  mem_arena *arena = mem_reserve(reserve_size);

  if (!mem_commit(arena, commit_size)) {
    fprintf(stderr, "Failed to commit memory\n");
    return NULL;
  }

  arena->reserved = reserve_size;
  arena->committed = commit_size;
  arena->position = ARENA_BASE_POS;
  arena->commit_position = commit_size;

  return arena;
}

void arena_destroy(mem_arena *arena) { mem_release(arena, arena->reserved); }

void *__arena_push_impl(mem_arena *arena, uint64_t size, bool non_zero) {
  uint64_t pos_aligned = ALIGN_UP_TO_POW2(arena->position, ARENA_ALIGN);
  uint64_t new_pos = pos_aligned + size;

  if (new_pos > arena->reserved) {
    fprintf(stderr, "Failed to allocate memory, pos is larger than reserved\n");
    return NULL;
  }

  if (new_pos > arena->commit_position) {
    uint64_t new_commit_pos = new_pos;
    new_commit_pos += arena->committed - 1;
    new_commit_pos -= new_commit_pos % arena->committed;
    new_commit_pos = MIN(new_commit_pos, arena->reserved);

    uint8_t *mem = (uint8_t *)arena + arena->commit_position;
    uint64_t commit_size = new_commit_pos - arena->commit_position;

    if (!mem_commit(mem, commit_size)) {
      fprintf(stderr, "Failed to commit memory\n");
      return NULL;
    }

    arena->commit_position = new_commit_pos;
  }

  arena->position = new_pos;

  uint8_t *out = (uint8_t *)arena + pos_aligned;
  if (!non_zero)
    memset(out, 0, size);

  return (void *)out;
}

void arena_pop(mem_arena *arena, mem_t size) {
  size = ALIGN_UP_TO_POW2(size, arena->position - ARENA_BASE_POS);
  arena->position -= size;
}

void arena_pop_to(mem_arena *arena, mem_t pos) {
  mem_t size = pos < arena->position ? arena->position - pos : 0;
  arena_pop(arena, size);
}

void arena_clear(mem_arena *arena) {
  arena_pop_to(arena, ARENA_BASE_POS);
  mem_decommit(arena, arena->committed);
  mem_commit(arena, arena->committed);
}

temp_arena temp_arena_begin(mem_arena *arena) {
  return (temp_arena){.arena = arena, .start_pos = arena->position};
}

void temp_arena_end(temp_arena temp) {
  arena_pop_to(temp.arena, temp.start_pos);
}

static __thread mem_arena *__scratch_arenas[2] = {NULL, NULL};

temp_arena arena_scratch_get(mem_arena **conflicts, uint32_t num_conflicts) {
  int32_t scratch_id = -1;

  for (int32_t i = 0; i < 2; i++) {
    bool found = false;

    for (uint32_t j = 0; j < num_conflicts; j++) {
      if (conflicts[j] == __scratch_arenas[i]) {
        found = true;
        break;
      }
    }

    if (!found) {
      scratch_id = i;
      break;
    }
  }

  if (scratch_id == -1) {
    return (temp_arena){0};
  }

  mem_arena **selected = &__scratch_arenas[scratch_id];

  if (*selected == NULL) {
    *selected = arena_init(MiB(64), MiB(1));
  }

  return temp_arena_begin(*selected);
}

void arena_scratch_release(temp_arena scratch) { temp_arena_end(scratch); }

void *arena_strdup(mem_arena *arena, char *str, mem_t size) {
  char *dup = arena_push_array(arena, char, size);
  strlcpy(dup, str, size);
  return dup;
}

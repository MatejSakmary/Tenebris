#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#include <shared/shared.inl>
// BIT 31 -> 0 - FREE/ 1 - ALLOCATED
// BIT 30 -> 1 - REQUESTS_ALLOCATION
// BIT 29 -> 1 - ALLOCATION_FAILED
// BIT 28 -> 1 - DIRTY
// BIT 27 -> 1 - VISITED_MARKED

u32 allocated_mask()           { return 1 << 31; }
u32 requests_allocation_mask() { return 1 << 30; }
u32 allocation_failed_mask()   { return 1 << 29; }
u32 dirty_mask()               { return 1 << 28; }
u32 visited_marked_mask()      { return 1 << 27; }

bool get_is_allocated(u32 page_entry)        { return (page_entry & allocated_mask()) != 0; }
bool get_requests_allocation(u32 page_entry) { return (page_entry & requests_allocation_mask()) != 0; }
bool get_allocation_failed(u32 page_entry)   { return (page_entry & allocation_failed_mask()) != 0; }
bool get_is_dirty(u32 page_entry)            { return (page_entry & dirty_mask()) != 0; }
bool get_is_visited_marked(u32 page_entry)   { return (page_entry & visited_marked_mask()) != 0; }
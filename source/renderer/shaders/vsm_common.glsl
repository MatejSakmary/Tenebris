#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#include <shared/shared.inl>
// BIT 31 - allocated
// BIT 30 - needs allocation
// BIT 29 - is dirty

bool is_allocated(u32 page_entry)
{
    return (page_entry & (1 << 31)) != 0;
}

u32 get_needs_allocation_mask()
{
    return 1 << 30;
}

bool needs_allocation(u32 page_entry)
{
    return (page_entry & (1 << 30)) != 0;
}

bool is_dirty(u32 page_entry)
{
    return (page_entry & (1 << 29)) != 0;
}
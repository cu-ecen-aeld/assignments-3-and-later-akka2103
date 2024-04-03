/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer implementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset. Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 * character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 * buffptr member corresponding to char_offset. This value is only set when a matching char_offset is found
 * in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
                                                                          size_t char_offset, size_t *entry_offset_byte_rtn)
{
    // Initialize offset to 0
    size_t offset = 0;
    
    // Initialize index to the current out offset
    uint8_t index = buffer->out_offs;
    
    // Pointer to the found buffer entry
    struct aesd_buffer_entry *res = NULL;

    // If char_offset is 0, return the entry at the current out offset
    if (char_offset == 0)
    {
        *entry_offset_byte_rtn = 0;
        return &buffer->entry[buffer->out_offs];
    }


    // Loop through entries to find the one at the specified char_offset
    while (1)
    {
        offset += buffer->entry[index].size;
        index = (index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

        if (offset > char_offset)
        {
            break;
        }

        // If the index reaches the in offset, no matching offset is found
        if (index == buffer->in_offs)
        {
            return NULL;
        }
    }

    // Calculate the offset within the found entry
    index = (index - 1 + AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    offset -= buffer->entry[index].size;

    // Set the output parameters and return the found entry
    *entry_offset_byte_rtn = char_offset - offset;
    res = &buffer->entry[index];

    return res;
}

/**
 * Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
 * If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
 * new start location.
 * Any necessary locking must be handled by the caller
 * Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
 */
const char *aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    const char *ret_buf = NULL;
    // Check for NULL pointers
    if (buffer == NULL || add_entry == NULL)
    {
        return ret_buf;
    }
    
    if(buffer->full)
    {
        ret_buf = buffer->entry[buffer->out_offs].buffptr;
    }
    // Add the new entry to the circular buffer
    buffer->entry[buffer->in_offs] = *add_entry;
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    // Check if the buffer is full
    if (buffer->full)
    {
        // If full, advance the out offset to overwrite the oldest entry
        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    else if (buffer->in_offs == buffer->out_offs)
    {
        // If in offset equals out offset, set full flag
        buffer->full = true;
    }
    return ret_buf;
}

/**
 * Initializes the circular buffer described by @param buffer to an empty struct
 */
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    // Use memset to initialize the buffer to 0
    memset(buffer, 0, sizeof(struct aesd_circular_buffer));
}


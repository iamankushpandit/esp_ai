#ifndef TLIB_HEAP_H_
#define TLIB_HEAP_H_

#include <cstddef>
#include <cstdint>

namespace tlib::heap
{

    /**
     * Possible heap type/location
     */
    enum class Type : uint8_t
    {
        SRAM = 0U,
        PSRAM = 1U,
        UNKNOWN = 2U
    };

    // Default location for tensors
    // TODO: Utilize extensively in tlib_ops and move to config.h
    inline constexpr const heap::Type kDefaultLocation{heap::Type::PSRAM};

    /**
     * Initializes the heap module. The heap module manages two heap buffers, one in SRAM and one in PSRAM.
     * Both given buffers need to obey 16-byte alignment.
     *
     * @param   sram_heap_buffer    Pointer to buffer in SRAM
     * @param   sram_heap_size      SRAM buffer size in bytes
     * @param   psram_heap_buffer   Pointer to buffer in PSRAM
     * @param   psram_heap_size     PSRAM buffer size in bytes
     */
    void Initialize(uint8_t *sram_heap_buffer, const size_t sram_heap_size, uint8_t *psram_heap_buffer, const size_t psram_heap_size);

    /**
     * Get the max usage in bytes of the given buffer.
     * 
     * @param   heap_type   The heap type for which to get the max usage
     * 
     * @return  The maximum observed usage in bytes.
     */
    uint64_t MaxUsage(const Type heap_type);

    /**
     * story: detaches both heap buffers (they belong to the caller's phase arenas).
     * All tensors must have been destroyed first.
     */
    void Deinitialize(void);

    /*
     * Allocates size bytes on the heap of the given type. The returned buffer
     * pointer is guaranteed to obey 16-byte alignment.
     *
     * @param   heap_type   The heap type used for allocation
     * @param   size        The number of bytes to allocate
     *
     * @return  Pointer to the allocated buffer if enough heap space was available.
     *          Nullpointer otherwise.
     */
    void *Allocate(const Type heap_type, size_t size);

    /*
     * Frees a previously allocated buffer.
     *
     * @param    buffer  Pointer to the previously allocated buffer
     */
    void Free(void *buffer);

    /*
     * Get the heap type where the given buffer is located.
     *
     * @param   buffer  The buffer for which the location should be determined.
     *
     * @return  The heap in which the buffer is located.
     */
    Type Locate(const void *buffer);

    /*
     * Print information about a heap memory.
     *
     * @param   heap_type   The type of heap memory for which to print information.
     */
    void Summary(const Type heap_type);

} // tlib::heap

#endif

#ifndef TLIB_FLASH_H_
#define TLIB_FLASH_H_

// story: the upstream module memory-mapped model.bin from a flash partition.
// This version reads it from a FILE on the SD card instead:
//   - all tensor headers are parsed once into a RAM table at Initialize();
//   - a caller-chosen "resident" set (small tensors used on every audio chunk,
//     plus everything accessed via MapTensor) is copied once into memory from
//     the resident allocator (PSRAM);
//   - every other tensor is fread() on demand straight into its destination
//     tensor (normally the internal-SRAM heap), exactly where upstream memcpy'd
//     it from the mmap.
// The public API used by the conformer code (MapTensor/LoadTensor) is unchanged.

#include <cstddef>
#include <cstdint>
#include <functional>

#include <tlib_tensor.h>
#include <tlib_heap.h>

namespace tlib::flash
{
    enum class FlashError : uint8_t
    {
        None = 0U,
        MagicInvalid = 1U,
        TensorCountInvalid = 2U,
        FileSizeInvalid = 3U,
        MetaSectorMountFailed = 4U,
        TensorSectorMountFailed = 5U,
        Uninitialized = 6U,
        OpenFailed = 7U,
        OutOfMemory = 8U,
        ReadFailed = 9U,
    };

    struct IoStats
    {
        uint64_t bytes{0};
        uint64_t us{0};
        uint32_t reads{0};
    };

    /**
     * Opens the model file and builds the header table. `alloc` provides
     * 16-byte aligned memory that stays valid until Deinitialize() (used for
     * the header table and the resident tensors).
     */
    FlashError Initialize(const char *path, const std::function<void *(size_t)> &alloc,
                          const uint32_t *resident_ids, size_t n_resident);
    void Deinitialize(void);

    template <typename dtype>
    TensorView<dtype> MapTensor(const uint32_t tensor_id);

    template <typename dtype>
    Tensor<dtype> LoadTensor(const uint32_t tensor_id, const heap::Type type = heap::Type::PSRAM);

    IoStats GetIoStats(void);
    void ResetIoStats(void);
    size_t ResidentBytes(void);
    void Summary(void);

} // tlib::flash

#endif

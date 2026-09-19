// story: SD-card tensor source replacing upstream's mmap'd tlib_flash.cpp.
// File format (upstream tlib_flash.cpp): 64 KB meta block
//   "CONFORMER1\0" u32 n_tensors u64 file_size u32 offsets[n]
// then tensor records at 65536 + offsets[i]:
//   u8 type u8 dims u32 shape[4] u32 stride[4] u8 shift u32 numel u8 quantized
//   (40 B packed) followed by data aligned to 16 B (file offsets are
//   equivalent to upstream's pointer alignment because the mmap base was
//   64 KB aligned).
#include "../inc/tlib_flash.h"

#include <cassert>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <print>

#include <config.h>

#ifdef ESP_PLATFORM
#include <esp_timer.h>
static inline uint64_t now_us() { return esp_timer_get_time(); }
#else
#include <chrono>
static inline uint64_t now_us()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}
#endif

namespace
{
    enum class TensorType : uint8_t { UINT8 = 0, INT8 = 1, UINT16 = 2, INT16 = 3, UINT32 = 4, INT32 = 5, INT64 = 6, FLOAT32 = 7 };

    struct Header
    {
        uint32_t data_off;         // absolute file offset of the data
        uint32_t numel;
        uint32_t shape[kFlashTensorMaxDims];
        uint32_t stride[kFlashTensorMaxDims];
        const uint8_t *resident;   // non-null when copied into memory
        TensorType type;
        uint8_t dims;
        uint8_t shift;
        bool quantized;
    };

    FILE *file_{nullptr};
    Header *headers_{nullptr};
    uint32_t n_tensors_{0};
    size_t resident_bytes_{0};
    tlib::flash::IoStats io_{};

    size_t ElemSize(TensorType t)
    {
        switch (t)
        {
        case TensorType::UINT8: case TensorType::INT8: return 1;
        case TensorType::UINT16: case TensorType::INT16: return 2;
        case TensorType::UINT32: case TensorType::INT32: case TensorType::FLOAT32: return 4;
        case TensorType::INT64: return 8;
        }
        return 1;
    }

    bool ReadAt(uint64_t off, void *dst, size_t n)
    {
        const uint64_t t0 = now_us();
        if (fseek(file_, (long)off, SEEK_SET) != 0)
        {
            std::println("tlib_flash: fseek({}) failed errno {}", off, errno);
            return false;
        }
        const size_t r = fread(dst, 1, n, file_);
        io_.us += now_us() - t0;
        io_.bytes += r;
        io_.reads++;
        if (r != n)
        {
            std::println("tlib_flash: fread @{} got {}/{} errno {} ferror {} feof {}", off, r, n, errno,
                         ferror(file_), feof(file_));
            return false;
        }
        return true;
    }

    template <typename dtype> constexpr bool TypeMatches(TensorType t);
    template <> constexpr bool TypeMatches<int8_t>(TensorType t) { return t == TensorType::INT8; }
    template <> constexpr bool TypeMatches<int16_t>(TensorType t) { return t == TensorType::INT16; }
    template <> constexpr bool TypeMatches<int32_t>(TensorType t) { return t == TensorType::INT32; }
    template <> constexpr bool TypeMatches<float>(TensorType t) { return t == TensorType::FLOAT32; }

    const Header &Get(uint32_t id)
    {
        if (!headers_ || id >= n_tensors_)
        {
            std::println("tlib_flash: tensor {} requested but model not open / out of range ({})", id, n_tensors_);
            assert(0);
        }
        return headers_[id];
    }
} // namespace

namespace tlib::flash
{
    FlashError Initialize(const char *path, const std::function<void *(size_t)> &alloc,
                          const uint32_t *resident_ids, size_t n_resident)
    {
        Deinitialize();
        io_ = {};
        file_ = fopen(path, "rb");
        if (!file_) return FlashError::OpenFailed;
        // No stdio buffer: tensor reads are large and go straight to FATFS/DMA,
        // and newlib would otherwise malloc st_blksize (16 KB) of internal RAM.
        setvbuf(file_, nullptr, _IONBF, 0);

        char magic[11];
        uint32_t n = 0;
        uint64_t file_size = 0;
        if (!ReadAt(0, magic, 11) || memcmp(magic, "CONFORMER1", 10) != 0)
        {
            std::println("tlib_flash: bad magic in {} (errno {}, got {:02x} {:02x} {:02x})", path, errno,
                         (uint8_t)magic[0], (uint8_t)magic[1], (uint8_t)magic[2]);
            Deinitialize();
            return FlashError::MagicInvalid;
        }
        if (!ReadAt(11, &n, 4) || n == 0 || n > 4096) { Deinitialize(); return FlashError::TensorCountInvalid; }
        if (!ReadAt(15, &file_size, 8) || file_size == 0) { Deinitialize(); return FlashError::FileSizeInvalid; }

        headers_ = static_cast<Header *>(alloc(sizeof(Header) * n));
        uint32_t *offsets = static_cast<uint32_t *>(alloc(sizeof(uint32_t) * n));
        if (!headers_ || !offsets) { Deinitialize(); return FlashError::OutOfMemory; }
        if (!ReadAt(23, offsets, sizeof(uint32_t) * n)) { Deinitialize(); return FlashError::ReadFailed; }
        n_tensors_ = n;

        for (uint32_t i = 0; i < n; i++)
        {
            uint8_t raw[40];
            const uint64_t rec = (uint64_t)kFlashMetaPartitionSize + offsets[i];
            if (!ReadAt(rec, raw, sizeof raw)) { Deinitialize(); return FlashError::ReadFailed; }
            Header &h = headers_[i];
            const uint8_t *p = raw;
            h.type = static_cast<TensorType>(*p++);
            h.dims = *p++;
            memcpy(h.shape, p, 16); p += 16;
            memcpy(h.stride, p, 16); p += 16;
            h.shift = *p++;
            memcpy(&h.numel, p, 4); p += 4;
            h.quantized = *p++;
            const uint64_t data = rec + 40;
            h.data_off = (uint32_t)((data + (kFlashTensorAlignment - 1)) & ~(uint64_t)(kFlashTensorAlignment - 1));
            h.resident = nullptr;
        }

        resident_bytes_ = 0;
        for (size_t r = 0; r < n_resident; r++)
        {
            const uint32_t id = resident_ids[r];
            if (id >= n) continue;
            Header &h = headers_[id];
            const size_t bytes = (size_t)h.numel * ElemSize(h.type);
            uint8_t *mem = static_cast<uint8_t *>(alloc(bytes));
            if (!mem) { Deinitialize(); return FlashError::OutOfMemory; }
            if (!ReadAt(h.data_off, mem, bytes)) { Deinitialize(); return FlashError::ReadFailed; }
            h.resident = mem;
            resident_bytes_ += bytes;
        }
        // Streamed tensors are rebuilt from their shape (contiguous strides);
        // non-contiguous ones (e.g. the transposed mel filter) must be resident.
        for (uint32_t i = 0; i < n; i++)
        {
            const Header &h = headers_[i];
            if (h.resident) continue;
            uint32_t s = 1;
            for (int d = h.dims - 1; d >= 0; d--)
            {
                if (h.stride[d] != s)
                {
                    std::println("tlib_flash: tensor {} is non-contiguous and not resident", i);
                    Deinitialize();
                    return FlashError::FileSizeInvalid;
                }
                s *= h.shape[d];
            }
        }
        return FlashError::None;
    }

    void Deinitialize(void)
    {
        if (file_) fclose(file_);
        file_ = nullptr;
        headers_ = nullptr;   // owned by the caller's arena
        n_tensors_ = 0;
        resident_bytes_ = 0;
    }

    template <typename dtype>
    TensorView<dtype> MapTensor(const uint32_t tensor_id)
    {
        const Header &h = Get(tensor_id);
        assert(TypeMatches<dtype>(h.type));
        if (!h.resident)
        {
            std::println("tlib_flash: MapTensor({}) needs a resident tensor", tensor_id);
            assert(0);
        }
        std::vector<uint32_t> shape(h.shape, h.shape + h.dims), stride(h.stride, h.stride + h.dims);
        return TensorView<dtype>(std::move(shape), std::move(stride),
                                 reinterpret_cast<const dtype *>(h.resident), h.shift, h.quantized);
    }

    template <typename dtype>
    Tensor<dtype> LoadTensor(const uint32_t tensor_id, const heap::Type type)
    {
        const Header &h = Get(tensor_id);
        assert(TypeMatches<dtype>(h.type));
        if (h.resident)
        {
            return Tensor<dtype>(MapTensor<dtype>(tensor_id), type);   // upstream semantics (keeps strides)
        }
        std::vector<uint32_t> shape(h.shape, h.shape + h.dims);
        Tensor<dtype> t(std::move(shape), h.quantized, h.shift, type);
        if (!ReadAt(h.data_off, t.Data(), t.Bytes()))
        {
            std::println("tlib_flash: SD read failed for tensor {} ({} B)", tensor_id, t.Bytes());
            assert(0);
        }
        return t;
    }

    template TensorView<int8_t> MapTensor<int8_t>(const uint32_t);
    template TensorView<int16_t> MapTensor<int16_t>(const uint32_t);
    template TensorView<int32_t> MapTensor<int32_t>(const uint32_t);
    template TensorView<float> MapTensor<float>(const uint32_t);
    template Tensor<int8_t> LoadTensor<int8_t>(const uint32_t, const heap::Type);
    template Tensor<int16_t> LoadTensor<int16_t>(const uint32_t, const heap::Type);
    template Tensor<int32_t> LoadTensor<int32_t>(const uint32_t, const heap::Type);
    template Tensor<float> LoadTensor<float>(const uint32_t, const heap::Type);

    IoStats GetIoStats(void) { return io_; }
    void ResetIoStats(void) { io_ = {}; }
    size_t ResidentBytes(void) { return resident_bytes_; }

    void Summary(void)
    {
        std::println("tlib_flash(SD): {} tensors, resident {} B, io {} B in {} reads, {} ms",
                     n_tensors_, resident_bytes_, io_.bytes, io_.reads, io_.us / 1000);
    }
} // namespace tlib::flash

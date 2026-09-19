#include "tlib_heap.h"

#include <vector>
#include <string>
#include <list>
#include <ranges>
#include <print>
#include <cassert>

#include <config.h>

namespace tlib::heap
{

    namespace utils
    {

        constexpr size_t AlignSize(const size_t size)
        {
            size_t remainder = size % kHeapAlignment;
            if (remainder == 0)
            {
                return size;
            }
            else
            {
                return size + (kHeapAlignment - remainder);
            }
        }

    } // namespace utils

    class Block
    {
    public:
        explicit Block(uint8_t *buffer, size_t size) : buffer_{buffer}, size_{size}
        {
            buffer_after_ = buffer + size;
        };

        inline constexpr uint8_t *Buffer(void) const { return buffer_; };

        inline constexpr uint8_t *BufferAfter(void) const { return buffer_after_; };

        void Summary(const uint8_t block_idx) const
        {
            std::println("| |-+Block\t\t{}", block_idx);
            std::println("| | |--Buffer:\t\t{}", (void *)buffer_);
            std::println("| | |--BufferAfter:\t{}", (void *)buffer_after_);
            std::println("| | |--Size:\t\t{}", size_);
            std::println("| |-+");
        };

    private:
        uint8_t *buffer_{nullptr};
        uint8_t *buffer_after_{nullptr};
        size_t size_{0};
    };

    class Heap
    {
    public:
        void Initialize(const Type heap_type, const std::string &&name, uint8_t *buffer, const size_t buffer_size)
        {
            assert(buffer_ == nullptr);
            assert(reinterpret_cast<uint64_t>(buffer) % kHeapAlignment == 0);
            type_ = heap_type;
            name_ = name;
            buffer_ = buffer;
            buffer_size_ = buffer_size;
        }

        uint64_t MaxUsage(void) const { return max_usage_; }

        // story: release the backing buffer so the phase arena can be reused.
        void Deinitialize(void)
        {
            if (!active_blocks_.empty())
            {
                std::println("Heap {} deinit with {} live blocks (leak in caller)", name_, active_blocks_.size());
            }
            active_blocks_.clear();
            buffer_ = nullptr;
            buffer_size_ = 0;
            max_usage_ = 0;
        }

        size_t LiveBlocks(void) const { return active_blocks_.size(); }

        size_t LargestFree(void) const
        {
            if (!buffer_ || active_blocks_.size() >= kHeapMaxBlocks) return 0;
            size_t best = 0;
            auto prev = buffer_;
            for (const auto &b : active_blocks_)
            {
                if (static_cast<size_t>(b.Buffer() - prev) > best) best = b.Buffer() - prev;
                prev = b.BufferAfter();
            }
            if (static_cast<size_t>((buffer_ + buffer_size_) - prev) > best) best = (buffer_ + buffer_size_) - prev;
            return best & ~(kHeapAlignment - 1);
        }

        void *Allocate(size_t size, bool quiet = false)
        {
            size = utils::AlignSize(size);

            // Is requested size larger than heap?
            if (size > buffer_size_)
            {
                if (!quiet) std::println("Heap {} OOM while trying to allocate {} bytes", name_, size);
                return nullptr;
            }

            // Is list empty ?
            if (active_blocks_.empty())
            {
                const auto new_block = Block(buffer_, size);
                active_blocks_.push_back(new_block);
                TrackMaxUsage(new_block);
                return new_block.Buffer();
            }

            // Are there any free blocks?
            if (active_blocks_.size() >= kHeapMaxBlocks)
            {
                return nullptr;
            }

            // Check if there is enough space between active blocks
            auto prev_buffer_after = buffer_;
            for (auto it = active_blocks_.begin(); it != active_blocks_.end(); it++)
            {
                if (static_cast<size_t>(it->Buffer() - prev_buffer_after) >= size)
                {
                    const auto new_block = Block(prev_buffer_after, size);
                    active_blocks_.insert(it, new_block);
                    TrackMaxUsage(new_block);
                    return new_block.Buffer();
                }
                prev_buffer_after = it->BufferAfter();
            }

            // Check if there is enough space after last block
            const auto &last_block = active_blocks_.back();
            if (static_cast<size_t>((buffer_ + buffer_size_) - last_block.BufferAfter()) >= size)
            {
                const auto new_block = Block(last_block.BufferAfter(), size);
                active_blocks_.push_back(new_block);
                TrackMaxUsage(new_block);
                return new_block.Buffer();
            }

            // Not enough memory on heap left
            if (!quiet) std::println("Heap {} OOM while trying to allocate {} bytes", name_, size);
            return nullptr;
        }

        void Free(void *buffer)
        {
            for (auto it = active_blocks_.begin(); it != active_blocks_.end(); it++)
            {
                if (it->Buffer() == buffer)
                {
                    active_blocks_.erase(it);
                    return;
                }
            }
        }

        inline bool Contains(const void *buffer) const
        {
            return buffer >= buffer_ && buffer < (buffer_ + buffer_size_);
        }

        void Summary(void) const
        {
            std::println("Heap Summary ({})", name_);
            std::println("|--Max Usage:\t\t{}", max_usage_);
            std::println("|--Heap Start:\t\t{}", (void *)buffer_);
            std::println("|--Heap End:\t\t{}", (void *)(buffer_ + buffer_size_));
            std::println("|--Heap Size:\t\t{}", buffer_size_);
            std::println("|--Active Blocks:\t{}", active_blocks_.size());
            for (auto const &[idx, block] : std::views::enumerate(active_blocks_))
            {
                std::println("| |");
                block.Summary(idx);
            }
            std::println("+-+\n");
        }

    private:
        void TrackMaxUsage(const Block &new_block)
        {
            const uint64_t block_end_addr = reinterpret_cast<uint64_t>(new_block.BufferAfter());
            const uint64_t curr_usage{block_end_addr - reinterpret_cast<uint64_t>(buffer_)};
            if (curr_usage > max_usage_)
            {
                max_usage_ = curr_usage;
            }
        }

        Type type_{};
        std::string name_{};
        uint8_t *buffer_{nullptr};
        size_t buffer_size_{};
        std::list<Block> active_blocks_{};
        uint64_t max_usage_{};
    };

    Heap sram_{}, psram_{};

    void Initialize(uint8_t *sram_heap_buffer, const size_t sram_heap_size, uint8_t *psram_heap_buffer, const size_t psram_heap_size)
    {
        sram_.Initialize(Type::SRAM, "SRAM", sram_heap_buffer, sram_heap_size);
        psram_.Initialize(Type::PSRAM, "PSRAM", psram_heap_buffer, psram_heap_size);
    }

    void Deinitialize(void)
    {
        sram_.Deinitialize();
        psram_.Deinitialize();
    }

    uint64_t MaxUsage(const Type heap_type)
    {
        switch (heap_type)
        {
        case Type::SRAM:
            return sram_.MaxUsage();
        case Type::PSRAM:
            return psram_.MaxUsage();
        default:
            assert(0);
        }
    }

    // story: an SRAM request that doesn't fit falls back to PSRAM (slower
    // matmul operand, but never a crash). Fallbacks are counted for the logs.
    static uint32_t sram_fallbacks_{0};
    static uint64_t sram_fallback_bytes_{0};

    void *Allocate(const Type heap_type, size_t size)
    {
        switch (heap_type)
        {
        case Type::SRAM:
            if (void *p = sram_.Allocate(size, /*quiet=*/true)) return p;
            sram_fallbacks_++;
            sram_fallback_bytes_ += size;
            return psram_.Allocate(size);
        case Type::PSRAM:
        default:
            return psram_.Allocate(size);
        }
    }

    uint32_t SramFallbacks(void) { return sram_fallbacks_; }
    uint64_t SramFallbackBytes(void) { return sram_fallback_bytes_; }
    void ResetSramFallbacks(void) { sram_fallbacks_ = 0; sram_fallback_bytes_ = 0; }

    size_t LargestFree(const Type heap_type)
    {
        return heap_type == Type::SRAM ? sram_.LargestFree() : psram_.LargestFree();
    }

    void Free(void *buffer)
    {
        const auto location = Locate(buffer);
        switch (location)
        {
        case Type::SRAM:
            sram_.Free(buffer);
            return;
        case Type::PSRAM:
            psram_.Free(buffer);
            return;
        default:
            assert(0);
        }
    }

    Type Locate(const void *buffer)
    {
        if (sram_.Contains(buffer))
        {
            return Type::SRAM;
        }
        else if (psram_.Contains(buffer))
        {
            return Type::PSRAM;
        }
        else
        {
            return Type::UNKNOWN;
        }
    }

    void Summary(const Type heap_type)
    {
        switch (heap_type)
        {
        case Type::SRAM:
            sram_.Summary();
            return;
        case Type::PSRAM:
        default:
            psram_.Summary();
            return;
        }
    }

} // tlib::heap
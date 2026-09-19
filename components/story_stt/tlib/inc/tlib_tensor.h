#ifndef TLIB_TENSOR_H_
#define TLIB_TENSOR_H_

#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <numeric>
#include <vector>
#include <algorithm>
#include <ranges>
#include <print>
#include <string>

#include <tlib_heap.h>

namespace tlib
{

    /**
     * Non-data-owning read-only interface for tensors.
     */
    template <typename dtype>
    class TensorView
    {
    public:
        /**
         * Constructs an empty tensor view
         */
        explicit TensorView(void) {};

        /**
         * Constructs a tensor view ontop of the given data with the given parameters.
         *
         * @param   shape       Shape of the tensor
         * @param   stride      Stride of the tensor
         * @param   data        Pointer to tensor data
         * @param   shift       Quantization shift of the tensor
         * @param   quantized   Flag to indicate if this tensor is quantized or not
         */
        explicit TensorView(
            const std::vector<uint32_t> &&shape,
            const std::vector<uint32_t> &&stride,
            const dtype *data,
            const uint8_t shift,
            const bool quantized)
            : shape_{shape}, stride_{stride}, data_immutable_{data}, shift_{shift}, quantized_{quantized}
        {
            n_elements_ = std::accumulate(shape_.begin(), shape_.end(), 1, std::multiplies<uint64_t>());
        };

        /**
         * View this tensor view as a tensor view of different shape. The resulting number of elements
         * must stay the same. Causes a recomputation of strides.
         *
         * @param   shape   The new shape of the tensor
         */
        inline void View(const std::vector<uint32_t> &&shape)
        {
            const uint64_t n_elements = std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<uint64_t>());
            assert(n_elements_ == n_elements);
            shape_ = shape;
            UpdateStride();
        }

        /**
         * Performs the unsqueeze operation at the given dimension.
         * Example:
         * x.Shape() -> [5, 6, 7]
         * x.Unsqueeze(0)
         * x.Shape() -> [1, 5, 6, 7]
         *
         * @param   dim     Position at which to insert new dimension
         */
        inline void Unsqueeze(const uint8_t dim)
        {
            assert(dim < shape_.size());
            shape_.insert(shape_.begin() + dim, 1);
            UpdateStride();
        }

        /**
         * Performs the flatten operation between the specified dimensions.
         * Example:
         * x.Shape() -> [5, 6, 7, 8]
         * x.Flatten(2, 3)
         * x.Shape() -> [5, 6, 56]
         *
         * @param   dim0    Start dimension for flattening
         * @param   dim1    End dimension for flattening
         */
        inline void Flatten(const uint8_t dim0, const uint8_t dim1)
        {
            assert(dim0 < dim1);
            assert(dim1 < shape_.size());
            shape_.at(dim0) *= shape_.at(dim1);
            shape_.erase(shape_.begin() + dim1);
            UpdateStride();
        }

        /**
         * Access the underlying tensor data (read-only).
         *
         * @return  The underlying tensor data
         */
        inline constexpr const dtype *DataImm(void) const { return data_immutable_; };

        /**
         * Get the size of the underlying tensor data buffer in bytes.
         *
         * @return  The tensor data buffer size in bytes
         */
        inline constexpr size_t Bytes(void) const { return n_elements_ * sizeof(dtype); };

        /**
         * Get the number of dimensions.
         *
         * @return  The number of dimensions
         */
        inline constexpr size_t Dim(void) const { return shape_.size(); };

        /**
         * Get the shape of this tensor.
         *
         * @return  The shape of this tensor
         */
        inline constexpr std::vector<uint32_t> Shape(void) const { return shape_; };

        /**
         * Get the strides of this tensor.
         *
         * @return  The strides of this tensor
         */
        inline constexpr std::vector<uint32_t> Stride(void) const { return stride_; };

        /**
         * Get the dimension size at a specific index of this tensor.
         *
         * @param   index   Index of dimension
         *
         * @return  Size of dimension at index
         */
        inline constexpr uint32_t Shape(const uint8_t index) const { return shape_.at(index); };

        /**
         * Get the stride size at a specific index of this tensor.
         *
         * @param   index   Index of stride
         *
         * @return  Size of stride at index
         */
        inline constexpr uint32_t Stride(const uint8_t index) const { return stride_.at(index); };

        /**
         * Get the number of elements of this tensor.
         *
         * @return  The number of elements
         */
        inline constexpr size_t Numel(void) const { return n_elements_; };

        /**
         * Get the quantization shift of this tensor.
         *
         * @return  The quantization shift
         */
        inline constexpr uint8_t Shift(void) const { return shift_; };

        /**
         * Query if this tensor is quantized.
         *
         * @return  True, if this tensor is quantized, false otherwise
         */
        inline constexpr bool Quantized(void) const { return quantized_; };

        /**
         * Print some information about this tensor.
         *
         * @param   name    Optional label to use for this tensor
         */
        void Summary(const std::string &name = "") const;

    protected:
        /**
         * Constructs a tensor view based on shape. Should only be used if the
         * underlying buffer is populated otherwise.
         *
         * @param   shape   The shape of this tensor
         */
        explicit TensorView(const std::vector<uint32_t> &&shape) : shape_{shape}
        {
            UpdateStride();
            shift_ = 0;
            quantized_ = false;
            n_elements_ = std::accumulate(shape_.begin(), shape_.end(), 1, std::multiplies<uint64_t>());
        }

        std::vector<uint32_t> shape_;          /*!< Shape of tensor */
        std::vector<uint32_t> stride_;         /*!< Strides of tensor */
        uint64_t n_elements_{0};               /*!< Number of elements in tensor */
        const dtype *data_immutable_{nullptr}; /*!< Read-only pointer to tensor data */
        uint8_t shift_{0};                     /*!< Quantization shift of tensor */
        bool quantized_{false};                /*!< Indicates if tensor is quantized */

    private:
        /**
         * Prints basic information about this tensor.
         *
         * @param   name    A label for this tensor.
         */
        inline void BasicSummary(const std::string &name) const
        {
            const uint8_t checksum = std::accumulate(
                reinterpret_cast<const uint8_t *>(data_immutable_),
                reinterpret_cast<const uint8_t *>(data_immutable_) + Bytes(), 0);
            std::println("+Tensor Summary ({})", name);
            std::println("|--Shape:\t{}", shape_);
            std::println("|--Stride:\t{}", stride_);
            std::println("|--Shift:\t{}", shift_);
            std::println("|--Checksum:\t{:#x}", checksum);
            std::println("|--Min:\t\t{}", *std::min_element(data_immutable_, data_immutable_ + n_elements_));
            std::println("|--Max:\t\t{}", *std::max_element(data_immutable_, data_immutable_ + n_elements_));
            float sum{0.0f};
            for (uint32_t i = 0; i < Numel(); i++)
            {
                sum += (float)(DataImm()[i]);
            }
            const float mean{sum / (float)Numel()};
            std::println("|--Mean:\t{}", mean);
            std::println("|--Sum:\t\t{}", sum);
            std::println("|--Val[:3]:\t{}", std::vector<dtype>(DataImm(), DataImm() + 3));
            std::println("|--Val[-3:]\t{}", std::vector<dtype>(DataImm() + Numel() - 3, DataImm() + Numel()));
        }

        /**
         * Adjusts the strides to match the current shape.
         * This function assumes that the underlying memory layout is contiguous.
         */
        inline void UpdateStride(void)
        {
            stride_.resize(Dim());
            stride_.at(Dim() - 1) = 1;

            std::inclusive_scan(
                shape_.rbegin(),
                shape_.rend() - 1,
                stride_.rbegin() + 1,
                std::multiplies<uint32_t>(),
                1U);
        }
    };

    /**
     * The tensor class is a wrapper around a contiguous data buffer, with additional information about how to read data from that buffer.
     */
    template <typename dtype>
    class Tensor : public TensorView<dtype>
    {
    public:
        /**
         * Constructs an empty tensor.
         */
        explicit Tensor(void) {}

        /**
         * Initialize an empty tensor of the given shape on the given heap.
         *
         * @param   shape   Shape of the tensor
         * @param   quantized   Indicates if this tensor is quantized
         * @param   shift       Quantization shift of this tensor
         * @param   heap_type   Heap location for tensor data
         */
        explicit Tensor(const std::vector<uint32_t> &&shape, const bool quantized, const uint8_t shift, const heap::Type heap_type = heap::kDefaultLocation)
            : TensorView<dtype>(std::move(shape))
        {
            data_ = reinterpret_cast<dtype *>(heap::Allocate(heap_type, TensorView<dtype>::Bytes()));
            assert(data_ != nullptr);
            TensorView<dtype>::data_immutable_ = data_;
            TensorView<dtype>::shift_ = shift;
            TensorView<dtype>::quantized_ = quantized;
        }

        /*
         * Move construct a Tensor from a TensorView.
         */
        explicit Tensor(TensorView<dtype> &&tensor_view, const heap::Type heap_type = heap::Type::PSRAM)
            : TensorView<dtype>(std::move(tensor_view))
        {
            data_ = reinterpret_cast<dtype *>(heap::Allocate(heap_type, TensorView<dtype>::Bytes()));
            assert(data_ != nullptr);
            std::memcpy(data_, TensorView<dtype>::data_immutable_, TensorView<dtype>::Bytes());
            TensorView<dtype>::data_immutable_ = data_;
        }

        /*
         * Copy Constructor
         */
        Tensor(const Tensor<dtype> &tensor)
            : TensorView<dtype>(std::move(tensor))
        {
            data_ = reinterpret_cast<dtype *>(heap::Allocate(heap::Type::PSRAM, TensorView<dtype>::Bytes()));
            assert(data_ != nullptr);
            std::memcpy(data_, tensor.data_, TensorView<dtype>::Bytes());
            TensorView<dtype>::data_immutable_ = data_;
        }

        /*
         * Move Constructor
         */
        Tensor(Tensor<dtype> &&tensor)
            : TensorView<dtype>(std::move(tensor))
        {
            assert(data_ == nullptr);
            data_ = tensor.data_;
            TensorView<dtype>::data_immutable_ = tensor.data_immutable_;
            tensor.data_ = nullptr;
            tensor.data_immutable_ = nullptr;
        }

        /*
         * Copy assignment
         */
        Tensor<dtype> &operator=(const Tensor<dtype> &tensor)
        {
            if (this == &tensor)
                return *this;

            TensorView<dtype>::shape_ = tensor.shape_;
            TensorView<dtype>::stride_ = tensor.stride_;
            if (TensorView<dtype>::n_elements_ != tensor.n_elements_)
            {
                const auto location = heap::Locate(data_);
                heap::Free(data_);
                TensorView<dtype>::n_elements_ = tensor.n_elements_;
                data_ = reinterpret_cast<dtype *>(heap::Allocate(location, TensorView<dtype>::Bytes()));
                assert(data_ != nullptr);
                TensorView<dtype>::data_immutable_ = data_;
            }
            TensorView<dtype>::shift_ = tensor.shift_;
            TensorView<dtype>::quantized_ = tensor.quantized_;
            std::memcpy(data_, tensor.data_, TensorView<dtype>::Bytes());
            return *this;
        }

        /*
         * Move assignment
         */
        Tensor<dtype> &operator=(Tensor<dtype> &&tensor)
        {
            if (this == &tensor)
                return *this;

            if (data_ != nullptr)
            {
                heap::Free(data_);
            }

            TensorView<dtype>::shape_ = std::move(tensor.shape_);
            TensorView<dtype>::stride_ = std::move(tensor.stride_);
            TensorView<dtype>::n_elements_ = tensor.n_elements_;
            TensorView<dtype>::shift_ = tensor.shift_;
            TensorView<dtype>::quantized_ = tensor.quantized_;
            data_ = tensor.data_;
            TensorView<dtype>::data_immutable_ = data_;
            tensor.data_ = nullptr;
            tensor.data_immutable_ = nullptr;
            return *this;
        }

        /*
         * Destructor
         */
        ~Tensor(void)
        {
            if (data_ != nullptr)
            {
                heap::Free(data_);
                data_ = nullptr;
            }
        }

        /**
         * Access the underlying tensor data.
         *
         * @return  The underlying tensor data
         */
        inline constexpr dtype *Data(void) const { return data_; };

        /**
         * Moves the underlying tensor data to a different heap.
         *
         * @param   location    The target heap location
         */
        inline void To(const heap::Type location)
        {
            const auto current_location = heap::Locate(data_);
            assert(location != heap::Type::UNKNOWN);
            if (current_location == location)
            {
                return;
            }

            dtype *new_data = reinterpret_cast<dtype *>(heap::Allocate(location, TensorView<dtype>::Bytes()));
            std::memcpy(new_data, data_, TensorView<dtype>::Bytes());
            heap::Free(data_);
            data_ = new_data;
            TensorView<dtype>::data_immutable_ = data_;
        }

    private:
        dtype *data_{nullptr}; /*!< Pointer to tensor data */
    };

} // tlib

#endif

/**
 * @file methods/ann/layer/max_pooling.hpp
 * @author Marcus Edel
 * @author Nilay Jain
 *
 * Definition of the MaxPooling class.
 *
 * mlpack is free software; you may redistribute it and/or modify it under the
 * terms of the 3-clause BSD license.  You should have received a copy of the
 * 3-clause BSD license along with mlpack.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */
#ifndef MLPACK_METHODS_ANN_LAYER_MAX_POOLING_HPP
#define MLPACK_METHODS_ANN_LAYER_MAX_POOLING_HPP

#include <mlpack/prereqs.hpp>

namespace mlpack {
namespace ann /** Artificial Neural Network. */ {

/*
 * The max pooling rule for convolution neural networks. Take the maximum value
 * within the receptive block.
 */
class MaxPoolingRule
{
 public:
  /*
   * Return the maximum value within the receptive block.
   *
   * @param input Input used to perform the pooling operation.  Could be an
   *     Armadillo subview.
   */
  template<typename InputType>
  typename InputType::elem_type Pooling(const InputType& input)
  {
    return arma::max(arma::vectorise(input));
  }

  template<typename InputType>
  std::tuple<size_t, typename InputType::elem_type> PoolingWithIndex(
      const InputType& input)
  {
    const typename InputType::elem_type maxVal =
        arma::max(arma::vectorise(input));
    const size_t index = arma::as_scalar(arma::find(input == maxVal, 1));

    return std::tuple<size_t, typename InputType::elem_type>(index, maxVal);
  }
};

/**
 * Implementation of the MaxPooling layer.
 *
 * @tparam InputType Type of the input data (arma::colvec, arma::mat,
 *         arma::sp_mat or arma::cube).
 * @tparam OutputType Type of the output data (arma::colvec, arma::mat,
 *         arma::sp_mat or arma::cube).
 */
template <
    typename InputType = arma::mat,
    typename OutputType = arma::mat
>
class MaxPoolingType : public Layer<InputType, OutputType>
{
 public:
  //! Create the MaxPooling object.
  MaxPoolingType();

  /**
   * Create the MaxPooling object using the specified number of units.
   *
   * @param kernelWidth Width of the pooling window.
   * @param kernelHeight Height of the pooling window.
   * @param strideWidth Width of the stride operation.
   * @param strideHeight Width of the stride operation.
   * @param floor Rounding operator (floor or ceil).
   */
  MaxPoolingType(const size_t kernelWidth,
                 const size_t kernelHeight,
                 const size_t strideWidth = 1,
                 const size_t strideHeight = 1,
                 const bool floor = true);

  // Virtual destructor.
  virtual ~MaxPoolingType() { }

  //! Copy the given MaxPoolingType.
  MaxPoolingType(const MaxPoolingType& other);
  //! Take ownership of the given MaxPoolingType.
  MaxPoolingType(MaxPoolingType&& other);
  //! Copy the given MaxPoolingType.
  MaxPoolingType& operator=(const MaxPoolingType& other);
  //! Take ownership of the given MaxPoolingType.
  MaxPoolingType& operator=(MaxPoolingType&& other);

  MaxPoolingType* Clone() const { return new MaxPoolingType(*this); }

  /**
   * Ordinary feed forward pass of a neural network, evaluating the function
   * f(x) by propagating the activity forward through f.
   *
   * @param input Input data used for evaluating the specified function.
   * @param output Resulting output activation.
   */
  void Forward(const InputType& input, OutputType& output);

  /**
   * Ordinary feed backward pass of a neural network, using 3rd-order tensors as
   * input, calculating the function f(x) by propagating x backwards through f.
   * Using the results from the feed forward pass.
   *
   * @param * (input) The propagated input activation.
   * @param gy The backpropagated error.
   * @param g The calculated gradient.
   */
  void Backward(const InputType& /* input */,
                const OutputType& gy,
                OutputType& g);

  //! Get the kernel width.
  size_t const& KernelWidth() const { return kernelWidth; }
  //! Modify the kernel width.
  size_t& KernelWidth() { return kernelWidth; }

  //! Get the kernel height.
  size_t const& KernelHeight() const { return kernelHeight; }
  //! Modify the kernel height.
  size_t& KernelHeight() { return kernelHeight; }

  //! Get the stride width.
  size_t const& StrideWidth() const { return strideWidth; }
  //! Modify the stride width.
  size_t& StrideWidth() { return strideWidth; }

  //! Get the stride height.
  size_t const& StrideHeight() const { return strideHeight; }
  //! Modify the stride height.
  size_t& StrideHeight() { return strideHeight; }

  //! Get the value of the rounding operation.
  bool const& Floor() const { return floor; }
  //! Modify the value of the rounding operation.
  bool& Floor() { return floor; }

  //! Compute the size of the output given `InputDimensions()`.
  void ComputeOutputDimensions();

  /**
   * Serialize the layer.
   */
  template<typename Archive>
  void serialize(Archive& ar, const uint32_t /* version */);

 private:
  /**
   * Apply pooling to the input and store the results.
   *
   * @param input The input to be apply the pooling rule.
   * @param output The pooled result.
   * @param poolingIndices The pooled indices.
   */
  void PoolingOperation(
      const arma::Cube<typename InputType::elem_type>& input,
      arma::Cube<typename OutputType::elem_type>& output,
      arma::Cube<size_t>& poolingIndices)
  {
    // Iterate over all slices individually.
    for (size_t s = 0; s < input.n_slices; ++s)
    {
      for (size_t j = 0, colidx = 0; j < output.n_cols;
          ++j, colidx += strideHeight)
      {
        for (size_t i = 0, rowidx = 0; i < output.n_rows;
            ++i, rowidx += strideWidth)
        {
          const std::tuple<size_t, typename InputType::elem_type> poolResult =
              pooling.PoolingWithIndex(input.slice(s).submat(
                  rowidx,
                  colidx,
                  rowidx + kernelWidth - 1 - offset,
                  colidx + kernelHeight - 1 - offset));

          // Now map the returned pooling index, which corresponds to the
          // submatrix we gave, back to its position in the (linearized) input.
          const size_t poolIndex = std::get<0>(poolResult);
          const size_t poolingCol = poolIndex / (kernelWidth - offset);
          const size_t poolingRow = poolIndex % (kernelWidth - offset);
          const size_t unmappedPoolingIndex = (rowidx + poolingRow) +
              input.n_rows * (colidx + poolingCol) +
              input.n_rows * input.n_cols * s;

          poolingIndices(i, j, s) = unmappedPoolingIndex;
          output(i, j, s) = std::get<1>(poolResult);
        }
      }
    }
  }

  /**
   * Apply pooling to all slices of the input and store the results, but not the
   * indices used.
   *
   * @param input The input to apply the pooling rule to.
   * @param output The pooled result.
   */
  void PoolingOperation(
      const arma::Cube<typename InputType::elem_type>& input,
      arma::Cube<typename OutputType::elem_type>& output)
  {
    // Iterate over all slices individually.
    for (size_t s = 0; s < input.n_slices; ++s)
    {
      for (size_t j = 0, colidx = 0; j < output.n_cols;
          ++j, colidx += strideHeight)
      {
        for (size_t i = 0, rowidx = 0; i < output.n_rows;
            ++i, rowidx += strideWidth)
        {
          output(i, j, s) = pooling.Pooling(input.slice(s).submat(
              rowidx,
              colidx,
              rowidx + kernelWidth - 1 - offset,
              colidx + kernelHeight - 1 - offset));
        }
      }
    }
  }

  /**
   * Apply unpooling to all slices of the input and store the results.
   *
   * @param error The backward error.
   * @param output The pooled result.
   * @param poolingIndices The pooled indices (from `PoolingOperation()`).
   */
  void UnpoolingOperation(
      const arma::Cube<typename InputType::elem_type>& error,
      arma::Cube<typename OutputType::elem_type>& output,
      const arma::Cube<size_t>& poolingIndices)
  {
    output.zeros();

    for (size_t i = 0; i < poolingIndices.n_elem; ++i)
    {
      output(poolingIndices(i)) += error(i);
    }
  }

  //! Locally-stored width of the pooling window.
  size_t kernelWidth;

  //! Locally-stored height of the pooling window.
  size_t kernelHeight;

  //! Locally-stored width of the stride operation.
  size_t strideWidth;

  //! Locally-stored height of the stride operation.
  size_t strideHeight;

  //! Rounding operation used.
  bool floor;

  //! Locally-stored number of channels.
  size_t channels;

  size_t offset;

  //! Locally-stored pooling strategy.
  MaxPoolingRule pooling;

  //! Locally-stored pooling indicies.
  arma::Cube<size_t> poolingIndices;
}; // class MaxPoolingType

// Standard MaxPooling layer.
typedef MaxPoolingType<arma::mat, arma::mat> MaxPooling;

} // namespace ann
} // namespace mlpack

// Include implementation.
#include "max_pooling_impl.hpp"

#endif

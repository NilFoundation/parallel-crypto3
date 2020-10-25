//---------------------------------------------------------------------------//
// Copyright (c) 2018-2020 Mikhail Komarov <nemo@nil.foundation>
// Copyright (c) 2020 Nikita Kaskov <nbering@nil.foundation>
//
// MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//---------------------------------------------------------------------------//

#ifndef CRYPTO3_ZK_SNARK_ACCUMULATION_VECTOR_HPP
#define CRYPTO3_ZK_SNARK_ACCUMULATION_VECTOR_HPP

#include <iostream>

#include <nil/crypto3/zk/snark/sparse_vector.hpp>

namespace nil {
    namespace crypto3 {
        namespace zk {
            namespace snark {

                /**
                 * An accumulation vector comprises an accumulation value and a sparse vector.
                 * The method "accumulate_chunk" allows one to accumlate portions of the sparse
                 * vector into the accumualation value.
                 */
                template<typename T>
                class accumulation_vector {
                public:
                    T first;
                    sparse_vector<T> rest;

                    accumulation_vector() = default;
                    accumulation_vector(const accumulation_vector<T> &other) = default;
                    accumulation_vector(accumulation_vector<T> &&other) = default;
                    accumulation_vector(T &&first, sparse_vector<T> &&rest) :
                        first(std::move(first)), rest(std::move(rest)) {};
                    accumulation_vector(T &&first, std::vector<T> &&v) : first(std::move(first)), rest(std::move(v)) {
                    }
                    accumulation_vector(std::vector<T> &&v) : first(T::zero()), rest(std::move(v)) {};

                    accumulation_vector<T> &operator=(const accumulation_vector<T> &other) = default;
                    accumulation_vector<T> &operator=(accumulation_vector<T> &&other) = default;

                    bool operator==(const accumulation_vector<T> &other) const {
                        return (this->first == other.first && this->rest == other.rest);
                    }

                    bool is_fully_accumulated() const {
                        return rest.empty();
                    }

                    std::size_t domain_size() const {
                        return rest.domain_size();
                    }

                    std::size_t size() const {
                        return rest.domain_size();
                    }

                    std::size_t size_in_bits() const {
                        const std::size_t first_size_in_bits = T::size_in_bits();
                        const std::size_t rest_size_in_bits = rest.size_in_bits();
                        return first_size_in_bits + rest_size_in_bits;
                    }

                    template<typename InputIterator>
                    accumulation_vector<T> accumulate_chunk(InputIterator it_begin, InputIterator it_end,
                                                            std::size_t offset) const {
                        std::pair<T, sparse_vector<T>> acc_result =
                            rest.template accumulate<InputIterator>(it_begin, it_end, offset);
                        T new_first = first + acc_result.first;
                        return accumulation_vector<T>(std::move(new_first), std::move(acc_result.second));
                    }
                };

            }    // namespace snark
        }        // namespace zk
    }            // namespace crypto3
}    // namespace nil

#endif    // CRYPTO3_ZK_SNARK_ACCUMULATION_VECTOR_HPP

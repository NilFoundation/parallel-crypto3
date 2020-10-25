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
// @file Declaration of interfaces for components for the SHA256 message schedule and round function.
//---------------------------------------------------------------------------//

#ifndef CRYPTO3_ZK_SHA256_COMPONENTS_HPP
#define CRYPTO3_ZK_SHA256_COMPONENTS_HPP

#include <nil/crypto3/hash/sha2.hpp>

#include <nil/crypto3/zk/snark/components/basic_components.hpp>
#include <nil/crypto3/zk/snark/components/hashes/hash_io.hpp>
#include <nil/crypto3/zk/snark/components/hashes/sha256/sha256_aux.hpp>

namespace nil {
    namespace crypto3 {
        namespace zk {
            namespace snark {
                template<typename FieldType>
                blueprint_linear_combination_vector<FieldType> SHA256_default_IV(blueprint<FieldType> &pb);

                template<typename FieldType>
                class sha256_message_schedule_component : public component<FieldType> {
                public:
                    std::vector<blueprint_variable_vector<FieldType>> W_bits;
                    std::vector<std::shared_ptr<packing_component<FieldType>>> pack_W;

                    std::vector<variable<FieldType>> sigma0;
                    std::vector<variable<FieldType>> sigma1;
                    std::vector<std::shared_ptr<small_sigma_component<FieldType>>> compute_sigma0;
                    std::vector<std::shared_ptr<small_sigma_component<FieldType>>> compute_sigma1;
                    std::vector<variable<FieldType>> unreduced_W;
                    std::vector<std::shared_ptr<lastbits_component<FieldType>>> mod_reduce_W;

                public:
                    blueprint_variable_vector<FieldType> M;
                    blueprint_variable_vector<FieldType> packed_W;
                    sha256_message_schedule_component(blueprint<FieldType> &pb,
                                                      const blueprint_variable_vector<FieldType> &M,
                                                      const blueprint_variable_vector<FieldType> &packed_W);
                    void generate_r1cs_constraints();
                    void generate_r1cs_witness();
                };

                template<typename FieldType>
                class sha256_round_function_component : public component<FieldType> {
                public:
                    variable<FieldType> sigma0;
                    variable<FieldType> sigma1;
                    std::shared_ptr<big_sigma_component<FieldType>> compute_sigma0;
                    std::shared_ptr<big_sigma_component<FieldType>> compute_sigma1;
                    variable<FieldType> choice;
                    variable<FieldType> majority;
                    std::shared_ptr<choice_component<FieldType>> compute_choice;
                    std::shared_ptr<majority_component<FieldType>> compute_majority;
                    variable<FieldType> packed_d;
                    std::shared_ptr<packing_component<FieldType>> pack_d;
                    variable<FieldType> packed_h;
                    std::shared_ptr<packing_component<FieldType>> pack_h;
                    variable<FieldType> unreduced_new_a;
                    variable<FieldType> unreduced_new_e;
                    std::shared_ptr<lastbits_component<FieldType>> mod_reduce_new_a;
                    std::shared_ptr<lastbits_component<FieldType>> mod_reduce_new_e;
                    variable<FieldType> packed_new_a;
                    variable<FieldType> packed_new_e;

                public:
                    blueprint_linear_combination_vector<FieldType> a;
                    blueprint_linear_combination_vector<FieldType> b;
                    blueprint_linear_combination_vector<FieldType> c;
                    blueprint_linear_combination_vector<FieldType> d;
                    blueprint_linear_combination_vector<FieldType> e;
                    blueprint_linear_combination_vector<FieldType> f;
                    blueprint_linear_combination_vector<FieldType> g;
                    blueprint_linear_combination_vector<FieldType> h;
                    variable<FieldType> W;
                    long K;
                    blueprint_linear_combination_vector<FieldType> new_a;
                    blueprint_linear_combination_vector<FieldType> new_e;

                    sha256_round_function_component(blueprint<FieldType> &pb,
                                                    const blueprint_linear_combination_vector<FieldType> &a,
                                                    const blueprint_linear_combination_vector<FieldType> &b,
                                                    const blueprint_linear_combination_vector<FieldType> &c,
                                                    const blueprint_linear_combination_vector<FieldType> &d,
                                                    const blueprint_linear_combination_vector<FieldType> &e,
                                                    const blueprint_linear_combination_vector<FieldType> &f,
                                                    const blueprint_linear_combination_vector<FieldType> &g,
                                                    const blueprint_linear_combination_vector<FieldType> &h,
                                                    const variable<FieldType> &W,
                                                    const long &K,
                                                    const blueprint_linear_combination_vector<FieldType> &new_a,
                                                    const blueprint_linear_combination_vector<FieldType> &new_e);

                    void generate_r1cs_constraints();
                    void generate_r1cs_witness();
                };

                template<typename FieldType>
                blueprint_linear_combination_vector<FieldType> SHA256_default_IV(blueprint<FieldType> &pb) {
                    using namespace hashes::detail;

                    typename sha2_policy<256>::state_type iv = sha2_policy<256>::iv_generator()();

                    blueprint_linear_combination_vector<FieldType> result;
                    result.reserve(hashes::sha2<256>::digest_bits);

                    for (std::size_t i = 0; i < hashes::sha2<256>::digest_bits; ++i) {
                        int iv_val =
                            iv[i / hashes::sha2<256>::word_bits] >> (31 - (i % hashes::sha2<256>::word_bits)) & 1;

                        blueprint_linear_combination<FieldType> iv_element;
                        iv_element.assign(pb, iv_val * variable<FieldType>(0));
                        iv_element.evaluate(pb);

                        result.emplace_back(iv_element);
                    }

                    return result;
                }

                template<typename FieldType>
                sha256_message_schedule_component<FieldType>::sha256_message_schedule_component(
                    blueprint<FieldType> &pb,
                    const blueprint_variable_vector<FieldType> &M,
                    const blueprint_variable_vector<FieldType> &packed_W) :
                    component<FieldType>(pb),
                    M(M), packed_W(packed_W) {
                    W_bits.resize(64);

                    pack_W.resize(16);
                    for (std::size_t i = 0; i < 16; ++i) {
                        W_bits[i] =
                            blueprint_variable_vector<FieldType>(M.rbegin() + (15 - i) * hashes::sha2<256>::word_bits,
                                                                 M.rbegin() + (16 - i) * hashes::sha2<256>::word_bits);
                        pack_W[i].reset(new packing_component<FieldType>(pb, W_bits[i], packed_W[i]));
                    }

                    /* NB: some of those will be un-allocated */
                    sigma0.resize(64);
                    sigma1.resize(64);
                    compute_sigma0.resize(64);
                    compute_sigma1.resize(64);
                    unreduced_W.resize(64);
                    mod_reduce_W.resize(64);

                    for (std::size_t i = 16; i < block::detail::shacal2_policy<256>::rounds; ++i) {
                        /* allocate result variables for sigma0/sigma1 invocations */
                        sigma0[i].allocate(pb);
                        sigma1[i].allocate(pb);

                        /* compute sigma0/sigma1 */
                        compute_sigma0[i].reset(
                            new small_sigma_component<FieldType>(pb, W_bits[i - 15], sigma0[i], 7, 18, 3));
                        compute_sigma1[i].reset(
                            new small_sigma_component<FieldType>(pb, W_bits[i - 2], sigma1[i], 17, 19, 10));

                        /* unreduced_W = sigma0(W_{i-15}) + sigma1(W_{i-2}) + W_{i-7} + W_{i-16} before modulo 2^32 */
                        unreduced_W[i].allocate(pb);

                        /* allocate the bit representation of packed_W[i] */
                        W_bits[i].allocate(pb, hashes::sha2<256>::word_bits);

                        /* and finally reduce this into packed and bit representations */
                        mod_reduce_W[i].reset(new lastbits_component<FieldType>(
                            pb, unreduced_W[i], hashes::sha2<256>::word_bits + 2, packed_W[i], W_bits[i]));
                    }
                }

                template<typename FieldType>
                void sha256_message_schedule_component<FieldType>::generate_r1cs_constraints() {
                    for (std::size_t i = 0; i < 16; ++i) {
                        pack_W[i]->generate_r1cs_constraints(false);    // do not enforce bitness here; caller be aware.
                    }

                    for (std::size_t i = 16; i < block::detail::shacal2_policy<256>::rounds; ++i) {
                        compute_sigma0[i]->generate_r1cs_constraints();
                        compute_sigma1[i]->generate_r1cs_constraints();

                        this->pb.add_r1cs_constraint(r1cs_constraint<FieldType>(
                            1, sigma0[i] + sigma1[i] + packed_W[i - 16] + packed_W[i - 7], unreduced_W[i]));

                        mod_reduce_W[i]->generate_r1cs_constraints();
                    }
                }

                template<typename FieldType>
                void sha256_message_schedule_component<FieldType>::generate_r1cs_witness() {
                    for (std::size_t i = 0; i < 16; ++i) {
                        pack_W[i]->generate_r1cs_witness_from_bits();
                    }

                    for (std::size_t i = 16; i < block::detail::shacal2_policy<256>::rounds; ++i) {
                        compute_sigma0[i]->generate_r1cs_witness();
                        compute_sigma1[i]->generate_r1cs_witness();

                        this->pb.val(unreduced_W[i]) = this->pb.val(sigma0[i]) + this->pb.val(sigma1[i]) +
                                                       this->pb.val(packed_W[i - 16]) + this->pb.val(packed_W[i - 7]);
                        mod_reduce_W[i]->generate_r1cs_witness();
                    }
                }

                template<typename FieldType>
                sha256_round_function_component<FieldType>::sha256_round_function_component(
                    blueprint<FieldType> &pb,
                    const blueprint_linear_combination_vector<FieldType> &a,
                    const blueprint_linear_combination_vector<FieldType> &b,
                    const blueprint_linear_combination_vector<FieldType> &c,
                    const blueprint_linear_combination_vector<FieldType> &d,
                    const blueprint_linear_combination_vector<FieldType> &e,
                    const blueprint_linear_combination_vector<FieldType> &f,
                    const blueprint_linear_combination_vector<FieldType> &g,
                    const blueprint_linear_combination_vector<FieldType> &h,
                    const variable<FieldType> &W,
                    const long &K,
                    const blueprint_linear_combination_vector<FieldType> &new_a,
                    const blueprint_linear_combination_vector<FieldType> &new_e) :
                    component<FieldType>(pb),
                    a(a), b(b), c(c), d(d), e(e), f(f), g(g), h(h), W(W), K(K), new_a(new_a), new_e(new_e) {
                    /* compute sigma0 and sigma1 */
                    sigma0.allocate(pb);
                    sigma1.allocate(pb);
                    compute_sigma0.reset(new big_sigma_component<FieldType>(pb, a, sigma0, 2, 13, 22));
                    compute_sigma1.reset(new big_sigma_component<FieldType>(pb, e, sigma1, 6, 11, 25));

                    /* compute choice */
                    choice.allocate(pb);
                    compute_choice.reset(new choice_component<FieldType>(pb, e, f, g, choice));

                    /* compute majority */
                    majority.allocate(pb);
                    compute_majority.reset(new majority_component<FieldType>(pb, a, b, c, majority));

                    /* pack d */
                    packed_d.allocate(pb);
                    pack_d.reset(new packing_component<FieldType>(pb, d, packed_d));

                    /* pack h */
                    packed_h.allocate(pb);
                    pack_h.reset(new packing_component<FieldType>(pb, h, packed_h));

                    /* compute the actual results for the round */
                    unreduced_new_a.allocate(pb);
                    unreduced_new_e.allocate(pb);

                    packed_new_a.allocate(pb);
                    packed_new_e.allocate(pb);

                    mod_reduce_new_a.reset(new lastbits_component<FieldType>(
                        pb, unreduced_new_a, hashes::sha2<256>::word_bits + 3, packed_new_a, new_a));
                    mod_reduce_new_e.reset(new lastbits_component<FieldType>(
                        pb, unreduced_new_e, hashes::sha2<256>::word_bits + 3, packed_new_e, new_e));
                }

                template<typename FieldType>
                void sha256_round_function_component<FieldType>::generate_r1cs_constraints() {
                    compute_sigma0->generate_r1cs_constraints();
                    compute_sigma1->generate_r1cs_constraints();

                    compute_choice->generate_r1cs_constraints();
                    compute_majority->generate_r1cs_constraints();

                    pack_d->generate_r1cs_constraints(false);
                    pack_h->generate_r1cs_constraints(false);

                    this->pb.add_r1cs_constraint(r1cs_constraint<FieldType>(
                        1, packed_h + sigma1 + choice + K + W + sigma0 + majority, unreduced_new_a));

                    this->pb.add_r1cs_constraint(
                        r1cs_constraint<FieldType>(1, packed_d + packed_h + sigma1 + choice + K + W, unreduced_new_e));

                    mod_reduce_new_a->generate_r1cs_constraints();
                    mod_reduce_new_e->generate_r1cs_constraints();
                }

                template<typename FieldType>
                void sha256_round_function_component<FieldType>::generate_r1cs_witness() {
                    compute_sigma0->generate_r1cs_witness();
                    compute_sigma1->generate_r1cs_witness();

                    compute_choice->generate_r1cs_witness();
                    compute_majority->generate_r1cs_witness();

                    pack_d->generate_r1cs_witness_from_bits();
                    pack_h->generate_r1cs_witness_from_bits();

                    this->pb.val(unreduced_new_a) = this->pb.val(packed_h) + this->pb.val(sigma1) +
                                                    this->pb.val(choice) + typename FieldType::value_type(K) +
                                                    this->pb.val(W) + this->pb.val(sigma0) + this->pb.val(majority);
                    this->pb.val(unreduced_new_e) = this->pb.val(packed_d) + this->pb.val(packed_h) +
                                                    this->pb.val(sigma1) + this->pb.val(choice) +
                                                    typename FieldType::value_type(K) + this->pb.val(W);

                    mod_reduce_new_a->generate_r1cs_witness();
                    mod_reduce_new_e->generate_r1cs_witness();
                }

            }    // namespace snark
        }        // namespace zk
    }            // namespace crypto3
}    // namespace nil

#endif    // CRYPTO3_ZK_SHA256_COMPONENTS_HPP

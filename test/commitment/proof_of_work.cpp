//---------------------------------------------------------------------------//
// Copyright (c) 2024 Vasiliy Olekhov <vasiliy.olekhov@nil.foundation>
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

//#define BOOST_TEST_MODULE proof_of_work_test

#include <string>
#include <random>
#include <regex>

#include <nil/actor/testing/random.hh>
#include <nil/actor/testing/test_case.hh>
#include <nil/actor/testing/thread_test_case.hh>

#include <nil/crypto3/algebra/curves/pallas.hpp>
#include <nil/crypto3/algebra/fields/pallas/scalar_field.hpp>
#include <nil/crypto3/algebra/fields/pallas/base_field.hpp>

#include <nil/actor/zk/commitments/detail/polynomial/proof_of_work.hpp>

#include <nil/crypto3/hash/poseidon.hpp>
#include <nil/crypto3/hash/detail/poseidon/poseidon_policy.hpp>
#include <nil/crypto3/hash/keccak.hpp>

#include <nil/actor/zk/transcript/fiat_shamir.hpp>

std::size_t test_global_seed = 0;
boost::random::mt11213b test_global_rnd_engine;
template <typename FieldType>
nil::crypto3::random::algebraic_engine<FieldType> test_global_alg_rnd_engine;

struct test_fixture {
    // Enumerate all fields used in tests;
    using field1_type = nil::crypto3::algebra::curves::pallas::base_field_type;

    test_fixture(){
        test_global_seed = 0;

        for( std::size_t i = 0; i + 1 < boost::unit_test::framework::master_test_suite().argc; i++){
            if(std::string(boost::unit_test::framework::master_test_suite().argv[i]) == "--seed"){
                if(std::string(boost::unit_test::framework::master_test_suite().argv[i+1]) == "random"){
                    std::random_device rd;
                    test_global_seed = rd();
                    std::cout << "Random seed=" << test_global_seed << std::endl;
                    break;
                }
                if(std::regex_match( boost::unit_test::framework::master_test_suite().argv[i+1], std::regex( ( "((\\+|-)?[[:digit:]]+)(\\.(([[:digit:]]+)?))?" ) ) ) ){
                    test_global_seed = atoi(boost::unit_test::framework::master_test_suite().argv[i+1]);
                    break;
                }
            }
        }

        BOOST_TEST_MESSAGE("test_global_seed = " << test_global_seed);
        test_global_rnd_engine = boost::random::mt11213b(test_global_seed);
        test_global_alg_rnd_engine<field1_type> = nil::crypto3::random::algebraic_engine<field1_type>(test_global_seed);
    }
    ~test_fixture(){}
};


using namespace nil::crypto3::algebra;
using namespace nil::actor::zk::commitments;

ACTOR_FIXTURE_TEST_CASE(pow_poseidon_basic_test, test_fixture) {
    using curve_type = curves::pallas;
    using field_type = curve_type::base_field_type;
    using integral_type = typename field_type::integral_type;
    using policy = nil::crypto3::hashes::detail::mina_poseidon_policy<field_type>;
    using poseidon = nil::crypto3::hashes::poseidon<policy>;
    using pow_type = nil::actor::zk::commitments::field_proof_of_work<poseidon, field_type, 16>;

    const integral_type expected_mask = integral_type(0xFFFF000000000000) << (field_type::modulus_bits - 64);
    nil::actor::zk::transcript::fiat_shamir_heuristic_sequential<poseidon> transcript;
    auto old_transcript_1 = transcript, old_transcript_2 = transcript;

    auto seed = nil::actor::testing::local_random_engine();

    nil::crypto3::random::algebraic_engine<field_type> rnd_engine = nil::crypto3::random::algebraic_engine<field_type>(seed);

    auto result = pow_type::generate(transcript,  rnd_engine);
    BOOST_ASSERT(expected_mask == pow_type::mask);
    BOOST_ASSERT(pow_type::verify(old_transcript_1, result));

    // manually reimplement verify to ensure that changes in implementation didn't break it
    old_transcript_2(result);
    auto chal = old_transcript_2.template challenge<field_type>();
    BOOST_ASSERT((integral_type(chal.data) & expected_mask) == 0);

    using hard_pow_type = nil::actor::zk::commitments::field_proof_of_work<poseidon, field_type, 32>;
    // check that random stuff doesn't pass verify
    BOOST_ASSERT(!hard_pow_type::verify(old_transcript_1, result));
}

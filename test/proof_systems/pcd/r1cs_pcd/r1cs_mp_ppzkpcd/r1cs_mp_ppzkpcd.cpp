//---------------------------------------------------------------------------//
// Copyright (c) 2018-2020 Mikhail Komarov <nemo@nil.foundation>
// Copyright (c) 2020 Nikita Kaskov <nbering@nil.foundation>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE r1cs_mp_ppzkpcd_test

#include <boost/test/unit_test.hpp>

#include "run_r1cs_mp_ppzkpcd.hpp"

using namespace nil::crypto3::zk::snark;

template<typename PCD_ppT>
void test_tally(std::size_t arity, std::size_t max_layer, bool test_multi_type, bool test_same_type_optimization) {
    const std::size_t wordsize = 32;
    const bool bit = run_r1cs_mp_ppzkpcd_tally_example<PCD_ppT>(wordsize, arity, max_layer,
                                                                test_multi_type, test_same_type_optimization);
    BOOST_CHECK(bit);
}

BOOST_AUTO_TEST_SUITE(r1cs_mp_ppzkpcd_test_suite)

BOOST_AUTO_TEST_CASE(r1cs_mp_ppzkpcd_test_case) {
    const std::size_t max_arity = 2;
    const std::size_t max_layer = 2;

    test_tally<default_r1cs_ppzkpcd_pp>(max_arity, max_layer, false, false);
    test_tally<default_r1cs_ppzkpcd_pp>(max_arity, max_layer, false, true);
    test_tally<default_r1cs_ppzkpcd_pp>(max_arity, max_layer, true, false);
    test_tally<default_r1cs_ppzkpcd_pp>(max_arity, max_layer, true, true);
}

BOOST_AUTO_TEST_SUITE_END()
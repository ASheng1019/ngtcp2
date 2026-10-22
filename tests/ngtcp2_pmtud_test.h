/*
 * ngtcp2
 *
 * Copyright (c) 2022 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef NGTCP2_PMTUD_TEST_H
#define NGTCP2_PMTUD_TEST_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* defined(HAVE_CONFIG_H) */

#define MUNIT_ENABLE_ASSERT_ALIASES

#include "munit.h"

extern const MunitSuite pmtud_suite;

munit_void_test_decl(test_ngtcp2_pmtud_probe)
munit_void_test_decl(test_ngtcp2_pmtud_blackhole_burst_once)
munit_void_test_decl(test_ngtcp2_pmtud_blackhole_separate_bursts)
munit_void_test_decl(test_ngtcp2_pmtud_blackhole_mixed_loss)
munit_void_test_decl(test_ngtcp2_pmtud_blackhole_small_ack)
munit_void_test_decl(test_ngtcp2_pmtud_blackhole_large_ack)
munit_void_test_decl(test_ngtcp2_pmtud_blackhole_old_ack)
munit_void_test_decl(test_ngtcp2_pmtud_blackhole_epoch)
munit_void_test_decl(test_ngtcp2_pmtud_blackhole_bounded)
munit_void_test_decl(test_ngtcp2_pmtud_blackhole_ordinary_loss)

#endif /* !defined(NGTCP2_PMTUD_TEST_H) */

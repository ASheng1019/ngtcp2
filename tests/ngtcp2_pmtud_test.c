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
#include "ngtcp2_pmtud_test.h"

#include <stdio.h>

#include "ngtcp2_pmtud.h"
#include "ngtcp2_test_helper.h"

static const MunitTest tests[] = {
  munit_void_test(test_ngtcp2_pmtud_probe),
  munit_void_test(test_ngtcp2_pmtud_blackhole_burst_once),
  munit_void_test(test_ngtcp2_pmtud_blackhole_separate_bursts),
  munit_void_test(test_ngtcp2_pmtud_blackhole_mixed_loss),
  munit_void_test(test_ngtcp2_pmtud_blackhole_small_ack),
  munit_void_test(test_ngtcp2_pmtud_blackhole_large_ack),
  munit_void_test(test_ngtcp2_pmtud_blackhole_old_ack),
  munit_void_test(test_ngtcp2_pmtud_blackhole_epoch),
  munit_void_test(test_ngtcp2_pmtud_blackhole_bounded),
  munit_void_test(test_ngtcp2_pmtud_blackhole_ordinary_loss),
  munit_test_end(),
};

const MunitSuite pmtud_suite = {
  .prefix = "/pmtud",
  .tests = tests,
};

void test_ngtcp2_pmtud_probe(void) {
  const ngtcp2_mem *mem = ngtcp2_mem_default();
  ngtcp2_pmtud *pmtud;
  int rv;
  static const uint16_t probes[] = {
    3000 - 48,
    9000 - 48,
  };

  /* Send probe and get success */
  rv = ngtcp2_pmtud_new(&pmtud, NGTCP2_MAX_UDP_PAYLOAD_SIZE, 1452, 0, NULL, 0,
                        mem);

  assert_int(0, ==, rv);
  assert_size(0, ==, pmtud->mtu_idx);
  assert_false(ngtcp2_pmtud_finished(pmtud));
  assert_true(ngtcp2_pmtud_require_probe(pmtud));
  assert_size(1454 - 48, ==, ngtcp2_pmtud_probelen(pmtud));

  ngtcp2_pmtud_probe_sent(pmtud, 2, 0);

  assert_size(1, ==, pmtud->num_pkts_sent);
  assert_uint64(2, ==, pmtud->expiry);
  assert_false(ngtcp2_pmtud_require_probe(pmtud));

  ngtcp2_pmtud_handle_expiry(pmtud, 1);

  assert_false(ngtcp2_pmtud_require_probe(pmtud));

  ngtcp2_pmtud_handle_expiry(pmtud, 2);

  assert_true(ngtcp2_pmtud_require_probe(pmtud));

  ngtcp2_pmtud_probe_sent(pmtud, 2, 2);

  assert_size(2, ==, pmtud->num_pkts_sent);
  assert_uint64(4, ==, pmtud->expiry);
  assert_false(ngtcp2_pmtud_require_probe(pmtud));

  ngtcp2_pmtud_handle_expiry(pmtud, 4);

  assert_true(ngtcp2_pmtud_require_probe(pmtud));

  ngtcp2_pmtud_probe_sent(pmtud, 2, 4);

  assert_size(3, ==, pmtud->num_pkts_sent);
  assert_uint64(10, ==, pmtud->expiry);
  assert_false(ngtcp2_pmtud_require_probe(pmtud));

  ngtcp2_pmtud_probe_success(pmtud, ngtcp2_pmtud_probelen(pmtud));

  assert_size(3, ==, pmtud->mtu_idx);
  assert_uint64(UINT64_MAX, ==, pmtud->expiry);
  assert_size(0, ==, pmtud->num_pkts_sent);
  assert_true(ngtcp2_pmtud_require_probe(pmtud));
  assert_size(1492 - 48, ==, ngtcp2_pmtud_probelen(pmtud));

  ngtcp2_pmtud_probe_sent(pmtud, 2, 10);
  ngtcp2_pmtud_handle_expiry(pmtud, 12);
  ngtcp2_pmtud_probe_sent(pmtud, 2, 12);
  ngtcp2_pmtud_handle_expiry(pmtud, 14);
  ngtcp2_pmtud_probe_sent(pmtud, 2, 14);
  ngtcp2_pmtud_handle_expiry(pmtud, 20);

  assert_size(1492 - 48, ==, pmtud->min_fail_udp_payload_size);
  assert_true(ngtcp2_pmtud_finished(pmtud));

  ngtcp2_pmtud_del(pmtud);

  /* Failing 2nd probe should skip the third probe */
  rv = ngtcp2_pmtud_new(&pmtud, NGTCP2_MAX_UDP_PAYLOAD_SIZE, 1452, 0, NULL, 0,
                        mem);

  ngtcp2_pmtud_probe_sent(pmtud, 2, 0);
  ngtcp2_pmtud_handle_expiry(pmtud, 2);
  ngtcp2_pmtud_probe_sent(pmtud, 2, 2);
  ngtcp2_pmtud_handle_expiry(pmtud, 4);
  ngtcp2_pmtud_probe_sent(pmtud, 2, 4);
  ngtcp2_pmtud_handle_expiry(pmtud, 10);

  assert_size(1454 - 48, ==, pmtud->min_fail_udp_payload_size);
  assert_size(1, ==, pmtud->mtu_idx);

  ngtcp2_pmtud_probe_sent(pmtud, 2, 10);
  ngtcp2_pmtud_handle_expiry(pmtud, 12);
  ngtcp2_pmtud_probe_sent(pmtud, 2, 12);
  ngtcp2_pmtud_handle_expiry(pmtud, 14);
  ngtcp2_pmtud_probe_sent(pmtud, 2, 14);
  ngtcp2_pmtud_handle_expiry(pmtud, 20);

  assert_size(1390 - 48, ==, pmtud->min_fail_udp_payload_size);
  assert_size(2, ==, pmtud->mtu_idx);

  ngtcp2_pmtud_probe_sent(pmtud, 2, 10);
  ngtcp2_pmtud_probe_success(pmtud, 1280 - 48);

  assert_true(ngtcp2_pmtud_finished(pmtud));

  ngtcp2_pmtud_del(pmtud);

  /* Skip 1st probe because it is larger than hard max. */
  rv = ngtcp2_pmtud_new(&pmtud, NGTCP2_MAX_UDP_PAYLOAD_SIZE, 1454 - 48 - 1, 0,
                        NULL, 0, mem);

  assert_int(0, ==, rv);
  assert_size(1, ==, pmtud->mtu_idx);

  ngtcp2_pmtud_del(pmtud);

  /* PMTUD finishes immediately because we know that all candidates
     are lower than the current maximum. */
  rv = ngtcp2_pmtud_new(&pmtud, 1492 - 48, 1452, 0, NULL, 0, mem);

  assert_int(0, ==, rv);
  assert_true(ngtcp2_pmtud_finished(pmtud));

  ngtcp2_pmtud_del(pmtud);

  /* PMTUD finishes immediately because the hard maximum size is lower
     than the candidates. */
  rv = ngtcp2_pmtud_new(&pmtud, NGTCP2_MAX_UDP_PAYLOAD_SIZE,
                        NGTCP2_MAX_UDP_PAYLOAD_SIZE, 0, NULL, 0, mem);

  assert_int(0, ==, rv);
  assert_true(ngtcp2_pmtud_finished(pmtud));

  ngtcp2_pmtud_del(pmtud);

  /* Custom probes */
  rv = ngtcp2_pmtud_new(&pmtud, NGTCP2_MAX_UDP_PAYLOAD_SIZE, 9000 - 48, 0,
                        probes, ngtcp2_arraylen(probes), mem);

  assert_int(0, ==, rv);
  assert_size(3000 - 48, ==, ngtcp2_pmtud_probelen(pmtud));

  ngtcp2_pmtud_probe_sent(pmtud, 230, 7);
  ngtcp2_pmtud_probe_success(pmtud, 3000 - 48);

  assert_false(ngtcp2_pmtud_finished(pmtud));
  assert_size(9000 - 48, ==, ngtcp2_pmtud_probelen(pmtud));

  ngtcp2_pmtud_probe_sent(pmtud, 230, 9);
  ngtcp2_pmtud_probe_success(pmtud, 9000 - 48);

  assert_true(ngtcp2_pmtud_finished(pmtud));

  ngtcp2_pmtud_del(pmtud);
}



/* Loss callbacks are ordered by decreasing packet number, as in ngtcp2 RTB. */
void test_ngtcp2_pmtud_blackhole_burst_once(void) {
  ngtcp2_pmtud_blackhole bh;
  int64_t pn;
  ngtcp2_pmtud_blackhole_init(&bh, 0);
  for (pn = 40; pn > 0; --pn) {
    ngtcp2_pmtud_blackhole_lost(&bh, pn, 1400);
  }
  assert_false(ngtcp2_pmtud_blackhole_detected(&bh));
  assert_size(1, ==, bh.num_bursts);
}

void test_ngtcp2_pmtud_blackhole_separate_bursts(void) {
  ngtcp2_pmtud_blackhole bh;
  int64_t pn;
  ngtcp2_pmtud_blackhole_init(&bh, 0);
  for (pn = 20; pn >= 2; pn -= 2) {
    ngtcp2_pmtud_blackhole_lost(&bh, pn, 1400);
  }
  assert_true(ngtcp2_pmtud_blackhole_detected(&bh));
  assert_size(4, ==, bh.num_bursts);
}

void test_ngtcp2_pmtud_blackhole_mixed_loss(void) {
  ngtcp2_pmtud_blackhole bh;
  int i;
  ngtcp2_pmtud_blackhole_init(&bh, 0);
  for (i = 0; i < 20; ++i) {
    ngtcp2_pmtud_blackhole_lost(&bh, 3 * i + 2, 1400);
    ngtcp2_pmtud_blackhole_lost(&bh, 3 * i + 1, 1200);
    assert_false(ngtcp2_pmtud_blackhole_detected(&bh));
  }
  assert_size(0, ==, bh.num_bursts);
}

void test_ngtcp2_pmtud_blackhole_small_ack(void) {
  ngtcp2_pmtud_blackhole bh;
  int i;
  ngtcp2_pmtud_blackhole_init(&bh, 0);
  for (i = 0; i < 4; ++i) {
    ngtcp2_pmtud_blackhole_lost(&bh, 2 * i, 1400);
    ngtcp2_pmtud_blackhole_ack(&bh, 2 * i + 1, 1100);
    assert_int(i == 3, ==, ngtcp2_pmtud_blackhole_detected(&bh));
  }
}

void test_ngtcp2_pmtud_blackhole_large_ack(void) {
  ngtcp2_pmtud_blackhole bh;
  int i;
  ngtcp2_pmtud_blackhole_init(&bh, 0);
  for (i = 0; i < 3; ++i) {
    ngtcp2_pmtud_blackhole_lost(&bh, 2 * i, 1400);
    assert_false(ngtcp2_pmtud_blackhole_detected(&bh));
  }
  ngtcp2_pmtud_blackhole_ack(&bh, 9, 1400);
  assert_size(0, ==, bh.num_bursts);
  /* Equal-sized subsequent delivery must advance the evidence's packet number. */
  ngtcp2_pmtud_blackhole_ack(&bh, 20, 1400);
  ngtcp2_pmtud_blackhole_lost(&bh, 18, 1400);
  assert_false(ngtcp2_pmtud_blackhole_detected(&bh));
  assert_size(0, ==, bh.num_bursts);
}

void test_ngtcp2_pmtud_blackhole_old_ack(void) {
  ngtcp2_pmtud_blackhole bh;
  ngtcp2_pmtud_blackhole_init(&bh, 0);
  ngtcp2_pmtud_blackhole_lost(&bh, 20, 1400);
  assert_false(ngtcp2_pmtud_blackhole_detected(&bh));
  ngtcp2_pmtud_blackhole_ack(&bh, 10, 1450);
  assert_size(1, ==, bh.num_bursts);
  ngtcp2_pmtud_blackhole_lost(&bh, 22, 1400);
  assert_false(ngtcp2_pmtud_blackhole_detected(&bh));
  assert_size(2, ==, bh.num_bursts);
}

void test_ngtcp2_pmtud_blackhole_epoch(void) {
  ngtcp2_pmtud_blackhole bh;
  int i;
  ngtcp2_pmtud_blackhole_init(&bh, 100);
  for (i = 0; i < 10; ++i) {
    ngtcp2_pmtud_blackhole_lost(&bh, i, 1400);
    ngtcp2_pmtud_blackhole_ack(&bh, i + 1, 1450);
    assert_false(ngtcp2_pmtud_blackhole_detected(&bh));
  }
  assert_size(0, ==, bh.num_bursts);
  assert_int64(-1, ==, bh.acked_pkt_num);
}

void test_ngtcp2_pmtud_blackhole_bounded(void) {
  ngtcp2_pmtud_blackhole bh;
  int64_t pn;
  ngtcp2_pmtud_blackhole_init(&bh, 0);
  for (pn = 2000; pn > 0; pn -= 2) {
    ngtcp2_pmtud_blackhole_lost(&bh, pn, 1400);
  }
  assert_true(ngtcp2_pmtud_blackhole_detected(&bh));
  assert_size(4, ==, bh.num_bursts);
  ngtcp2_pmtud_blackhole_ack(&bh, 2001, 1400);
  assert_false(ngtcp2_pmtud_blackhole_detected(&bh));
}

void test_ngtcp2_pmtud_blackhole_ordinary_loss(void) {
  ngtcp2_pmtud_blackhole bh;
  int i;
  ngtcp2_pmtud_blackhole_init(&bh, 0);
  for (i = 0; i < 1000; ++i) {
    ngtcp2_pmtud_blackhole_lost(&bh, 2 * i, 1400);
    assert_false(ngtcp2_pmtud_blackhole_detected(&bh));
    ngtcp2_pmtud_blackhole_ack(&bh, 2 * i + 1, 1400);
  }
  assert_size(0, ==, bh.num_bursts);
}

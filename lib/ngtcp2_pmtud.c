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
#include "ngtcp2_pmtud.h"

#include <assert.h>

#include "ngtcp2_mem.h"
#include "ngtcp2_macro.h"

/* NGTCP2_PMTUD_PROBE_NUM_MAX is the maximum number of packets sent
   for each probe. */
#define NGTCP2_PMTUD_PROBE_NUM_MAX 3

static uint16_t pmtud_default_probes[] = {
  1454 - 48, /* The well known MTU used by a domestic optic fiber
                service in Japan. */
  1390 - 48, /* Typical Tunneled MTU */
  1280 - 48, /* IPv6 minimum MTU */
  1492 - 48, /* PPPoE */
};

int ngtcp2_pmtud_new(ngtcp2_pmtud **ppmtud, size_t max_udp_payload_size,
                     size_t hard_max_udp_payload_size, int64_t tx_pkt_num,
                     const uint16_t *probes, size_t probeslen,
                     const ngtcp2_mem *mem) {
  ngtcp2_pmtud *pmtud = ngtcp2_mem_malloc(mem, sizeof(ngtcp2_pmtud));

  if (pmtud == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  pmtud->mem = mem;
  pmtud->mtu_idx = 0;
  pmtud->num_pkts_sent = 0;
  pmtud->expiry = UINT64_MAX;
  pmtud->tx_pkt_num = tx_pkt_num;
  pmtud->max_udp_payload_size = max_udp_payload_size;
  pmtud->hard_max_udp_payload_size = hard_max_udp_payload_size;
  pmtud->min_fail_udp_payload_size = SIZE_MAX;

  if (probeslen) {
    pmtud->probes = probes;
    pmtud->probeslen = probeslen;
  } else {
    pmtud->probes = pmtud_default_probes;
    pmtud->probeslen = ngtcp2_arraylen(pmtud_default_probes);
  }

  for (; pmtud->mtu_idx < pmtud->probeslen; ++pmtud->mtu_idx) {
    if (pmtud->probes[pmtud->mtu_idx] > pmtud->hard_max_udp_payload_size) {
      continue;
    }
    if (pmtud->probes[pmtud->mtu_idx] > pmtud->max_udp_payload_size) {
      break;
    }
  }

  *ppmtud = pmtud;

  return 0;
}

void ngtcp2_pmtud_del(ngtcp2_pmtud *pmtud) {
  if (!pmtud) {
    return;
  }

  ngtcp2_mem_free(pmtud->mem, pmtud);
}

size_t ngtcp2_pmtud_probelen(ngtcp2_pmtud *pmtud) {
  assert(pmtud->mtu_idx < pmtud->probeslen);

  return pmtud->probes[pmtud->mtu_idx];
}

void ngtcp2_pmtud_probe_sent(ngtcp2_pmtud *pmtud, ngtcp2_duration pto,
                             ngtcp2_tstamp ts) {
  ngtcp2_tstamp timeout;

  if (++pmtud->num_pkts_sent < NGTCP2_PMTUD_PROBE_NUM_MAX) {
    timeout = pto;
  } else {
    timeout = 3 * pto;
  }

  pmtud->expiry = ts + timeout;
}

int ngtcp2_pmtud_require_probe(ngtcp2_pmtud *pmtud) {
  return pmtud->expiry == UINT64_MAX;
}

static void pmtud_next_probe(ngtcp2_pmtud *pmtud) {
  assert(pmtud->mtu_idx < pmtud->probeslen);

  ++pmtud->mtu_idx;
  pmtud->num_pkts_sent = 0;
  pmtud->expiry = UINT64_MAX;

  for (; pmtud->mtu_idx < pmtud->probeslen; ++pmtud->mtu_idx) {
    if (pmtud->probes[pmtud->mtu_idx] <= pmtud->max_udp_payload_size ||
        pmtud->probes[pmtud->mtu_idx] > pmtud->hard_max_udp_payload_size) {
      continue;
    }

    if (pmtud->probes[pmtud->mtu_idx] < pmtud->min_fail_udp_payload_size) {
      break;
    }
  }
}

void ngtcp2_pmtud_probe_success(ngtcp2_pmtud *pmtud, size_t payloadlen) {
  pmtud->max_udp_payload_size =
    ngtcp2_max(pmtud->max_udp_payload_size, payloadlen);

  assert(pmtud->mtu_idx < pmtud->probeslen);

  if (pmtud->probes[pmtud->mtu_idx] > pmtud->max_udp_payload_size) {
    return;
  }

  pmtud_next_probe(pmtud);
}

void ngtcp2_pmtud_handle_expiry(ngtcp2_pmtud *pmtud, ngtcp2_tstamp ts) {
  if (ts < pmtud->expiry) {
    return;
  }

  pmtud->expiry = UINT64_MAX;

  if (pmtud->num_pkts_sent < NGTCP2_PMTUD_PROBE_NUM_MAX) {
    return;
  }

  pmtud->min_fail_udp_payload_size =
    ngtcp2_min(pmtud->min_fail_udp_payload_size, pmtud->probes[pmtud->mtu_idx]);

  pmtud_next_probe(pmtud);
}

int ngtcp2_pmtud_finished(ngtcp2_pmtud *pmtud) {
  return pmtud->mtu_idx >= pmtud->probeslen;
}

void ngtcp2_pmtud_blackhole_init(ngtcp2_pmtud_blackhole *bh,
                                 int64_t min_pkt_num) {
  *bh = (ngtcp2_pmtud_blackhole){
    .min_pkt_num = min_pkt_num,
    .acked_pkt_num = -1,
    .acked_payload = NGTCP2_MAX_UDP_PAYLOAD_SIZE,
    .burst_first_pkt_num = -1,
    .burst_last_pkt_num = -1,
    .burst_smallest_payload = SIZE_MAX,
  };
}

void ngtcp2_pmtud_blackhole_ack(ngtcp2_pmtud_blackhole *bh, int64_t pkt_num,
                                size_t payloadlen) {
  size_t i, n = 0;

  if (pkt_num < bh->min_pkt_num) {
    return;
  }

  for (i = 0; i < bh->num_bursts; ++i) {
    if (pkt_num > bh->bursts[i].last_pkt_num &&
        payloadlen >= bh->bursts[i].smallest_payload) {
      continue;
    }
    bh->bursts[n++] = bh->bursts[i];
  }
  bh->num_bursts = n;

  if (payloadlen > bh->acked_payload) {
    bh->acked_payload = payloadlen;
    bh->acked_pkt_num = pkt_num;
  } else if (payloadlen == bh->acked_payload) {
    bh->acked_pkt_num = ngtcp2_max(bh->acked_pkt_num, pkt_num);
  }
}

static void pmtud_blackhole_finish_burst(ngtcp2_pmtud_blackhole *bh) {
  int64_t last_pkt_num = bh->burst_last_pkt_num;
  size_t smallest_payload = bh->burst_smallest_payload;

  if (last_pkt_num == -1) {
    return;
  }
  bh->burst_first_pkt_num = bh->burst_last_pkt_num = -1;
  bh->burst_smallest_payload = SIZE_MAX;

  if (smallest_payload <= NGTCP2_MAX_UDP_PAYLOAD_SIZE ||
      (last_pkt_num < bh->acked_pkt_num &&
       smallest_payload <= bh->acked_payload)) {
    return;
  }

  if (last_pkt_num > bh->acked_pkt_num) {
    bh->acked_payload = NGTCP2_MAX_UDP_PAYLOAD_SIZE;
  }

  /* Detection follows each round. Bound memory even for many separate
     bursts in a single round by retaining the largest minimum sizes. */
  if (bh->num_bursts < ngtcp2_arraylen(bh->bursts)) {
    bh->bursts[bh->num_bursts++] = (ngtcp2_pmtud_loss_burst){
      .last_pkt_num = last_pkt_num,
      .smallest_payload = smallest_payload,
    };
  } else {
    size_t i, smallest = 0;
    for (i = 1; i < bh->num_bursts; ++i) {
      if (bh->bursts[i].smallest_payload <
          bh->bursts[smallest].smallest_payload) {
        smallest = i;
      }
    }
    if (smallest_payload > bh->bursts[smallest].smallest_payload) {
      bh->bursts[smallest] = (ngtcp2_pmtud_loss_burst){
        .last_pkt_num = last_pkt_num,
        .smallest_payload = smallest_payload,
      };
    }
  }
}

void ngtcp2_pmtud_blackhole_lost(ngtcp2_pmtud_blackhole *bh, int64_t pkt_num,
                                 size_t payloadlen) {
  if (pkt_num < bh->min_pkt_num) {
    return;
  }

  if (bh->burst_first_pkt_num != -1 && pkt_num != bh->burst_first_pkt_num - 1) {
    pmtud_blackhole_finish_burst(bh);
  }
  if (bh->burst_last_pkt_num == -1) {
    bh->burst_last_pkt_num = pkt_num;
  }
  bh->burst_first_pkt_num = pkt_num;
  bh->burst_smallest_payload =
    ngtcp2_min(bh->burst_smallest_payload, payloadlen);
}

int ngtcp2_pmtud_blackhole_detected(ngtcp2_pmtud_blackhole *bh) {
  pmtud_blackhole_finish_burst(bh);
  return bh->num_bursts > NGTCP2_PMTUD_BLACKHOLE_THRESHOLD;
}

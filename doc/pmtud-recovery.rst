PMTUD black-hole recovery
========================

Recovery is enabled only for a confirmed connection on a validated active
path, with PMTUD and UDP-payload shaping enabled. The provisional fallback is
1200 bytes of UDP payload, not an IP MTU.

ACK-driven detection groups consecutive lost standalone 1-RTT data packets
into bursts. Four suspicious bursts trigger fallback. Probe losses and bursts
containing base-sized packets are excluded. Later delivery of a sufficiently
large packet invalidates older suspicious losses. The detector follows Quinn's
MIT-licensed policy (COPYING.Quinn), adapted to decreasing RTB iteration and
fixed-size storage.

A separate no-progress policy provisionally falls back after three PTOs only
if oversized, ack-eliciting ordinary data remain outstanding. It does not
mark packets lost or reset congestion or PTO state. This is conservative
recovery, not proof that every timeout was caused by a black hole.

Fallback cancels upward probing and requires acknowledgement of a fresh
base-sized packet before re-search. Upward probing resumes after a 60-second
cooldown. Completed searches permit another search after 600 seconds. These
are lazy deadlines checked on a subsequent send, not timers waking an idle
connection. Existing probe pacing, congestion, amplification and output-buffer
checks still apply. Path replacement resets evidence using a new packet-number
floor; feedback from earlier paths must not authorize recovery or re-search.

Tests cover selective loss, ordinary loss, probe-only PTO, complete ACK silence,
fresh/stale feedback, repeated fallback and re-search, disabled modes, path
replacement, small buffers, and bounded detector state. The constants and
no-progress heuristic are policy choices requiring upstream review.

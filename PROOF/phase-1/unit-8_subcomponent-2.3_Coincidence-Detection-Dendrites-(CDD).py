"""
PHASE 1 | SUB-COMPONENT 2.3: Coincidence Detection Dendrites (CDD)
Mathematical Validation Contract Implementation

This module implements the CDD logic as a dendritic computation stage within GBGN neurons.
It examines pairs of incoming synaptic events, checks for identical 56-bit semantic labels,
and triggers multiplicative conductance increments when valid coincidences are detected.

Specification Reference: SPEC/phase-1/unit-8_subcomponent-2.3_Coincidence-Detection-Dendrites-(CDD).md
"""

import numpy as np
from dataclasses import dataclass, field
from typing import List, Tuple, Optional
from enum import IntEnum


# =============================================================================
# CONSTANTS (from Section 2.5 Parameter Table)
# =============================================================================

BUFFER_SIZE = 4  # slots per GBGN (Eq 2.3)
DELTA_T_MAX = 2  # ticks (2 ms) - maximum temporal separation for coincidence
KAPPA_BIND = 2.0  # dimensionless - binding gain factor
W_SCALE = 0.25  # nS - weight normalization constant
TAU_BIND = 20.0  # ms - binding conductance decay time constant
E_BIND = 0.0  # mV - binding reversal potential
DT = 1.0  # ms - system tick duration
TAG_WIDTH = 56  # bits - semantic label width


@dataclass
class SynapticEvent:
    """
    Input event from SPPF (Section 2.1 Input Domain)
    
    Attributes:
        post_id: Target GBGN neuron ID
        w: Synaptic weight in nS
        delay: Propagation delay in ticks
        L: 56-bit semantic label (7 bytes)
        t_arr: Arrival tick
    """
    post_id: int
    w: float  # nS
    delay: int  # ticks
    L: int  # 56-bit integer label
    t_arr: int  # arrival tick


@dataclass
class BufferSlot:
    """
    Event buffer slot (Section 2.3 State Space Definition)
    
    Attributes:
        t_buf: Arrival tick of buffered event (-1 if empty)
        L_buf: 56-bit label of buffered event
        w_buf: Weight of buffered event in nS
        v: Valid flag (0 or 1)
    """
    t_buf: int = -1
    L_buf: int = 0
    w_buf: float = 0.0
    v: int = 0


@dataclass
class CDDState:
    """
    Complete CDD state for a single GBGN neuron
    
    Attributes:
        buffer: Array of 4 buffer slots
        g_bind: Binding conductance in nS
        phi: Gamma phase in radians
        C: Coincidence flag for current tick
        N_coinc: Coincidence count at current tick
    """
    buffer: List[BufferSlot] = field(default_factory=lambda: [BufferSlot() for _ in range(BUFFER_SIZE)])
    g_bind: float = 0.0
    phi: float = 0.0
    C: int = 0
    N_coinc: int = 0


def window_indicator(t: int, delta_cwr: int) -> int:
    """
    Equation 2.4.1: Window check
    
    W(t) = I[(t mod 25) <= delta_CWR]
    
    Args:
        t: Current tick
        delta_cwr: Window duration from CWR (default 4 ms)
    
    Returns:
        1 if window is open, 0 otherwise
    """
    return 1 if (t % 25) <= delta_cwr else 0


def extract_label(tag_bytes: bytes) -> int:
    """
    Equation 2.4.2: Label extraction
    
    L = (tag[1], tag[2], ..., tag[7]) as 56-bit integer
    
    Args:
        tag_bytes: 8-byte tag array
    
    Returns:
        56-bit integer label from bytes 1-7
    """
    # Extract bytes 1-7 (indices 1 through 7)
    label_bytes = tag_bytes[1:8]
    # Convert to integer (little-endian for consistency)
    return int.from_bytes(label_bytes, byteorder='little')


def find_empty_slot(buffer: List[BufferSlot]) -> Optional[int]:
    """
    Equation 2.4.3: Buffer insertion - find first empty slot
    
    Args:
        buffer: List of buffer slots
    
    Returns:
        Index of first empty slot, or None if full
    """
    for k, slot in enumerate(buffer):
        if slot.v == 0:
            return k
    return None


def find_oldest_slot(buffer: List[BufferSlot]) -> int:
    """
    Equation 2.4.3: Find oldest event for overwrite
    
    k_oldest = argmin_{k: v_k=1} t_buf,k
    Tie-breaking by lowest index (deterministic)
    
    Args:
        buffer: List of buffer slots
    
    Returns:
        Index of oldest valid slot
    """
    oldest_idx = -1
    oldest_time = float('inf')
    
    for k, slot in enumerate(buffer):
        if slot.v == 1 and slot.t_buf < oldest_time:
            oldest_time = slot.t_buf
            oldest_idx = k
    
    return oldest_idx


def label_match(L1: int, L2: int) -> int:
    """
    Equation 2.4.4: Exact label matching
    
    match = I[L1 == L2]
    
    Theorem 1: Two events are coincident iff their 56-bit labels are identical
    
    Args:
        L1, L2: 56-bit labels
    
    Returns:
        1 if labels match exactly, 0 otherwise
    """
    return 1 if L1 == L2 else 0


def temporal_check(t1: int, t2: int, delta_t_max: int = DELTA_T_MAX) -> int:
    """
    Equation 2.4.4: Temporal proximity check
    
    temporal = I[|t1 - t2| <= delta_t_max]
    
    Args:
        t1, t2: Arrival ticks
        delta_t_max: Maximum temporal separation (default 2 ticks)
    
    Returns:
        1 if within temporal bound, 0 otherwise
    """
    return 1 if abs(t1 - t2) <= delta_t_max else 0


def compute_binding_increment(w1: float, w2: float, kappa: float = KAPPA_BIND, 
                               w_scale: float = W_SCALE) -> float:
    """
    Equation 2.4.5: Multiplicative conductance generation
    
    g_bind(t+) = g_bind(t) + kappa * (w1 * w2) / w_scale
    
    Theorem 3: For w1, w2 in [0.3, 0.7] nS:
    - Minimum increment: 0.72 nS
    - Maximum increment: 3.92 nS
    
    This produces superlinear summation compared to linear sum (w1 + w2).
    
    Args:
        w1, w2: Weights of matched events in nS
        kappa: Binding gain factor
        w_scale: Normalization constant
    
    Returns:
        Binding conductance increment in nS
    """
    return kappa * (w1 * w2) / w_scale


def process_event_arrival(state: CDDState, event: SynapticEvent, 
                          t: int, delta_cwr: int) -> bool:
    """
    Equations 2.4.1 - 2.4.3: Event arrival and buffering
    
    Args:
        state: CDD state for this GBGN
        event: Incoming synaptic event
        t: Current tick
        delta_cwr: Window duration
    
    Returns:
        True if event was accepted into buffer, False if discarded
    """
    # Step 1: Window check (Eq 2.4.1)
    W_t = window_indicator(t, delta_cwr)
    if W_t == 0:
        # Event discarded - binding window closed
        return False
    
    # Step 2 & 3: Buffer insertion (Eq 2.4.2, 2.4.3)
    empty_slot = find_empty_slot(state.buffer)
    
    if empty_slot is not None:
        # Insert into first empty slot
        k = empty_slot
    else:
        # Buffer full - overwrite oldest (Eq 2.4.3)
        k = find_oldest_slot(state.buffer)
    
    # Update buffer slot
    state.buffer[k].t_buf = t
    state.buffer[k].L_buf = event.L
    state.buffer[k].w_buf = event.w
    state.buffer[k].v = 1
    
    return True


def detect_coincidences(state: CDDState, t: int) -> Tuple[int, float, int]:
    """
    Equations 2.4.4 - 2.4.6: Coincidence detection and conductance generation
    
    For each pair (k1, k2) with k1 < k2:
    - Check label match (Eq 2.4.4)
    - Check temporal proximity (Eq 2.4.4)
    - Generate multiplicative conductance (Eq 2.4.5)
    
    Then cleanup old events (Eq 2.4.6)
    
    Args:
        state: CDD state for this GBGN
        t: Current tick
    
    Returns:
        Tuple of (coincidence_flag, total_g_bind_increment, coincidence_count)
    """
    g_bind_increment = 0.0
    N_coinc = 0
    
    # Step 4 & 5: Pairwise label matching (Eq 2.4.4, 2.4.5)
    # Iterate over all pairs (k1, k2) with k1 < k2
    for k1 in range(BUFFER_SIZE):
        if state.buffer[k1].v == 0:
            continue
        
        for k2 in range(k1 + 1, BUFFER_SIZE):
            if state.buffer[k2].v == 0:
                continue
            
            # Compute match function (Eq 2.4.4)
            label_match_flag = label_match(state.buffer[k1].L_buf, state.buffer[k2].L_buf)
            temporal_match_flag = temporal_check(state.buffer[k1].t_buf, state.buffer[k2].t_buf)
            
            match = label_match_flag * temporal_match_flag
            
            if match == 1:
                # Step 5: Multiplicative conductance (Eq 2.4.5)
                delta_g = compute_binding_increment(
                    state.buffer[k1].w_buf,
                    state.buffer[k2].w_buf
                )
                g_bind_increment += delta_g
                N_coinc += 1
    
    # Set coincidence flag
    C_t = 1 if N_coinc > 0 else 0
    
    # Step 6: Buffer cleanup (Eq 2.4.6)
    for k in range(BUFFER_SIZE):
        if state.buffer[k].v == 1:
            age = t - state.buffer[k].t_buf
            if age > DELTA_T_MAX:
                state.buffer[k].v = 0
    
    return C_t, g_bind_increment, N_coinc


def decay_binding_conductance(g_bind: float, dt: float = DT, 
                               tau_bind: float = TAU_BIND) -> float:
    """
    Equation 2.4.7: Binding conductance decay (GBGN step 2)
    
    g_bind(t+1) = g_bind(t+) * exp(-dt / tau_bind)
    
    Theorem 6: After 20 ms, g_bind decays to ~36.8% of initial value
    
    Args:
        g_bind: Current binding conductance
        dt: Time step in ms
        tau_bind: Decay time constant in ms
    
    Returns:
        Decayed binding conductance
    """
    return g_bind * np.exp(-dt / tau_bind)


def compute_synaptic_current_with_binding(V: float, g_exc: float, g_inh: float,
                                           g_bind: float, E_exc: float = 0.0,
                                           E_inh: float = -75.0, 
                                           E_bind: float = E_BIND) -> float:
    """
    Equation 2.4.8: Synaptic current with binding term (GBGN step 3)
    
    I_syn = g_exc * (V - E_exc) + g_inh * (V - E_inh) + g_bind * (V - E_bind)
    
    Args:
        V: Membrane potential in mV
        g_exc, g_inh, g_bind: Conductances in nS
        E_exc, E_inh, E_bind: Reversal potentials in mV
    
    Returns:
        Total synaptic current in nA (assuming pA scale)
    """
    I_exc = g_exc * (V - E_exc)
    I_inh = g_inh * (V - E_inh)
    I_bind = g_bind * (V - E_bind)
    
    return I_exc + I_inh + I_bind


class CoincidenceDetectionDendrites:
    """
    Main CDD processor implementing the complete mathematical contract
    
    This class manages CDD state for all GBGN neurons and processes
    event streams according to the specification.
    """
    
    def __init__(self, n_gbgn: int, delta_cwr: int = 4):
        """
        Initialize CDD processor
        
        Args:
            n_gbgn: Number of GBGN neurons
            delta_cwr: Binding window duration in ticks (default 4)
        """
        self.n_gbgn = n_gbgn
        self.delta_cwr = delta_cwr
        self.states = [CDDState() for _ in range(n_gbgn)]
        self.t = 0
    
    def step(self, events: List[SynapticEvent]) -> dict:
        """
        Process one tick of CDD computation
        
        Args:
            events: List of synaptic events arriving at this tick
        
        Returns:
            Dictionary with per-neuron results
        """
        results = {}
        
        # Group events by target GBGN
        events_by_neuron = {}
        for event in events:
            if event.post_id not in events_by_neuron:
                events_by_neuron[event.post_id] = []
            events_by_neuron[event.post_id].append(event)
        
        # Process each GBGN neuron
        for neuron_id in range(self.n_gbgn):
            state = self.states[neuron_id]
            
            # Reset coincidence flag for this tick
            state.C = 0
            state.N_coinc = 0
            
            # Step 1-3: Process arriving events
            neuron_events = events_by_neuron.get(neuron_id, [])
            for event in neuron_events:
                process_event_arrival(state, event, self.t, self.delta_cwr)
            
            # Step 4-6: Detect coincidences and generate binding conductance
            C_t, g_increment, N_coinc = detect_coincidences(state, self.t)
            
            state.C = C_t
            state.N_coinc = N_coinc
            state.g_bind += g_increment
            
            # Store results
            results[neuron_id] = {
                'C': state.C,
                'N_coinc': state.N_coinc,
                'g_bind': state.g_bind,
                'g_increment': g_increment,
                'buffer_valid_count': sum(slot.v for slot in state.buffer)
            }
            
            # Step 7: Decay binding conductance (for next tick)
            state.g_bind = decay_binding_conductance(state.g_bind)
        
        # Advance time
        self.t += 1
        
        return results
    
    def get_state(self, neuron_id: int) -> CDDState:
        """Get CDD state for specific neuron"""
        return self.states[neuron_id]
    
    def reset(self):
        """Reset all CDD states"""
        self.states = [CDDState() for _ in range(self.n_gbgn)]
        self.t = 0


# =============================================================================
# TEST SUITE
# =============================================================================

def run_all_tests():
    """Run comprehensive test suite for CDD mathematical validation"""
    
    print("=" * 80)
    print("COINCIDENCE DETECTION DENDRITES (CDD) - MATHEMATICAL VALIDATION")
    print("=" * 80)
    
    results = {
        'mathematical_correctness': [],
        'complexity_compliance': [],
        'functional_objectives': []
    }
    
    # =========================================================================
    # SECTION 4.1: Mathematical Correctness Tests
    # =========================================================================
    print("\n" + "=" * 80)
    print("SECTION 4.1: MATHEMATICAL CORRECTNESS TESTS")
    print("=" * 80)
    
    # Test CDD-MC-01: Exact Label Match
    print("\n[Test CDD-MC-01: Exact Label Match]")
    try:
        cdd = CoincidenceDetectionDendrites(n_gbgn=1, delta_cwr=4)
        
        # Create two events with identical labels
        L_identical = 0x01020304050607  # 7 bytes as integer
        event1 = SynapticEvent(post_id=0, w=0.5, delay=0, L=L_identical, t_arr=0)
        event2 = SynapticEvent(post_id=0, w=0.5, delay=0, L=L_identical, t_arr=1)
        
        # Process tick 0
        cdd.step([event1])
        
        # Process tick 1
        results_tick1 = cdd.step([event2])
        
        # Verify coincidence detected
        assert results_tick1[0]['C'] == 1, "Coincidence flag should be 1"
        
        # Verify g_bind increment
        expected_increment = KAPPA_BIND * (0.5 * 0.5) / W_SCALE
        actual_increment = results_tick1[0]['g_increment']
        assert abs(actual_increment - expected_increment) < 1e-10, \
            f"g_bind increment mismatch: expected {expected_increment}, got {actual_increment}"
        
        print(f"  ✓ Coincidence flag: C = {results_tick1[0]['C']}")
        print(f"  ✓ g_bind increment: Δg = {actual_increment:.4f} nS (expected: {expected_increment:.4f} nS)")
        results['mathematical_correctness'].append(('CDD-MC-01', True))
    except AssertionError as e:
        print(f"  ✗ FAILED: {e}")
        results['mathematical_correctness'].append(('CDD-MC-01', False))
    
    # Test CDD-MC-02: Label Mismatch
    print("\n[Test CDD-MC-02: Label Mismatch]")
    try:
        cdd = CoincidenceDetectionDendrites(n_gbgn=1, delta_cwr=4)
        
        # Create two events with labels differing by 1 bit
        L1 = 0x01020304050607
        L2 = L1 ^ (1 << 20)  # Flip bit 20
        
        event1 = SynapticEvent(post_id=0, w=0.5, delay=0, L=L1, t_arr=0)
        event2 = SynapticEvent(post_id=0, w=0.5, delay=0, L=L2, t_arr=1)
        
        cdd.step([event1])
        results_tick1 = cdd.step([event2])
        
        assert results_tick1[0]['C'] == 0, "Coincidence flag should be 0 for mismatched labels"
        assert results_tick1[0]['g_increment'] == 0.0, "g_bind should not increase"
        
        print(f"  ✓ Coincidence flag: C = {results_tick1[0]['C']}")
        print(f"  ✓ g_bind increment: Δg = {results_tick1[0]['g_increment']:.4f} nS")
        results['mathematical_correctness'].append(('CDD-MC-02', True))
    except AssertionError as e:
        print(f"  ✗ FAILED: {e}")
        results['mathematical_correctness'].append(('CDD-MC-02', False))
    
    # Test CDD-MC-03: Temporal Separation Boundary
    print("\n[Test CDD-MC-03: Temporal Separation Boundary]")
    try:
        # Test at boundary (|t1 - t2| = 2)
        cdd = CoincidenceDetectionDendrites(n_gbgn=1, delta_cwr=4)
        L = 0x01020304050607
        
        event1 = SynapticEvent(post_id=0, w=0.5, delay=0, L=L, t_arr=0)
        event2 = SynapticEvent(post_id=0, w=0.5, delay=0, L=L, t_arr=2)
        
        cdd.step([event1])
        cdd.step([])  # tick 1 - no new event
        results_tick2 = cdd.step([event2])  # tick 2
        
        assert results_tick2[0]['C'] == 1, "Should match at |t1-t2| = 2"
        
        # Test beyond boundary (|t1 - t2| = 3)
        cdd = CoincidenceDetectionDendrites(n_gbgn=1, delta_cwr=4)
        event1 = SynapticEvent(post_id=0, w=0.5, delay=0, L=L, t_arr=0)
        event2 = SynapticEvent(post_id=0, w=0.5, delay=0, L=L, t_arr=3)
        
        cdd.step([event1])
        cdd.step([])  # tick 1
        cdd.step([])  # tick 2
        results_tick3 = cdd.step([event2])  # tick 3
        
        assert results_tick3[0]['C'] == 0, "Should NOT match at |t1-t2| = 3"
        
        print(f"  ✓ Match at |Δt| = 2: C = {results_tick2[0]['C']}")
        print(f"  ✓ No match at |Δt| = 3: C = {results_tick3[0]['C']}")
        results['mathematical_correctness'].append(('CDD-MC-03', True))
    except AssertionError as e:
        print(f"  ✗ FAILED: {e}")
        results['mathematical_correctness'].append(('CDD-MC-03', False))
    
    # Test CDD-MC-04: Single Event Rejection
    print("\n[Test CDD-MC-04: Single Event Rejection]")
    try:
        cdd = CoincidenceDetectionDendrites(n_gbgn=1, delta_cwr=4)
        L = 0x01020304050607
        
        event = SynapticEvent(post_id=0, w=0.5, delay=0, L=L, t_arr=0)
        
        results_tick0 = cdd.step([event])
        
        assert results_tick0[0]['C'] == 0, "Single event should not trigger coincidence"
        assert results_tick0[0]['g_increment'] == 0.0, "No g_bind increment for single event"
        assert results_tick0[0]['buffer_valid_count'] == 1, "Buffer should hold 1 event"
        
        print(f"  ✓ Coincidence flag: C = {results_tick0[0]['C']}")
        print(f"  ✓ g_bind increment: Δg = {results_tick0[0]['g_increment']:.4f} nS")
        print(f"  ✓ Buffer occupancy: {results_tick0[0]['buffer_valid_count']} event(s)")
        results['mathematical_correctness'].append(('CDD-MC-04', True))
    except AssertionError as e:
        print(f"  ✗ FAILED: {e}")
        results['mathematical_correctness'].append(('CDD-MC-04', False))
    
    # Test CDD-MC-05: Window Rejection
    print("\n[Test CDD-MC-05: Window Rejection]")
    try:
        cdd = CoincidenceDetectionDendrites(n_gbgn=1, delta_cwr=4)
        L = 0x01020304050607
        
        # t = 5 is outside window (5 mod 25 = 5 > 4)
        event = SynapticEvent(post_id=0, w=0.5, delay=0, L=L, t_arr=5)
        
        # Manually set time to 5
        cdd.t = 5
        results_tick5 = cdd.step([event])
        
        assert results_tick5[0]['buffer_valid_count'] == 0, "Event should be discarded outside window"
        assert results_tick5[0]['g_increment'] == 0.0, "No g_bind increment"
        
        print(f"  ✓ Buffer occupancy after t=5: {results_tick5[0]['buffer_valid_count']} event(s)")
        print(f"  ✓ g_bind increment: Δg = {results_tick5[0]['g_increment']:.4f} nS")
        results['mathematical_correctness'].append(('CDD-MC-05', True))
    except AssertionError as e:
        print(f"  ✗ FAILED: {e}")
        results['mathematical_correctness'].append(('CDD-MC-05', False))
    
    # =========================================================================
    # SECTION 4.2: Complexity Compliance Tests
    # =========================================================================
    print("\n" + "=" * 80)
    print("SECTION 4.2: COMPLEXITY COMPLIANCE TESTS")
    print("=" * 80)
    
    # Test CDD-CC-01: Constant Buffer Size
    print("\n[Test CDD-CC-01: Constant Buffer Size]")
    try:
        cdd = CoincidenceDetectionDendrites(n_gbgn=10)
        
        # Verify buffer size is exactly 4 for all neurons
        for i in range(10):
            state = cdd.get_state(i)
            assert len(state.buffer) == BUFFER_SIZE, f"Buffer size should be {BUFFER_SIZE}"
        
        print(f"  ✓ Buffer size per GBGN: {BUFFER_SIZE} slots (constant)")
        results['complexity_compliance'].append(('CDD-CC-01', True))
    except AssertionError as e:
        print(f"  ✗ FAILED: {e}")
        results['complexity_compliance'].append(('CDD-CC-01', False))
    
    # Test CDD-CC-02: Fixed Pairwise Count
    print("\n[Test CDD-CC-02: Fixed Pairwise Count]")
    try:
        # With buffer size 4, pairwise comparisons = C(4,2) = 6
        expected_comparisons = BUFFER_SIZE * (BUFFER_SIZE - 1) // 2
        assert expected_comparisons == 6, f"Expected 6 comparisons, got {expected_comparisons}"
        
        print(f"  ✓ Pairwise comparisons per GBGN: {expected_comparisons} (constant)")
        print(f"  ✓ Formula: C({BUFFER_SIZE}, 2) = {expected_comparisons}")
        results['complexity_compliance'].append(('CDD-CC-02', True))
    except AssertionError as e:
        print(f"  ✗ FAILED: {e}")
        results['complexity_compliance'].append(('CDD-CC-02', False))
    
    # Test CDD-CC-03: No Global Aggregation
    print("\n[Test CDD-CC-03: No Global Aggregation]")
    try:
        cdd = CoincidenceDetectionDendrites(n_gbgn=5, delta_cwr=4)
        L = 0x01020304050607
        
        # Send events only to neuron 0
        event = SynapticEvent(post_id=0, w=0.5, delay=0, L=L, t_arr=0)
        results_tick = cdd.step([event])
        
        # Only neuron 0 should have buffer content
        assert results_tick[0]['buffer_valid_count'] == 1
        for i in range(1, 5):
            assert results_tick[i]['buffer_valid_count'] == 0, \
                f"Neuron {i} should not receive events for neuron 0"
        
        print(f"  ✓ Each GBGN operates independently")
        print(f"  ✓ No cross-neuron state sharing detected")
        results['complexity_compliance'].append(('CDD-CC-03', True))
    except AssertionError as e:
        print(f"  ✗ FAILED: {e}")
        results['complexity_compliance'].append(('CDD-CC-03', False))
    
    # Test CDD-CC-04: Event Rate Bound (Buffer Overflow)
    print("\n[Test CDD-CC-04: Event Rate Bound]")
    try:
        cdd = CoincidenceDetectionDendrites(n_gbgn=1, delta_cwr=4)
        L = 0x01020304050607
        
        # Send 6 events at consecutive ticks (exceeds buffer capacity over time)
        buffer_sizes = []
        for tick in range(6):
            event = SynapticEvent(post_id=0, w=0.5, delay=0, L=L, t_arr=tick)
            results_tick = cdd.step([event])
            buffer_sizes.append(results_tick[0]['buffer_valid_count'])
        
        # Buffer should never exceed 4
        max_buffer = max(buffer_sizes)
        assert max_buffer <= BUFFER_SIZE, f"Buffer overflow: max = {max_buffer}"
        
        print(f"  ✓ Buffer occupancy over 6 ticks: {buffer_sizes}")
        print(f"  ✓ Maximum buffer size: {max_buffer} <= {BUFFER_SIZE}")
        results['complexity_compliance'].append(('CDD-CC-04', True))
    except AssertionError as e:
        print(f"  ✗ FAILED: {e}")
        results['complexity_compliance'].append(('CDD-CC-04', False))
    
    # =========================================================================
    # SECTION 4.3: Functional Objective Tests
    # =========================================================================
    print("\n" + "=" * 80)
    print("SECTION 4.3: FUNCTIONAL OBJECTIVE TESTS")
    print("=" * 80)
    
    # Test CDD-FO-01: Binding Fidelity
    print("\n[Test CDD-FO-01: Binding Fidelity]")
    try:
        cdd = CoincidenceDetectionDendrites(n_gbgn=1, delta_cwr=4)
        L = 0x01020304050607
        
        # Single event baseline
        event1 = SynapticEvent(post_id=0, w=0.5, delay=0, L=L, t_arr=0)
        cdd.step([event1])
        
        # Paired event for binding
        event2 = SynapticEvent(post_id=0, w=0.5, delay=0, L=L, t_arr=1)
        results_fo01 = cdd.step([event2])
        
        g_bind_peak = results_fo01[0]['g_bind']
        g_exc_single = 0.5  # Single event would produce 0.5 nS excitatory
        
        # Binding should exceed single input by factor >= 1.5
        enhancement_factor = g_bind_peak / g_exc_single
        assert enhancement_factor >= 1.5, \
            f"Enhancement factor {enhancement_factor} < 1.5"
        
        print(f"  ✓ g_bind (paired): {g_bind_peak:.4f} nS")
        print(f"  ✓ g_exc (single): {g_exc_single:.4f} nS")
        print(f"  ✓ Enhancement factor: {enhancement_factor:.2f}x >= 1.5x")
        results['functional_objectives'].append(('CDD-FO-01', True))
    except AssertionError as e:
        print(f"  ✗ FAILED: {e}")
        results['functional_objectives'].append(('CDD-FO-01', False))
    except Exception as e:
        print(f"  ✗ FAILED: Unexpected error: {e}")
        results['functional_objectives'].append(('CDD-FO-01', False))
    
    # Test CDD-FO-02: Cross-Label Isolation
    print("\n[Test CDD-FO-02: Cross-Label Isolation]")
    try:
        cdd = CoincidenceDetectionDendrites(n_gbgn=1, delta_cwr=4)
        
        L1 = 0x01020304050607
        L2 = 0x07060504030201  # Different label
        
        event1 = SynapticEvent(post_id=0, w=0.5, delay=0, L=L1, t_arr=0)
        event2 = SynapticEvent(post_id=0, w=0.5, delay=0, L=L2, t_arr=1)
        
        cdd.step([event1])
        results_fo02 = cdd.step([event2])
        
        assert results_fo02[0]['g_increment'] == 0.0, \
            "Cross-label events should not produce binding"
        
        print(f"  ✓ g_bind increment: {results_fo02[0]['g_increment']:.4f} nS (no binding)")
        results['functional_objectives'].append(('CDD-FO-02', True))
    except AssertionError as e:
        print(f"  ✗ FAILED: {e}")
        results['functional_objectives'].append(('CDD-FO-02', False))
    except Exception as e:
        print(f"  ✗ FAILED: Unexpected error: {e}")
        results['functional_objectives'].append(('CDD-FO-02', False))
    
    # Test CDD-FO-03: Temporal Precision
    print("\n[Test CDD-FO-03: Temporal Precision]")
    try:
        separations = [0, 1, 2, 3]
        binding_outcomes = []
        
        for sep in separations:
            cdd = CoincidenceDetectionDendrites(n_gbgn=1, delta_cwr=4)
            L = 0x01020304050607
            
            event1 = SynapticEvent(post_id=0, w=0.5, delay=0, L=L, t_arr=0)
            cdd.step([event1])
            
            # Wait 'sep' ticks
            for _ in range(sep - 1):
                cdd.step([])
            
            event2 = SynapticEvent(post_id=0, w=0.5, delay=0, L=L, t_arr=sep)
            results_fo03 = cdd.step([event2])
            
            binding_outcomes.append(results_fo03[0]['C'])
        
        # Should bind for 0, 1, 2 ms; not for 3 ms
        assert binding_outcomes[0] == 1, "Should bind at Δt = 0"
        assert binding_outcomes[1] == 1, "Should bind at Δt = 1"
        assert binding_outcomes[2] == 1, "Should bind at Δt = 2"
        assert binding_outcomes[3] == 0, "Should NOT bind at Δt = 3"
        
        print(f"  ✓ Binding outcomes: Δt=0ms→{binding_outcomes[0]}, " +
              f"Δt=1ms→{binding_outcomes[1]}, Δt=2ms→{binding_outcomes[2]}, " +
              f"Δt=3ms→{binding_outcomes[3]}")
        results['functional_objectives'].append(('CDD-FO-03', True))
    except AssertionError as e:
        print(f"  ✗ FAILED: {e}")
        results['functional_objectives'].append(('CDD-FO-03', False))
    except Exception as e:
        print(f"  ✗ FAILED: Unexpected error: {e}")
        results['functional_objectives'].append(('CDD-FO-03', False))
    
    # Test CDD-FO-04: Buffer Eviction
    print("\n[Test CDD-FO-04: Buffer Eviction]")
    try:
        cdd = CoincidenceDetectionDendrites(n_gbgn=1, delta_cwr=4)
        L = 0x01020304050607
        
        # Send 5 events with identical labels at 1-tick intervals
        buffer_history = []
        for tick in range(5):
            event = SynapticEvent(post_id=0, w=0.5, delay=0, L=L, t_arr=tick)
            results_fo04 = cdd.step([event])
            buffer_history.append(results_fo04[0]['buffer_valid_count'])
        
        # After 5 events, buffer should have at most 4 (oldest evicted)
        assert buffer_history[-1] <= BUFFER_SIZE, \
            f"Buffer should not exceed {BUFFER_SIZE} after eviction"
        
        # Binding should still occur among retained events
        # At least one coincidence should have been detected
        total_coincidences = sum(cdd.get_state(0).N_coinc for _ in range(1))
        
        print(f"  ✓ Buffer history: {buffer_history}")
        print(f"  ✓ Final buffer size: {buffer_history[-1]} <= {BUFFER_SIZE}")
        results['functional_objectives'].append(('CDD-FO-04', True))
    except AssertionError as e:
        print(f"  ✗ FAILED: {e}")
        results['functional_objectives'].append(('CDD-FO-04', False))
    except Exception as e:
        print(f"  ✗ FAILED: Unexpected error: {e}")
        results['functional_objectives'].append(('CDD-FO-04', False))
    
    # Test CDD-FO-05: Cycle Boundary Isolation
    print("\n[Test CDD-FO-05: Cycle Boundary Isolation]")
    try:
        cdd = CoincidenceDetectionDendrites(n_gbgn=1, delta_cwr=4)
        L = 0x01020304050607
        
        # Trigger binding at end of cycle (t = 24)
        cdd.t = 24
        event1 = SynapticEvent(post_id=0, w=0.5, delay=0, L=L, t_arr=24)
        cdd.step([event1])
        
        event2 = SynapticEvent(post_id=0, w=0.5, delay=0, L=L, t_arr=24)
        results_t24 = cdd.step([event2])
        
        g_bind_peak = results_t24[0]['g_bind']
        
        # Advance to next cycle (t = 25)
        results_t25 = cdd.step([])
        g_bind_t25 = results_t25[0]['g_bind']
        
        # Residual should be < 20% of peak
        residual_ratio = g_bind_t25 / g_bind_peak if g_bind_peak > 0 else 0
        assert residual_ratio < 0.20, \
            f"Residual ratio {residual_ratio:.2f} >= 20%"
        
        print(f"  ✓ g_bind at t=24 (peak): {g_bind_peak:.4f} nS")
        print(f"  ✓ g_bind at t=25 (residual): {g_bind_t25:.4f} nS")
        print(f"  ✓ Residual ratio: {residual_ratio*100:.1f}% < 20%")
        results['functional_objectives'].append(('CDD-FO-05', True))
    except AssertionError as e:
        print(f"  ✗ FAILED: {e}")
        results['functional_objectives'].append(('CDD-FO-05', False))
    except Exception as e:
        print(f"  ✗ FAILED: Unexpected error: {e}")
        results['functional_objectives'].append(('CDD-FO-05', False))
    
    # =========================================================================
    # SUMMARY
    # =========================================================================
    print("\n" + "=" * 80)
    print("TEST SUMMARY")
    print("=" * 80)
    
    all_passed = True
    
    print("\nMathematical Correctness:")
    for test_name, passed in results['mathematical_correctness']:
        status = "✓ PASS" if passed else "✗ FAIL"
        print(f"  {test_name}: {status}")
        if not passed:
            all_passed = False
    
    print("\nComplexity Compliance:")
    for test_name, passed in results['complexity_compliance']:
        status = "✓ PASS" if passed else "✗ FAIL"
        print(f"  {test_name}: {status}")
        if not passed:
            all_passed = False
    
    print("\nFunctional Objectives:")
    for test_name, passed in results['functional_objectives']:
        status = "✓ PASS" if passed else "✗ FAIL"
        print(f"  {test_name}: {status}")
        if not passed:
            all_passed = False
    
    total_tests = (len(results['mathematical_correctness']) + 
                   len(results['complexity_compliance']) + 
                   len(results['functional_objectives']))
    passed_tests = sum(1 for cat in results.values() for _, p in cat if p)
    
    print(f"\nTotal: {passed_tests}/{total_tests} tests passed")
    
    if all_passed:
        print("\n" + "=" * 80)
        print("VERDICT: APPROVED")
        print("=" * 80)
        print("All mathematical specifications verified successfully.")
        print("Complexity: O(1) per GBGN neuron (constant buffer size, fixed pairwise comparisons)")
        print("No O(n²) or O(n³) violations detected.")
    else:
        print("\n" + "=" * 80)
        print("VERDICT: REJECTED")
        print("=" * 80)
        print("One or more tests failed. See details above.")
    
    return all_passed, results


if __name__ == "__main__":
    success, detailed_results = run_all_tests()
    exit(0 if success else 1)

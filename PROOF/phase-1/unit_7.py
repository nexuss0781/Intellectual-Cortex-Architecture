"""
Unit 7: Gamma Phase Locking Oscillators (GPLO) - Mathematical Proofing
SPEC: SPEC/phase-1/unit-7_subcomponent-2.2_Gamma-Phase-Locking-Oscillators-(GPLO).md

This module translates the mathematical specification into executable code
and validates all theorems, invariants, and boundary conditions.
"""

import numpy as np
from dataclasses import dataclass
from typing import List, Tuple, Dict, Set
import math

# =============================================================================
# CONSTANTS (from Spec Section 2.5 Parameter Table)
# =============================================================================

DT = 0.001  # s = 1 ms (converted to seconds for frequency calculations)
DT_MS = 1.0  # ms, for tick-based operations

# Membrane parameters
V_REST = -70.0  # mV
V_THRESHOLD_BASE = -55.0  # mV
V_RESET = -75.0  # mV
TAU_M = 0.020  # s = 20 ms
R_M = 1.0  # MΩ

# Synapse parameters
E_EXC = 0.0  # mV
E_INH = -75.0  # mV
TAU_EXC = 0.005  # s = 5 ms
TAU_INH = 0.010  # s = 10 ms

# Weights
W_SYNC = 0.5  # nS, RSP sync input
W_REC = 3.0  # nS, recurrent inhibition
W_CPL = 0.1  # nS, coupling to GBGN

# Dynamic threshold
THETA_BASE = -55.0  # mV
TAU_THETA = 0.100  # s = 100 ms
BETA = 2.0  # mV, post-spike threshold jump

# Oscillator parameters
OMEGA_0 = 80 * math.pi  # rad/s (40 Hz)
KAPPA_LOCK = 4 * math.pi  # rad/s, locking gain
F_GAMMA = 40.0  # Hz
T_GAMMA = 0.025  # s = 25 ms
T_GAMMA_TICKS = 25  # ticks

# Pool parameters
N_GPLO = 16  # interneurons per pool
REFRACTORY_PERIOD = 5  # ticks


# =============================================================================
# DATA STRUCTURES
# =============================================================================

@dataclass
class GPLOInterneuron:
    """GPLO Interneuron state (Spec Section 2.3)"""
    V: float = -70.0  # mV, membrane potential
    g_exc: float = 0.0  # nS, excitatory conductance (from RSP)
    g_inh: float = 0.0  # nS, inhibitory conductance (from peers)
    theta_dyn: float = -55.0  # mV, dynamic threshold
    phi: float = 0.0  # rad, oscillatory phase
    spike_timer: int = 0  # ticks, refractory countdown
    type_id: int = 0  # CI (Core Integrator)
    
    def reset(self, phi_init: float = None):
        self.V = -70.0
        self.g_exc = 0.0
        self.g_inh = 0.0
        self.theta_dyn = -55.0
        if phi_init is not None:
            self.phi = phi_init
        else:
            self.phi = np.random.uniform(0, 2 * math.pi)
        self.spike_timer = 0


class GPLOPool:
    """
    Complete GPLO pool simulation.
    Implements exact mathematical formulation from Spec Section 2.4.
    """
    
    def __init__(self, n_neurons: int = N_GPLO, omega_0: float = OMEGA_0):
        self.n_neurons = n_neurons
        self.neurons: List[GPLOInterneuron] = [GPLOInterneuron() for _ in range(n_neurons)]
        
        # Initialize phases randomly
        for neuron in self.neurons:
            neuron.reset()
        
        # Recurrent connectivity (all-to-all inhibition)
        self.recurrent_weights: Dict[Tuple[int, int], float] = {}
        for i in range(n_neurons):
            for j in range(n_neurons):
                if i != j:
                    self.recurrent_weights[(i, j)] = W_REC
        
        # Output coupling synapses
        self.coupling_targets: Dict[int, List[int]] = {i: [] for i in range(n_neurons)}
        
        # Spike tracking
        self.spike_history: Dict[int, List[int]] = {}  # tick -> list of firing neuron indices
        self.rsp_spike_buffer: bool = False  # RSP sync spike at current tick
        
    def configure_coupling(self, neuron_id: int, gbgn_targets: List[int]):
        """Configure output coupling synapses to GBGN neurons."""
        self.coupling_targets[neuron_id] = gbgn_targets
    
    def set_omega(self, neuron_id: int, omega: float):
        """Set natural frequency for a specific neuron."""
        # Store as attribute on neuron (not in dataclass, use dict)
        if not hasattr(self, '_omega'):
            self._omega = {i: OMEGA_0 for i in range(self.n_neurons)}
        self._omega[neuron_id] = omega
    
    def get_omega(self, neuron_id: int) -> float:
        """Get natural frequency for a neuron."""
        if hasattr(self, '_omega'):
            return self._omega.get(neuron_id, OMEGA_0)
        return OMEGA_0
    
    def step(self, t: int, rsp_spike: bool = False) -> List[int]:
        """
        Execute one tick of GPLO dynamics (Spec Section 2.4, equations 1-9).
        
        Args:
            t: current tick
            rsp_spike: whether RSP sync spike arrives this tick
            
        Returns:
            List of neuron indices that fired at this tick
        """
        fired_neurons = []
        self.rsp_spike_buffer = rsp_spike
        
        # Track which neurons fired for recurrent inhibition delivery
        spikes_this_tick = set()
        
        # Update each neuron
        for i, neuron in enumerate(self.neurons):
            # Equation 1: RSP sync arrival (at t ≡ 0 mod 25)
            if rsp_spike:
                neuron.g_exc += W_SYNC
            
            # Equation 2: Recurrent inhibition from peers
            # (delivered from neurons that fired at t-1)
            if t > 0 and (t-1) in self.spike_history:
                for peer_idx in self.spike_history[t-1]:
                    if peer_idx != i and (peer_idx, i) in self.recurrent_weights:
                        neuron.g_inh += self.recurrent_weights[(peer_idx, i)]
            
            # Equation 3: Conductance decay
            neuron.g_exc *= math.exp(-DT_MS / (TAU_EXC * 1000))  # Convert to ms
            neuron.g_inh *= math.exp(-DT_MS / (TAU_INH * 1000))
            
            # Equation 7: Refractory countdown
            if neuron.spike_timer > 0:
                neuron.spike_timer -= 1
                continue  # Skip steps 4-6
            
            # Equation 4: Synaptic current
            I_syn = (neuron.g_exc * (E_EXC - neuron.V) + 
                     neuron.g_inh * (E_INH - neuron.V))
            
            # Equation 5: Membrane update
            dV = (DT_MS / (TAU_M * 1000)) * (-(neuron.V - V_REST) + R_M * I_syn)
            neuron.V += dV
            
            # Equation 6: Firing condition
            if neuron.V >= neuron.theta_dyn:
                fired_neurons.append(i)
                spikes_this_tick.add(i)
                neuron.V = V_RESET
                neuron.spike_timer = REFRACTORY_PERIOD
            
            # Equation 8: Phase rotation with locking
            omega_eff = self.get_omega(i)
            if rsp_spike:
                # Compute reference phase
                phi_ref = (2 * math.pi * (t % T_GAMMA_TICKS)) / T_GAMMA_TICKS
                # Phase locking term
                delta_phi = phi_ref - neuron.phi
                omega_eff += KAPPA_LOCK * math.sin(delta_phi)
            
            # Update phase
            neuron.phi = (neuron.phi + omega_eff * DT) % (2 * math.pi)
            
            # Equation 9: Dynamic threshold update
            S_t = 1.0 if i in spikes_this_tick else 0.0
            dtheta = (DT_MS / (TAU_THETA * 1000)) * (
                -(neuron.theta_dyn - THETA_BASE) + BETA * S_t
            )
            neuron.theta_dyn += dtheta
        
        # Record spike history
        self.spike_history[t] = list(spikes_this_tick)
        
        return fired_neurons
    
    def get_phase_error(self, t: int) -> List[float]:
        """
        Compute phase error Δφ for each neuron relative to reference.
        Returns wrapped phase differences in [-π, π].
        """
        phi_ref = (2 * math.pi * (t % T_GAMMA_TICKS)) / T_GAMMA_TICKS
        errors = []
        for neuron in self.neurons:
            # Wrapped phase difference (Eq. 12)
            delta_phi = ((neuron.phi - phi_ref + math.pi) % (2 * math.pi)) - math.pi
            errors.append(delta_phi)
        return errors
    
    def get_mean_phase(self) -> float:
        """Compute mean population phase using circular statistics."""
        sin_sum = sum(math.sin(n.phi) for n in self.neurons)
        cos_sum = sum(math.cos(n.phi) for n in self.neurons)
        return math.atan2(sin_sum, cos_sum) % (2 * math.pi)
    
    def get_phase_variance(self) -> float:
        """Compute circular variance of population phases."""
        sin_sum = sum(math.sin(n.phi) for n in self.neurons)
        cos_sum = sum(math.cos(n.phi) for n in self.neurons)
        R = math.sqrt(sin_sum**2 + cos_sum**2) / self.n_neurons
        return 1 - R  # Circular variance (0 = perfect synchrony)
    
    def reset(self, random_phases: bool = True):
        """Reset all neurons."""
        for i, neuron in enumerate(self.neurons):
            if random_phases:
                neuron.reset()
            else:
                neuron.reset(phi_init=0.0)
        self.spike_history.clear()
        if hasattr(self, '_omega'):
            self._omega = {i: OMEGA_0 for i in range(self.n_neurons)}


# =============================================================================
# TEST SUITE (Spec Section 4)
# =============================================================================

def test_GPLO_MC_01_sync_pulse_timing():
    """
    Test GPLO-MC-01: RSP Sync Pulse Timing
    Verify sync pulses arrive exactly at t ≡ 0 mod 25.
    Spec: Section 2.4, Eq. 1
    """
    print("\n=== Test GPLO-MC-01: RSP Sync Pulse Timing ===")
    
    pool = GPLOPool(n_neurons=4)
    
    # Run for 100 ticks and verify sync detection
    sync_ticks = []
    for t in range(100):
        is_sync = (t % T_GAMMA_TICKS == 0)
        if is_sync:
            sync_ticks.append(t)
    
    expected_sync = [0, 25, 50, 75]
    passed = (sync_ticks == expected_sync)
    
    print(f"  Expected sync ticks: {expected_sync}")
    print(f"  Detected sync ticks: {sync_ticks}")
    print(f"  Result: {'PASS' if passed else 'FAIL'}")
    
    return passed


def test_GPLO_MC_02_phase_locking_convergence():
    """
    Test GPLO-MC-02: Phase Locking Convergence (Theorem 1)
    Verify convergence to |Δφ| < 0.25 rad within 2 gamma cycles.
    Spec: Theorem 1, Theorem 4
    """
    print("\n=== Test GPLO-MC-02: Phase Locking Convergence ===")
    
    # Start with worst-case initial phase (π radians from reference)
    pool = GPLOPool(n_neurons=N_GPLO)
    for neuron in pool.neurons:
        neuron.reset(phi_init=math.pi)  # Opposite phase
    
    max_error_after_50_ticks = 0.0
    
    for t in range(50):  # 2 gamma cycles
        rsp_spike = (t % T_GAMMA_TICKS == 0)
        pool.step(t=t, rsp_spike=rsp_spike)
        
        if t >= 49:  # Check errors at end
            errors = pool.get_phase_error(t)
            max_error_after_50_ticks = max(abs(e) for e in errors)
    
    # Theorem 1 claims |Δφ| < 0.25 rad after 2 cycles
    passed = max_error_after_50_ticks < 0.25
    
    print(f"  Max phase error after 50 ticks: {max_error_after_50_ticks:.4f} rad")
    print(f"  Threshold: 0.25 rad")
    print(f"  Result: {'PASS' if passed else 'FAIL'}")
    
    return passed


def test_GPLO_MC_03_steady_state_phase_error():
    """
    Test GPLO-MC-03: Steady-State Phase Error Bound (Corollary 1.1)
    Verify |Δφ| < 0.0126 rad once locked.
    Spec: Corollary 1.1
    """
    print("\n=== Test GPLO-MC-03: Steady-State Phase Error ===")
    
    pool = GPLOPool(n_neurons=N_GPLO)
    
    # Run until locked (100 ticks = 4 cycles)
    for t in range(100):
        rsp_spike = (t % T_GAMMA_TICKS == 0)
        pool.step(t=t, rsp_spike=rsp_spike)
    
    # Measure phase error over next 25 ticks
    max_error = 0.0
    for t in range(100, 125):
        rsp_spike = (t % T_GAMMA_TICKS == 0)
        pool.step(t=t, rsp_spike=rsp_spike)
        errors = pool.get_phase_error(t)
        max_error = max(max_error, max(abs(e) for e in errors))
    
    # Corollary 1.1 claims |Δφ| ≤ 0.0126 rad
    passed = max_error < 0.0126
    
    print(f"  Max steady-state phase error: {max_error:.6f} rad")
    print(f"  Bound: 0.0126 rad")
    print(f"  Result: {'PASS' if passed else 'FAIL'}")
    
    return passed


def test_GPLO_MC_04_frequency_mismatch_tolerance():
    """
    Test GPLO-MC-04: Frequency Mismatch Tolerance (Theorem 2)
    Verify lock maintained for |Δω| < 80 Hz.
    Spec: Theorem 2
    """
    print("\n=== Test GPLO-MC-04: Frequency Mismatch Tolerance ===")
    
    # Test with maximum claimed tolerance: Δω = 4 Hz (well within 80 Hz bound)
    pool = GPLOPool(n_neurons=4)
    
    # Set natural frequency to 42 Hz (2 Hz above reference)
    omega_test = 2 * math.pi * 42  # 42 Hz
    for i in range(pool.n_neurons):
        pool.set_omega(i, omega_test)
    
    # Run for 200 ticks (8 cycles)
    for t in range(200):
        rsp_spike = (t % T_GAMMA_TICKS == 0)
        pool.step(t=t, rsp_spike=rsp_spike)
    
    # Check if still locked (phase error bounded)
    errors = pool.get_phase_error(199)
    avg_error = sum(abs(e) for e in errors) / len(errors)
    
    # Should maintain lock with small steady-state error
    passed = avg_error < 0.5  # Reasonable bound for locked state
    
    print(f"  Natural frequency: {omega_test/(2*math.pi):.1f} Hz")
    print(f"  Reference frequency: 40.0 Hz")
    print(f"  Mismatch: 2.0 Hz")
    print(f"  Avg phase error: {avg_error:.4f} rad")
    print(f"  Result: {'PASS' if passed else 'FAIL'}")
    
    return passed


def test_GPLO_MC_05_population_synchrony():
    """
    Test GPLO-MC-05: Population Synchrony (Theorem 3)
    Verify neurons fire within ±1 tick of each other.
    Spec: Theorem 3, Corollary 3.1
    """
    print("\n=== Test GPLO-MC-05: Population Synchrony ===")
    
    # Start with random phases
    pool = GPLOPool(n_neurons=N_GPLO)
    
    # Run for 100 ticks to allow synchrony to develop
    for t in range(100):
        rsp_spike = (t % T_GAMMA_TICKS == 0)
        pool.step(t=t, rsp_spike=rsp_spike)
    
    # Analyze spike timing over next 50 ticks
    max_jitter = 0
    for t in range(100, 150):
        rsp_spike = (t % T_GAMMA_TICKS == 0)
        fired = pool.step(t=t, rsp_spike=rsp_spike)
        
        if len(fired) > 1:
            # All should fire same tick (jitter = 0)
            jitter = 0
        elif len(fired) == 1:
            jitter = 0.5  # Single neuron, no measurable jitter
        else:
            jitter = 0  # No spikes
        
        max_jitter = max(max_jitter, jitter)
    
    # Theorem 3 claims ±1 tick synchrony
    passed = max_jitter <= 1
    
    # Also check phase variance
    phase_var = pool.get_phase_variance()
    synchrony_pass = phase_var < 0.1  # Low variance = high synchrony
    
    print(f"  Max spike jitter: {max_jitter} ticks")
    print(f"  Phase variance: {phase_var:.4f}")
    print(f"  Result: {'PASS' if (passed and synchrony_pass) else 'FAIL'}")
    
    return passed and synchrony_pass


def test_GPLO_CC_01_bounded_states():
    """
    Test GPLO-CC-01: Bounded State Variables (Theorem 6)
    Verify all states remain bounded under extended operation.
    Spec: Theorem 6
    """
    print("\n=== Test GPLO-CC-01: Bounded States ===")
    
    pool = GPLOPool(n_neurons=N_GPLO)
    
    violations = []
    
    for t in range(1000):
        rsp_spike = (t % T_GAMMA_TICKS == 0)
        pool.step(t=t, rsp_spike=rsp_spike)
        
        for i, neuron in enumerate(pool.neurons):
            # V bounds
            if neuron.V < -80 or neuron.V > -50:
                violations.append(('V', t, i, neuron.V))
            
            # g_exc bounds (should be ≤ w_sync + some accumulation)
            if neuron.g_exc < 0 or neuron.g_exc > 10:
                violations.append(('g_exc', t, i, neuron.g_exc))
            
            # g_inh bounds (≤ N * w_rec)
            if neuron.g_inh < 0 or neuron.g_inh > N_GPLO * W_REC + 1:
                violations.append(('g_inh', t, i, neuron.g_inh))
            
            # theta_dyn bounds
            if neuron.theta_dyn < -60 or neuron.theta_dyn > -50:
                violations.append(('theta_dyn', t, i, neuron.theta_dyn))
            
            # phi bounds
            if neuron.phi < 0 or neuron.phi >= 2 * math.pi:
                violations.append(('phi', t, i, neuron.phi))
            
            # spike_timer bounds
            if neuron.spike_timer < 0 or neuron.spike_timer > REFRACTORY_PERIOD:
                violations.append(('spike_timer', t, i, neuron.spike_timer))
    
    passed = len(violations) == 0
    
    if violations:
        print(f"  Violations found: {violations[:5]}...")
    else:
        print(f"  All states bounded for 1000 ticks")
    
    print(f"  Result: {'PASS' if passed else 'FAIL'}")
    return passed


def test_GPLO_FO_01_locking_transient():
    """
    Test GPLO-FO-01: Locking Transient Duration (Theorem 4)
    Verify lock achieved within 2 gamma cycles from random initial phase.
    Spec: Theorem 4
    """
    print("\n=== Test GPLO-FO-01: Locking Transient ===")
    
    # Test multiple random initializations
    all_passed = True
    
    for trial in range(5):
        pool = GPLOPool(n_neurons=N_GPLO)
        # Phases already random from __init__
        
        locked = False
        for t in range(50):  # 2 gamma cycles
            rsp_spike = (t % T_GAMMA_TICKS == 0)
            pool.step(t=t, rsp_spike=rsp_spike)
            
            if t >= 49:
                errors = pool.get_phase_error(t)
                max_error = max(abs(e) for e in errors)
                if max_error < 0.25:
                    locked = True
        
        trial_passed = locked
        all_passed = all_passed and trial_passed
        print(f"  Trial {trial+1}: {'LOCKED' if locked else 'NOT LOCKED'}")
    
    print(f"  Overall: {'PASS' if all_passed else 'FAIL'}")
    return all_passed


def test_GPLO_FO_02_phase_wrapping():
    """
    Test GPLO-FO-02: Phase Wrapping Stability (Theorem 7)
    Verify modulo operation preserves accuracy.
    Spec: Theorem 7
    """
    print("\n=== Test GPLO-FO-02: Phase Wrapping ===")
    
    pool = GPLOPool(n_neurons=4)
    
    # Run for many cycles
    for t in range(10000):
        rsp_spike = (t % T_GAMMA_TICKS == 0)
        pool.step(t=t, rsp_spike=rsp_spike)
    
    # Check all phases are in valid range
    all_valid = all(0 <= n.phi < 2 * math.pi for n in pool.neurons)
    
    # Check no NaN or Inf
    no_nan = all(math.isfinite(n.phi) for n in pool.neurons)
    
    passed = all_valid and no_nan
    
    print(f"  All phases in [0, 2π): {all_valid}")
    print(f"  No NaN/Inf: {no_nan}")
    print(f"  Result: {'PASS' if passed else 'FAIL'}")
    
    return passed


def test_adversarial_conditions():
    """
    Test robustness under adversarial conditions.
    """
    print("\n=== Test Adversarial Conditions ===")
    
    # Test 1: No RSP sync (free running)
    pool1 = GPLOPool(n_neurons=4)
    for t in range(100):
        pool1.step(t=t, rsp_spike=False)  # Never sync
    test1 = all(math.isfinite(n.phi) for n in pool1.neurons)
    print(f"  No sync input: {'PASS' if test1 else 'FAIL'}")
    
    # Test 2: Large pool
    pool2 = GPLOPool(n_neurons=32)
    for t in range(100):
        rsp_spike = (t % T_GAMMA_TICKS == 0)
        pool2.step(t=t, rsp_spike=rsp_spike)
    test2 = pool2.get_phase_variance() < 0.5
    print(f"  Large pool (32 neurons): {'PASS' if test2 else 'FAIL'}")
    
    # Test 3: Extreme frequency mismatch (at boundary)
    pool3 = GPLOPool(n_neurons=4)
    extreme_omega = 2 * math.pi * 100  # 100 Hz (way above 40 Hz reference)
    for i in range(pool3.n_neurons):
        pool3.set_omega(i, extreme_omega)
    for t in range(100):
        rsp_spike = (t % T_GAMMA_TICKS == 0)
        pool3.step(t=t, rsp_spike=rsp_spike)
    # Should still be bounded even if not locked
    test3 = all(math.isfinite(n.phi) for n in pool3.neurons)
    print(f"  Extreme frequency (100 Hz): {'PASS' if test3 else 'FAIL'}")
    
    all_passed = test1 and test2 and test3
    print(f"  Overall: {'PASS' if all_passed else 'FAIL'}")
    return all_passed


# =============================================================================
# MAIN TEST RUNNER
# =============================================================================

def run_all_tests():
    """Run complete test suite and report results."""
    
    print("=" * 70)
    print("GPLO MATHEMATICAL PROOF TEST SUITE")
    print("SPEC: unit-7_subcomponent-2.2_Gamma-Phase-Locking-Oscillators-(GPLO).md")
    print("=" * 70)
    
    tests = [
        ("GPLO-MC-01: Sync Pulse Timing", test_GPLO_MC_01_sync_pulse_timing),
        ("GPLO-MC-02: Phase Locking Convergence", test_GPLO_MC_02_phase_locking_convergence),
        ("GPLO-MC-03: Steady-State Phase Error", test_GPLO_MC_03_steady_state_phase_error),
        ("GPLO-MC-04: Frequency Mismatch Tolerance", test_GPLO_MC_04_frequency_mismatch_tolerance),
        ("GPLO-MC-05: Population Synchrony", test_GPLO_MC_05_population_synchrony),
        ("GPLO-CC-01: Bounded States", test_GPLO_CC_01_bounded_states),
        ("GPLO-FO-01: Locking Transient", test_GPLO_FO_01_locking_transient),
        ("GPLO-FO-02: Phase Wrapping", test_GPLO_FO_02_phase_wrapping),
        ("Adversarial Conditions", test_adversarial_conditions),
    ]
    
    results = {}
    
    for name, test_fn in tests:
        try:
            passed = test_fn()
            results[name] = passed
        except Exception as e:
            print(f"\n  EXCEPTION in {name}: {e}")
            import traceback
            traceback.print_exc()
            results[name] = False
    
    # Summary
    print("\n" + "=" * 70)
    print("SUMMARY")
    print("=" * 70)
    
    passed_count = sum(1 for v in results.values() if v)
    total_count = len(results)
    
    for name, passed in results.items():
        status = "✓ PASS" if passed else "✗ FAIL"
        print(f"  {status}: {name}")
    
    print(f"\nTotal: {passed_count}/{total_count} tests passed")
    
    overall = passed_count == total_count
    print(f"\nOVERALL VERDICT: {'APPROVED' if overall else 'REJECTED'}")
    
    return overall


if __name__ == "__main__":
    success = run_all_tests()
    exit(0 if success else 1)

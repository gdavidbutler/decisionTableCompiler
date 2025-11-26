#!/usr/bin/env python3
"""
Test decision table evaluators.

Generated with Claude Code (https://claude.ai/code)
Co-Authored-By: Claude <noreply@anthropic.com>
"""

import power
import DisjunctiveNormalForm as dnf

# Test power decision table (single output)
pwr = power.evaluate(
    power.APU_PWR.OFF,
    power.EXT_PLG.OFF,
    power.K701.OFF,
    power.K702.OFF,
    power.K703.OFF,
    power.K704.OFF,
    power.L_AMAD_RPM.FLGT_IDLE,
    power.L_THR.OFF,
    power.R_AMAD_RPM.FLGT_IDLE,
    power.R_THR.OFF
)
print(f"Power (all OFF): {pwr.name} (expected BATT)")

# Test traffic light decision table
accel, brake, proceed = dnf.evaluate(
    dnf.canStop.yes,
    dnf.isClose.no,
    dnf.signal.green
)
print(f"Green light: accel={accel.name} brake={brake.name} proceed={proceed.name}")

accel, brake, proceed = dnf.evaluate(
    dnf.canStop.no,
    dnf.isClose.yes,
    dnf.signal.yellow
)
print(f"Yellow (can't stop): accel={accel.name} brake={brake.name} proceed={proceed.name}")

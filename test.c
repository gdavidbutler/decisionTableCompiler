/*
 * decisionTableCompiler - generate optimal pseudocode for decision tables
 * Copyright (C) 1993-2025 G. David Butler <gdb@dbSystems.com>
 *
 * This file is part of decisionTableCompiler
 *
 * decisionTableCompiler is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * decisionTableCompiler is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include "power.h"
#include "DisjunctiveNormalForm.h"

int
main(
  void
){
  enum power_POWER_e Pwr;
  enum DisjunctiveNormalForm_accelerator_e Accel;
  enum DisjunctiveNormalForm_brake_e Brake;
  enum DisjunctiveNormalForm_proceed_e Proceed;

  /* Test power decision table */
  powerEvaluate(
    power_APU_PWR_OFF, power_EXT_PLG_OFF,
    power_K701_OFF, power_K702_OFF, power_K703_OFF, power_K704_OFF,
    power_L_AMAD_RPM_FLGT_IDLE, power_L_THR_OFF,
    power_R_AMAD_RPM_FLGT_IDLE, power_R_THR_OFF,
    &Pwr
  );
  printf("Power (all OFF): %d (expected BATT=%d)\n", Pwr, power_POWER_BATT);

  /* Test traffic light decision table */
  DisjunctiveNormalFormEvaluate(
    DisjunctiveNormalForm_canStop_yes,
    DisjunctiveNormalForm_isClose_no,
    DisjunctiveNormalForm_signal_green,
    &Accel, &Brake, &Proceed
  );
  printf("Green light: accel=%d brake=%d proceed=%d\n", Accel, Brake, Proceed);

  DisjunctiveNormalFormEvaluate(
    DisjunctiveNormalForm_canStop_no,
    DisjunctiveNormalForm_isClose_yes,
    DisjunctiveNormalForm_signal_yellow,
    &Accel, &Brake, &Proceed
  );
  printf("Yellow (can't stop): accel=%d brake=%d proceed=%d\n", Accel, Brake, Proceed);

  return (0);
}

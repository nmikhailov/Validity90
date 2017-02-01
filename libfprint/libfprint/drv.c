/*
 * Functions to assist with asynchronous driver <---> library communications
 * Copyright (C) 2007-2008 Daniel Drake <dsd@gentoo.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define FP_COMPONENT "drv"

#include <config.h>
#include <errno.h>

#include "fp_internal.h"

/* SSM: sequential state machine
 * Asynchronous driver design encourages some kind of state machine behind it.
 * In most cases, the state machine is entirely linear - you only go to the
 * next state, you never jump or go backwards. The SSM functions help you
 * implement such a machine.
 *
 * e.g. S1 --> S2 --> S3 --> S4
 * S1 is the start state
 * There is also an implicit error state and an implicit accepting state
 * (both with implicit edges from every state).
 *
 * You can also jump to any arbitrary state (while marking completion of the
 * current state) while the machine is running. In other words there are
 * implicit edges linking one state to every other state. OK, we're stretching
 * the "state machine" description at this point.
 *
 * To create a ssm, you pass a state handler function and the total number of
 * states (4 in the above example).
 *
 * To start a ssm, you pass in a completion callback function which gets
 * called when the ssm completes (both on error and on failure).
 *
 * To iterate to the next state, call fpi_ssm_next_state(). It is legal to
 * attempt to iterate beyond the final state - this is equivalent to marking
 * the ssm as successfully completed.
 *
 * To mark successful completion of a SSM, either iterate beyond the final
 * state or call fpi_ssm_mark_completed() from any state.
 *
 * To mark failed completion of a SSM, call fpi_ssm_mark_aborted() from any
 * state. You must pass a non-zero error code.
 *
 * Your state handling function looks at ssm->cur_state in order to determine
 * the current state and hence which operations to perform (a switch statement
 * is appropriate).
 * Typically, the state handling function fires off an asynchronous libusb
 * transfer, and the callback function iterates the machine to the next state
 * upon success (or aborts the machine on transfer failure).
 *
 * Your completion callback should examine ssm->error in order to determine
 * whether the ssm completed or failed. An error code of zero indicates
 * successful completion.
 */

/* Allocate a new ssm */
struct fpi_ssm *fpi_ssm_new(struct fp_dev *dev, ssm_handler_fn handler,
	int nr_states)
{
	struct fpi_ssm *machine;
	BUG_ON(nr_states < 1);

	machine = g_malloc0(sizeof(*machine));
	machine->handler = handler;
	machine->nr_states = nr_states;
	machine->dev = dev;
	machine->completed = TRUE;
	return machine;
}

/* Free a ssm */
void fpi_ssm_free(struct fpi_ssm *machine)
{
	if (!machine)
		return;
	g_free(machine);
}

/* Invoke the state handler */
static void __ssm_call_handler(struct fpi_ssm *machine)
{
	fp_dbg("%p entering state %d", machine, machine->cur_state);
	machine->handler(machine);
}

/* Start a ssm. You can also restart a completed or aborted ssm. */
void fpi_ssm_start(struct fpi_ssm *ssm, ssm_completed_fn callback)
{
	BUG_ON(!ssm->completed);
	ssm->callback = callback;
	ssm->cur_state = 0;
	ssm->completed = FALSE;
	ssm->error = 0;
	__ssm_call_handler(ssm);
}

static void __subsm_complete(struct fpi_ssm *ssm)
{
	struct fpi_ssm *parent = ssm->parentsm;
	BUG_ON(!parent);
	if (ssm->error)
		fpi_ssm_mark_aborted(parent, ssm->error);
	else
		fpi_ssm_next_state(parent);
	fpi_ssm_free(ssm);
}

/* start a SSM as a child of another. if the child completes successfully, the
 * parent will be advanced to the next state. if the child aborts, the parent
 * will be aborted with the same error code. the child will be automatically
 * freed upon completion/abortion. */
void fpi_ssm_start_subsm(struct fpi_ssm *parent, struct fpi_ssm *child)
{
	child->parentsm = parent;
	fpi_ssm_start(child, __subsm_complete);
}

/* Mark a ssm as completed successfully. */
void fpi_ssm_mark_completed(struct fpi_ssm *machine)
{
	BUG_ON(machine->completed);
	machine->completed = TRUE;
	fp_dbg("%p completed with status %d", machine, machine->error);
	if (machine->callback)
		machine->callback(machine);
}

/* Mark a ssm as aborted with error. */
void fpi_ssm_mark_aborted(struct fpi_ssm *machine, int error)
{
	fp_dbg("error %d from state %d", error, machine->cur_state);
	BUG_ON(error == 0);
	machine->error = error;
	fpi_ssm_mark_completed(machine);
}

/* Iterate to next state of a ssm */
void fpi_ssm_next_state(struct fpi_ssm *machine)
{
	BUG_ON(machine->completed);
	machine->cur_state++;
	if (machine->cur_state == machine->nr_states) {
		fpi_ssm_mark_completed(machine);
	} else {
		__ssm_call_handler(machine);
	}
}

void fpi_ssm_jump_to_state(struct fpi_ssm *machine, int state)
{
	BUG_ON(machine->completed);
	BUG_ON(state >= machine->nr_states);
	machine->cur_state = state;
	__ssm_call_handler(machine);
}


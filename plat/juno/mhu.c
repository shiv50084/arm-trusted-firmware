/*
 * Copyright (c) 2014-2015, ARM Limited and Contributors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of ARM nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <arch_helpers.h>
#include <assert.h>
#include <bakery_lock.h>
#include <mmio.h>
#include "juno_def.h"
#include "juno_private.h"
#include "mhu.h"

/* SCP MHU secure channel registers */
#define SCP_INTR_S_STAT		0x200
#define SCP_INTR_S_SET		0x208
#define SCP_INTR_S_CLEAR	0x210

/* CPU MHU secure channel registers */
#define CPU_INTR_S_STAT		0x300
#define CPU_INTR_S_SET		0x308
#define CPU_INTR_S_CLEAR	0x310

#if IMAGE_BL31
#if USE_COHERENT_MEM
static bakery_lock_t mhu_secure_lock __attribute__ ((section("tzfw_coherent_mem")));
#define LOCK_ARG		&mhu_secure_lock
#else
#define LOCK_ARG		JUNO_MHU_BAKERY_ID
#endif /*__USE_COHERENT_MEM__ */
#else
#define LOCK_ARG	/* Locks required only for BL3-1 images */
#endif /* __IMAGE_BL31__ */

/*
 * Slot 31 is reserved because the MHU hardware uses this register bit to
 * indicate a non-secure access attempt. The total number of available slots is
 * therefore 31 [30:0].
 */
#define MHU_MAX_SLOT_ID		30

void mhu_secure_message_start(unsigned int slot_id)
{
	assert(slot_id <= MHU_MAX_SLOT_ID);

	juno_lock_get(LOCK_ARG);

	/* Make sure any previous command has finished */
	while (mmio_read_32(MHU_BASE + CPU_INTR_S_STAT) & (1 << slot_id))
		;
}

void mhu_secure_message_send(unsigned int slot_id)
{
	assert(slot_id <= MHU_MAX_SLOT_ID);
	assert(!(mmio_read_32(MHU_BASE + CPU_INTR_S_STAT) & (1 << slot_id)));

	/* Send command to SCP */
	mmio_write_32(MHU_BASE + CPU_INTR_S_SET, 1 << slot_id);
}

void mhu_secure_message_wait(unsigned int slot_id)
{
	assert(slot_id <= MHU_MAX_SLOT_ID);

	/* Wait for response from SCP */
	while (!(mmio_read_32(MHU_BASE + SCP_INTR_S_STAT) & (1 << slot_id)))
		;
}

void mhu_secure_message_end(unsigned int slot_id)
{
	assert(slot_id <= MHU_MAX_SLOT_ID);

	/*
	 * Clear any response we got by writing one in the relevant slot bit to
	 * the CLEAR register
	 */
	mmio_write_32(MHU_BASE + SCP_INTR_S_CLEAR, 1 << slot_id);

	juno_lock_release(LOCK_ARG);
}

void mhu_secure_init(void)
{
	juno_lock_init(LOCK_ARG);

	/*
	 * The STAT register resets to zero. Ensure it is in the expected state,
	 * as a stale or garbage value would make us think it's a message we've
	 * already sent.
	 */
	assert(mmio_read_32(MHU_BASE + CPU_INTR_S_STAT) == 0);
}

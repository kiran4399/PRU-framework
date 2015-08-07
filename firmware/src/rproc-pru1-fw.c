/*
 * Copyright (c) 2014, Texas Instruments, Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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
 * POSSIBILITY OF SUCH DAMAGE. */

/* This program waits for sysevent 16 from ARM. It sends and interrupt to ARM
   using sysevent 32 which is generated by writing to its own R31 register.
   Sysevent 16 is then cleared by this code. The ARM remoteproc driver is
   responsible for clearing sysevent 32 */

#include <stdint.h>
#include <pru_cfg.h>
#include <pru_intc.h>
#include "resource_table_1.h"
#include <pru_vring.h>
volatile register uint32_t __R30;
volatile register uint32_t __R31;

struct pru_vring tx_ring;
struct pru_vring rx_ring;

/* Mapping Constant table register to variable */
volatile pruCfg CT_CFG __attribute__((cregister("PRU_CFG", near), peripheral));
volatile far pruIntc CT_INTC __attribute__((cregister("PRU_INTC", far), peripheral));
volatile far uint32_t CT_DRAM __attribute__((cregister("PRU_DMEM_0_1", near), peripheral));

/* Defines */
#define PRU1
#define HOST1_MASK		(0x80000000)
#define PRU0_PRU1_EVT	(16)
#define RSC_TABLE		(0x00000500)

#define TOGGLE_BLUE (__R30 ^= (1 << 7))

static inline void *pa_to_da(u32 pa)
{
	/* we don't support physical addresses in GPMC */
	if (pa < 0x00080000)
		return NULL;

	return (void *)pa;
}

void main(){
	 __delay_cycles(200000000); //Wait for drivers to load on host processor
	struct my_resource_table volatile * const rsc = (struct my_resource_table *)RSC_TABLE;

	struct fw_rsc_vdev_vring tx_vring_rsc = rsc->rpmsg_vring0;
	struct fw_rsc_vdev_vring rx_vring_rsc = rsc->rpmsg_vring1;
	struct vring_desc *vring_desc = NULL;
	pru_vring_init(&tx_ring, "tx", &tx_vring_rsc);
	pru_vring_init(&rx_ring, "rx", &rx_vring_rsc);
	
	/* Clear SYSCFG[STANDBY_INIT] to enable OCP master port */
	CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;

	/* Configure GPI and GPO as Mode 0 (Direct Connect) */
	CT_CFG.GPCFG0 = 0x0000;

	/* Clear GPO pins */
	__R30 &= 0x00000000;
	int tx_data[1]={99};
	void *ptr;
	int i=0;

	for(i=0;i<5;i++) {
			if(pru_vring_buf_is_avail(&tx_ring)) {
				
				vring_desc = pru_vring_get_next_avail_desc(&tx_ring);

				ptr = pa_to_da(vring_desc->addr);

				memcpy(ptr,tx_data,sizeof(tx_data));

				vring_desc->len = sizeof(tx_data);
				vring_desc->flags |= VRING_DESC_F_NEXT;	/* no buffer is linked to this */
				pru_vring_push_one(&tx_ring, sizeof(tx_data)); //must be size of data written to buffer
				
				//__delay_cycles(100000000);
				
			}
		}
		vring_desc->flags &= ~VRING_DESC_F_NEXT;	/* no buffer is linked to this */
		__R31 |= 0x00000021; //send sys ent 32

	/* Spin in loop until interrupt on HOST 1 is detected */
	while(1){
		//Read value from ddr
		
		
		/* Wait for sysevent 16 from ARM which mapped to HOST1 */
		if (__R31 & HOST1_MASK){
			TOGGLE_BLUE;

			//pDdr[2] = pDdr[0] + pDdr[1];
			
			/* Interrupt ARM using sys evt 32 */
			//__R31 |= 0x00000021;
			
			/* Clear interrupt event */
			CT_INTC.SICR = 16;
			__delay_cycles(5);
		}
	}

	/* Halt the PRU core - shouldn't get here */
	//__halt();
}

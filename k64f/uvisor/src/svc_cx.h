/***************************************************************
 * This confidential and  proprietary  software may be used only
 * as authorised  by  a licensing  agreement  from  ARM  Limited
 *
 *             (C) COPYRIGHT 2013-2014 ARM Limited
 *                      ALL RIGHTS RESERVED
 *
 *  The entire notice above must be reproduced on all authorised
 *  copies and copies  may only be made to the  extent permitted
 *  by a licensing agreement from ARM Limited.
 *
 ***************************************************************/
#ifndef __SVC_CX_H__
#define __SVC_CX_H__

#include <uvisor.h>
#include <uvisor-lib.h>
#include "vmpu.h"

#define SVC_CX_EXC_SF_SIZE 8

#define SVC_CX_xPSR_DW_Pos 9
#define SVC_CX_xPSR_DW(x)  ((uint32_t) (x) << SVC_CX_xPSR_DW_Pos)
#define SVC_CX_xPSR_DW_Msk SVC_CX_xPSR_DW(1)

#define SVC_CX_SP_DW_Pos 2
#define SVC_CX_SP_DW(x)  ((uint32_t) (x) << SVC_CX_SP_DW_Pos)
#define SVC_CX_SP_DW_Msk SVC_CX_SP_DW(1)

#define SVC_CX_MAX_DEPTH 0x10

typedef struct {
    uint8_t   rfu[3];  /* for 32 bit alignment */
    uint8_t   src_id;
    uint32_t *src_sp;
} PACKED TBoxCx;

/* FIXME this goes somewhere else */
#define SVC_CX_MAX_BOXES   0x80U

/* state variables */
extern TBoxCx    g_svc_cx_state[SVC_CX_MAX_DEPTH];
extern int       g_svc_cx_state_ptr;
extern uint32_t *g_svc_cx_curr_sp[SVC_CX_MAX_BOXES];
extern uint8_t   g_svc_cx_curr_id;

static inline uint8_t svc_cx_get_src_id(void)
{
    return g_svc_cx_state[g_svc_cx_state_ptr].src_id;
}

static inline uint32_t *svc_cx_get_src_sp(void)
{
    return g_svc_cx_state[g_svc_cx_state_ptr].src_sp;
}

static inline uint32_t *svc_cx_get_curr_sp(uint8_t box_id)
{
    return g_svc_cx_curr_sp[box_id];
}

static inline uint8_t svc_cx_get_curr_id(void)
{
    return g_svc_cx_curr_id;
}

static void inline svc_cx_push_state(uint8_t src_id, uint32_t *src_sp,
                                     uint8_t dst_id)
{
    /* check state stack overflow */
    if(g_svc_cx_state_ptr == SVC_CX_MAX_DEPTH - 1)
    {
        /* FIXME raise fault */
        while(1);
    }

    /* push state */
    g_svc_cx_state[g_svc_cx_state_ptr].src_id = src_id;
    g_svc_cx_state[g_svc_cx_state_ptr].src_sp = src_sp;
    ++g_svc_cx_state_ptr;

    /* save curr stack pointer for the src box */
    g_svc_cx_curr_sp[src_id] = src_sp;

    /* update current box id */
    g_svc_cx_curr_id = dst_id;
}

static inline void svc_cx_pop_state(uint8_t dst_id, uint32_t *dst_sp)
{
    /* check state stack underflow */
    if(!g_svc_cx_state_ptr)
    {
        /* FIXME raise fault */
        while(1);
    }

    /* pop state */
    --g_svc_cx_state_ptr;

    /* save curr stack pointer for the dst box */
    uint32_t dst_sp_align = (dst_sp[7] & SVC_CX_xPSR_DW_Msk) ? 1 : 0;
    g_svc_cx_curr_sp[dst_id] = dst_sp + SVC_CX_EXC_SF_SIZE + dst_sp_align;

    /* update current box id */
    g_svc_cx_curr_id = svc_cx_get_src_id();
}

static inline uint32_t *svc_cx_validate_sf(uint32_t *sp)
{
    /* the box stack pointer is validated through direct access: if it has been
     * tampered with access is proihibited by the uvisor and a fault occurs;
     * the approach is applied to the whole stack frame:
     *   from sp[0] to sp[7] if the stack is double word aligned
     *   from sp[0] to sp[8] if the stack is not double word aligned */
    uint32_t tmp = 0;
    asm volatile(
        "ldrt   %[tmp], [%[sp]]\n"                     /* test sp[0] */
        "ldrt   %[tmp], [%[sp], %[exc_sf_size]]\n"     /* test sp[7] */
        "tst    %[tmp], %[exc_sf_dw_msk]\n"            /* test alignment */
        "it     ne\n"
        "ldrtne %[tmp], [%[sp], %[max_exc_sf_size]]\n" /* test sp[8] */
        : [tmp]             "=r" (tmp)
        : [sp]              "r"  ((uint32_t) sp),
          [exc_sf_dw_msk]   "i"  ((uint32_t) SVC_CX_xPSR_DW_Msk),
          [exc_sf_size]     "i"  ((uint32_t) (sizeof(uint32_t) *
                                              (SVC_CX_EXC_SF_SIZE - 1))),
          [max_exc_sf_size] "i"  ((uint32_t) (sizeof(uint32_t) *
                                              (SVC_CX_EXC_SF_SIZE)))
    );

    /* the initial stack pointer, if validated, is returned unchanged */
    return sp;
}

static inline uint32_t *svc_cx_create_sf(uint32_t *src_sp, uint32_t *dst_sp,
                                         uint32_t  thunk,  uint32_t  dst_fn)
{
    uint32_t  dst_sf_align;
    uint32_t *dst_sf;

    /* create destination stack frame */
    dst_sf_align = ((uint32_t) dst_sp & SVC_CX_SP_DW_Msk) ? 1 : 0;
    dst_sf       = dst_sp - SVC_CX_EXC_SF_SIZE - dst_sf_align;

    /* populate destination stack frame:
     *   r0 - r3         are copied
     *   r12             is cleared
     *   lr              is the thunk function
     *   return address  is the destination function
     *   xPSR            is modified to account for stack alignment */
    memcpy((void *) dst_sf, (void *) src_sp, sizeof(uint32_t) * 4);
    dst_sf[4] = 0;
    dst_sf[5] = thunk;
    dst_sf[6] = dst_fn;
    dst_sf[7] = src_sp[7] | SVC_CX_xPSR_DW(dst_sf_align);

    return dst_sf;
}

static inline void svc_cx_return_sf(uint32_t *src_sp, uint32_t *dst_sp)
{
    /* copy return value to source stack */
    src_sp[0] = dst_sp[0];
}
#endif/*__SVC_CX_H__*/
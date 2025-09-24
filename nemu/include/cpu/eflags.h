#ifndef __EFLAGS_H__
#define __EFLAGS_H__

#include "common.h"

void update_eflags_pf_zf_sf(uint32_t);

static inline bool check_cc_e(){
        return cpu.eflags.ZF;
}


static inline bool check_cc_be(){
        return cpu.eflags.CF | cpu.eflags.ZF;

}

static inline bool check_cc_ne(){
        return !cpu.eflags.ZF;
}

#endif

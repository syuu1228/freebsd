
#ifndef _BIOSEMUL_H_
#define	_BIOSEMUL_H_

#include <stdbool.h>
#include <machine/vmm.h>
#include <vmmapi.h>

void biosemul_init(struct vmctx *ctx, int vcpu, char *lomem, int trace_mode);
int biosemul_call(struct vmctx *ctx, int vcpu);
bool biosemul_inout_registered(int in, int port);
int biosemul_inout(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
	uint32_t *eax, int strict);
#endif


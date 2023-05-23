#ifndef TOY_RISCV_KERNEL_KERNEL_PLIC_H
#define TOY_RISCV_KERNEL_KERNEL_PLIC_H

void plicinit(void);

void plicinithart(void);

// ask the PLIC what interrupt we should serve.
int plic_claim(void);

// tell the PLIC we've served this IRQ.
void plic_complete(int irq);

#endif //TOY_RISCV_KERNEL_KERNEL_PLIC_H

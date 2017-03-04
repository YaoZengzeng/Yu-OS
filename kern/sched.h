#ifndef YUOS_KERN_SHCED_H
#define YUOS_KERN_SHCED_H

// This function does not return.
void sched_yield(void) __attribute__((noreturn));

#endif /* !YUOS_KERN_SHCED_H */
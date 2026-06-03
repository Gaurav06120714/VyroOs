#ifndef POWER_H
#define POWER_H

void power_shutdown();   // power off the machine (does not return)
void power_reboot();     // reset the machine (does not return)
void power_halt();       // stop the BSP (CLI + HLT loop) — does not return
void power_suspend();    // suspend-to-RAM placeholder (halts today)

#endif

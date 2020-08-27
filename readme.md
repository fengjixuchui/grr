# GRR
The GRUB Replacing Rootkit. GRR is a "malicious" bare metal hypervisor built
atop AMD SVM.

## Design
The project is broken into two stages efi and hv. The goal of the efi stage
is to load the kernel and initrd from disk, and gather system specific structs.
Then it jumps into the hv stage. The hv stage is the hypvervisor itself, it
sets up kernel boot parameters, then jumps into the kernel now as a guest.

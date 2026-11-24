/*
 * tfp0probe — checkra1n kernel-access probe (READ ONLY)
 *
 * Run this as root on a checkra1n-jailbroken device, over SSH from a connected
 * computer (see migrator/connect.sh). It tries to borrow checkra1n's kernel task
 * port two ways and, if it gets one, proves real kernel read access. It writes
 * NOTHING to the kernel or disk — it only reads and prints.
 *
 * Why a command-line binary instead of the Amethyst app: checkra1n does not hand
 * the kernel task port to UI apps (confirmed by the checkra1n team — "UI apps
 * running as root are generally cursed"), but a root CLI binary over SSH can get
 * it. This probe tells us, on THIS device, whether that path works before we build
 * anything destructive on top of it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>

#define MH_MAGIC_64 0xfeedfacf

static mach_port_t try_get_tfp0(void) {
    mach_port_t port = MACH_PORT_NULL;
    kern_return_t kr;

    kr = host_get_special_port(mach_host_self(), 0, 4, &port);
    printf("[1] host_get_special_port(host, 0, 4): kr=%d (%s) port=0x%x\n",
           kr, mach_error_string(kr), port);
    if (MACH_PORT_VALID(port)) return port;

    port = MACH_PORT_NULL;
    kr = task_for_pid(mach_task_self(), 0, &port);
    printf("[2] task_for_pid(self, 0): kr=%d (%s) port=0x%x\n",
           kr, mach_error_string(kr), port);
    if (MACH_PORT_VALID(port)) return port;

    return MACH_PORT_NULL;
}

int main(void) {
    printf("=== tfp0probe (read-only) ===\n");
    printf("uid=%d euid=%d pid=%d\n", getuid(), geteuid(), getpid());

    mach_port_t tfp0 = try_get_tfp0();
    if (!MACH_PORT_VALID(tfp0)) {
        printf("\nRESULT: NO kernel task port. checkra1n did not share kernel access "
               "with this root CLI process.\n");
        return 1;
    }
    printf("\nGot a candidate kernel task port: 0x%x\n", tfp0);

    /* Sanity 1: the kernel task's pid should be 0. */
    int pid = -1;
    kern_return_t kr = pid_for_task(tfp0, &pid);
    printf("pid_for_task(port): kr=%d pid=%d  (expect pid=0)\n", kr, pid);

    /* Sanity 2: walk kernel memory upward looking for the kernelcache Mach-O
     * header (MH_MAGIC_64). This proves real kernel READ access and finds the
     * (slid) kernel base. Nothing is written. */
    mach_vm_address_t addr = 0xFFFFFFF000000000ULL;
    int found = 0;
    for (int i = 0; i < 0x10000 && !found; i++) {
        mach_vm_size_t rsize = 0;
        vm_region_basic_info_data_64_t info;
        mach_msg_type_number_t cnt = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t obj = MACH_PORT_NULL;
        kr = mach_vm_region(tfp0, &addr, &rsize, VM_REGION_BASIC_INFO_64,
                            (vm_region_info_t)&info, &cnt, &obj);
        if (kr != KERN_SUCCESS) {
            printf("mach_vm_region stopped at 0x%llx: kr=%d (%s)\n",
                   (unsigned long long)addr, kr, mach_error_string(kr));
            break;
        }
        uint32_t magic = 0;
        mach_vm_size_t out = 0;
        kr = mach_vm_read_overwrite(tfp0, addr, sizeof(magic),
                                    (mach_vm_address_t)&magic, &out);
        if (kr == KERN_SUCCESS && magic == MH_MAGIC_64) {
            printf("\nFOUND kernelcache header (MH_MAGIC_64) at 0x%llx\n",
                   (unsigned long long)addr);
            printf("  => kernel slide vs unslid base 0xFFFFFFF007004000: 0x%llx\n",
                   (unsigned long long)(addr - 0xFFFFFFF007004000ULL));
            found = 1;
            break;
        }
        addr += (rsize ? rsize : 0x1000);
    }

    if (found) {
        printf("\nRESULT: SUCCESS — root CLI has real kernel read access via checkra1n.\n");
        printf("The computer-driven migration path is viable on this device.\n");
        return 0;
    }

    printf("\nRESULT: got a port (0x%x) but could NOT confirm kernel read "
           "(no kernelcache header found). Inconclusive — do not proceed.\n", tfp0);
    return 2;
}

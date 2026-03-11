#include "kernel.hh"
#include "k-apic.hh"
#include "k-vmiter.hh"
#include "obj/k-firstprocess.h"
#include <atomic>

// kernel.cc
//
//    This is the kernel.


// INITIAL PHYSICAL MEMORY LAYOUT
//
//  +-------------- Base Memory --------------+
//  v                                         v
// +-----+--------------------+----------------+--------------------+---------/
// |     | Kernel      Kernel |       :    I/O | App 1        App 1 | App 2
// |     | Code + Data  Stack |  ...  : Memory | Code + Data  Stack | Code ...
// +-----+--------------------+----------------+--------------------+---------/
// 0  0x40000              0x80000 0xA0000 0x100000             0x140000
//                                             ^
//                                             | \___ PROC_SIZE ___/
//                                      PROC_START_ADDR

#define PROC_SIZE 0x40000       // initial state only

proc ptable[PID_MAX];           // array of process descriptors
                                // Note that `ptable[0]` is never used.
proc* current;                  // pointer to currently executing proc

#define HZ 100                  // timer interrupt frequency (interrupts/sec)
static std::atomic<unsigned long> ticks; // # timer interrupts so far


// Memory state - see `kernel.hh`
physpageinfo physpages[NPAGES];


[[noreturn]] void schedule();
[[noreturn]] void run(proc* p);
void exception(regstate* regs);
uintptr_t syscall(regstate* regs);
void memshow();
void free_pagetable(x86_64_pagetable *pagetable);


// kernel_start(command)
//    Initialize the hardware and processes and start running. The `command`
//    string is an optional string passed from the boot loader.

static void process_setup(pid_t pid, const char* program_name);

void kernel_start(const char* command) {
    // initialize hardware
    init_hardware();
    log_printf("Starting WeensyOS\n");

    ticks = 1;
    init_timer(HZ);

    // clear screen
    console_clear();

    // (re-)initialize kernel page table
    for (uintptr_t addr = 0; addr < MEMSIZE_PHYSICAL; addr += PAGESIZE) {
        int perm = PTE_P | PTE_W | PTE_U;
        if (addr == 0) {
            // nullptr is inaccessible even to the kernel
            perm = 0;
        }

        if ((addr < PROC_START_ADDR) && (addr != CONSOLE_ADDR)) {
            perm = PTE_P | PTE_W;
        }

        // install identity mapping
        int r = vmiter(kernel_pagetable, addr).try_map(addr, perm);
        assert(r == 0); // Assume mapping is successful
                        // (OK at startup, but won't always be true later!)
    }

    // set up process descriptors
    for (pid_t i = 0; i < PID_MAX; i++) {
        ptable[i].pid = i;
        ptable[i].state = P_FREE;
    }
    if (!command) {
        command = WEENSYOS_FIRST_PROCESS;
    }
    if (!program_image(command).empty()) {
        process_setup(1, command);
    } else {
        process_setup(1, "allocator");
        process_setup(2, "allocator2");
        process_setup(3, "allocator3");
        process_setup(4, "allocator4");
    }

    // switch to first process using run()
    run(&ptable[1]);
}


// kalloc(sz)
//    Kernel physical memory allocator. Allocates at least `sz` contiguous bytes
//    and returns a pointer to the allocated memory, or `nullptr` on failure.
//    The returned pointer’s address is a valid physical address, but since the
//    WeensyOS kernel uses an identity mapping for virtual memory, it is also a
//    valid virtual address that the kernel can access or modify.
//
//    The allocator selects from physical pages that can be allocated for
//    process use (i.e., not reserved pages or kernel data), and from physical
//    pages that are currently unused (`physpages[N].refcount == 0`).
//
//    On WeensyOS, `kalloc` is a page-based allocator: if `sz > PAGESIZE`
//    the allocation fails; if `sz < PAGESIZE` it allocates a whole page
//    anyway.
//
//    The returned memory is initially filled with 0xCC, which corresponds to
//    the `int3` instruction. Executing that instruction will cause a `PANIC:
//    Unhandled exception 3!` This may help you debug.
void* kalloc(size_t sz) {
    if (sz > PAGESIZE) {
        return nullptr;
    }

    int pageno = 0;
    int page_increment = 1;
    // `pageno` indicates the starting point to search for free pages.
    // In the stencil versions, `kalloc` returns the first free page.
    // Other search strategies can be faster, or may help you debug.
    // For example, this initialization returns a random free page:
    //     int pageno = rand(0, NPAGES - 1);
    // This initialization remembers the most-recently-allocated page and
    // starts the search from there:
    //     static int pageno = 0;

    for (int tries = 0; tries != NPAGES; tries++) {
        uintptr_t pa = pageno * PAGESIZE;
        if (allocatable_physical_address(pa)
            && physpages[pageno].refcount == 0) {
            physpages[pageno].refcount++;
            memset((void*) pa, 0xCC, PAGESIZE);
            return (void*) pa;
        }
        pageno = (pageno + page_increment) % NPAGES;
    }

    return nullptr;
}


// kfree(kptr)
//    Free `kptr`, which must have been previously returned by `kalloc`.
//    If `kptr == nullptr` does nothing.
void kfree(void* kptr) {

    // return nothing if pointer is invalid
    if (!kptr) return;
    
    uintptr_t pa = (uintptr_t)kptr;
    
    if (pa % PAGESIZE != 0) return; // not a page address
    if (pa >= MEMSIZE_PHYSICAL) return; // out of bounds
    if (!allocatable_physical_address(pa)) return; // not allocatable (e.g. kernel memory)

    int pageno = (int) (pa / PAGESIZE);

    if (physpages[pageno].refcount <= 0) {
        return;
    }
    physpages[pageno].refcount--;
}


// process_setup(pid, program_name)
//    Load application program `program_name` as process number `pid`.
//    This loads the application's code and data into memory, sets its
//    %rip and %rsp, gives it a stack page, and marks it as runnable.
void process_setup(pid_t pid, const char* program_name) {
    init_process(&ptable[pid], 0);

    // initialize process page table
    ptable[pid].pagetable = kalloc_pagetable();

    // copy the mappings from the kernel pagetable into process pagetable
    for (vmiter it(kernel_pagetable, KERNEL_START_ADDR); it.va() < KERNEL_STACK_TOP; it += PAGESIZE) {

        if (!it.present()) continue;
        unsigned long perm = PTE_P;
        if (it.writable()) perm |= PTE_W;
        if (it.user()) perm |= PTE_U;

        vmiter(ptable[pid].pagetable, it.va()).map(it.pa(), perm);
    }

    // map console
    vmiter(ptable[pid].pagetable, CONSOLE_ADDR).map(CONSOLE_ADDR, PTE_P | PTE_W | PTE_U);

    // obtain reference to program image
    // (The program image models the process executable.)
    program_image pgm(program_name);

    // for each segment in the program image (code, data, ...)
    for (auto seg = pgm.begin(); seg != pgm.end(); seg++) {

        // for each page in this segment
        for (uintptr_t a = round_down(seg.va(), PAGESIZE);
             a < seg.va() + seg.size();
             a += PAGESIZE) {
            // In this loop, `a` is the process virtual address for
            // the next code/data page

            // Stencil version: assume that the physical address at
            // virtual address `a` is currently free, and claim it by
            // implementing its refcount.

            int perm = PTE_P | PTE_U;
            if (seg.writable()) {
                perm |= PTE_W;
            }

            void* pa = kalloc(PAGESIZE);
            vmiter(ptable[pid].pagetable, a).map(pa, perm);
        }
    }

    // copy instructions and data from program image into process memory
    for (auto seg = pgm.begin(); seg != pgm.end(); seg++) {
    void* pa = (void*)vmiter(ptable[pid].pagetable, seg.va()).pa();
        memset(pa, 0, seg.size());
        memcpy(pa, seg.data(), seg.data_size());
    }

    // mark entry point
    ptable[pid].regs.reg_rip = pgm.entry();

    // allocate and map stack segment
    // Compute process virtual address for stack page
    uintptr_t stack_addr = MEMSIZE_VIRTUAL - PAGESIZE;

    // map stack_pa into process pagetable
    void* stack_pa = kalloc(PAGESIZE);
    ptable[pid].regs.reg_rsp = stack_addr + PAGESIZE;
    vmiter(ptable[pid].pagetable, stack_addr).map(stack_pa, PTE_P | PTE_W | PTE_U);

    // mark process as runnable
    ptable[pid].state = P_RUNNABLE;
}



// exception(regs)
//    Exception handler (for interrupts, traps, and faults).
//
//    The register values from exception time are stored in `regs`.
//    The processor responds to an exception by saving application state on
//    the kernel's stack, then jumping to kernel assembly code (in
//    k-exception.S). That code saves more registers on the kernel's stack,
//    then calls exception().
//
//    Note that hardware interrupts are disabled when the kernel is running.

void exception(regstate* regs) {
    // Copy the saved registers into the `current` process descriptor.
    current->regs = *regs;
    regs = &current->regs;

    // It can be useful to log events using `log_printf`.
    // Events logged this way are stored in the host's `log.txt` file.
    /* log_printf("proc %d: exception %d at rip %p\n",
                current->pid, regs->reg_intno, regs->reg_rip); */

    // Show the current cursor location and memory state
    // (unless this is a kernel fault).
    console_show_cursor(cursorpos);
    if (regs->reg_intno != INT_PF || (regs->reg_errcode & PTE_U)) {
        memshow();
    }

    // If Control-C was typed, exit the virtual machine.
    check_keyboard();


    // Actually handle the exception.
    switch (regs->reg_intno) {

    case INT_IRQ + IRQ_TIMER:
        ticks++;
        lapicstate::get().ack();
        schedule();
        break;                  /* will not be reached */

    case INT_PF: {
        // Analyze faulting address and access type.
        uintptr_t addr = rdcr2();
        const char* operation = regs->reg_errcode & PTE_W
                ? "write" : "read";
        const char* problem = regs->reg_errcode & PTE_P
                ? "protection problem" : "missing page";

        if (!(regs->reg_errcode & PTE_U)) {
            proc_panic(current, "Kernel page fault on %p (%s %s, rip=%p)!\n",
                       addr, operation, problem, regs->reg_rip);
        }
        error_printf(CPOS(24, 0), COLOR_ERROR,
                     "PAGE FAULT on %p (pid %d, %s %s, rip=%p)!\n",
                     addr, current->pid, operation, problem, regs->reg_rip);
        log_print_backtrace(current);
        current->state = P_FAULTED;
        break;
    }

    default:
        proc_panic(current, "Unhandled exception %d (rip=%p)!\n",
                   regs->reg_intno, regs->reg_rip);

    }


    // Return to the current process (or run something else).
    if (current->state == P_RUNNABLE) {
        run(current);
    } else {
        schedule();
    }
}


int syscall_page_alloc(uintptr_t addr);
pid_t syscall_fork();
void syscall_exit();

// syscall(regs)
//    Handle a system call initiated by a `syscall` instruction.
//    The process’s register values at system call time are accessible in
//    `regs`.
//
//    If this function returns with value `V`, then the user process will
//    resume with `V` stored in `%rax` (so the system call effectively
//    returns `V`). Alternately, the kernel can exit this function by
//    calling `schedule()`, perhaps after storing the eventual system call
//    return value in `current->regs.reg_rax`.
//
//    It is only valid to return from this function if
//    `current->state == P_RUNNABLE`.
//
//    Note that hardware interrupts are disabled when the kernel is running.

uintptr_t syscall(regstate* regs) {
    // Copy the saved registers into the `current` process descriptor.
    current->regs = *regs;
    regs = &current->regs;

    // It can be useful to log events using `log_printf`.
    // Events logged this way are stored in the host's `log.txt` file.
    /* log_printf("proc %d: syscall %d at rip %p\n",
                  current->pid, regs->reg_rax, regs->reg_rip); */

    // Show the current cursor location and memory state.
    console_show_cursor(cursorpos);
    memshow();

    // If Control-C was typed, exit the virtual machine.
    check_keyboard();


    // Actually handle the exception.
    switch (regs->reg_rax) {

    case SYSCALL_PANIC:
        user_panic(current);
        break; // will not be reached

    case SYSCALL_GETPID:
        return current->pid;

    case SYSCALL_YIELD:
        current->regs.reg_rax = 0;
        schedule();             // does not return

    case SYSCALL_PAGE_ALLOC:
        return syscall_page_alloc(current->regs.reg_rdi);

    case SYSCALL_FORK:
        return syscall_fork();

    case SYSCALL_EXIT:
        syscall_exit();
        schedule(); // does not return

    default:
        proc_panic(current, "Unhandled system call %ld (pid=%d, rip=%p)!\n",
                   regs->reg_rax, current->pid, regs->reg_rip);

    }

    panic("Should not get here!\n");
}


// syscall_page_alloc(addr)
//    Handles the SYSCALL_PAGE_ALLOC system call. This function
//    should implement the specification for `sys_page_alloc`
//    in `u-lib.hh` (but in the stencil version, it does not).
int syscall_page_alloc(uintptr_t addr) {

    // Stencil version: assume that physical page at `addr` is free
    // and claim it by incrementing its refcount.  `addr` is the
    // virtual address where the program wants to allocate a page
    //
    // (This assumes that the physical address at `addr` is not already
    // allocated, which won't work long term!)

    // allocate page of physical memory
    void* pa = kalloc(PAGESIZE);

    if (pa == nullptr) {
        return -1;
    }

    // set page to zero
    memset(pa, 0, PAGESIZE);

    // map the page into the current process's pagetable
    int r = vmiter(current->pagetable, addr).try_map(pa, PTE_P | PTE_W | PTE_U);

    if (r < 0) return -1;

    return 0;
}

// syscall_fork()
//    Handles the SYSCALL_FORK system call. This function
//    implements the specification for `sys_fork` in `u-lib.hh`.
pid_t syscall_fork() {
    // find free process
    pid_t pid = -1;
    for (pid_t i = 1; i < PID_MAX; i++) {
        if (ptable[i].state == P_FREE) {
            pid = i;
            break;
        }
    }
    if (pid < 0) return -1; // return -1 if no process can be found

    x86_64_pagetable* pt = kalloc_pagetable();
    if (!pt) return -1;

    // copy kernel mappings
    for (vmiter it(current, KERNEL_START_ADDR); it.va() < KERNEL_STACK_TOP; it += PAGESIZE) {
        if (!it.present()) continue;
        int perm = PTE_P;
        if (it.writable()) perm |= PTE_W;
        if (it.user()) perm |= PTE_U;

        int rc0 = vmiter(pt, it.va()).try_map(it.pa(), perm);
        if (rc0 < 0) {
            free_pagetable(pt);
            return -1;
        }
    }

    int rc1 = vmiter(pt, CONSOLE_ADDR).try_map(CONSOLE_ADDR, PTE_P | PTE_W | PTE_U);
    if (rc1 < 0) {
        free_pagetable(pt);
        return -1;
    }

    // deep copy process mappings
    for (vmiter it(current, 0); it.va() < MEMSIZE_VIRTUAL; it += PAGESIZE) {
        if (!(it.present() && it.user())) continue;
        if (it.va() == CONSOLE_ADDR) continue;
        int perm = PTE_P | PTE_U;

        // if writable, allocate new memory for the page
        if (it.writable()) {
            perm |= PTE_W;

            void* pa = kalloc(PAGESIZE);
            if (pa == nullptr) {
                free_pagetable(pt);
                return -1;
            }

            // copy memory
            memcpy(pa, (void*) it.pa(), PAGESIZE);

            int rc2 = vmiter(pt, it.va()).try_map((uintptr_t) pa, perm);
            if (rc2 < 0) {
                free_pagetable(pt);
                return -1;
            }
        }
        // otherwise, allocate the same memory
        else {
            int rc3 = vmiter(pt, it.va()).try_map(it.pa(), perm);
            if (rc3 < 0) {
                free_pagetable(pt);
                return -1;
            }
            physpages[(int)it.pa() / PAGESIZE].refcount++;
        }
    }

    // set child's process data
    ptable[pid].pagetable = pt;
    ptable[pid].pid = pid;
    ptable[pid].state = P_RUNNABLE;
    ptable[pid].regs = current->regs;
    ptable[pid].regs.reg_rax = 0;

    return pid;
}

// syscall_exit()
//    Handles the SYSCALL_EXIT system call. This function
//    implements the specification for `sys_exit` in `u-lib.hh`.
void syscall_exit() {
    x86_64_pagetable* pt = current->pagetable;

    free_pagetable(pt);

    // clear state, pagetable, regs
    current->state = P_FREE;
    if (current->pagetable) {
        current->pagetable = nullptr;
    }
    memset(&current->regs, 0, sizeof(current->regs));
}

// free_pagetable(x86_64_pagetable *pagetable)
//    Suggested helper function: free all pages associated with a page table:
//    this includes the pages that have mappings in the page table, as well as
//    the pages used to store the page table itself.
//    (If you modify the arguments/return type, be sure to change the function
//     prototype at the top of the file as well!)
void free_pagetable(x86_64_pagetable *pagetable) {
    // free process memory
    for (vmiter it(pagetable, 0); it.va() < MEMSIZE_VIRTUAL; it += PAGESIZE) {
        // skip if not present or not userspace memory
        if (!it.present() || !it.user()) continue;
        if (it.va() == CONSOLE_ADDR) continue;
        
        // otherwise, blow it away
        kfree((void*) it.pa());
    }

    // free pagetables
    for (ptiter it(pagetable); !it.done(); it.next()) {
        kfree((void*) it.kptr());
    }
    kfree(pagetable);
}


// schedule
//    Pick the next process to run and then run it.
//    If there are no runnable processes, spins forever.

void schedule() {
    pid_t pid = current->pid;
    for (unsigned spins = 1; true; spins++) {
        pid = (pid + 1) % PID_MAX;
        if (ptable[pid].state == P_RUNNABLE) {
            run(&ptable[pid]);
        }

        // If Control-C was typed, exit the virtual machine.
        check_keyboard();

        // If spinning forever, show the memviewer.
        if (spins % (1 << 12) == 0) {
            memshow();
        }
    }
}


// run(p)
//    Run process `p`. This involves setting `current = p` and calling
//    `exception_return` to restore its page table and registers.

void run(proc* p) {
    assert(p->state == P_RUNNABLE);
    current = p;

    // Check the process's current registers.
    check_process_registers(p);

    // Check the process's current pagetable.
    check_pagetable(p->pagetable);

    // This function is defined in k-exception.S. It restores the process's
    // registers then jumps back to user mode.
    exception_return(p);

    // should never get here
    while (true) {
    }
}


// memshow()
//    Draw a picture of memory (physical and virtual) on the CGA console.
//    Switches to a new process's virtual memory map every 0.25 sec.
//    Uses `console_memviewer()`, a function defined in `k-memviewer.cc`.

void memshow() {
    static unsigned last_ticks = 0;
    static int showing = 0;

    // switch to a new process every 0.25 sec
    if (last_ticks == 0 || ticks - last_ticks >= HZ / 2) {
        last_ticks = ticks;
        showing = (showing + 1) % PID_MAX;
    }

    proc* p = nullptr;
    for (int search = 0; !p && search < PID_MAX; search++) {
        if (ptable[showing].state != P_FREE
            && ptable[showing].pagetable) {
            p = &ptable[showing];
        } else {
            showing = (showing + 1) % PID_MAX;
        }
    }

    console_memviewer(p);
    if (!p) {
        console_printf(CPOS(10, 26), 0x0F00, "   VIRTUAL ADDRESS SPACE\n"
            "                          [All processes have exited]\n"
            "\n\n\n\n\n\n\n\n\n\n\n");
    }
}

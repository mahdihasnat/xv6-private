#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

const char *
getTrapName(uint trapno){
  const char * ret;
  switch(trapno){
    case T_DIVIDE:
      ret = "T_DIVIDE";
      break;
    case T_DEBUG:
      ret = "T_DEBUG";
      break;
    case T_NMI:
      ret = "T_NMI";
      break;
    case T_BRKPT:
      ret = "T_BRKPT";
      break;
    case T_OFLOW:
      ret = "T_OFLOW";
      break;
    case T_BOUND:
      ret = "T_BOUND";
      break;
    case T_ILLOP:
      ret = "T_ILLOP";
      break;
    case T_DEVICE:
      ret = "T_DEVICE";
      break;
    case T_DBLFLT:
      ret = "T_DBLFLT";
      break;
    case T_TSS:
      ret = "T_TSS";
      break;
    case T_SEGNP:
      ret = "T_SEGNP";
      break;
    case T_STACK:
      ret = "T_STACK";
      break;
    case T_GPFLT:
      ret = "T_GPFLT";
      break;
    case T_PGFLT:
      ret = "T_PGFLT";
      break;
    case T_FPERR:
      ret = "T_FPERR";
      break;
    case T_ALIGN:
      ret = "T_ALIGN";
      break;
    case T_MCHK:
      ret = "T_MCHK";
      break;
    case T_SIMDERR:
      ret = "T_SIMDERR";
      break;
    case T_SYSCALL:
      ret = "T_SYSCALL";
      break;
    case T_DEFAULT:
      ret = "T_DEFAULT";
      break;
    case T_IRQ0:
      ret = "T_IRQ0";
      break;
    // case IRQ_TIMER:
    //   ret = "IRQ_TIMER";
    //   break;
    // case IRQ_KBD:
    //   ret = "IRQ_KBD";
    //   break;
    // case IRQ_COM1:
    //   ret = "IRQ_COM1";
    //   break;
    // case IRQ_IDE:
    //   ret = "IRQ_IDE";
    //   break;
    // case IRQ_ERROR:
    //   ret = "IRQ_ERROR";
    //   break;
    case IRQ_SPURIOUS:
      ret = "IRQ_SPURIOUS";
      break;

    default:
      ret = "T_UNKNOWN";
      break;

  }
  return ret;
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    cprintf("\nTrap Name: %s\n", getTrapName(tf->trapno));
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}

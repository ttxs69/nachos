// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"


// deal with the user program's thread
static void ThreadFuncForUserProg(int arg);

static void SysCallHaltHandler();
static void SysCallExecHandler();
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);


    switch(which)
      {
      case SyscallException:
        switch(type)
          {
          case SC_Halt:
            SysCallHaltHandler();
            break;
          case SC_Exec:
            SysCallExecHandler();
            break;
          default:
            break;
          }
        break;
      default:
        printf("Unexpected user mode exception %d %d\n", which, type);
	ASSERT(FALSE);
      }
}


static void ThreadFuncForUserProg(int arg)
{
  currentThread->RestoreUserState();
  //TODO modify 3 registers
  if(arg && currentThread->space!=NULL)
    {
      currentThread->space->InitRegisters();
      currentThread->space->RestoreState();
    }

  machine->Run();
}

static void SysCallHaltHandler()
{
  DEBUG('a',"ShutDown, initiated by user program.\n");
  interrupt->Halt();
}

static void SysCallExecHandler()
{
  char filename[100];
  int arg = machine->ReadRegister(4);
  int i=0;
  do
    {
      machine->ReadMem(arg+i,1,(int *)&filename[i]);
    }
  while (filename[i++]!='\0');

  OpenFile *executable = fileSystem->Open(filename);
  if (executable!=NULL)
    {
      Thread *thread = new Thread(filename,nextPid++);
      thread->space = new AddrSpace(thread->getThreadId(),executable);
      machine->WriteRegister(2,thread->getThreadId());

      DEBUG('a',"Exec from thread %d -> executable %s\n",currentThread->getThreadId(),filename);
      thread->Fork(ThreadFuncForUserProg,1);
    }
  else
    {
      machine->WriteRegister(2,-1);
    }
  machine->AdvancePC();
}

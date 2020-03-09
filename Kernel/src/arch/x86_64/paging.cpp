#include <paging.h>
#include <idt.h>
#include <memory.h>
#include <panic.h>
#include <string.h>
#include <logging.h>
#include <system.h>
#include <scheduler.h>
#include <physicalallocator.h>
#include <panic.h>

//extern uint32_t kernel_end;

#define KERNEL_HEAP_PDPT_INDEX 511
#define KERNEL_HEAP_PML4_INDEX 511

address_space_t* currentAddressSpace;

uint64_t kernelPML4Phys;

namespace Memory{
	pml4_t kernelPML4 __attribute__((aligned(4096)));
	pdpt_t kernelPDPT __attribute__((aligned(4096))); // Kernel itself will reside here (0xFFFFFFFF80000000)
	//uint64_t* kernelHeapPDPT = kernel_pdpt; // We will reserve 1 GB of virtual memory for the kernel heap
	page_dir_t kernelHeapDir __attribute__((aligned(4096)));
	page_t kernelHeapDirTables[TABLES_PER_DIR][PAGES_PER_TABLE] __attribute__((aligned(4096)));

	uint64_t VirtualToPhysicalAddress(uint64_t addr) {
		uint64_t address;

		uint32_t pml4Index = PML4_GET_INDEX(addr);
		uint32_t pdptIndex = PDPT_GET_INDEX(addr);
		uint32_t pageDirIndex = PAGE_DIR_GET_INDEX(addr);
		uint32_t pageTableIndex = PAGE_TABLE_GET_INDEX(addr);

		if(pml4Index == 0){ // From Process Address Space
			
		} else { // From Kernel Address Space
			if(kernelHeapDir[pageDirIndex] & 0x80){
				address = (GetPageFrame(kernelHeapDir[pageDirIndex])) << 12;
			} else {
				address = (GetPageFrame(kernelHeapDirTables[pageDirIndex][pageTableIndex])) << 12;
			}
		}
		return address;
	}

	void InitializeVirtualMemory()
	{
		IDT::RegisterInterruptHandler(14,PageFaultHandler);
		memset(kernelPML4, 0, sizeof(pml4_t));
		memset(kernelPDPT, 0, sizeof(pdpt_t));
		memset(kernelHeapDir, 0, sizeof(page_dir_t));
		/*for(int i = 0; i < 512; i++){
			kernelPML4[i] = 0x2;
			kernelPDPT[i] = 0x2;
			kernelHeapDir[i] = 0x2;
		}*/
		SetPageFrame(&(kernelPML4[PML4_GET_INDEX(KERNEL_VIRTUAL_BASE)]),((uint64_t)kernelPDPT - KERNEL_VIRTUAL_BASE));
		kernelPML4[PML4_GET_INDEX(KERNEL_VIRTUAL_BASE)] |= 0x3;
		Log::Info((PML4_GET_INDEX(KERNEL_VIRTUAL_BASE)), false);
		kernelPML4[0] = kernelPML4[PML4_GET_INDEX(KERNEL_VIRTUAL_BASE)];
		kernelPDPT[PDPT_GET_INDEX(KERNEL_VIRTUAL_BASE)] = 0x83;
		kernelPDPT[0] = 0x83;
		kernelPDPT[KERNEL_HEAP_PDPT_INDEX] = 0x3;
		SetPageFrame(&(kernelPDPT[KERNEL_HEAP_PDPT_INDEX]), (uint64_t)kernelHeapDir - KERNEL_VIRTUAL_BASE);
		Log::Info(kernelPDPT[KERNEL_HEAP_PDPT_INDEX]);

		for(int i = 0; i < TABLES_PER_DIR; i++){
			memset(&(kernelHeapDirTables[i]),0,sizeof(page_t)*PAGES_PER_TABLE);
			//kernelHeapDir[i] = (uint64_t)(&kernelHeapDirTables[i]) - KERNEL_VIRTUAL_BASE | 0x3;
		}
		
		kernelPML4Phys = (uint64_t)kernelPML4 - KERNEL_VIRTUAL_BASE;
		asm("mov %%rax, %%cr3" :: "a"((uint64_t)kernelPML4 - KERNEL_VIRTUAL_BASE));
	}

	address_space_t* CreateAddressSpace(){
		asm("cli");
		address_space_t* addressSpace = (address_space_t*)kmalloc(sizeof(address_space_t));

		pdpt_entry_t* pdpt = (pdpt_entry_t*)Memory::KernelAllocate4KPages(1); // PDPT;
		uintptr_t pdptPhys = Memory::AllocatePhysicalMemoryBlock();
		Memory::KernelMapVirtualMemory4K(pdptPhys, (uintptr_t)pdpt,1);
		memset((pdpt_entry_t*)pdpt,0,4096);

		pd_entry_t** pageDirs = (pd_entry_t**)KernelAllocate4KPages(1); // Page Dirs
		Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), (uintptr_t)pageDirs,1);
		uint64_t* pageDirsPhys = (uint64_t*)KernelAllocate4KPages(1); // Page Dirs
		Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), (uintptr_t)pageDirsPhys,1);
		page_t*** pageTables = (page_t***)KernelAllocate4KPages(1); // Page Tables
		Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), (uintptr_t)pageTables,1);

		pml4_entry_t* pml4 = (pml4_entry_t*)KernelAllocate4KPages(1); // Page Tables
		uintptr_t pml4Phys = Memory::AllocatePhysicalMemoryBlock();
		Memory::KernelMapVirtualMemory4K(pml4Phys, (uintptr_t)pml4,1);
		memcpy(pml4, kernelPML4, 4096);

		for(int i = 0; i < 64; i++){
			pageDirs[i] = (pd_entry_t*)KernelAllocate4KPages(1);
			pageDirsPhys[i] = Memory::AllocatePhysicalMemoryBlock();
			KernelMapVirtualMemory4K(pageDirsPhys[i],(uintptr_t)pageDirs[i],1);

			pageTables[i] = (page_t**)Memory::KernelAllocate4KPages(1);
			KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),(uintptr_t)pageTables[i],1);

			SetPageFrame(&(pdpt[i]),pageDirsPhys[i]);
			pdpt[i] |= 0x3; // R/W, Present

			memset(pageDirs[i],0,4096);
			memset(pageTables[i],0,4096);
		}

		addressSpace->pageDirs = pageDirs;
		addressSpace->pageDirsPhys = pageDirsPhys;
		addressSpace->pageTables = pageTables;
		addressSpace->pml4 = pml4;
		addressSpace->pdptPhys = pdptPhys;
		addressSpace->pml4Phys = pml4Phys;

		pml4[0] = pdptPhys | 3;

		Log::Info(pml4Phys);

		asm("sti");
		return addressSpace;
	}

	page_table_t AllocatePageTable(){
		void* virt = KernelAllocate4KPages(1);
		uint64_t phys = Memory::AllocatePhysicalMemoryBlock();

		KernelMapVirtualMemory4K(phys,(uintptr_t)virt, 1);

		page_table_t pTable;
		pTable.phys = phys;
		pTable.virt = (page_t*)virt;

		for(int i = 0; i < PAGES_PER_TABLE; i++){
			((page_t*)virt)[i] = 0;
		}

		return pTable;
	}

	void CreatePageTable(uint16_t pdptIndex, uint16_t pageDirIndex, address_space_t* addressSpace){
		page_table_t pTable = AllocatePageTable();
		SetPageFrame(&(addressSpace->pageDirs[pdptIndex][pageDirIndex]),pTable.phys);
		addressSpace->pageDirs[pdptIndex][pageDirIndex] |= 0x3;
		addressSpace->pageTables[pdptIndex][pageDirIndex] = pTable.virt;
	}

	void* Allocate4KPages(uint64_t amount, address_space_t* addressSpace){
		uint64_t offset = 0;
		uint64_t pageDirOffset = 0;
		uint64_t counter = 0;
		uintptr_t address = 0;

		uint64_t pml4Index = 0;
		for(int d = 0; d < DIRS_PER_PDPT; d++){
			uint64_t pdptIndex = d;
			/* Attempt 1: Already Allocated Page Tables*/
			for(int i = 0; i < TABLES_PER_DIR; i++){
				if(addressSpace->pageDirs[d][i] & 0x1 && !(addressSpace->pageDirs[d][i] & 0x80)){
					for(int j = 0; j < PAGES_PER_TABLE; j++){
						if(addressSpace->pageTables[d][i][j] & 0x1){
							pageDirOffset = i;
							offset = j+1;
							counter = 0;
							continue;
						}

						counter++;

						if(counter >= amount){
							address = (PDPT_SIZE * pml4Index) + (pdptIndex * PAGE_SIZE_1G) + (pageDirOffset * PAGE_SIZE_2M) + (offset*PAGE_SIZE_4K);
							while(counter--){
								if(offset >= 512){
									pageDirOffset++;
									offset = 0;
								}
								addressSpace->pageTables[d][pageDirOffset][offset] = 0x3;
								offset++;
							}

							return (void*)address;
						}
					}
				} else {
					pageDirOffset = i+1;
					offset = 0;
					counter = 0;
				}
			}
			
			pageDirOffset = 0;
			offset = 0;
			counter = 0;

			/* Attempt 2: Allocate Page Tables*/
			for(int i = 0; i < TABLES_PER_DIR; i++){
				if(!(addressSpace->pageDirs[d][i] & 0x1)){
					
					CreatePageTable(d,i,addressSpace);
					for(int j = 0; j < PAGES_PER_TABLE; j++){

						address = (PDPT_SIZE * pml4Index) + (pdptIndex * PAGE_SIZE_1G) + (pageDirOffset * PAGE_SIZE_2M) + (offset*PAGE_SIZE_4K);
						counter++;
						
						if(counter >= amount){
							address = (PDPT_SIZE * pml4Index) + (pdptIndex * PAGE_SIZE_1G) + (pageDirOffset * PAGE_SIZE_2M) + (offset*PAGE_SIZE_4K);
							while(counter--){
								if(offset >= 512){
									pageDirOffset ++;
									offset = 0;
								}
								addressSpace->pageTables[d][pageDirOffset][offset] = 0x3;
								offset++;
							}
							return (void*)address;
						}
					}
				} else {
					pageDirOffset = i+1;
					offset = 0;
					counter = 0;
				}
			}
			Log::Info("new dir");
		}
	}

	void* KernelAllocate4KPages(uint64_t amount){
		uint64_t offset = 0;
		uint64_t pageDirOffset = 0;
		uint64_t counter = 0;
		uintptr_t address = 0;

		uint64_t pml4Index = KERNEL_HEAP_PML4_INDEX;
		uint64_t pdptIndex = KERNEL_HEAP_PDPT_INDEX;

		/* Attempt 1: Already Allocated Page Tables*/
		for(int i = 0; i < TABLES_PER_DIR; i++){
			if(kernelHeapDir[i] & 0x1 && !(kernelHeapDir[i] & 0x80)){
				for(int j = 0; j < TABLES_PER_DIR; j++){
					if(kernelHeapDirTables[i][j] & 0x1){
						pageDirOffset = i;
						offset = j+1;
						counter = 0;
						continue;
					}

					counter++;

					if(counter >= amount){
						address = (PDPT_SIZE * pml4Index) + (pdptIndex * PAGE_SIZE_1G) + (pageDirOffset * PAGE_SIZE_2M) + (offset*PAGE_SIZE_4K);
						address |= 0xFFFF000000000000;
						while(counter--){
							if(offset >= 512){
								pageDirOffset++;
								offset = 0;
							}
							kernelHeapDirTables[pageDirOffset][offset] = 0x3;
							offset++;
						}

						return (void*)address;
					}
				}
			} else {
				pageDirOffset = i+1;
				offset = 0;
				counter = 0;
			}
		}
		
		pageDirOffset = 0;
		offset = 0;
		counter = 0;

		/* Attempt 2: Allocate Page Tables*/
		for(int i = 0; i < TABLES_PER_DIR; i++){
			if(!(kernelHeapDir[i] & 0x1)){
				for(int j = 0; j < TABLES_PER_DIR; j++){
					address = (PDPT_SIZE * pml4Index) + (pdptIndex * PAGE_SIZE_1G) + (pageDirOffset * PAGE_SIZE_2M) + (offset*PAGE_SIZE_4K);
					address |= 0xFFFF000000000000;
					//kernelHeapDir[i] = (PAGE_FRAME & ((uintptr_t)&(kernelHeapDirTables[i]) - KERNEL_VIRTUAL_BASE)) | 0x3;
					SetPageFrame(&(kernelHeapDir[pageDirOffset]),((uintptr_t)&(kernelHeapDirTables[pageDirOffset]) - KERNEL_VIRTUAL_BASE));
					kernelHeapDir[pageDirOffset] |= 0x3;
					while(amount--){
						if(offset >= 512){
							pageDirOffset ++;
							offset = 0;	
							SetPageFrame(&(kernelHeapDir[pageDirOffset]),((uintptr_t)&(kernelHeapDirTables[pageDirOffset]) - KERNEL_VIRTUAL_BASE));
							kernelHeapDir[pageDirOffset] |= 0x3;
						}
						kernelHeapDirTables[pageDirOffset][offset] = 0x3;
						offset++;
					}
					return (void*)address;
				}
			} else {
				pageDirOffset = i+1;
				offset = 0;
				counter = 0;
			}
		}
	}

	void* KernelAllocate2MPages(uint64_t amount){
		uint64_t address = 0;
		uint64_t offset = 0;
		uint64_t counter = 0;
		uint64_t pdptIndex = KERNEL_HEAP_PDPT_INDEX;
		uint64_t pml4Index = KERNEL_HEAP_PML4_INDEX;
		
		for(int i = 0; i < TABLES_PER_DIR; i++){
			if(kernelHeapDir[i] & 0x1){
				offset = i+1;
				counter = 0;
				continue;
			}
			counter++;

			if(counter >= amount){
				address = (PDPT_SIZE * pml4Index) + (pdptIndex * PAGE_SIZE_1G) + offset * PAGE_SIZE_2M;
				address |= 0xFFFF000000000000;
				while(counter--){
					kernelHeapDir[offset] = 0x83;
					offset++;
				}
				return (void*)address;
			}
		}

		*(int*)0xFFFDC0DEDEADBEEF = 0xDEADBEEF;
		for(;;);
	}

	void KernelFree2MPages(void* addr, uint64_t amount){
		while(amount--){
			uint64_t pageDirIndex = PAGE_DIR_GET_INDEX((uint64_t)addr);
			kernelHeapDir[pageDirIndex] = 0;
			addr = (void*)((uint64_t)addr + 0x200000);
		}
	}

	void KernelMapVirtualMemory2M(uint64_t phys, uint64_t virt, uint64_t amount){
		uint64_t pml4Index = PML4_GET_INDEX(virt);
		uint64_t pdptIndex = PDPT_GET_INDEX(virt);
		uint64_t pageDirIndex = PAGE_DIR_GET_INDEX(virt);

		while(amount--){
			kernelHeapDir[pageDirIndex] = 0x83;
			SetPageFrame(&(kernelHeapDir[pageDirIndex]), phys);
			kernelHeapDir[pageDirIndex] |= 0x83;
			pageDirIndex++;
			phys += PAGE_SIZE_2M;
		}
	}

	void KernelMapVirtualMemory4K(uint64_t phys, uint64_t virt, uint64_t amount){
		uint64_t pml4Index, pdptIndex, pageDirIndex, pageIndex;

		while(amount--){
			pml4Index = PML4_GET_INDEX(virt);
			pdptIndex = PDPT_GET_INDEX(virt);
			pageDirIndex = PAGE_DIR_GET_INDEX(virt);
			pageIndex = PAGE_TABLE_GET_INDEX(virt);
			SetPageFrame(&(kernelHeapDirTables[pageDirIndex][pageIndex]), phys);
			kernelHeapDirTables[pageDirIndex][pageIndex] |= 0x3;
			invlpg(virt);
			phys += PAGE_SIZE_4K;
			virt += PAGE_SIZE_4K;
		}
	}

	void MapVirtualMemory4K(uint64_t phys, uint64_t virt, uint64_t amount){
		MapVirtualMemory4K(phys,virt,amount,currentAddressSpace);
	}

	void MapVirtualMemory4K(uint64_t phys, uint64_t virt, uint64_t amount, address_space_t* addressSpace){
		uint64_t pml4Index, pdptIndex, pageDirIndex, pageIndex;

		phys &= ~(PAGE_SIZE_4K-1);
		virt &= ~(PAGE_SIZE_4K-1);

		while(amount--){
			pml4Index = PML4_GET_INDEX(virt);
			pdptIndex = PDPT_GET_INDEX(virt);
			pageDirIndex = PAGE_DIR_GET_INDEX(virt);
			pageIndex = PAGE_TABLE_GET_INDEX(virt);

			if(pdptIndex > MAX_PDPT_INDEX || pml4Index) KernelPanic((const char**)(&("Process address space cannot be >64GB")),1);

			if(!(addressSpace->pageDirs[pdptIndex][pageDirIndex] & 0x1)) CreatePageTable(pdptIndex,pageDirIndex,addressSpace); // If we don't have a page table at this address, create one.
			
			SetPageFrame(&(addressSpace->pageTables[pdptIndex][pageDirIndex][pageIndex]), phys);
			addressSpace->pageTables[pdptIndex][pageDirIndex][pageIndex] |= 0x3;

			invlpg(virt);

			phys += PAGE_SIZE_4K;
			virt += PAGE_SIZE_4K; /* Go to next page */
		}
	}

	void ChangeAddressSpace(address_space_t* addressSpace){
		currentAddressSpace = addressSpace;
		//SetPageFrame(&(kernelPML4[0]), addressSpace->pdptPhys);
	}

	void PageFaultHandler(regs64_t* regs)
	{
		asm("cli");
		Log::Error("Page Fault!\r\n");
		Log::SetVideoConsole(nullptr);

		int err_code = IDT::GetErrCode();

		uint64_t faultAddress;
		asm volatile("movq %%cr2, %0" : "=r" (faultAddress));

		int present = !(err_code & 0x1); // Page not present
		int rw = err_code & 0x2;           // Attempted write to read only page
		int us = err_code & 0x4;           // Processor was in user-mode and tried to access kernel page
		int reserved = err_code & 0x8;     // Overwritten CPU-reserved bits of page entry
		int id = err_code & 0x10;          // Caused by an instruction fetch

		char* msg = "Page fault ";
		char* reason;
		if (present)
			Log::Info("Page not present"); // Print fault to serial
		if (rw)
			Log::Info("Read Only");
		if (us)
			Log::Info("User mode process tried to access kernel memory");
		if (reserved)
			Log::Info("Reserved");
		if (id)
			Log::Info("instruction fetch");

		Log::Info("RIP:");

		Log::Info(regs->rip);

		Log::Info("Process:");
		Log::Info(Scheduler::GetCurrentProcess()->pid);

		Log::Info("\r\nFault address: ");
		Log::Info(faultAddress);

		Log::Info("Register Dump: a: ");
		Log::Write(regs->rax);
		Log::Write(", b:");
		Log::Write(regs->rbx);
		Log::Write(", c:");
		Log::Write(regs->rcx);
		Log::Write(", d:");
		Log::Write(regs->rdx);
		Log::Write(", S:");
		Log::Write(regs->rsi);
		Log::Write(", D:");
		Log::Write(regs->rdi);
		Log::Write(", sp:");
		Log::Write(regs->rsp);
		Log::Write(", bp:");
		Log::Write(regs->rbp);

		/*uint8_t xmm_x[16];
		asm("movdqu %%xmm0, (%0)" : "=r"(xmm_x));
		Log::Write("\r\nxmm0: {");
		for(int i = 15; i >= 0; i--){
			Log::Write(xmm_x[i]);
			Log::Write(", ");
		}
		asm("movdqu %%xmm1, (%0)" : "=r"(xmm_x));
		Log::Write("}, xmm1: {");
		for(int i = 15; i >= 0; i--){
			Log::Write(xmm_x[i]);
			Log::Write(", ");
		}
		asm("movdqu %%xmm2, (%0)" : "=r"(xmm_x));
		Log::Write("}, xmm3: {");
		for(int i = 15; i >= 0; i--){
			Log::Write(xmm_x[i]);
			Log::Write(", ");
		}
		asm("movdqu %%xmm3, (%0)" : "=r"(xmm_x));
		Log::Write("}, xmm3: {");
		for(int i = 15; i >= 0; i--){
			Log::Write(xmm_x[i]);
			Log::Write(", ");
		}
		asm("movdqu %%xmm4, (%0)" : "=r"(xmm_x));
		Log::Write("}, xmx4: {");
		for(int i = 15; i >= 0; i--){
			Log::Write(xmm_x[i]);
			Log::Write(", ");
		}*/

		char temp[16];
		char temp2[16];
		char temp3[16];
		const char* reasons[]{"Page Fault","RIP: ", itoa(regs->rip, temp, 16),"Address: ",itoa(faultAddress, temp2, 16), "Process:", itoa(Scheduler::GetCurrentProcess()->pid,temp3,10)};;
		KernelPanic(reasons,7);
		for (;;);
	}
}
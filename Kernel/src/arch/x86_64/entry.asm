BITS    32

global entry
global kernel_pml4
global kernel_pdpt
global kernel_pdpt2
extern kmain

global GDT64
global GDT64.TSS
global GDT64.TSS.low
global GDT64.TSS.mid
global GDT64.TSS.high
global GDT64.TSS.high32

global GDT64Pointer64

KERNEL_VIRTUAL_BASE equ 0xFFFFFFFF80000000
KERNEL_BASE_PML4_INDEX equ (((KERNEL_VIRTUAL_BASE) >> 39) & 0x1FF)
KERNEL_BASE_PDPT_INDEX equ  (((KERNEL_VIRTUAL_BASE) >> 30) & 0x1FF)

section .boot.data
align 4096
kernel_pml4:
times 512 dq 0

align 4096
kernel_pdpt:
dq 0x83 ; 1GB Page
times 511 dq 0

align 4096
kernel_pdpt2:
times KERNEL_BASE_PDPT_INDEX dq 0
dq 0x83 ; 1GB Page

align 16
GDT64:                           ; Global Descriptor Table (64-bit).
    .Null: equ $ - GDT64         ; The null descriptor.
    dw 0xFFFF                    ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 0                         ; Access.
    db 0                         ; Granularity.
    db 0                         ; Base (high).
    .Code: equ $ - GDT64         ; The code descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10011010b                 ; Access (exec/read).
    db 00100000b                 ; Granularity, 64 bits flag, limit19:16.
    db 0                         ; Base (high).
    .Data: equ $ - GDT64         ; The data descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10010010b                 ; Access (read/write).
    db 00000000b                 ; Granularity.
    db 0                         ; Base (high).
    .UserCode: equ $ - GDT64     ; The usermode code descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 11111010b                 ; Access (exec/read).
    db 00100000b                 ; Granularity, 64 bits flag, limit19:16.
    db 0                         ; Base (high).
    .UserData: equ $ - GDT64     ; The usermode data descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 11110010b                 ; Access (read/write).
    db 00000000b                 ; Granularity.
    db 0                         ; Base (high).
    .TSS: ;equ $ - GDT64         ; TSS Descriptor
    .len:
    dw 108                       ; TSS Length - the x86_64 TSS is 108 bytes loong
    .low:
    dw 0                         ; Base (low).
    .mid:
    db 0                         ; Base (middle)
    db 10001001b                 ; Flags
    db 00000000b                 ; Flags 2
    .high:
    db 0                         ; Base (high).
    .high32:
    dd 0                         ; High 32 bits
    dd 0                         ; Reserved
GDT64Pointer:                    ; The GDT-pointer.
    dw $ - GDT64 - 1             ; Limit.
    dq GDT64                     ; Base.
GDT64Pointer64:                    ; The GDT-pointer.
    dw GDT64Pointer - GDT64 - 1             ; Limit.
    dq GDT64 + KERNEL_VIRTUAL_BASE; Base.

MAGIC       equ 0xE85250D6
ARCH        equ 0 ; x86
LENGTH      equ (multiboot_header_end - multiboot_header)
CHECKSUM    equ -(MAGIC + ARCH + LENGTH)

section .boot.text
align 8 ; Multiboot Header
multiboot_header:
dd MAGIC
dd ARCH
dd LENGTH
dd CHECKSUM

align 8
; Framebuffer tag
dw 5  ; Type - framebuffer
dw 0  ; Flags
dd 20 ; Size - 20
dd 0  ; Width
dd 0  ; Height
dd 32  ; Depth

align 8
; Module alignment tag
dw 6  ; Type - module alignment
dw 0  ; Flags
dd 8  ; Size

; End tag
dw 0
dw 0
dd 8
multiboot_header_end:

no64:
  mov ecx, dword [fb_pitch]
  mov eax, ecx
  mov edx, dword [fb_height]
  mul edx
  mov ecx, eax
.fill:
  mov al, 0xFF
  mov ebx, dword [fb_addr]
  mov byte[ebx + ecx], al
  sub ecx, 3
  loop .fill
  jmp $

entry:
  mov dword [mb_addr], ebx

  mov eax, [ebx+88] ; Framebuffer pointer address in multiboot header
  mov dword [fb_addr], eax 

  mov eax, [ebx+96] ; Framebuffer pitch
  mov dword [fb_pitch], eax

  mov eax, [ebx+104] ; Framebuffer height
  mov dword [fb_height], eax

  mov eax, 0x80000000
  cpuid                  ; CPU identification.
  cmp eax, 0x80000001
  jb no64

  mov eax, 0x80000001
  cpuid                  ; CPU identification.
  test edx, 1 << 29      ; Long Mode bit is 29
  jz no64
  
  mov eax, cr4
  or eax, 1 << 5  ; Set PAE bit
  mov cr4, eax

  mov eax, kernel_pdpt ; Get address of PDPT
  or eax, 3 ; Present, Write
  mov dword [kernel_pml4], eax

  mov eax, kernel_pdpt2 ; Second PDPT
  or eax, 3
  mov dword [kernel_pml4 + KERNEL_BASE_PML4_INDEX * 8], eax

  mov eax, kernel_pml4
  mov cr3, eax

  mov ecx, 0xC0000080 ; EFER Model Specific Register
  rdmsr               ; Read from the MSR 
  or eax, 1 << 8
  wrmsr

  mov eax, cr0
  or eax, 1 << 31
  mov cr0, eax

  lgdt [GDT64Pointer]
  jmp 0x8:entry64 - KERNEL_VIRTUAL_BASE
BITS 64

  cli
  hlt


fb_addr:
dd 0
fb_pitch:
dd 0
fb_height:
dd 0
mb_addr:
dd 0
dd 0

extern _bss
extern _bss_end

BITS 64
section .text
entry64:

  lgdt [GDT64Pointer64]

  mov ax, 0x10
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax

  mov rdi, _bss
  mov rcx, _bss_end
  sub rcx, _bss
  xor rax, rax
  rep stosb

  mov rsp, stack_top

  mov rax, cr0
	and ax, 0xFFFB		; Clear coprocessor emulation
	or ax, 0x2			; Set coprocessor monitoring
	mov cr0, rax

	;Enable SSE
	mov rax, cr4
	or ax, 3 << 9		; Set flags for SSE
	mov cr4, rax

  xor rbp, rbp
  mov rdi, qword[mb_addr] ; Pass multiboot info struct
  add rdi, KERNEL_VIRTUAL_BASE
  call kmain

  cli
  hlt

global __cxa_atexit
__cxa_atexit:
  ret

section .bss
align 64
stack_bottom:
resb 16384
stack_top:

global __dso_handle
__dso_handle: resq 1
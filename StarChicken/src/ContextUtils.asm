.code

save_registers PROC
	;save instruction and stack pointer
	mov r8, [rsp]
	mov [rcx], r8
	lea r8, [rsp+8]
	mov [rcx+8], r8

	;Save preserved registers
	mov [rcx+16], rbx
	mov [rcx+24], rbp
	mov [rcx+32], r12
	mov [rcx+40], r13
	mov [rcx+48], r14
	mov [rcx+56], r15
	mov [rcx+64], rdi
	mov [rcx+72], rsi
	movups [rcx+80+16*0], xmm6
	movups [rcx+80+16*1], xmm7
	movups [rcx+80+16*2], xmm8
	movups [rcx+80+16*3], xmm9
	movups [rcx+80+16*4], xmm10
	movups [rcx+80+16*5], xmm11
	movups [rcx+80+16*6], xmm12
	movups [rcx+80+16*7], xmm13
	movups [rcx+80+16*8], xmm14
	movups [rcx+80+16*9], xmm15

	;Return 0
	xor rax, rax
	ret
save_registers ENDP

load_registers PROC
	;Load stack pointer, instruction pointer is stored because we can't set it directly
	mov r8, [rcx]
	mov rsp, [rcx+8]

	;Load preserved registers
	mov rbx, [rcx+16]
	mov rbp, [rcx+24]
	mov r12, [rcx+32]
	mov r13, [rcx+40]
	mov r14, [rcx+48]
	mov r15, [rcx+56]
	mov rdi, [rcx+64]
	mov rsi, [rcx+72]
	movups xmm6, [rcx+80+16*0]
	movups xmm7, [rcx+80+16*1]
	movups xmm8, [rcx+80+16*2]
	movups xmm9, [rcx+80+16*3]
	movups xmm10, [rcx+80+16*4]
	movups xmm11, [rcx+80+16*5]
	movups xmm12, [rcx+80+16*6]
	movups xmm13, [rcx+80+16*7]
	movups xmm14, [rcx+80+16*8]
	movups xmm15, [rcx+80+16*9]

	;Push r8 so the instruction pointer gets loaded
	push r8

	xor rax, rax
	ret
load_registers ENDP

swap_registers PROC
	;Same as save_registers, saves to first argument
	mov r8, [rsp]
	mov [rcx], r8
	lea r8, [rsp+8]
	mov [rcx+8], r8

	mov [rcx+16], rbx
	mov [rcx+24], rbp
	mov [rcx+32], r12
	mov [rcx+40], r13
	mov [rcx+48], r14
	mov [rcx+56], r15
	mov [rcx+64], rdi
	mov [rcx+72], rsi
	movups [rcx+80+16*0], xmm6
	movups [rcx+80+16*1], xmm7
	movups [rcx+80+16*2], xmm8
	movups [rcx+80+16*3], xmm9
	movups [rcx+80+16*4], xmm10
	movups [rcx+80+16*5], xmm11
	movups [rcx+80+16*6], xmm12
	movups [rcx+80+16*7], xmm13
	movups [rcx+80+16*8], xmm14
	movups [rcx+80+16*9], xmm15

	;Same as load_registers, loads from second argument
	mov r8, [rdx]
	mov rsp, [rdx+8]

	mov rbx, [rdx+16]
	mov rbp, [rdx+24]
	mov r12, [rdx+32]
	mov r13, [rdx+40]
	mov r14, [rdx+48]
	mov r15, [rdx+56]
	mov rdi, [rdx+64]
	mov rsi, [rdx+72]
	movups xmm6, [rdx+80+16*0]
	movups xmm7, [rdx+80+16*1]
	movups xmm8, [rdx+80+16*2]
	movups xmm9, [rdx+80+16*3]
	movups xmm10, [rdx+80+16*4]
	movups xmm11, [rdx+80+16*5]
	movups xmm12, [rdx+80+16*6]
	movups xmm13, [rdx+80+16*7]
	movups xmm14, [rdx+80+16*8]
	movups xmm15, [rdx+80+16*9]

	push r8

	xor rax, rax
	ret
swap_registers ENDP
	
load_registers_arg PROC
	;Load stack pointer, instruction pointer is stored because we can't set it directly
	mov r8, [rcx]
	mov rsp, [rcx+8]

	;Load preserved registers
	mov rbx, [rcx+16]
	mov rbp, [rcx+24]
	mov r12, [rcx+32]
	mov r13, [rcx+40]
	mov r14, [rcx+48]
	mov r15, [rcx+56]
	mov rdi, [rcx+64]
	mov rsi, [rcx+72]
	movups xmm6, [rcx+80+16*0]
	movups xmm7, [rcx+80+16*1]
	movups xmm8, [rcx+80+16*2]
	movups xmm9, [rcx+80+16*3]
	movups xmm10, [rcx+80+16*4]
	movups xmm11, [rcx+80+16*5]
	movups xmm12, [rcx+80+16*6]
	movups xmm13, [rcx+80+16*7]
	movups xmm14, [rcx+80+16*8]
	movups xmm15, [rcx+80+16*9]

	mov rcx, rdx

	;Push r8 so the instruction pointer gets loaded
	push r8

	xor rax, rax
	ret
load_registers_arg ENDP
END
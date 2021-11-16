.code

save_registers PROC
	;save instruction and stack pointer
	mov r8, [rsp]
	mov [rcx], r8
	lea r8, [rsp+0008h]
	mov [rcx+0008h], r8

	;Save preserved registers
	mov [rcx+0010h], rbx
	mov [rcx+0018h], rbp
	mov [rcx+0020h], r12
	mov [rcx+0028h], r13
	mov [rcx+0030h], r14
	mov [rcx+0038h], r15
	mov [rcx+0040h], rdi
	mov [rcx+0048h], rsi
	movaps [rcx+0050h], xmm6
	movaps [rcx+0060h], xmm7
	movaps [rcx+0070h], xmm8
	movaps [rcx+0080h], xmm9
	movaps [rcx+0090h], xmm10
	movaps [rcx+00a0h], xmm11
	movaps [rcx+00b0h], xmm12
	movaps [rcx+00c0h], xmm13
	movaps [rcx+00d0h], xmm14
	movaps [rcx+00e0h], xmm15

	;Save floating point control words
	stmxcsr [rcx+00f0h]
	fnstcw [rcx+00f4h]
	
	;Save stack base, limit, deallocationstack, and guaranteed stack bytes
	mov r8, gs:[30h]
	;Base
	mov r9, [r8+0008h]
	mov [rcx+00f8h], r9
	;Limit
	mov r9, [r8+0010h]
	mov [rcx+0100h], r9
	;Deallocationstack
	mov r9, [r8+1478h]
	mov [rcx+0108h], r9
	;Guaranteed stack bytes
	mov r9, [r8+1748h]
	mov [rcx+0110h], r9
	;Fiber data
	mov r9, [r8+0020h]
	mov [rcx+0118h], r9

	;Return 0
	xor rax, rax
	ret
save_registers ENDP

load_registers PROC
	;Load stack pointer, instruction pointer is stored because we can't set it directly
	mov r8, [rcx]
	mov rsp, [rcx+0008h]

	;Load preserved registers
	mov rbx, [rcx+0010h]
	mov rbp, [rcx+0018h]
	mov r12, [rcx+0020h]
	mov r13, [rcx+0028h]
	mov r14, [rcx+0030h]
	mov r15, [rcx+0038h]
	mov rdi, [rcx+0040h]
	mov rsi, [rcx+0048h]
	movaps xmm6, [rcx+0050h]
	movaps xmm7, [rcx+0060h]
	movaps xmm8, [rcx+0070h]
	movaps xmm9, [rcx+0080h]
	movaps xmm10, [rcx+0090h]
	movaps xmm11, [rcx+00a0h]
	movaps xmm12, [rcx+00b0h]
	movaps xmm13, [rcx+00c0h]
	movaps xmm14, [rcx+00d0h]
	movaps xmm15, [rcx+00e0h]

	;Load floating point control words
	ldmxcsr [rcx+00f0h]
	fldcw [rcx+00f4h]

	;Load stack base, limit, deallocationstack, and guaranteed stack bytes
	mov r9, gs:[30h]
	;Base
	mov r10, [rcx+00f8h]
	mov [r9+0008h], r10
	;Limit
	mov r10, [rcx+0100h]
	mov [r9+0010h], r10
	;Deallocationstack
	mov r10, [rcx+0108h]
	mov [r9+1478h], r10
	;Guaranteed stack bytes
	mov r10, [rcx+0110h]
	mov [r9+1748h], r10
	;Fiber data
	mov r10, [rcx+0118h]
	mov [r9+0020h], r10

	;Jump to next code
	jmp r8
load_registers ENDP

swap_registers PROC
	;Same as save_registers
	;save instruction and stack pointer
	mov r8, [rsp]
	mov [rcx], r8
	lea r8, [rsp+0008h]
	mov [rcx+0008h], r8

	;Save preserved registers
	mov [rcx+0010h], rbx
	mov [rcx+0018h], rbp
	mov [rcx+0020h], r12
	mov [rcx+0028h], r13
	mov [rcx+0030h], r14
	mov [rcx+0038h], r15
	mov [rcx+0040h], rdi
	mov [rcx+0048h], rsi
	movaps [rcx+0050h], xmm6
	movaps [rcx+0060h], xmm7
	movaps [rcx+0070h], xmm8
	movaps [rcx+0080h], xmm9
	movaps [rcx+0090h], xmm10
	movaps [rcx+00a0h], xmm11
	movaps [rcx+00b0h], xmm12
	movaps [rcx+00c0h], xmm13
	movaps [rcx+00d0h], xmm14
	movaps [rcx+00e0h], xmm15

	;Save floating point control words
	stmxcsr [rcx+00f0h]
	fnstcw [rcx+00f4h]
	
	;Save stack base, limit, deallocationstack, and guaranteed stack bytes
	mov r8, gs:[30h]
	;Base
	mov r9, [r8+0008h]
	mov [rcx+00f8h], r9
	;Limit
	mov r9, [r8+0010h]
	mov [rcx+0100h], r9
	;Deallocationstack
	mov r9, [r8+1478h]
	mov [rcx+0108h], r9
	;Guaranteed stack bytes
	mov r9, [r8+1748h]
	mov [rcx+0110h], r9
	;Fiber data
	mov r9, [r8+0020h]
	mov [rcx+0118h], r9



	;Same as load_registers, but from second argument
	;Load stack pointer, instruction pointer is stored because we can't set it directly
	mov r8, [rdx]
	mov rsp, [rdx+0008h]

	;Load preserved registers
	mov rbx, [rdx+0010h]
	mov rbp, [rdx+0018h]
	mov r12, [rdx+0020h]
	mov r13, [rdx+0028h]
	mov r14, [rdx+0030h]
	mov r15, [rdx+0038h]
	mov rdi, [rdx+0040h]
	mov rsi, [rdx+0048h]
	movaps xmm6, [rdx+0050h]
	movaps xmm7, [rdx+0060h]
	movaps xmm8, [rdx+0070h]
	movaps xmm9, [rdx+0080h]
	movaps xmm10, [rdx+0090h]
	movaps xmm11, [rdx+00a0h]
	movaps xmm12, [rdx+00b0h]
	movaps xmm13, [rdx+00c0h]
	movaps xmm14, [rdx+00d0h]
	movaps xmm15, [rdx+00e0h]

	;Load floating point control words
	ldmxcsr [rdx+00f0h]
	fldcw [rdx+00f4h]

	;Load stack base, limit, deallocationstack, and guaranteed stack bytes
	mov r9, gs:[30h]
	;Base
	mov r10, [rdx+00f8h]
	mov [r9+0008h], r10
	;Limit
	mov r10, [rdx+0100h]
	mov [r9+0010h], r10
	;Deallocationstack
	mov r10, [rdx+0108h]
	mov [r9+1478h], r10
	;Guaranteed stack bytes
	mov r10, [rdx+0110h]
	mov [r9+1748h], r10
	;Fiber data
	mov r10, [rcx+0118h]
	mov [r9+0020h], r10

	;Jump to next code
	jmp r8
swap_registers ENDP
	
load_registers_arg PROC
	;Load stack pointer, instruction pointer is stored because we can't set it directly
	mov r8, [rcx]
	mov rsp, [rcx+0008h]

	;Load preserved registers
	mov rbx, [rcx+0010h]
	mov rbp, [rcx+0018h]
	mov r12, [rcx+0020h]
	mov r13, [rcx+0028h]
	mov r14, [rcx+0030h]
	mov r15, [rcx+0038h]
	mov rdi, [rcx+0040h]
	mov rsi, [rcx+0048h]
	movaps xmm6, [rcx+0050h]
	movaps xmm7, [rcx+0060h]
	movaps xmm8, [rcx+0070h]
	movaps xmm9, [rcx+0080h]
	movaps xmm10, [rcx+0090h]
	movaps xmm11, [rcx+00a0h]
	movaps xmm12, [rcx+00b0h]
	movaps xmm13, [rcx+00c0h]
	movaps xmm14, [rcx+00d0h]
	movaps xmm15, [rcx+00e0h]

	;Load floating point control words
	ldmxcsr [rcx+00f0h]
	fldcw [rcx+00f4h]

	;Load stack base, limit, deallocationstack, and guaranteed stack bytes
	mov r9, gs:[30h]
	;Base
	mov r10, [rcx+00f8h]
	mov [r9+0008h], r10
	;Limit
	mov r10, [rcx+0100h]
	mov [r9+0010h], r10
	;Deallocationstack
	mov r10, [rcx+0108h]
	mov [r9+1478h], r10
	;Guaranteed stack bytes
	mov r10, [rcx+0110h]
	mov [r9+1748h], r10
	;Fiber data
	mov r10, [rcx+0118h]
	mov [r9+0020h], r10

	;Move the second argument into the first for the next call
	mov rcx, rdx

	;Jump to next code
	jmp r8
load_registers_arg ENDP

swap_registers_arg PROC
	mov r11, r8

	;Same as save_registers
	;save instruction and stack pointer
	mov r8, [rsp]
	mov [rcx], r8
	lea r8, [rsp+0008h]
	mov [rcx+0008h], r8

	;Save preserved registers
	mov [rcx+0010h], rbx
	mov [rcx+0018h], rbp
	mov [rcx+0020h], r12
	mov [rcx+0028h], r13
	mov [rcx+0030h], r14
	mov [rcx+0038h], r15
	mov [rcx+0040h], rdi
	mov [rcx+0048h], rsi
	movaps [rcx+0050h], xmm6
	movaps [rcx+0060h], xmm7
	movaps [rcx+0070h], xmm8
	movaps [rcx+0080h], xmm9
	movaps [rcx+0090h], xmm10
	movaps [rcx+00a0h], xmm11
	movaps [rcx+00b0h], xmm12
	movaps [rcx+00c0h], xmm13
	movaps [rcx+00d0h], xmm14
	movaps [rcx+00e0h], xmm15

	;Save floating point control words
	stmxcsr [rcx+00f0h]
	fnstcw [rcx+00f4h]
	
	;Save stack base, limit, deallocationstack, and guaranteed stack bytes
	mov r8, gs:[30h]
	;Base
	mov r9, [r8+0008h]
	mov [rcx+00f8h], r9
	;Limit
	mov r9, [r8+0010h]
	mov [rcx+0100h], r9
	;Deallocationstack
	mov r9, [r8+1478h]
	mov [rcx+0108h], r9
	;Guaranteed stack bytes
	mov r9, [r8+1748h]
	mov [rcx+0110h], r9
	;Fiber data
	mov r9, [r8+0020h]
	mov [rcx+0118h], r9



	;Same as load_registers, but from second argument
	;Load stack pointer, instruction pointer is stored because we can't set it directly
	mov r8, [rdx]
	mov rsp, [rdx+0008h]

	;Load preserved registers
	mov rbx, [rdx+0010h]
	mov rbp, [rdx+0018h]
	mov r12, [rdx+0020h]
	mov r13, [rdx+0028h]
	mov r14, [rdx+0030h]
	mov r15, [rdx+0038h]
	mov rdi, [rdx+0040h]
	mov rsi, [rdx+0048h]
	movaps xmm6, [rdx+0050h]
	movaps xmm7, [rdx+0060h]
	movaps xmm8, [rdx+0070h]
	movaps xmm9, [rdx+0080h]
	movaps xmm10, [rdx+0090h]
	movaps xmm11, [rdx+00a0h]
	movaps xmm12, [rdx+00b0h]
	movaps xmm13, [rdx+00c0h]
	movaps xmm14, [rdx+00d0h]
	movaps xmm15, [rdx+00e0h]

	;Load floating point control words
	ldmxcsr [rdx+00f0h]
	fldcw [rdx+00f4h]

	;Load stack base, limit, deallocationstack, and guaranteed stack bytes
	mov r9, gs:[30h]
	;Base
	mov r10, [rdx+00f8h]
	mov [r9+0008h], r10
	;Limit
	mov r10, [rdx+0100h]
	mov [r9+0010h], r10
	;Deallocationstack
	mov r10, [rdx+0108h]
	mov [r9+1478h], r10
	;Guaranteed stack bytes
	mov r10, [rdx+0110h]
	mov [r9+1748h], r10
	;Fiber data
	mov r10, [rcx+0118h]
	mov [r9+0020h], r10

	mov rcx, r11

	;Jump to next code
	jmp r8
swap_registers_arg ENDP
END
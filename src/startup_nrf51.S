.syntax unified
.cpu cortex-m0
.thumb

.global g_pfnVectors
.global Reset_Handler
.global Default_Handler
.extern GPIOTE_IRQHandler
.extern RTC0_IRQHandler
.extern RTC1_IRQHandler
.extern SWI2_IRQHandler

.section .isr_vector, "a", %progbits
.type g_pfnVectors, %object
g_pfnVectors:
    .word __StackTop
    .word Reset_Handler
    .word Default_Handler /* NMI */
    .word Default_Handler /* HardFault */
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word Default_Handler /* SVCall */
    .word 0
    .word 0
    .word Default_Handler /* PendSV */
    .word Default_Handler /* SysTick */
    /* External IRQ 0..5. */
    .rept 6
    .word Default_Handler
    .endr
    .word GPIOTE_IRQHandler /* IRQ 6 */
    /* External IRQ 7..10. */
    .rept 4
    .word Default_Handler
    .endr
    .word RTC0_IRQHandler /* IRQ 11 */
    /* External IRQ 12..16. */
    .rept 5
    .word Default_Handler
    .endr
    .word RTC1_IRQHandler /* IRQ 17 */
    /* External IRQ 18..21. */
    .rept 4
    .word Default_Handler
    .endr
    .word SWI2_IRQHandler /* IRQ 22 / SoftDevice event notification */
    /* External IRQ 23..31. */
    .rept 9
    .word Default_Handler
    .endr
.size g_pfnVectors, . - g_pfnVectors

.section .text.Reset_Handler, "ax", %progbits
.thumb_func
.type Reset_Handler, %function
Reset_Handler:
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
1:
    cmp r1, r2
    bcs 2f
    ldr r3, [r0]
    str r3, [r1]
    adds r0, #4
    adds r1, #4
    b 1b
2:
    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0
3:
    cmp r1, r2
    bcs 4f
    str r3, [r1]
    adds r1, #4
    b 3b
4:
    bl main
5:
    wfe
    b 5b
.size Reset_Handler, . - Reset_Handler

.section .text.Default_Handler, "ax", %progbits
.thumb_func
.type Default_Handler, %function
Default_Handler:
    b Default_Handler
.size Default_Handler, . - Default_Handler

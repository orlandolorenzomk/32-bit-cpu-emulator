.org 0x0005

main:
    LOADI   R0, 0   ; counter = 0
    LOADI   R2, 5  ; comparison

loop:
    ADD     R0, 1   ; increment counter TODO Source can also be a numeric value
    CMP     R0, R2  ; comparison        TODO Source can also be a numeric value
    JNZ     loop
    HALT
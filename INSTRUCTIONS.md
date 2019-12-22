### 32-bits instruction

```elm
0 1 2 3 4 5 6 7 8 9 ...                                   ... 31
[_6_op____] [_8_A_________] [_9_Bx__________] [_9_Cx__________]
                            [_8_B_________] [ [_8_C_________] [
            [_16_Ax_______________________] sB]               sC]
```

Mode    | 6-bits | 8-bits | 9-bits | 9-bits
:------ |:------:|:------:|:------:|:------:
OpAx    |`opcode`|`Ax` [8]|`Ax` [8]|
OpABC   |`opcode`|`A`  [8]|`B`  [8]|`C`  [8]
OpABxCx |`opcode`|`A`  [8]|`Bx` [9]|`Cx` [9]
OpAsBsC |`opcode`|`A`  [8]|`sB` [1]|`sC` [1]

### Instruction listings

- `NOP`

- `RET`

- `MOV %r, %r/k`

- `NOT %r, %r/k`
- `NEG %r, %r/k`

- `LT %r, %r/k, %r/k`
- `LE %r, %r/k, %r/k`
- `EQ %r, %r/k, %r/k`

- `ADD %r, %r/k, %r/k`
- `SUB %r, %r/k, %r/k`
- `MUL %r, %r/k, %r/k`
- `DIV %r, %r/k, %r/k`

- `DEF %k, %r/k`
- `GLD %r, %r/k`
- `GST %r, %r/k`

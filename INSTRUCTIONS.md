### 32-bits instruction

```elm
0 1 2 3 4 5 6 7 8 9 ...                                   ... 31
[_6_op____] [_8_A_________] [_9_Bx__________] [_9_Cx__________]
                            [_8_B_________] [ [_8_C_________] [
            [_16_Ax_______________________] sB]               sC]
```

| 6-bits | 8-bits | 9-bits | 9-bits
|:------:|:------ |:------ |:------
|`op`    |`Ax` [8]|`Ax` [8]|
| | |`B`  [8] + `sB` [1]|`C`  [8] + `sC` [1]
| | |`Bx` [9]|`Cx` [9]

### Instruction listings

- `NOP`, do nothing.

- `RET %r/k`, return `r`.

- `PUT %r, n`, print `n` values from `r`.

- `MOV %r, %r/k`, copy value to `r`.

- `NOT %r, %r/k`
- `LT %r, %r/k, %r/k`
- `LE %r, %r/k, %r/k`
- `EQ %r, %r/k, %r/k`

- `NEG %r, %r/k`
- `ADD %r, %r/k, %r/k`
- `SUB %r, %r/k, %r/k`
- `MUL %r, %r/k, %r/k`
- `DIV %r, %r/k, %r/k`

- `DEF %k, %r/k/[s]`, define global `k`, as nil value if `s`.
- `GLD %r, %k`, load global `k`.
- `GST %k, %r/k`, store value to global `k`.

- `CLO %k [<i, s>, ...]`, make closure function in `k` constant index.
- `ULD %r %u`, load upvalue from `u` to `r`.
- `UST %u %r/k`, store value to `u`.

### Example

```go
a = 10
puts a + 5
```

Disassemble:

```
.k[0] = a
.k[1] = 10
.k[2] = a
.k[3] = 5
LDK r[0], k[1]          ; r[0] = 10
GST k[0], r[0]          ; a = r[0]
GLD r[0], k[2]          ; r[0] = a
LDK r[1], k[3]          ; r[1] = 5
ADD r[0], r[0], r[1]    ; r[0] = r[0] + r[1]
PUT r[0]                ; puts r[0]
```

Optimized:

```c
.k[0] = a
.k[1] = 10
.k[2] = 5
GST k[0], k[1]          ; a = 10
GLD r[0], k[0]          ; r[0] = a
ADD r[0], r[0], k[2]    ; r[0] = r[0] + 5
PUT r[0]                ; puts r[0]
```

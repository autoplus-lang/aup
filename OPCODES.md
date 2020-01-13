### List of detailed opcodes are used in virtual machine

- Each opcode and its arg used one byte
- A word is combined from two bytes

<br>

Opcode|Args|Stack|Description
:--|:--:|:--:|:--
`PRINT` | `[n]`    | `[-n, +0]` | - Print `n` values and pop them<br>- In **print** statement
`POP`   | `[]`     | `[-1, +0]` | - Pop a value
_
`CALL`  | `[n]`    | `[-n, +1]` | - Call a value with `n` args
`RET`   | `[]`     | `[-1, +0]` | - Return from function<br>- In **return** statement
_
`NIL`   | `[]`     | `[-0, +1]` | - Push `nil`
`TRUE`  | `[]`     | `[-0, +1]` | - Push `true`
`FALSE` | `[]`     | `[-0, +1]` | - Push `false`
`INT`   | `[i]`    | `[-0, +1]` | - Push a `byte` number
`INTL`  | `[i, i]` | `[-0, +1]` | - Push a `word` number
`CONST` | `[k]`    | `[-0, +1]` | - Push a constant at index `k`
_
`NEG`   | `[]`     | `[-1, +1]` | - Negative
`ADD`   | `[]`     | `[-2, +1]` | - Addition
`SUB`   | `[]`     | `[-2, +1]` | - Subtraction
`MUL`   | `[]`     | `[-2, +1]` | - Multiplication
`DIV`   | `[]`     | `[-2, +1]` | - Division
`MOD`   | `[]`     | `[-2, +1]` | - Remainder
_
`NOT`   | `[]`     | `[-1, +1]` | - Logical NOT
`LT`    | `[]`     | `[-2, +1]` | - Relational less than
`LE`    | `[]`     | `[-2, +1]` | - Relational less or equal
`EQ`    | `[]`     | `[-2, +1]` | - Equality
_
`BNOT`  | `[]`     | `[-1, +1]` | - Bitwise NOT
`BAND`  | `[]`     | `[-2, +1]` | - Bitwise AND
`BOR`   | `[]`     | `[-2, +1]` | - Bitwise OR
`BXOR`  | `[]`     | `[-2, +1]` | - Bitwise XOR
`SHL`   | `[]`     | `[-2, +1]` | - Left shift 
`SHR`   | `[]`     | `[-2, +1]` | - Right shift
_
`LD`    | `[s]`    | `[-0, +1]` | - Load a local
`ST`    | `[s]`    | `[-0, +0]` | - Store value to local
_
`DEF`   | `[k]`    | `[-1, +0]` | - Define a global<br>- In global **variable**, **function** declaration
`GLD`   | `[k]`    | `[-0, +1]` | - Load a global
`GST`   | `[k]`    | `[-0, +0]` | - Store value as global
_
`GET`   | `[k]`    | `[-1, +1]` | - Get by name
`SET`   | `[k]`    | `[-2, +1]` | - Set by name
_
`GETI`  | `[]`     | `[-2, +1]` | - Get by index
`SETI`  | `[]`     | `[-3, +1]` | - Set by index
_
`CLOSURE` | `[k, ...] | `[-0, +0]` | - Make closure function at index 'k'
`CLOSE`   | `[]`      | `[-1, +0]` | - Close an upvalue
`ULD`     | `[u]`     | `[-0, +1]` | - Load an upvalue
`UST`     | `[u]`     | `[-0, +0]` | - Store value to upvalue
_
`MAP`   | `[n]`    | `[-n, +1]` | - Create a map, push `n` values to this map<br>- In **map** declaration
_
`JMP`   | `[s, s]` | `[-0, +0]` | - `ip += s`
`JMPF`  | `[s, s]` | `[-0, +0]` | - `ip += s`, if top is false<br>- In **if** statement
`JNE`   | `[s, s]` | `[-1, +0]` | - `ip += s`, if two top values are not equal<br>- In **match** statement

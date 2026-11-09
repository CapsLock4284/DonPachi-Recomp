"""
Name:        call_tables.py
Description: adapts the shared core's 68000 decompiler (tools/core/m68000) to how DonPachi's code is written, without
             changing tools/core: installed by tools/decompile.py before the core runs
Version:     0.3.0 (draft)
Created:     08-11-2026

Modified:    08-11-2026 (0.3.0: 1b, 5 and 6)

What DonPachi does that TMNT's code does not, and that the core does not take:

1. Calls and jumps straight into a table of stubs: "jsr (A0,D0.w)", "jsr ($c,PC,D0.w)", "jmp ($2,PC,D0.w)", each entry
   a bra.w or a jmp xxx.l. The core follows a jump or call through (An), (d,An) and what an address register holds,
   not one with an index register: those had no targets ("goes to no code the analysis finds"). Here the table is
   found (a PC-relative one's place is in the instruction; an (An,Xn) one's is what the register holds), its stubs are
   read (find_tables.py's way) and entered as code: the jump or call goes to the stubs, as the CPU does.
   1b. "movea.l (d,PC,Dn), A0; jmp (A0)" (007830, 007934): an entry of a table of code addresses, for the tables
   symbols.txt describes (the core reads an entry only through an address register): the jump goes to the entries.

2. A switch inside a frame: a routine pushes registers, jumps through a table to its cases, and each case pops them
   and returns. The core makes every place a computed jump goes a function of its own, which then returns with the
   stack where the routine left it ("returns with SP +6 from where it started"). Here a computed jump made where the
   routine's stack is not where it started keeps its cases in the routine: they are code of that function, not
   functions (the stack is followed by stack_effect() from the routine's start).

3. A tail two routines share after each pushed its own registers ("bra $5140" into the end of another routine). The
   core writes shared code longer than 8 instructions once, as a function both reach with a tail call, which then
   returns with their registers still on the stack. Here shared code is split off only where every routine that runs
   into it has the stack where it started; elsewhere each keeps its own copy.

4. Vectors that hold no code (DonPachi has 212 zero ones; TMNT's all go to code): the C writer goes through every
   vector's handler and stops at one with no function. Here such a vector stands for the reset.

5. Code reached with the stack in two places ("SP 0 and -2 meet at"): 00BA86 pushes a word only while $1018FC is
   set and pops it later under the same test; player 2's pass at 030010 pushes A5 and both players run 03001C. Here
   each place gets its own copy of the code (stack_slots(), the core's stack pass with copies), and the two ways the
   game never goes (pushed and not popped, popped and never pushed) end in the core's lost_stack(). Code that sets
   SP before it uses it (the reset, which the soft reset jumps back to) takes the stack from anywhere. A block takes
   at most MAX_DEPTHS places of the stack: 02FFCE's loop over the two players, followed statically, would push again
   on every turn.

6. A push or pop of one register (move.w D0, -(A7); move.w (A7)+, D0): the core's stack pass misses the temporary
   the 68000 lifter puts its address in, and the C writer stops ("a stack address is used as a value"). The core's
   movem pushes, which TMNT uses, are not affected.

7. A jump through a register the game never makes (UNTAKEN_JUMPS: 02D9A2, the scripts' callback no script sets): the
   C writer takes no jump it knows no places for; here it stops the program, as lost_stack().

8. The soft resets ("jmp $3425a.l" from other routines) and the reset code's registers: see soft_reset(); the reset
   code starts with every register 0 (they hold nothing the game uses then), so it takes no parameters; so do the
   exception handlers (CRASH_HANDLERS), whose crash screen shows the registers they saved.

And the calls MANUAL names: "jsr (A1)" at 012D22 runs the object types' init routines, "jmp (A1)" at 012D86 their
routines at +4, and "jmp (A0)" at 02D9A2 the stage scripts' callbacks listed in build/callbacks.txt.

Tested against the core's own code (Program, Builder, the stack analysis) on a small program built with these
patterns; on the real program, check with: python tools/decompile.py --check
"""
import sys
from collections import deque
from functools import lru_cache
from pathlib import Path

import game_profile

sys.path.insert(0, str(Path(__file__).resolve().parent))

from find_tables import best_table  # noqa: E402

rom = game_profile.rom

# the object types: records of 8 bytes from the second one (the first is unused), each a pointer to a word and the
# object's init code, which "jsr (A1)" at 012D22 runs: the code is the pointer plus 2 (the word is skipped)
OBJECT_TYPES = (0x01275E, 8, 168)
# {address of a jump or call through a register: function giving the addresses it can go to}
MANUAL = {}

_image = None


def program():
    global _image
    if _image is None:
        _image = rom.build_program(rom.load_set(Path(game_profile.ROOT) / "rom"))
    return _image


@lru_cache(maxsize=None)
def object_init_routines():
    start, size, count = OBJECT_TYPES
    image = program()
    found = set()
    for index in range(count):
        place = start + index * size
        target = int.from_bytes(image[place:place + 4], "big") + 2
        if not target & 1 and 0x420 <= target < rom.PROGRAM_SIZE:
            found.add(target)
    return frozenset(found)


MANUAL[0x012D22] = object_init_routines


@lru_cache(maxsize=None)
def object_update_routines():
    """The routine at +4 of each object type, which "jmp (A1)" at 012D86 goes to (012D82 "movea.l ($4,A0,D0.w), A1",
    A0 the table less one record: type 0's, unused)"""
    start, size, count = OBJECT_TYPES
    image = program()
    found = set()
    for index in range(count):
        place = start + index * size + 4
        target = int.from_bytes(image[place:place + 4], "big")
        if not target & 1 and 0x420 <= target < rom.PROGRAM_SIZE:
            found.add(target)
    return frozenset(found)


MANUAL[0x012D86] = object_update_routines

# the routines the stage scripts hand to "jmp (A0)" at 02D9A2 (script command 15 stores one in $10885C): a list
# reviewed from tools/find_callbacks.py's output, one address a line, in build/callbacks.txt
CALLBACKS = Path(game_profile.ROOT) / "build" / "callbacks.txt"


@lru_cache(maxsize=None)
def script_callbacks():
    if not CALLBACKS.exists():
        return frozenset()
    found = set()
    for line in CALLBACKS.read_text(encoding="utf-8-sig", errors="replace").splitlines():
        text = line.split("#")[0].strip()
        if text:
            found.add(int(text.split()[0], 16))
    return frozenset(found)


MANUAL[0x02D9A2] = script_callbacks

# jumps through a register the game never makes, with what shows it: in C they stop the program (the core's
# lost_stack()), as long as MANUAL gives them nowhere to go. The C writer takes no jump it has no places for
UNTAKEN_JUMPS = {
    # $10885C is set only by script command 15 (02DE46), and no stage's script has one (tools/find_callbacks.py walks
    # them all); the RAM clear leaves it 0, and 02D99C "beq" skips the jump while it is
    0x02D9A2: "the stage scripts' callback, which no script sets",
}


@lru_cache(maxsize=None)
def stub_addresses(base):
    """The addresses of the stubs of the table at 'base': its records, as far as they decode as jumps and stay before
    the lowest place any of them jumps to above the table (the first case usually follows the table). Cached: the
    analysis asks again and again."""
    size, entries = best_table(program(), base)
    if not entries:
        return ()
    targets = [entry[3] for entry in entries if entry[3] is not None]
    above = [target for target in targets if target > base]
    limit = min(above) if above else None
    return tuple(entry[1] for entry in entries if limit is None or entry[1] < limit)


def stack_effect(instruction):
    """How far an instruction moves the stack pointer (negative: a push), or None when it sets it to something the
    code works out (the boot's own stack)"""
    mnemonic = instruction.mnemonic
    operands = instruction.operands
    unit = 4 if instruction.size == "l" else 2      # (a byte pushed or popped moves A7 by 2)
    if mnemonic == "movem" and len(operands) == 2:
        first, second = operands
        if first.mode == "reglist" and second.mode == "predec" and second.reg == "A7":
            return -unit * len(first.registers)
        if second.mode == "reglist" and first.mode == "postinc" and first.reg == "A7":
            return unit * len(second.registers)
        return 0
    if mnemonic == "pea":
        return -4
    if len(operands) == 2 and operands[1].mode == "areg" and operands[1].reg == "A7":
        source = operands[0]
        if mnemonic == "lea" and source.mode == "disp" and source.reg == "A7":
            return source.disp
        if mnemonic in ("addq", "adda", "addi") and source.mode in ("quick", "immediate"):
            return source.value
        if mnemonic in ("subq", "suba", "subi") and source.mode in ("quick", "immediate"):
            return -source.value
        return None
    if mnemonic in ("link", "unlk"):
        return None
    effect = 0
    for operand in operands:
        if operand.reg == "A7" and operand.mode == "predec":
            effect -= unit
        elif operand.reg == "A7" and operand.mode == "postinc":
            effect += unit
    return effect


def depths(program_object, entry, body):
    """{address: how far the stack pointer is from where it was at the function's start}, along the function's code
    (an address reached two ways keeps the first; past an instruction that sets the stack pointer, nothing)"""
    found = {entry: 0}
    work = deque([entry])
    while work:
        address = work.popleft()
        instruction = body.get(address)
        if instruction is None:
            continue
        effect = stack_effect(instruction)
        if effect is None:
            continue
        for successor in program_object.successors(instruction):
            if successor in body and successor not in found:
                found[successor] = found[address] + effect
                work.append(successor)
    return found


# ---- 5. code reached with the stack in two places gets a copy for each ----

# the instructions that save A7 itself to RAM: the exception handlers' "movem.l D0-D7/A0-A7, $100f4c.l", for the
# crash screen that shows the registers. C has no stack pointer to save: the long A7 goes to is 0 instead
SP_STORES = {0x0364AE, 0x0364C8, 0x0364E2, 0x0364FC, 0x036516}
# the exception handlers that start there (default, bus error, address error, illegal instruction, divide by zero):
# they save the registers for the crash screen, which never comes back (03663C goes to the reset). Like the reset code,
# each starts with every register 0: C keeps the registers in its own variables, so the crash screen shows zeros, and
# the handlers take no parameters (the divide one is also run from every divu/divs, as the 68000 does)
CRASH_HANDLERS = set(SP_STORES)

MAX_STACK_COPIES = 512   # blocks one function may get copied before the core's error stands (a loop that pushes would
                         # need copies without end)
MAX_DEPTHS = 2           # places of the stack one block may be reached with: 02FFCE runs its players' code twice (dbra D7)
                         # and only player 2's pushes A5; a static pass goes round again and again, one push deeper each
                         # time. A third place is a way the game never goes (lost_stack())


def first_int(label):
    """The address a block's label stands for (a label the decompiler makes holds it inside)"""
    if isinstance(label, int):
        return label
    for part in label:
        found = first_int(part) if isinstance(part, (int, tuple)) else None
        if found is not None:
            return found
    return None


def joined_halves(statements):
    """A long pushed with "move.l An, -(A7)": the 68000's lifter stores its two halves (the low word first, as the
    CPU does), and the pop reads the long; the C writer wants a slot one size. Two 16-bit stores, next to each other,
    of the low and the high half of one value at offsets o+2 and o become one 32-bit store at o"""
    from v60 import ir

    def half(value):
        # (value, True) for trunc(V) to 16 bits, (value, False) for trunc(shr(V, 16)) to 16 bits, else None
        if not (isinstance(value, ir.Op) and value.name == "trunc" and value.bits == 16):
            return None
        inner = value.args[0]
        if isinstance(inner, ir.Op) and inner.name == "shr" and isinstance(inner.args[1], ir.Const) \
                and inner.args[1].value == 16 and inner.args[0].bits == 32:
            return inner.args[0], False
        return (inner, True) if inner.bits == 32 else None

    joined = []
    index = 0
    while index < len(statements):
        first = statements[index]
        second = statements[index + 1] if index + 1 < len(statements) else None
        if isinstance(first, ir.SetStack) and isinstance(second, ir.SetStack) and first.bits == second.bits == 16 \
                and first.offset == second.offset + 2:
            low, high = half(first.value), half(second.value)
            value = None
            if low and high and low[1] and not high[1] and low[0] == high[0]:
                value = low[0]
            elif isinstance(first.value, ir.Const) and high and not high[1] and isinstance(high[0], ir.Const):
                # (clr.l -(A7), move.l #n, -(A7): the low half folded to a constant, the high one not yet)
                value = ir.const(high[0].value & 0xFFFF0000 | first.value.value & 0xFFFF, 32)
            elif isinstance(first.value, ir.Const) and isinstance(second.value, ir.Const):
                value = ir.const((second.value.value & 0xFFFF) << 16 | first.value.value & 0xFFFF, 32)
            if value is not None:
                slot = ir.SetStack(second.offset, value, 32)
                if hasattr(first, "origin"):
                    slot.origin = first.origin
                joined.append(slot)
                index += 2
                continue
        joined.append(first)
        index += 1
    return joined


def split_mixed_slots(function):
    """A slot the function uses at two sizes (034BB0: a word at -8 from "movem.w D6-D7, -(A7)", later a long there from
    "movem.l D7/A5, -(A7)"; 037C2E: two words, cleared with "clr.l (A7)"): the C writer wants one size a slot. In such
    a function every long access to the stack that meets a word becomes two words (the high one first), so both sizes
    are words"""
    from v60 import ir

    widths = {}

    def note(node):
        if isinstance(node, ir.Stack):
            widths.setdefault(node.offset, set()).add(node.bits)
        return None
    for block in function.blocks.values():
        for statement in block.statements + [block.end]:
            if isinstance(statement, ir.SetStack):
                widths.setdefault(statement.offset, set()).add(statement.bits)
            for expression in ir.statement_expressions(statement):
                for node in ir.walk(expression):
                    note(node)

    def words_at(offset):
        return 16 in widths.get(offset, ()) or 16 in widths.get(offset + 2, ())
    longs = {offset for offset, sizes in widths.items() if 32 in sizes and words_at(offset)}
    if not longs:
        return

    def change(node):
        if isinstance(node, ir.Stack) and node.bits == 32 and node.offset in longs:
            high = ir.op("shl", (ir.op("zext", (ir.Stack(node.offset, 16),), 32), ir.const(16, 8)), 32)
            return ir.op("or", (high, ir.op("zext", (ir.Stack(node.offset + 2, 16),), 32)), 32)
        return None
    for block in function.blocks.values():
        statements = []
        for statement in block.statements:
            ir.map_statement(statement, change)
            if isinstance(statement, ir.SetStack) and statement.bits == 32 and statement.offset in longs:
                value = statement.value
                high = ir.SetStack(statement.offset, ir.op("trunc", (ir.op("shr", (value, ir.const(16, 8)), 32),), 16), 16)
                low = ir.SetStack(statement.offset + 2, ir.op("trunc", (value,), 16), 16)
                for half in (high, low):
                    if hasattr(statement, "origin"):
                        half.origin = statement.origin
                statements += [high, low]
                continue
            statements.append(statement)
        block.statements = statements
        ir.map_statement(block.end, change)


def read_later(function, block, index):
    """Whether the code after 'block' may read register 'index' before it sets it again (the functions it calls or
    goes on to are taken not to read it: none of DonPachi's takes a place of its caller's stack in an address
    register; 037C5E's "jmp $37b32" waits and goes to the reset)"""
    from v60 import ir

    def reads(statement):
        return any(isinstance(node, ir.Reg) and node.index == index
                   for expression in ir.statement_expressions(statement) for node in ir.walk(expression))
    seen = set()
    work = list(block.successors())
    while work:
        label = work.pop()
        if label in seen or label not in function.blocks:
            continue
        seen.add(label)
        current = function.blocks[label]
        for statement in current.statements:
            if reads(statement):
                return True
            if isinstance(statement, ir.SetReg) and statement.index == index:
                break
        else:
            if reads(current.end) or isinstance(current.end, ir.ComputedJump):
                return True
            work += current.successors()
    return False


def lift_flags():
    from m68000 import lift
    return lift.FLAGS


def soft_reset(builder, function_object, block, instruction, reset):
    """A jump to the reset code from another routine (the soft resets: 002FA0, 03663C, 037B42 "jmp $3425a.l"). The core
    makes it a tail call, which hands back to the routine's callers what the reset code leaves in the registers; but
    the reset code never comes back (it sets its own stack and runs the main loop), and its "results" made it a
    function of every register ("the reset code takes parameters"). Here it is a call of the reset code, then a loop
    the program never gets to: nothing after it reads a register"""
    from decompiler.function import Block
    from v60 import ir
    address = instruction.address
    call = ir.Call(target=reset, return_address=function_key(address + instruction.length))
    call.origin = address
    block.statements.append(call)
    never = ("never", address)
    block.end = ir.Goto(never)
    block.end.origin = address
    stay = Block(never, [], ir.Goto(never))
    stay.end.origin = address
    builder.extra_blocks.append(stay)


def function_key(address):
    return address & 0xFFFFFF


def never_back(built):
    """A divu/divs runs the divide by zero handler when the divisor is 0 (the core: a call, then the code goes on after
    the instruction); that handler is a crash screen that never comes back (CRASH_HANDLERS), so the call is followed by
    the loop the program never gets to, as a soft reset's: nothing after it uses what the handler leaves in the
    registers"""
    from decompiler.function import Block
    from v60 import ir
    for label, block in list(built.blocks.items()):
        if not (isinstance(label, tuple) and label[0] == "exception" and isinstance(block.end, ir.Goto)):
            continue
        calls = [statement for statement in block.statements if isinstance(statement, ir.Call)]
        if not calls or calls[-1].target is None or function_key(calls[-1].target) not in CRASH_HANDLERS:
            continue
        never = ("never", label[1])
        origin = getattr(block.end, "origin", None)
        block.end = ir.Goto(never)
        block.end.origin = origin
        if never not in built.blocks:
            stay = Block(never, [], ir.Goto(never))
            stay.end.origin = origin
            built.blocks[never] = stay


def stack_slots(function):
    """The core's decompiler/stack.py slots() (0.1.0), with two changes for DonPachi. Code the program reaches with
    the stack in different places (00BA86 pushes a word only while $1018FC is set and pops it under the same test;
    030010 pushes A5 for player 2 only, and both players run the code at 03001C) gets a copy of its blocks for each
    place, as many as it needs: in C, the two ways are two copies of the code. And a block that sets SP before it
    uses it (the reset, which the soft reset at 0343CC jumps back to) takes the code there with the stack anywhere.
    Also: a push or pop through -(A7) or (A7)+ becomes a slot (the core's misses the temporary its address is in)."""
    import copy
    from decompiler import cpu
    from decompiler import stack
    from v60 import ir

    sp_index = cpu.current.sp
    original = copy.deepcopy(function.blocks)

    widths = {}     # {slot: the size the function stores it at}

    def place_of(expression, sp, temps, regs):
        """The core's offset_of(), which also takes the address registers that hold a place of the stack (003AC4
        "movea.l A7, A0", then "move.b D1, -(A0)")"""
        if isinstance(expression, ir.Reg) and expression.index in regs:
            return regs[expression.index]
        if isinstance(expression, ir.Op) and expression.name in ("add", "sub") and expression.bits == 32 \
                and isinstance(expression.args[1], ir.Const):
            inner = place_of(expression.args[0], sp, temps, regs)
            if inner is not None:
                amount = expression.args[1].value
                delta = amount if amount < 1 << 31 else amount - (1 << 32)
                return inner + delta if expression.name == "add" else inner - delta
        return stack.offset_of(expression, sp, temps)

    def reader(sp, temps, regs):
        # (the core's slot_reader(), which also takes the temporaries that hold stack addresses: the 68000's lifter
        # puts the address of -(A7) and (A7)+ in one, so "move.w D0, -(A7)" stores through it. A long or a word read
        # where the function stored bytes is made of them: 003ACE "move.l (A0), D2" after four "move.b Dn, -(A0)")
        def change(node):
            if isinstance(node, ir.Load):
                offset = place_of(node.address, sp, temps, regs)
                if offset is not None:
                    count = node.bits // 8
                    if count > 1 and widths.get(offset) == 8 \
                            and all(widths.get(offset + part) == 8 for part in range(count)):
                        value = None
                        for part in range(count):
                            byte = ir.op("zext", (ir.Stack(offset + part, 8),), node.bits)
                            shift = 8 * (count - 1 - part)
                            if shift:
                                byte = ir.op("shl", (byte, ir.const(shift, 8)), node.bits)
                            value = byte if value is None else ir.op("or", (value, byte), node.bits)
                        return value
                    return ir.Stack(offset, node.bits)
            return None
        return change

    def sets_sp_first(block):
        for statement in block.statements:
            if isinstance(statement, ir.SetReg) and statement.index == sp_index:
                return isinstance(statement.value, ir.Const)
            if any(stack.mentions_sp(expression) for expression in ir.statement_expressions(statement)):
                return False
        return False

    function.exits_caller = False
    stacks = 0
    start = {function.entry: 0}
    pending = [function.entry]
    done = set()
    copies = {}     # {(the block copied, SP): the copy}
    uneven = []     # (block, SP) of the returns with the stack away from where it started
    above = []      # blocks that leave SP above where it was at the start (popped what the function never pushed)
    while pending:
        label = pending.pop()
        if label in done:
            continue
        done.add(label)
        block = function.blocks[label]
        sp = start[label]
        statements = []
        temps = {}
        regs = {}       # {address register: the place of the stack it holds}
        held = {}       # {address register: the statements that set it, left out while it holds a place of the stack}
        for statement in block.statements:
            if isinstance(statement, ir.SetTemp):
                offset = place_of(statement.value, sp, temps, regs)
                if offset is not None:
                    temps[statement.temp.name] = offset
                    continue
            if isinstance(statement, ir.SetReg) and statement.index == sp_index:
                new = place_of(statement.value, sp, temps, regs)
                if new is None:
                    if not isinstance(statement.value, ir.Const):
                        raise stack.StackError(f"{function.name}: SP set to {statement.value}")
                    stacks += 1
                    new = stack.NEW_STACK * stacks
                sp = new
                continue
            if isinstance(statement, ir.SetReg) and 8 <= statement.index < 15:
                offset = place_of(statement.value, sp, temps, regs)
                if offset is not None:
                    regs[statement.index] = offset
                    held.setdefault(statement.index, []).append(statement)
                    continue
                regs.pop(statement.index, None)
                held.pop(statement.index, None)
            if isinstance(statement, ir.Effect) and statement.name == "return_from_interrupt":
                frame = statement.args[0]
                if not isinstance(frame, ir.Const):
                    raise stack.StackError(f"{function.name}: RETIS with a computed frame")
                statements.append(ir.Effect("write_psw", (ir.Stack(sp + 4),)))
                sp += 8 + frame.value
                continue
            if isinstance(statement, ir.Store):
                offset = place_of(statement.address, sp, temps, regs)
                if offset is not None:
                    value = ir.rebuild(statement.value, reader(sp, temps, regs))
                    slot = ir.SetStack(offset, value, statement.bits)
                    widths[offset] = statement.bits
                    if hasattr(statement, "origin"):
                        slot.origin = statement.origin
                    statements.append(slot)
                    continue
            if isinstance(statement, ir.Store) and getattr(statement, "origin", None) in SP_STORES \
                    and isinstance(statement.value, ir.Temp) and statement.value.name in temps:
                origin = statement.origin
                statement = ir.Store(statement.address, ir.const(0, statement.bits), statement.bits)
                statement.origin = origin
            ir.map_statement(statement, reader(sp, temps, regs))
            statements.append(statement)
        if regs and not isinstance(block.end, (ir.Return, ir.InterruptReturn)):
            # (a register that still holds a place of the stack where the block goes on: as the core, it stays, and
            # the C writer refuses it, unless the code sets it again before it reads it (037C34 "lea (A7), A6": the
            # loop sets A6 at 037C68 before its next use). At a return it goes: what it holds is no use to the
            # caller, under its stack)
            statements += [setting for index in sorted(regs) if read_later(function, block, index)
                           for setting in held[index]]
        block.statements = joined_halves(statements)
        ir.map_statement(block.end, reader(sp, temps, regs))

        def opaque(node):
            if isinstance(node, ir.Temp) and node.name in temps:
                return ir.op("stack_address", (ir.const(temps[node.name] & 0xFFFFFFFF),), cpu.current.address_bits)
            return None
        for statement in statements + [block.end]:
            ir.map_statement(statement, opaque)
        if sp > 0 and not (isinstance(block.end, ir.Return) and sp == cpu.current.return_address_bytes):
            above.append(block)    # (a list: blocks compare by their contents)
        if isinstance(block.end, ir.Return) and sp == cpu.current.return_address_bytes:
            function.exits_caller = True
            block.end.leaves_caller = True
        elif isinstance(block.end, (ir.Return, ir.InterruptReturn)) and sp != 0 and sp != stack.NEW_STACK * stacks:
            uneven.append((block, sp))
        for successor in block.successors():
            if successor in start and start[successor] != sp:
                base = getattr(function.blocks[successor], "stack_copy_of", successor)
                if base in original and sets_sp_first(original[base]):
                    # (the code sets SP itself before it uses it: where the stack was does not matter)
                    continue
                if (base, sp) in copies or (base in start and start[base] == sp):
                    target = copies.get((base, sp), base)
                    stack.redirect(block.end, successor, target)
                    continue
                if base not in original or len(copies) >= MAX_STACK_COPIES:
                    raise stack.StackError(f"{function.name}: SP {start[successor]} and {sp} meet at {successor}")
                places = {start[base]} | {place for (copied, place) in copies if copied == base}
                if len(places) >= MAX_DEPTHS:
                    lost_label = ("lost", first_int(base), len(copies) + 1)
                    where = first_int(base)
                    lost = ir.Effect("lost_stack", (ir.const(where & 0xFFFFFF),))
                    lost.origin = where
                    end = ir.Return()
                    end.origin = where
                    function.blocks[lost_label] = type(original[base])(lost_label, [lost], end, [])
                    copies[(base, sp)] = lost_label
                    start[lost_label] = sp
                    done.add(lost_label)
                    stack.redirect(block.end, successor, lost_label)
                    continue
                clone = ("stack", first_int(base), len(copies) + 1)
                twin = copy.deepcopy(original[base])
                twin.label = clone
                twin.stack_copy_of = base
                function.blocks[clone] = twin
                copies[(base, sp)] = clone
                stack.redirect(block.end, successor, clone)
                successor = clone
            if successor in start:
                continue
            start[successor] = sp
            pending.append(successor)
    if uneven and not copies:
        block, sp = uneven[0]
        raise stack.StackError(f"{function.name}: returns with SP {sp:+d} from where it started")
    if not copies:
        above = []    # (as the core: only where the stack took two ways is a way taken as one the game never goes)
    else:
        # (with copies, a return 4 bytes up is the pop without the push too, not a return for the caller as well:
        # 02FFCE's "movea.l (A7)+, A5" on the way out after player 1's pass, which pushed nothing)
        for label, block in function.blocks.items():
            if getattr(block.end, "leaves_caller", False) and label in done and block not in above:
                above.append(block)
    for block in above:
        # (the other way the game never goes: the pop without the push. The block stops before it)
        where = block.addresses[0] if block.addresses else first_int(block.label)
        lost = ir.Effect("lost_stack", (ir.const(where & 0xFFFFFF),))
        lost.origin = where
        block.statements = [lost]
        block.end = ir.Return()
        block.end.origin = where
    if above:
        function.exits_caller = any(getattr(block.end, "leaves_caller", False) for block in function.blocks.values())
    reached, work = set(), [function.entry]
    while work:
        label = work.pop()
        if label not in reached:
            reached.add(label)
            work += function.blocks[label].successors()
    function.blocks = {label: block for label, block in function.blocks.items() if label in reached}
    for label in list(start):
        if label not in reached:
            del start[label]
    split_mixed_slots(function)
    cut = {id(block) for block in above}
    for block, sp in uneven:
        if id(block) in cut or block.label not in reached:
            continue
        # (a way the game never goes: with the stack in two places, the code takes the same test twice, and a
        # static pass sees the two answers mixed (00BA86's push, then no pop). The core's lost_stack(): the program
        # would go on with a stack the C does not have, so it stops)
        where = block.addresses[-1] if block.addresses else first_int(block.label)
        lost = ir.Effect("lost_stack", (ir.const(where & 0xFFFFFF),))
        lost.origin = where
        block.statements.append(lost)
    return start


def install():
    from m68000 import analysis
    from m68000 import disassembler as dis
    from m68000 import function
    import recompile

    found_class = function.Found
    program_class = function.Program
    if getattr(found_class, "_call_tables_installed", False):
        return
    original_computed = found_class.computed
    original_targets = found_class.targets
    original_successors = program_class.successors
    original_source_values = found_class.source_values

    # ---- 1. calls and jumps into tables of stubs ----

    def places(instruction, state):
        operand = instruction.operands[0]
        bases = set()
        if operand.mode == "pc_index":
            bases.add((operand.base + operand.disp) & 0xFFFFFF)
        elif operand.mode == "index" and state is not None:
            for kind, value in analysis.known(state.get(operand.reg)):
                if kind == analysis.CONSTANT:
                    bases.add((value + operand.disp) & 0xFFFFFF)
        return bases

    def computed(self, instruction, state, block):
        original_computed(self, instruction, state, block)
        bases = places(instruction, state)
        manual = MANUAL.get(instruction.address)
        if not bases and manual is None:
            return
        self.__dict__.setdefault("table_places", {}).setdefault(instruction.address, set()).update(bases)
        kind = "func" if instruction.flow == dis.FLOW_CALL else "loc"
        entries = set(manual()) if manual is not None else set()
        for base in bases:
            entries.update(stub_addresses(base))
        self.in_computed = True       # (the places the core's Program makes functions of)
        try:
            for address in sorted(entries):
                self.enter(address, state, kind)
        finally:
            self.in_computed = False

    def targets(self, address):
        found = set(original_targets(self, address))
        for base in self.__dict__.get("table_places", {}).get(address, ()):
            found.update(stub_addresses(base))
        manual = MANUAL.get(address)
        if manual is not None:
            found.update(manual())
        return sorted(target for target in found if target in self.instructions)

    def source_values(self, operand, size, state):
        # (1b. "movea.l (d,PC,Dn), An" reads an entry of a table of code addresses (007830, 007934): the core reads
        # those only through a register; here only for the tables symbols.txt describes, so no data is taken for code)
        if operand.mode == "pc_index" and size == "l":
            base = (operand.base + operand.disp) & 0xFFFFFF
            if self.layout_of(base) is not None:
                return frozenset({(analysis.ENTRY, base)})
        return original_source_values(self, operand, size, state)

    # ---- 2. switches inside a frame stay in their function ----

    def successors(self, instruction):
        following = original_successors(self, instruction)
        cases = getattr(self, "inline_switches", {}).get(instruction.address)
        return following + sorted(cases) if cases else following

    def inline_switches(self):
        """{computed jmp made with the stack away from where its function started: the places it goes}"""
        found = {}
        jumps = [address for address, instruction in self.analysis.instructions.items()
                 if instruction.mnemonic == "jmp" and instruction.target is None
                 and address not in self.register_returns]
        for address in jumps:
            cases = self.analysis.targets(address)
            if not cases:
                continue
            for entry, body in self.bodies.items():
                if address in body:
                    depth = depths(self, entry, body).get(address)
                    if depth:
                        found[address] = set(cases)
                        break
        return found

    def keep_switches_inside(self, symbols):
        """The cases of the switches made inside a frame, and the code entries they run into, are no functions"""
        self.inline_switches = {}
        self.bodies = {entry: self.walk(entry) for entry in sorted(self.entries)}
        inline = inline_switches(self)
        if not inline:
            return
        self.inline_switches = inline
        calls = set()
        for address, instruction in self.analysis.instructions.items():
            if instruction.mnemonic == "jsr" and instruction.target is None:
                calls.update(self.analysis.targets(address))
        keep = (set(self.handlers) | set(self.analysis.functions) | set(self.register_routines) | calls
                | {address for address, symbol in symbols.items() if symbol.kind == "func"})
        drop = set()
        work = deque(address for cases in inline.values() for address in cases)
        seen = set()
        while work:
            address = work.popleft()
            if address in seen or address in keep:
                continue
            seen.add(address)
            if address in self.entries:
                drop.add(address)
            instruction = self.analysis.instructions.get(address)
            if instruction is not None:
                work.extend(self.successors(instruction))
        self.entries -= drop

    def program_init(self, image, symbols):
        # the core's Program.__init__ (m68000/function.py), with keep_switches_inside() before the bodies are walked
        self.image = image
        self.mem = dis.FlatMemory(image)
        self.symbols = symbols
        self.analysis = function.Found(image, symbols)
        self.analysis.run()
        self.analysis.pointer_writes = []
        self.analysis.table_writes = []
        self.analysis.pointer_tables = {}
        self.analysis.table_targets = {}
        instructions = self.analysis.instructions
        self.handlers = [function.key(self.mem.r32(4 * vector)) for vector in function.VECTORS]
        # (4. a vector that holds no code, as DonPachi's 212 zero ones do, stands for the reset: the C writer goes through
        # every vector's handler and has none at 000000; what each vector is matters only for the interrupts')
        self.handlers = [handler if handler in instructions else self.handlers[0] for handler in self.handlers]
        self.inline_switches = {}
        self.find_register_returns()

        entries = set(self.analysis.functions) | self.analysis.computed_targets
        entries.update(self.handlers)
        for table in self.analysis.code_tables.values():
            entries.update(table)
        for table in self.analysis.field_tables.values():
            entries.update(table)
        entries.update(address for address, symbol in symbols.items() if symbol.kind in ("func", "code"))
        entries -= set(self.return_points)
        entries |= set(self.register_routines)
        self.entries = {address for address in entries if address in instructions}
        keep_switches_inside(self, symbols)

        self.bodies = {entry: self.walk(entry) for entry in sorted(self.entries)}
        covered = set().union(*self.bodies.values())
        self.orphans = sorted(address for address in instructions if address not in covered)
        while True:
            covered = set().union(*self.bodies.values())
            starts = [address for address in self.orphans if address not in covered]
            if not starts:
                break
            self.entries.add(starts[0])
            self.bodies[starts[0]] = self.walk(starts[0])
        self.split_shared_code()
        self.names = self.choose_names()
        self.unreached = {address for address, symbol in symbols.items()
                          if symbol.kind == "func" and symbol.name.startswith("unreached_") and address in self.entries}
        self.second_entries = {}
        self.inner_entries = {}

    # ---- 3. shared code split off only where every routine that runs into it has an even stack ----

    def split_shared_code(self):
        # the core's (recompile.py), with the shared code that a routine reaches with registers on the stack kept copied
        original = set(self.entries)
        rejected = set()
        while True:
            owners = {}
            for entry, body in self.bodies.items():
                for address in body:
                    owners.setdefault(address, set()).add(entry)
            starts = set()
            for entry, body in self.bodies.items():
                for address, instruction in body.items():
                    for successor in self.successors(instruction):
                        if successor in body and len(owners[successor]) > 1 \
                                and owners[successor] != owners[address]:
                            starts.add(successor)
            starts -= rejected
            small = {start for start in starts if len(self.walk(start)) < recompile.SHARED_MINIMUM}
            uneven = {start for start in starts - small
                      if any(depths(self, entry, self.bodies[entry]).get(start, 0) != 0 for entry in owners[start])}
            rejected |= small | uneven
            starts -= small | uneven
            if not starts:
                return
            self.entries |= starts
            self.bodies = {entry: self.walk(entry) for entry in sorted(self.entries)}
            looping = {entry for component in self.tail_call_cycles() for entry in component} - original
            if looping:
                rejected |= looping
                self.entries -= looping
                self.bodies = {entry: self.walk(entry) for entry in sorted(self.entries)}

    original_lift = function.Builder.lift

    def lift(self, function_object, block, instruction):
        address = instruction.address
        reset = self.program.handlers[0]
        if instruction.mnemonic in ("bra", "jmp") and instruction.target is not None \
                and function.key(instruction.target) == reset and function_object.entry != reset:
            # (8. a soft reset: see soft_reset())
            soft_reset(self, function_object, block, instruction, reset)
            return None
        if address in UNTAKEN_JUMPS and instruction.mnemonic == "jmp" and not self.analysis.targets(address):
            # (7. a jump the game never makes: see UNTAKEN_JUMPS)
            from v60 import ir
            stop = ir.Effect("lost_stack", (ir.const(address),))
            stop.origin = address
            block.statements.append(stop)
            block.end = ir.Return()
            return None
        return original_lift(self, function_object, block, instruction)

    original_build = function.Builder.build

    def build(self, entry):
        built = original_build(self, entry)
        never_back(built)
        if entry == self.program.handlers[0] or entry in CRASH_HANDLERS:
            # (8. the registers hold nothing the game uses when the reset code starts; CRASH_HANDLERS)
            from v60 import ir
            start = []
            for index in range(15):
                start.append(ir.SetReg(index, ir.const(0)))
            for name in lift_flags():
                start.append(ir.SetFlag(name, ir.const(0, 1)))
            for statement in start:
                statement.origin = entry
            first = built.blocks[built.entry]
            first.statements[:0] = start
        return built

    function.Builder.lift = lift
    function.Builder.build = build
    found_class.computed = computed
    found_class.targets = targets
    found_class.source_values = source_values
    program_class.successors = successors
    program_class.__init__ = program_init
    program_class.split_shared_code = split_shared_code
    from decompiler import stack as decompiler_stack
    decompiler_stack.slots = stack_slots
    found_class._call_tables_installed = True

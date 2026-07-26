# vim:tw=80:ft=pnlc:syn=ps1:
# inc:../forth.pnlc

# read brainfuck commands from `stdin` until '!' or EOF. unbalanced brackets
# in the input stream will blow things up. compile the commands to bytecode
# then interpret the bytecode. every bytecode instruction is a one-byte opcode
# followed by a one-byte operand. there are six opcodes:
#    OPCODE                           OPERATION
#     '>'    offset head pointer by operand
#     '?'    offset byte at head by operand
#     '.'    write byte at head to `stdout`, ignoring operand
#     ','    read byte from `stdin` and store at head, ignoring operand
#     '('    unconditionally set instruction pointer to operand
#     ']'    set instruction pointer to operand if byte at head is nonzero

# initialize the instruction pointer to $00 and push a dummy no-op command '\0'
# to start the loop
..lit $00 ..lit '\0'
..repeat
  ..lit $00
  .over ..lit '>' .et .or
  .over ..lit '<' .et .or
  .over ..lit '+' .et .or
  .over ..lit '-' .et .or
  ...either
    # the opcode is the command bitwise ORed with '>'. this way, commands '>'
    # and '<' get opcode '>', and commands '+' and '-' get opcode '?'
    .over .over ..lit '>' .or .swap .store
    # consume commands while they're equal to the current command. for commands
    # '>' and '+' the opcode is the count, and for commands '<' and '-' it is
    # the negative of the count. the second-least significant bit differentiates
    # the cases: '>' and '+' have it set while '<' and '-' have it clear
    ..lit $00 ..repeat
      .over ..lit $02 .and .dec .add # +1 or -1 depending on second bit
      .get .rot .over .et .rot .swap # loop while equal to current command
    end
    .rot .inc .tuck .store .inc .swap end
  ..lit $00
  .over ..lit '.' .et .or
  .over ..lit ',' .et .or
  .over ..lit '[' .et .or
  ...either
    # these commands are their own opcodes and ignore their operands. a '['
    # instruction will be converted to a '(' instruction when the matching
    # ']' is read
    .over .store .inc .inc .get end
  .dup ..lit ']' .et ...either
    .over .store .dup
    # find the matching '[' and replace it with '(' to hide it from future
    # searches. this way, "the matching '['" is always just the latest '['
    ..repeat .dec .dec .dup .load ..lit '[' .xor end
    ..lit '(' .over .store
    # the operand to '(' is a pointer to the instruction before the ']' and
    # the operand to ']' is a pointer to the '('
    .over .dec .dec .over .inc .store
    .over .inc .store .inc .inc .get end
  .drop .get end end end
  # if the next command is '!' or EOF, break
  .dup .dup ..lit '!' .et .not .and end
# increment the instruction pointer one last time to effectively append a '\0'
# instruction that will halt the program. now the instruction pointer becomes
# the head pointer and a new instruction pointer is allocated with value $00
.drop .inc ..lit $00 .swap
..repeat
  .over .load
  .dup ..lit '>' .et ...either .drop
    .over .inc .load .add end
  .dup ..lit '?' .et ...either .drop
    .over .inc .load .over .load .add .over .store end
  .dup ..lit '.' .et ...either .drop
    .dup .load .put end
  .dup ..lit ',' .et ...either .drop
    .get .over .store end
  .dup ..lit '(' .et ...either .drop
    .swap .inc .load .swap end
  .dup ..lit ']' .et ...either .drop
    .dup .load ..then .swap .inc .load .swap end end
  # any unknown instructions, including '\0' and an unmatched '[', halt the
  # program. we halt by setting the head pointer to $00 because it's normally
  # never $00
  .drop .drop ..lit $00 end end end end end end
  # increment the instruction pointer. if the head pointer was set to $00, break
  .swap .inc .inc .swap .dup end
end

# vim:tw=80:ft=pnlc:syn=ps1:
# inc:../forth.pnlc

# Collatz conjecture, produces 0 on u8 overflow. among 7-bit inputs, the longest
# sequences that reach one are  r 9 V + A B !  and the longuest sequences that
# end in overflow are  l 6 ^[ R ) | > I

.get ...while
  .dup ..lit $01 .gt end
  .dup .dup ..lit $01 .and ...either
    .dup ..lit $55 .lt .swap # 3n+1
      .dup .shl .add .inc .and end
    .shr end # n/2
  end
.dump end

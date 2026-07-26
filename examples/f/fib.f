# vim:tw=80:ft=pnlc:syn=ps1:
# inc:../forth.pnlc

# Fibonacci sequence until u8 overflow

..lit $00 ..lit $01 ..repeat
  .over .over .add
  .over .over .gt .not end
.drop .dump end

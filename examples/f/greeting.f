# vim:tw=80:ft=pnlc:syn=ps1:
# inc:../forth.pnlc

..repeat
  ..lit  'E' .put
  ..lit  'n' .put
  ..lit  't' .put
  ..lit  'e' .put
  ..lit  'r' .put
  ..lit .' ' .put
  ..lit  'y' .put
  ..lit  'o' .put
  ..lit  'u' .put
  ..lit  'r' .put
  ..lit .' ' .put
  ..lit  'n' .put
  ..lit  'a' .put
  ..lit  'm' .put
  ..lit  'e' .put
  ..lit  ':' .put
  ..lit .' ' .put
  # read until '\0' (EOF) or '\n'
  ..lit $00 ..repeat
    .get .tuck .over .store .inc .swap
    .dup ..lit '\n' .et .not .and end
  # if input is neither "" (EOF) nor "\n" (empty), greet
  ..lit $00 .load
  .dup ..lit '\n' .et .not .and ..then
    ..lit  'G' .put
    ..lit  'r' .put
    ..lit  'e' .put
    ..lit  'e' .put
    ..lit  't' .put
    ..lit  'i' .put
    ..lit  'n' .put
    ..lit  'g' .put
    ..lit  's' .put
    ..lit  ',' .put
    ..lit .' ' .put
    ..lit $00 ...while
      .over .over .xor end
      .dup .load .put .inc end
    .drop .drop end
  # if input is "" (EOF), break
  ..lit $00 .load end
end

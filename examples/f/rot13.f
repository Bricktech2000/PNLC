# vim:tw=80:ft=pnlc:syn=ps1:
# inc:../forth.pnlc

..repeat .get .dup
  .dup ..lit .' ' .or ..lit 'a' .sub
  .dup ..lit $1a .lt ..then
    ..lit '\r' .lt ...either
      ..lit '\r' .add end
      ..lit '\r' .sub end
    .dup end
  .drop .put end
end

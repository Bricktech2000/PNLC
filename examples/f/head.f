# vim:tw=80:ft=pnlc:syn=ps1:
# inc:../forth.pnlc

# outputs the first ten lines of input

..lit $0a ...while
  .dup ...either .get end ..lit '\0' end .dup end
  .dup .put ..lit '\n' .et .add end
.drop .drop end

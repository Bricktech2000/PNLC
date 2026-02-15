setlocal commentstring=#\ %s
setlocal comments=b:#
setlocal indentexpr=-1 indentkeys=
let &l:define = '\sdef:' . '\|^\(\S.*\)\?\\\ze\S*\(\s\+\(#.*\)\?\)\?$'
let &l:include = '\sinc:'

" keep in sync with grammar.bnf and pnlc.c

syntax match pnlcLam '\\\_[[:space:]]*[^[:space:]]\+\_[[:space:]]\+'
syntax match pnlcIgn '\_[[:space:].]\@<=\\\_[[:space:]]*[.\#]\@=[^[:space:]]\+\_[[:space:]]\+'
" keep in sync with pnlc.c, examples/pnlc.pnlc, README.md and io\ hook.pnlc
syntax match pnlcIO '\_[[:space:].]\@<=\(\$exit\|\$err\|\$get\|\$put\|\$eput\|\$dump\)\_[[:space:]]\+'
syntax match pnlcApp0 '
      \\(\(\.\_[[:space:]]*[.\#]\@!\|\\\_[[:space:]]*\)[^[:space:]]\+\_[[:space:]]\+\)*
      \[.\#]\@![^[:space:]]\+
      \' contains=pnlcIgn,pnlcIO
syntax match pnlcApp1 '
      \\(\(\.\_[[:space:]]*[.\#]\@!\|\\\_[[:space:]]*\)[^[:space:]]\+\_[[:space:]]\+\)*
      \\.\_[[:space:]]*
      \\.\_[[:space:]]*
      \[.\#]\@![^[:space:]]\+\_[[:space:]]\+
      \[.\#]\@![^[:space:]]\+\_[[:space:]]\+
      \[.\#]\@![^[:space:]]\+\_[[:space:]]\+
      \' contains=pnlcIgn,pnlcIO
syntax match pnlcApp2 '
      \\(\(\.\_[[:space:]]*[.\#]\@!\|\\\_[[:space:]]*\)[^[:space:]]\+\_[[:space:]]\+\)*
      \\.\_[[:space:]]*
      \\.\_[[:space:]]*
      \\.\_[[:space:]]*
      \[.\#]\@![^[:space:]]\+\_[[:space:]]\+
      \[.\#]\@![^[:space:]]\+\_[[:space:]]\+
      \[.\#]\@![^[:space:]]\+\_[[:space:]]\+
      \[.\#]\@![^[:space:]]\+\_[[:space:]]\+
      \' contains=pnlcIgn,pnlcIO
syntax match pnlcApp3 '
      \\(\(\.\_[[:space:]]*[.\#]\@!\|\\\_[[:space:]]*\)[^[:space:]]\+\_[[:space:]]\+\)*
      \\.\_[[:space:]]*
      \\.\_[[:space:]]*
      \[.\#]\@![^[:space:]]\+\_[[:space:]]\+
      \\.\_[[:space:]]*
      \[.\#]\@![^[:space:]]\+\_[[:space:]]\+
      \[.\#]\@![^[:space:]]\+\_[[:space:]]\+
      \[.\#]\@![^[:space:]]\+\_[[:space:]]\+
      \' contains=pnlcIgn,pnlcIO
syntax match pnlcApp4 '
      \\(\(\.\_[[:space:]]*[.\#]\@!\|\\\_[[:space:]]*\)[^[:space:]]\+\_[[:space:]]\+\)*
      \\.\_[[:space:]]*
      \\.\_[[:space:]]*
      \[.\#]\@![^[:space:]]\+\_[[:space:]]\+
      \[.\#]\@![^[:space:]]\+\_[[:space:]]\+
      \\.\_[[:space:]]*
      \[.\#]\@![^[:space:]]\+\_[[:space:]]\+
      \[.\#]\@![^[:space:]]\+\_[[:space:]]\+
      \' contains=pnlcIgn,pnlcIO
syntax match pnlcVar '[.\#]\@![^[:space:]]\+' contains=pnlcIO
syntax match pnlcComment '#.*$' contains=pnlcTodo
syntax keyword pnlcTodo TODO FIXME XXX NOTE contained

" mostly the same order as vim/runtime/syntax/csv.vim
highlight default link pnlcLam Statement
highlight default link pnlcIgn Comment
highlight default link pnlcIO Constant
highlight default link pnlcApp0 Type
highlight default link pnlcApp1 PreProc
highlight default link pnlcApp2 Identifier
highlight default link pnlcApp3 Special
highlight default link pnlcApp4 String

" make sure \.# within variables aren't treated as the start of a
" pnlcAppX, and make sure lone variables aren't treated as a pnlcApp0
highlight default link pnlcVar NONE

highlight default link pnlcComment Comment
highlight default link pnlcTodo Todo

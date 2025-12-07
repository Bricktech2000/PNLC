setlocal commentstring=#\ %s
setlocal comments=b:#
setlocal define=\\sdef:\\\|^\\(\\S.*\\)\\?\\\\\\ze\\S*\\s*$
setlocal include=\\sinc:

syntax match pnlcLam '\\\_s*#\@![[:graph:]]\+'
syntax match pnlcLamC '\\\_s*#\@![\.$]\@=[[:graph:]]\+'
syntax match pnlcIO '\$[[:graph:]]\+'
syntax match pnlcApp0 '
      \\(\(\.\_s*[\.#]\@!\|\\\_s*#\@!\)[[:graph:]]\+\_s\+\)*
      \[\.#]\@![[:graph:]]\+
      \' contains=pnlcLamC,pnlcIO
syntax match pnlcApp1 '
      \\(\(\.\_s*[\.#]\@!\|\\\_s*#\@!\)[[:graph:]]\+\_s\+\)*
      \\.\_s*
      \\.\_s*
      \[\.#]\@![[:graph:]]\+\_s\+
      \[\.#]\@![[:graph:]]\+\_s\+
      \[\.#]\@![[:graph:]]\+\_s\+
      \' contains=pnlcLamC,pnlcIO
syntax match pnlcApp2 '
      \\(\(\.\_s*[\.#]\@!\|\\\_s*#\@!\)[[:graph:]]\+\_s\+\)*
      \\.\_s*
      \\.\_s*
      \\.\_s*
      \[\.#]\@![[:graph:]]\+\_s\+
      \[\.#]\@![[:graph:]]\+\_s\+
      \[\.#]\@![[:graph:]]\+\_s\+
      \[\.#]\@![[:graph:]]\+\_s\+
      \' contains=pnlcLamC,pnlcIO
syntax match pnlcApp3 '
      \\(\(\.\_s*[\.#]\@!\|\\\_s*#\@!\)[[:graph:]]\+\_s\+\)*
      \\.\_s*
      \\.\_s*
      \[\.#]\@![[:graph:]]\+\_s\+
      \\.\_s*
      \[\.#]\@![[:graph:]]\+\_s\+
      \[\.#]\@![[:graph:]]\+\_s\+
      \[\.#]\@![[:graph:]]\+\_s\+
      \' contains=pnlcLamC,pnlcIO
syntax match pnlcApp4 '
      \\(\(\.\_s*[\.#]\@!\|\\\_s*#\@!\)[[:graph:]]\+\_s\+\)*
      \\.\_s*
      \\.\_s*
      \[\.#]\@![[:graph:]]\+\_s\+
      \[\.#]\@![[:graph:]]\+\_s\+
      \\.\_s*
      \[\.#]\@![[:graph:]]\+\_s\+
      \[\.#]\@![[:graph:]]\+\_s\+
      \' contains=pnlcLamC,pnlcIO
syntax match pnlcVar '[\.#]\@![[:graph:]]\+' contains=pnlcIO
syntax match pnlcComment '#.*$' contains=pnlcTodo
syntax keyword pnlcTodo TODO FIXME XXX NOTE contained

" mostly the same order as vim/runtime/syntax/csv.vim
highlight default link pnlcLam Statement
highlight default link pnlcLamC Comment
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

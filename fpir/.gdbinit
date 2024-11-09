set debuginfod enabled off
set record full insn-number-max unlimited
set confirm off

define dump_cons
  printf "cons: "
  x/2xg $arg0
  printf "\n"
end

define print_cons
  if $arg0 == -1
    printf "-1"
  else
    if (*($arg0)&0xf) == 0
      print_cons *(($arg0&~0xf))
      print_cons *($arg0+8)
    else
      dump_cons $arg0
    end
  end
end

alias pP = dump_cons return_stack.car&(~0xf)
alias pB = dump_cons (((ulong*)(return_stack.car&(~0xf)))[0])&(~0xf)
alias pC = dump_cons ((ulong*)((((ulong*)(return_stack.car&(~0xf)))[0])&(~0xf)))[0]&(~0xf)

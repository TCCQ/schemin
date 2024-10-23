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

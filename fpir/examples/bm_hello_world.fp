(:x x) :force
(cswap drop force) :if
(:f (:x ($x x) f) (:x ($x x) f) force) :fix
(:g ($g fix)) :rec
(:self :n $n 1 sub (self) (drop 'done print) $n print $n 0 eq if) rec :count

(
  268435456 :ubase
  0 $ubase 1 add store_b
  128 $ubase 3 add store_b
  3 $ubase store_b
  0 $ubase 1 add store_b
  3 $ubase 3 add store_b
  7 $ubase 2 add store_b
  $ubase
) :uartinit
(
  uartinit
  :ubase
  ($ubase store_b)
) :defuwrite
defuwrite :putc
42 43 42 putc putc putc

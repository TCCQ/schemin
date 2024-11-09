(:x x) :force
(cswap drop force) :if
(:f (:x ($x x) f) (:x ($x x) f) force) :fix
(:g ($g fix)) :rec
(:self :n $n 1 sub (self) (drop 'done print) $n print $n 0 eq if) rec :count

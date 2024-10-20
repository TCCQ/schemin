(:x x) :force
(cswap drop force) :if
(:f (:x ($x x) f) (:x ($x x) f) force) :fix
(:g ($g fix)) :rec
(:self :n $n 1 add dup print self) rec :count

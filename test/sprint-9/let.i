(begin
    (let x 1)
    (let y 1)
    (+ x y)
)

(set k 2)
(define func (x) (begin
    (let k (/ x 2))
    (/ k 2)
))

(func 2)
(func 3)
(func 4)

k

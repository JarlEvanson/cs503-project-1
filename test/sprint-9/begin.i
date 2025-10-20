(begin
    (set x 'y)
    (set y 1)
    (eval x)
)

(begin
    1
    2
    3
)

(define func (x) (begin
    (set a 'b)
    (set b x)
    (eval a)
))

(func 1)
(func 2)

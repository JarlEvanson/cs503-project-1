(define map (func list) (cond
    ((nil? list) ())
    ('t (cons (funcall func (car list)) (map func (cdr list))))
))
(define square (x) (* x x))

(map (lambda (val) (* val val)) '(1 2 3))
(map (function square) '(1 2 3))

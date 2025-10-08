(define map (func list) (cond
    ((nil? list) ())
    ('t (cons (funcall func (car list)) (map func (cdr list))))
))

(map (lambda (x) (* x x)) '(1 2 3))

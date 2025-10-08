(define map (func list) (cond
    ((nil? list) ())
    ('t (cons (funcall func (car list)) (map func (cdr list))))
))

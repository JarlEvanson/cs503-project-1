(cond
    (() (car '(1 2 3)))
    ((cdr '(1 2)) (add 1 2))
    ((div 1 2) (mul 100 200)))
(cond
    (1 (car '(1 2 3)))
    ((cdr '(1 2)) (add 1 2))
    ((div 1 2) (mul 100 200)))
(cond
    (() (car '(1 2 3)))
    ((cdr '(1)) (add 1 2))
    ((div 1 2) (mul 100 200)))

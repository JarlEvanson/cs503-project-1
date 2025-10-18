(define caar (L) (car (car L)))
(define cadr (L) (car (cdr L)))
(define cddr(L) (cdr (cdr L)))
(define caddr (L) (car (cddr L)))
(define cdddr(L) (cdr (cddr L)))
(define list3 (a b c)
    (cons a (cons b (cons c ())))
)

(define insertionsort(L) (INSERTSORT L () ))

(define INSERTSORT(unsorted sorted)(cond
    ((nil? unsorted) sorted)
    ('t  (INSERTSORT
            (cdr unsorted)
            (insert (car unsorted) sorted)
        )
    )
))

(define insert(x L)(cond
    ((nil? L) (cons x ()); tail)
    ((lte x (car L)) (cons x L); head)
    ('t  (cons (car L) (insert x (cdr L))); middle)
))








(define quicksort(L)(cond
    ((nil? L) ())
    ((nil? (cdr L)) L)
    ('t (QSORT (partition (car L) (cdr L) () ()) ))
))

(define partition(p L smaller larger)(cond
    ((nil? L) (list3 smaller p larger))
    ((lte (car L) p)
        (partition p (cdr L) (cons (car L) smaller) larger))
    ('t
        (partition p (cdr L) smaller (cons (car L) larger)))
))

(define QSORT(P)
    (QKSORT (car P) (cadr P) (caddr P))
)

(define QKSORT(left p right)
    (append (quicksort left) (cons p (quicksort right)))
)

(define append (L M)(cond
    ((nil? L) M)
    ('t (cons (car L) (append (cdr L) M)))
))













(define mergesort (L)(cond
    ((nil? L) ())
    ((nil? (cdr L)) L)
    ('t (mergelevel (makelists L)))
))

(define makelists (L) (cond
    ((nil? L) ())
    ('t (cons
        (cons (car L) ())
        (makelists (cdr L))
    ))
))


(define mergelevel(LL)(cond
    ((nil? LL) ())
    ((nil? (cdr LL)) (car LL))
    ('t (mergelevel (mergepairs LL)))
))


(define mergepairs(LL)(cond
    ((nil? LL) ())
    ((nil? (cdr LL)) LL)
    ('t (cons (merge (car LL) (cadr LL)) (mergepairs (cddr LL)) ))
))


(define merge (L M)(cond
    ((nil? L) M)
    ((nil? M) L)
    ((lte (car L) (car M))
        (cons (car L) (merge (cdr L) M)))
    ('t  (cons (car M) (merge L (cdr M))))
))

'insertionsort
(insertionsort ())
(insertionsort '(1))
(insertionsort '(2 1))
(insertionsort '(3 2 1 4 5 6 9 8 7))
(insertionsort '(38 72 61 45 54 36 92 18 70 93 28 71 46 55 46 93 82 17))

'quicksort
(quicksort ())
(quicksort '(1))
(quicksort '(2 1))
(quicksort '(3 2 1 4 5 6 9 8 7))
(quicksort '(38 72 61 45 54 36 92 18 70 93 28 71 46 55 46 93 82 17))

'mergesort
(mergesort ())
(mergesort '(1))
(mergesort '(2 1))
(mergesort '(3 2 1 4 5 6 9 8 7))
(mergesort '(38 72 61 45 54 36 92 18 70 93 28 71 46 55 46 93 82 17))

import itertools

f = [
    '01001',
    '01010',
    '01011',
    '01110',
    '01111',
    '10010',
    '10011',
    '10100',
    '10101',
    '10110',
    '10111',
    '11010',
    '11011',
    '11100',
    '11101',
    '11110'
]

naturally_occur = set(''.join(c) for c in itertools.product(f, f))

zo = ['0', '1']

possibilities = (''.join(c) for c in itertools.product(zo, zo, zo, zo, zo))

preambles = [pre for pre in possibilities if all(pre not in occ for occ in naturally_occur)]

print(preambles)

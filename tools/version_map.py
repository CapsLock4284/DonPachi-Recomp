"""
Name:        version_map.py
Description: where each part of the US program is in another version's program (the Japanese, 'donpachij'): the two
             programs aligned word by word, for carrying symbols.txt's names over (tools/decompile.py --set japan)
Version:     0.1.0 (draft)
Created:     13-11-2026

The alignment is a patience diff: runs of K words found once in each program are matched in order (the longest
increasing run of them), then each gap between matches is aligned the same way, and gaps small enough by difflib;
the matched words are then extended forwards and back while they stay the same. A US address maps to the other
program where it lies in a matched run (the same code or data, moved); elsewhere it has none (the code differs).
"""
import bisect
import difflib
from collections import Counter

K = 6                 # the words of an anchor
SMALL = 2_000_000     # a gap aligned by difflib when its two sides' product is at most this


def words(image):
    return [image[i:i + 2] for i in range(0, len(image) - 1, 2)]


def _anchors(a, alo, ahi, b, blo, bhi, K=K):
    """(i, j) of the K-word runs found once in a[alo:ahi] and once in b[blo:bhi], in order of i, and an
    increasing subsequence of them in j (the longest)."""
    def unique(seq, lo, hi):
        count = Counter(tuple(seq[i:i + K]) for i in range(lo, hi - K + 1))
        return {tuple(seq[i:i + K]): i for i in range(lo, hi - K + 1) if count[tuple(seq[i:i + K])] == 1}
    ua, ub = unique(a, alo, ahi), unique(b, blo, bhi)
    pairs = sorted((i, ub[key]) for key, i in ua.items() if key in ub)
    # the longest increasing subsequence in j
    tails, tail_index, previous = [], [], [None] * len(pairs)
    for n, (_, j) in enumerate(pairs):
        at = bisect.bisect_left(tails, j)
        if at == len(tails):
            tails.append(j)
            tail_index.append(n)
        else:
            tails[at] = j
            tail_index[at] = n
        previous[n] = tail_index[at - 1] if at else None
    chain, n = [], tail_index[-1] if tail_index else None
    while n is not None:
        chain.append(pairs[n])
        n = previous[n]
    return chain[::-1]


def _align(a, alo, ahi, b, blo, bhi, out):
    if alo >= ahi or blo >= bhi:
        return
    if (ahi - alo) * (bhi - blo) <= SMALL:
        matcher = difflib.SequenceMatcher(None, a[alo:ahi], b[blo:bhi], autojunk=False)
        for block in matcher.get_matching_blocks():
            if block.size:
                out.append((alo + block.a, blo + block.b, block.size))
        return
    chain = _anchors(a, alo, ahi, b, blo, bhi)
    if not chain:
        chain = _anchors(a, alo, ahi, b, blo, bhi, 3)    # (a short gap's own words)
    if not chain:
        return
    i0, j0 = alo, blo
    for i, j in chain:
        if i < i0 or j < j0:
            continue
        _align(a, i0, i, b, j0, j, out)
        n = 0
        while i + n < ahi and j + n < bhi and a[i + n] == b[j + n]:
            n += 1    # (the anchor's run as far as it goes)
        out.append((i, j, n))
        i0, j0 = i + n, j + n
    _align(a, i0, ahi, b, j0, bhi, out)


def align(image_a, image_b):
    """The matched runs of words: [(word in a, word in b, words)], in order, merged where they run on."""
    a, b = words(image_a), words(image_b)
    runs = []
    _align(a, 0, len(a), b, 0, len(b), runs)
    runs.sort()
    # each run grown back and forth over the same words, up to its neighbours
    grown = []
    for index, (i, j, n) in enumerate(runs):
        low_a = grown[-1][0] + grown[-1][2] if grown else 0
        low_b = grown[-1][1] + grown[-1][2] if grown else 0
        while i > low_a and j > low_b and a[i - 1] == b[j - 1]:
            i, j, n = i - 1, j - 1, n + 1
        high_a = runs[index + 1][0] if index + 1 < len(runs) else len(a)
        high_b = runs[index + 1][1] if index + 1 < len(runs) else len(b)
        while i + n < high_a and j + n < high_b and a[i + n] == b[j + n]:
            n += 1
        if grown and (i < grown[-1][0] + grown[-1][2] or j < grown[-1][1] + grown[-1][2]):
            continue
        grown.append((i, j, n))
    runs = grown
    merged = []
    for i, j, n in runs:
        if merged and merged[-1][0] + merged[-1][2] == i and merged[-1][1] + merged[-1][2] == j:
            merged[-1] = (merged[-1][0], merged[-1][1], merged[-1][2] + n)
        else:
            merged.append((i, j, n))
    return merged


class AddressMap:
    """A US program address to the other version's, or None where the code differs."""

    def __init__(self, image_a, image_b):
        self.runs = align(image_a, image_b)
        self.starts = [i for i, _, _ in self.runs]

    def __call__(self, address):
        word = address >> 1
        at = bisect.bisect_right(self.starts, word) - 1
        if at < 0:
            return None
        i, j, n = self.runs[at]
        if not i <= word < i + n:
            return None
        return ((j + word - i) << 1) | (address & 1)

    def matched_words(self):
        return sum(n for _, _, n in self.runs)

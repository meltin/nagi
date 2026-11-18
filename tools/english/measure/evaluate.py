#!/usr/bin/env python3
"""Applies a rule to the numbers of prefix_collect.py, key by key, and counts
- inputs whose final display is wrong (Japanese shown as English, or the English word
  missed), and
- inputs and keys where English was shown on the way although it should not have been.

  python3 tools/english/measure/evaluate.py PREFIX.tsv

The rules tried here are a prototype for choosing thresholds. The IME's own detector
is measured with the same inputs once it exists.
"""
import csv, collections, sys
rows=collections.defaultdict(lambda: collections.defaultdict(list))  # input -> typed -> candidates
meta={}
for r in csv.DictReader(open(sys.argv[1]),delimiter='\t',quoting=csv.QUOTE_NONE):
    for k in ('input','typed','tail','begin','end','english_begin','english_end','scowl','reads','cost_kana','cost_english','cost_word','word_is_prefix'): r[k]=int(r[k])
    r['delta']=r['cost_kana']-r['cost_english']; r['len']=r['end']-r['begin']
    meta[r['input']]=(r['set'],r['keys'],(r['english_begin'],r['english_end']))
    rows[r['input']][r['typed']].append(r)

progress=collections.defaultdict(dict)
import os
if os.path.exists(sys.argv[1]+'.progress'):
    for r in csv.DictReader(open(sys.argv[1]+'.progress'),delimiter='\t',quoting=csv.QUOTE_NONE):
        progress[int(r['input'])][int(r['typed'])]=int(r['begin'])

def make_rule2(t_unread, readable):
    # readable: list of (min_len, min_cost_word, min_delta); any match makes it English
    def rule(c, body_len):
        if c['word']=='-': return False
        if c['reads']==0: return c['delta']>t_unread
        if c['end']==body_len and c['word_is_prefix']: return False
        return any(c['len']>=l and c['cost_word']>w and c['delta']>d for l,w,d in readable)
    return rule

def make_rule(t_unread, t_read, hold):
    def rule(c, body_len):
        if c['word']=='-': return False
        if c['reads']==0: return c['delta']>t_unread
        t=t_read.get(min(c['len'],5))
        if t is None or c['delta']<=t: return False
        if hold and c['end']==body_len and c['word_is_prefix']: return False
        return True
    return rule

def evaluate(rule, verbose=False):
    stats=collections.defaultdict(collections.Counter); examples=collections.defaultdict(list)
    for i,steps in rows.items():
        name,keys,english=meta[i]
        shown=[]  # per step: chosen (begin,end) or None
        for n in sorted(steps):
            cands=steps[n]; body_len=n-cands[0]['tail']
            ok=[c for c in cands if rule(c,body_len)]
            # The run that starts first wins; among those, the one that fits best.
            best=min(ok,key=lambda c:(c['begin'],-c['delta'])) if ok else None
            span=(best['begin'],best['end']) if best else None
            if n in progress.get(i,{}) and (span is None or progress[i][n]<=span[0]): span=(progress[i][n],n)
            shown.append((n,span))
        final=shown[-1][1] if shown and shown[-1][0]==len(keys) else None
        s=stats[name]; s['inputs']+=1; s['keys']+=len(shown)
        expected=english if english[0]>=0 else None
        if final!=expected:
            s['final_wrong']+=1
            if len(examples[name,'final'])<12: examples[name,'final'].append((keys,final and keys[final[0]:final[1]],expected and keys[expected[0]:expected[1]]))
        # transient: a step shows English that the expected answer does not contain
        bad_steps=[n for n,sp in shown if sp and not (expected and sp[0]==expected[0] and sp[1]<=expected[1])]
        if bad_steps:
            s['inputs_with_flicker']+=1; s['flicker_keys']+=len(bad_steps)
            if len(examples[name,'flicker'])<8: examples[name,'flicker'].append((keys,[ (n,keys[sp[0]:sp[1]]) for n,sp in shown if sp][:4]))
    for name in sorted(stats):
        s=stats[name]
        print(f"  {name:20s} inputs {s['inputs']:6d}  final wrong {s['final_wrong']:5d} ({100*s['final_wrong']/s['inputs']:.2f}%)  inputs with flicker {s['inputs_with_flicker']:5d} ({100*s['inputs_with_flicker']/s['inputs']:.2f}%)  flicker keys {100*s['flicker_keys']/max(1,s['keys']):.2f}%")
    if verbose:
        for k,v in examples.items(): print(' ',k); [print('    ',e) for e in v]

if __name__=='__main__':
    for label,rule in [
        ('E2 unread>-5500; readable (len4,word>12000,d>-10000) or (len4,word>9000,d>-6000); earliest run wins', make_rule2(-5500,[(4,12000,-10000),(4,9000,-6000)])),
    ]:
        print(label); evaluate(rule, verbose=True)

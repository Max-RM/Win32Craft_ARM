import pickle, sys, csv, collections
S = r"C:\Users\hamed\AppData\Local\Temp\claude\D--repo-Win32Craft-main\7ed8744f-0d72-482c-82d8-cd88a334a2ff\scratchpad"
P = r"D:\repo\Win32Craft-main\games_recon\bad_piggies_shader_proto\auto\all_programs.csv"
d = pickle.load(open(S + r"\out\shaders.pkl", "rb"))
rows = list(csv.DictReader(open(P, newline='', encoding='utf-8')))
need = {}
for r in rows:
    key = (int(r['shader_idx']), int(r['subshader']), int(r['pass']), r['stage'], int(r['blob']))
    need[key] = r
allreq = collections.Counter()
needreq = collections.Counter()
needreq_by = collections.defaultdict(set)
tiers = collections.Counter()
missing = 0
for si, sh in enumerate(d):
    pf = sh['parsed']
    for ssi, ss in enumerate(pf['subshaders']):
        for pi, p in enumerate(ss['passes']):
            for stage, lst in (('vs', p['vs']), ('ps', p['ps'])):
                for sp in lst:
                    req = sp['requirements']
                    allreq[req] += 1
                    k = (si, ssi, pi, stage, sp['blobIndex'])
                    r = need.get(k)
                    if r is None:
                        missing += 1
                        continue
                    if r['needed'] == '1':
                        needreq[req] += 1
                        needreq_by[req].add(pf['name'])
                        tiers[(sp['tier'], r['tiers'])] += 1
print('all subprogram requirement masks:', {hex(k): v for k, v in sorted(allreq.items())})
print('needed-program requirement masks:', {hex(k): v for k, v in sorted(needreq.items())})
for k in sorted(needreq_by):
    print(hex(k), sorted(needreq_by[k])[:40])
print('unmatched subprograms', missing)
print('tier pairs (sp.tier, csv tiers) for needed:', tiers.most_common(10))

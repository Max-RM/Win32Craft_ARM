import sys,re
def strip(path):
    out=[]; stack=[]  # each: [active_parent, kind('arm'/'other'), in_else]
    # active = emit lines
    emit=[True]
    lines=open(path,'rb').read().split(b'\n')
    for ln in lines:
        s=ln.strip()
        m=re.match(rb'#\s*(ifdef|ifndef|if|elif|else|endif)\b(.*)',s)
        if m:
            d,rest=m.group(1),m.group(2).strip()
            if d in(b'ifdef',b'ifndef',b'if'):
                arm = (d==b'ifdef' and rest.startswith(b'WIN32CRAFT_ARM32')) or (d==b'if' and rest in (b'defined(WIN32CRAFT_ARM32)',b'defined WIN32CRAFT_ARM32')) 
                notarm = (d==b'ifndef' and rest.startswith(b'WIN32CRAFT_ARM32')) or (d==b'if' and rest.startswith(b'!defined') and b'WIN32CRAFT_ARM32' in rest)
                if arm: stack.append(('arm',emit[-1])); emit.append(False); continue
                if notarm: stack.append(('notarm',emit[-1])); emit.append(emit[-1]); continue
                stack.append(('other',emit[-1])); emit.append(emit[-1]);
                if emit[-1]: out.append(ln)
                continue
            if d==b'else':
                k,par=stack[-1]
                if k=='arm': emit[-1]=par; continue
                if k=='notarm': emit[-1]=False; continue
                if emit[-1]: out.append(ln)
                continue
            if d==b'elif':
                k,par=stack[-1]
                if k=='other' and emit[-1]: out.append(ln)
                continue
            if d==b'endif':
                k,par=stack.pop(); emit.pop()
                if k=='other' and emit[-1]: out.append(ln)
                continue
        if emit[-1]: out.append(ln)
    return out
a=strip(sys.argv[1]); b=strip(sys.argv[2])
import difflib
d=list(difflib.unified_diff([x.decode('latin1') for x in a],[x.decode('latin1') for x in b],lineterm='',n=1))
print(len(d),'diff lines'); print('\n'.join(d[:80]))
